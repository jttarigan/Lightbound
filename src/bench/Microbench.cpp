#include "bench/Microbench.h"

#include "GitInfo.h"
#include "app/Version.h"
#include "bench/MicroPattern.h"
#include "bench/SysInfo.h"
#include "core/Log.h"
#include "core/Time.h"
#include "frame/MemoryPolicy.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

namespace lb::bench {

using namespace lb::gfx;

namespace {

constexpr u64 kWaitTimeoutNs = 5'000'000'000ull;
constexpr u32 kRecalibrateEvery = 500;
constexpr f64 kNaN = std::numeric_limits<f64>::quiet_NaN();

// Timestamp slots per iteration.
enum : u32 {
    kTsWriteBegin = 0, kTsWriteEnd, kTsCopyDownBegin, kTsCopyDownEnd,
    kTsCopyUpBegin, kTsCopyUpEnd, kTsReadBegin, kTsReadEnd, kTsCount
};

struct MicroParams {
    u32 v[4];
};
static_assert(sizeof(MicroParams) == 16);

struct Row {
    u32 iter = 0;
    f64 rtUs = kNaN, g2cUs = kNaN, c2gUs = kNaN, cpuReadUs = kNaN, cpuWriteUs = kNaN;
    f64 copyG2cUs = kNaN, copyC2gUs = kNaN, gpuWriteUs = kNaN, gpuReadUs = kNaN;
};

struct PathDef {
    std::string name;
    bool empty = false;
    frame::TransferPath path = frame::TransferPath::S2Direct;
};

struct PathBuffers {
    frame::PathMemory mem{};
    BufferHandle down, up, downStaging, upStaging;
    BindingSetHandle writeSet, readSet;
    BufferHandle cpuReadBuf, cpuWriteBuf;  // what the CPU touches (staging for copy paths)
    u32* cpuRead = nullptr;
    u32* cpuWrite = nullptr;
};

f64 usBetween(u64 a, u64 b) {
    if (a == 0 || b == 0) return kNaN;
    return (static_cast<f64>(b) - static_cast<f64>(a)) * 1e-3;
}

f64 percentile(std::vector<f64> v, f64 q) {
    v.erase(std::remove_if(v.begin(), v.end(), [](f64 x) { return std::isnan(x); }), v.end());
    if (v.empty()) return kNaN;
    std::sort(v.begin(), v.end());
    const f64 pos = q * static_cast<f64>(v.size() - 1);
    const usize lo = static_cast<usize>(pos);
    const usize hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (v[hi] - v[lo]) * (pos - static_cast<f64>(lo));
}

std::string payloadLabel(u64 bytes) {
    char buf[32];
    if (bytes == 0) return "0";
    if (bytes % (1024ull * 1024ull) == 0) std::snprintf(buf, sizeof(buf), "%lluM", static_cast<unsigned long long>(bytes >> 20));
    else if (bytes % 1024ull == 0) std::snprintf(buf, sizeof(buf), "%lluK", static_cast<unsigned long long>(bytes >> 10));
    else std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(bytes));
    return buf;
}

void writeF(std::FILE* f, f64 v) {
    if (std::isnan(v)) std::fputs(",nan", f);
    else std::fprintf(f, ",%.3f", v);
}

class Microbench {
public:
    Microbench(Gfx& g, const MicrobenchConfig& cfg) : m_g(g), m_cfg(cfg) {}

    int run() {
        const GfxCaps& caps = m_g.caps();
        if (!caps.timelineSync) {
            LB_LOG_ERROR("microbench needs timeline sync");
            return 2;
        }
        if (!caps.gpuTimestamps) LB_LOG_WARN("GPU timestamps unavailable: rt/g2c/c2g will be NaN");
        if (!setup()) return 2;

        m_platform = platformId(caps);
        const KeyValues sys = collectSysInfo(caps);
        if (sys.get("power_source") == "Battery Power") {
            LB_LOG_WARN("running on battery power: protocol §2 requires the Mac to be plugged in; "
                        "these results are not valid for the study");
        }
        if (sys.get("low_power_mode") == "on") LB_LOG_WARN("Low Power Mode is on: protocol §2 requires it off");
        m_maxPayload = 16;
        for (u64 p : m_cfg.payloads) m_maxPayload = std::max(m_maxPayload, p);

        // Raw copy bandwidth first: it keeps the GPU busy while nvidia-smi samples the
        // PCIe link (links downshift at idle), and the result goes into the CSV header.
        std::string pcie = caps.unifiedMemory ? "n/a (unified memory)" : "not measured";
        std::vector<std::pair<std::string, std::vector<Row>>> bwRows;
        runBandwidth(bwRows, pcie);

        if (!openCsv(pcie)) return 2;
        for (auto& [name, rows] : bwRows) writeRows(name, "na", "na", m_maxPayload, rows);

        for (const PathDef& def : pathList()) {
            PathBuffers pb;
            if (!createPathBuffers(def, pb)) {
                ++m_failures;
                continue;
            }
            for (bool chain : m_cfg.chainModes) {
                for (CpuWaitMode wait : m_cfg.waitModes) {
                    if (def.empty) {
                        runCell(def, pb, 0, chain, wait);
                    } else {
                        for (u64 p : m_cfg.payloads) runCell(def, pb, p, chain, wait);
                    }
                }
            }
            destroyPathBuffers(pb);
        }

        std::fprintf(m_csv, "# thermal_state_end: %s\n", thermalState().c_str());
        std::fprintf(m_csv, "# failures: %llu\n", static_cast<unsigned long long>(m_failures));
        std::fclose(m_csv);
        m_csv = nullptr;

        printSummary();
        const u64 apiErrors = m_g.errorCount();
        LB_LOG_INFO("microbench done: %s, %llu verification/R7 failures, %llu API errors, thermal state %s",
                    m_cfg.outPath.c_str(), static_cast<unsigned long long>(m_failures),
                    static_cast<unsigned long long>(apiErrors), thermalState().c_str());
        return (m_failures == 0 && apiErrors == 0) ? 0 : 1;
    }

private:
    struct Summary {
        std::string path, submit, wait;
        u64 payload = 0;
        f64 rt50 = kNaN, rt99 = kNaN, g2c50 = kNaN, c2g50 = kNaN;
    };

    bool setup() {
        ComputePipelineDesc pd;
        pd.shader = "microbench";
        pd.bufferCount = 2;
        pd.pushConstantBytes = sizeof(MicroParams);
        pd.threadsPerGroup = kMicroGroupSize;
        pd.entry = "micro_write";
        m_write = m_g.createComputePipeline(pd);
        pd.entry = "micro_read";
        m_read = m_g.createComputePipeline(pd);
        if (!m_write.valid() || !m_read.valid()) return false;

        // Checksum partials: GPU-written, CPU-read verification data (not a research buffer).
        const MemoryClass checksumClass = m_g.supportsMemoryClass(MemoryClass::Shared) ? MemoryClass::Shared
                                          : m_g.supportsMemoryClass(MemoryClass::HostVisibleCached)
                                              ? MemoryClass::HostVisibleCached
                                              : MemoryClass::HostVisibleCoherent;
        m_checksum = m_g.createBuffer({kMicroMaxReadGroups * sizeof(u32), checksumClass, false, "micro.checksum"});
        m_checksumPtr = static_cast<u32*>(m_g.mappedPtr(m_checksum));
        m_timeline = m_g.createTimeline(0);
        m_ts = m_g.createTimestampPool(kTsCount);
        return m_checksumPtr != nullptr && m_timeline.valid();
    }

    std::vector<PathDef> pathList() const {
        std::vector<PathDef> all;
        all.push_back({"empty", true, frame::TransferPath::S2Direct});
        const frame::TransferPath paths[] = {frame::TransferPath::S1Copy, frame::TransferPath::S2Direct,
                                             frame::TransferPath::S2HostCached, frame::TransferPath::S2Rebar,
                                             frame::TransferPath::S2Coherent};
        for (frame::TransferPath p : paths) all.push_back({frame::transferPathName(p), false, p});

        std::vector<PathDef> out;
        for (const PathDef& d : all) {
            if (!m_cfg.paths.empty() && std::find(m_cfg.paths.begin(), m_cfg.paths.end(), d.name) == m_cfg.paths.end()) {
                continue;
            }
            frame::PathMemory mem;
            if (!frame::pathMemory(d.path, m_g.caps(), mem)) {
                LB_LOG_INFO("path %s not available on this backend/device; skipped", d.name.c_str());
                continue;
            }
            out.push_back(d);
        }
        return out;
    }

    bool createPathBuffers(const PathDef& def, PathBuffers& pb) {
        if (!frame::pathMemory(def.path, m_g.caps(), pb.mem)) return false;
        const u64 size = def.empty ? 256 : m_maxPayload;
        pb.down = m_g.createBuffer({size, pb.mem.down, true, "micro.down"});
        pb.up = m_g.createBuffer({size, pb.mem.up, true, "micro.up"});
        if (pb.mem.copy) {
            pb.downStaging = m_g.createBuffer({size, pb.mem.downStaging, true, "micro.down.staging"});
            pb.upStaging = m_g.createBuffer({size, pb.mem.upStaging, true, "micro.up.staging"});
        }
        pb.cpuReadBuf = pb.mem.copy ? pb.downStaging : pb.down;
        pb.cpuWriteBuf = pb.mem.copy ? pb.upStaging : pb.up;
        pb.cpuRead = static_cast<u32*>(m_g.mappedPtr(pb.cpuReadBuf));
        pb.cpuWrite = static_cast<u32*>(m_g.mappedPtr(pb.cpuWriteBuf));
        if (pb.cpuRead == nullptr || pb.cpuWrite == nullptr) {
            LB_LOG_ERROR("path %s: buffers not created or not CPU-visible", def.name.c_str());
            destroyPathBuffers(pb);
            return false;
        }
        const BufferHandle writeBufs[2] = {pb.down, m_checksum};
        const BufferHandle readBufs[2] = {pb.up, m_checksum};
        pb.writeSet = m_g.createBindingSet(m_write, writeBufs, 2);
        pb.readSet = m_g.createBindingSet(m_read, readBufs, 2);
        if (!pb.writeSet.valid() || !pb.readSet.valid()) {
            destroyPathBuffers(pb);
            return false;
        }
        LB_LOG_INFO("path %-13s down=%s up=%s%s%s%s%s", def.name.c_str(), memoryClassName(pb.mem.down),
                    memoryClassName(pb.mem.up), pb.mem.copy ? " downStaging=" : "",
                    pb.mem.copy ? memoryClassName(pb.mem.downStaging) : "", pb.mem.copy ? " upStaging=" : "",
                    pb.mem.copy ? memoryClassName(pb.mem.upStaging) : "");
        return true;
    }

    void destroyPathBuffers(PathBuffers& pb) {
        m_g.waitIdle();
        m_g.destroyBindingSet(pb.writeSet);
        m_g.destroyBindingSet(pb.readSet);
        m_g.destroyBuffer(pb.down);
        m_g.destroyBuffer(pb.up);
        m_g.destroyBuffer(pb.downStaging);
        m_g.destroyBuffer(pb.upStaging);
        pb = PathBuffers{};
    }

    // Records the GPU segment that consumes the CPU's data (docs/05 §3: wait C, pass, signal).
    SegmentHandle recordConsume(const PathBuffers& pb, u64 bytes, const MicroParams& params, u64 waitValue, u64 endValue) {
        SegmentSync sync;
        sync.waitTimeline = m_timeline;
        sync.waitValue = waitValue;
        sync.signalTimeline = m_timeline;
        sync.signalValue = endValue;
        SegmentHandle s = m_g.beginSegment(sync);
        if (pb.mem.copy && bytes > 0) {
            m_g.cmdBarrier(s, kAccessHostWrite, kAccessCopyRead);
            m_g.cmdCopyBuffer(s, pb.upStaging, 0, pb.up, 0, bytes, {m_ts, kTsCopyUpBegin, kTsCopyUpEnd});
            m_g.cmdBarrier(s, kAccessCopyWrite, kAccessComputeRead);
        } else {
            m_g.cmdBarrier(s, kAccessHostWrite, kAccessComputeRead);
        }
        m_g.cmdDispatch(s, m_read, pb.readSet, params.v[2], &params, sizeof(params), {m_ts, kTsReadBegin, kTsReadEnd});
        m_g.cmdBarrier(s, kAccessComputeWrite, kAccessHostRead);
        m_g.endSegment(s);
        return s;
    }

    void runCell(const PathDef& def, const PathBuffers& pb, u64 bytes, bool chain, CpuWaitMode wait) {
        const u32 elements = static_cast<u32>(bytes / 16);
        const u32 total = m_cfg.warmup + m_cfg.iterations;
        const u32 copiesPerIter = (pb.mem.copy && bytes > 0) ? 2u : 0u;
        std::vector<Row> rows(m_cfg.iterations);
        u64 cellFailures = 0;
        f64 worstCalibNs = 0.0;

        for (u32 it = 0; it < total; ++it) {
            if (it % kRecalibrateEvery == 0) {
                const ClockCalibration c = m_g.calibrateClocks();
                if (c.valid) worstCalibNs = std::max(worstCalibNs, c.maxDeviationNs);
            }
            const u64 vGpu = ++m_value;   // GPU produced
            const u64 vCpu = ++m_value;   // CPU produced
            const u64 vEnd = ++m_value;   // GPU consumed

            MicroParams writeParams{{elements, it, 0, 0}};
            MicroParams readParams{{elements, it, readGroups(elements), 0}};
            std::memset(m_checksumPtr, 0, kMicroMaxReadGroups * sizeof(u32));
            m_g.flushHostWrites(m_checksum, 0, kMicroMaxReadGroups * sizeof(u32));
            m_g.resetTimestamps(m_ts, 0, kTsCount);
            const u64 copiesBefore = m_g.researchCopyCount();

            // GPU produce segment: write P bytes (+ copy to staging on S1), signal.
            SegmentSync produceSync;
            produceSync.signalTimeline = m_timeline;
            produceSync.signalValue = vGpu;
            SegmentHandle produce = m_g.beginSegment(produceSync);
            m_g.cmdDispatch(produce, m_write, pb.writeSet, writeGroups(elements), &writeParams, sizeof(writeParams),
                            {m_ts, kTsWriteBegin, kTsWriteEnd});
            if (pb.mem.copy && bytes > 0) {
                m_g.cmdBarrier(produce, kAccessComputeWrite, kAccessCopyRead);
                m_g.cmdCopyBuffer(produce, pb.down, 0, pb.downStaging, 0, bytes, {m_ts, kTsCopyDownBegin, kTsCopyDownEnd});
                m_g.cmdBarrier(produce, kAccessCopyWrite, kAccessHostRead);
            } else {
                m_g.cmdBarrier(produce, kAccessComputeWrite, kAccessHostRead);
            }
            m_g.endSegment(produce);

            if (chain) {
                const SegmentHandle segs[2] = {produce, recordConsume(pb, bytes, readParams, vCpu, vEnd)};
                if (!m_g.submit(segs, 2)) { ++cellFailures; break; }
            } else if (!m_g.submit(&produce, 1)) {
                ++cellFailures;
                break;
            }

            // CPU: wait, read every byte, write every byte back, signal.
            if (!m_g.cpuWait(m_timeline, vGpu, wait, kWaitTimeoutNs)) {
                LB_LOG_ERROR("%s P=%llu it=%u: timeout waiting for GPU", def.name.c_str(),
                             static_cast<unsigned long long>(bytes), it);
                ++cellFailures;
                break;
            }
            const u64 tWake = nowNs();
            m_g.invalidateHostReads(pb.cpuReadBuf, 0, bytes > 0 ? bytes : 16);
            const u32 readSum = sumWords(pb.cpuRead, static_cast<usize>(bytes / 4));
            const u64 tRead = nowNs();
            writeCpuPattern(pb.cpuWrite, elements, it);
            m_g.flushHostWrites(pb.cpuWriteBuf, 0, bytes > 0 ? bytes : 16);
            const u64 tSignal = nowNs();
            m_g.cpuSignal(m_timeline, vCpu);

            if (!chain) {
                const SegmentHandle consume = recordConsume(pb, bytes, readParams, vCpu, vEnd);
                if (!m_g.submit(&consume, 1)) { ++cellFailures; break; }
            }
            if (!m_g.cpuWait(m_timeline, vEnd, CpuWaitMode::Block, kWaitTimeoutNs) || !m_g.waitIdle()) {
                LB_LOG_ERROR("%s P=%llu it=%u: timeout waiting for GPU consume", def.name.c_str(),
                             static_cast<unsigned long long>(bytes), it);
                ++cellFailures;
                break;
            }

            // Verification (outside every measured interval).
            m_g.invalidateHostReads(m_checksum, 0, kMicroMaxReadGroups * sizeof(u32));
            const u32 gpuSum = sumWords(m_checksumPtr, readParams.v[2]);
            const u32 wantRead = expectedCpuReadSum(elements, it);
            const u32 wantGpu = expectedGpuChecksum(elements, it);
            const u64 copies = m_g.researchCopyCount() - copiesBefore;
            if (readSum != wantRead || gpuSum != wantGpu || copies != copiesPerIter) {
                if (cellFailures < 5) {
                    LB_LOG_ERROR("%s P=%llu it=%u: cpu read sum %08x (want %08x), gpu checksum %08x (want %08x), "
                                 "research copies %llu (want %u)",
                                 def.name.c_str(), static_cast<unsigned long long>(bytes), it, readSum, wantRead,
                                 gpuSum, wantGpu, static_cast<unsigned long long>(copies), copiesPerIter);
                }
                ++cellFailures;
            }

            if (it < m_cfg.warmup) continue;
            u64 ts[kTsCount];
            m_g.readTimestamps(m_ts, 0, kTsCount, ts);
            Row& r = rows[it - m_cfg.warmup];
            r.iter = it - m_cfg.warmup;
            r.rtUs = usBetween(ts[kTsWriteEnd], ts[kTsReadBegin]);
            r.g2cUs = usBetween(ts[kTsWriteEnd], tWake);
            r.c2gUs = usBetween(tSignal, ts[kTsReadBegin]);
            r.cpuReadUs = usBetween(tWake, tRead);
            r.cpuWriteUs = usBetween(tRead, tSignal);
            r.gpuWriteUs = usBetween(ts[kTsWriteBegin], ts[kTsWriteEnd]);
            r.gpuReadUs = usBetween(ts[kTsReadBegin], ts[kTsReadEnd]);
            if (copiesPerIter > 0) {
                r.copyG2cUs = usBetween(ts[kTsCopyDownBegin], ts[kTsCopyDownEnd]);
                r.copyC2gUs = usBetween(ts[kTsCopyUpBegin], ts[kTsCopyUpEnd]);
            }
        }
        m_failures += cellFailures;

        const char* submitName = chain ? "chain" : "perpass";
        const char* waitName = wait == CpuWaitMode::Spin ? "spin" : "block";
        writeRows(def.name, submitName, waitName, bytes, rows);

        std::vector<f64> rt, g2c, c2g;
        for (const Row& r : rows) {
            rt.push_back(r.rtUs);
            g2c.push_back(r.g2cUs);
            c2g.push_back(r.c2gUs);
        }
        Summary s{def.name, submitName, waitName, bytes, percentile(rt, 0.5), percentile(rt, 0.99),
                  percentile(g2c, 0.5), percentile(c2g, 0.5)};
        m_summaries.push_back(s);
        LB_LOG_INFO("%-13s %-7s %-5s %5s  rt p50 %9.1f us  p99 %9.1f  | g2c p50 %8.1f  c2g p50 %8.1f  | calib ±%.1f us%s",
                    def.name.c_str(), submitName, waitName, payloadLabel(bytes).c_str(), s.rt50, s.rt99, s.g2c50,
                    s.c2g50, worstCalibNs * 1e-3, cellFailures != 0 ? "  FAILURES" : "");
    }

    // GPU copy bandwidth device→host staging and host staging→device (docs/06 §6 "raw copy
    // bandwidth"), measured with GPU timestamps; queries the PCIe link while it runs.
    void runBandwidth(std::vector<std::pair<std::string, std::vector<Row>>>& out, std::string& pcie) {
        frame::PathMemory mem;
        if (!frame::pathMemory(frame::TransferPath::S1Copy, m_g.caps(), mem) || !m_g.caps().gpuTimestamps) return;
        const u64 bytes = m_maxPayload;
        const BufferHandle dev = m_g.createBuffer({bytes, mem.down, false, "bw.device"});
        const BufferHandle hostDown = m_g.createBuffer({bytes, mem.downStaging, false, "bw.host.down"});
        const BufferHandle hostUp = m_g.createBuffer({bytes, mem.upStaging, false, "bw.host.up"});
        if (!dev.valid() || !hostDown.valid() || !hostUp.valid()) {
            LB_LOG_WARN("bandwidth test skipped (buffer allocation failed)");
            return;
        }

        const bool queryPcie = !m_g.caps().unifiedMemory && std::strstr(m_g.caps().driverInfo, "NVIDIA") != nullptr;
        std::atomic<bool> pcieDone{!queryPcie};
        std::string pcieResult;
        std::thread pcieThread;

        struct Dir { const char* name; BufferHandle src, dst; };
        const Dir dirs[] = {{"bw_d2h", dev, hostDown}, {"bw_h2d", hostUp, dev}};
        constexpr u32 kMeasured = 100, kWarm = 10;
        for (const Dir& d : dirs) {
            std::vector<Row> rows(kMeasured);
            if (queryPcie && !pcieThread.joinable()) {
                pcieThread = std::thread([&] {
                    // Let the copies ramp the link up before sampling it.
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    pcieResult = queryNvidiaPcieLink();
                    pcieDone.store(true);
                });
            }
            const u64 deadline = nowNs() + 10'000'000'000ull;
            u32 it = 0;
            // Keep copying until enough samples are measured and the link query (if any) is done.
            while (it < kWarm + kMeasured || (!pcieDone.load() && nowNs() < deadline)) {
                m_g.resetTimestamps(m_ts, 0, 2);
                SegmentHandle s = m_g.beginSegment({});
                m_g.cmdCopyBuffer(s, d.src, 0, d.dst, 0, bytes, {m_ts, 0, 1});
                m_g.endSegment(s);
                if (!m_g.submit(&s, 1) || !m_g.waitIdle()) {
                    ++m_failures;
                    break;
                }
                if (it >= kWarm && it < kWarm + kMeasured) {
                    u64 ts[2];
                    m_g.readTimestamps(m_ts, 0, 2, ts);
                    rows[it - kWarm].iter = it - kWarm;
                    rows[it - kWarm].rtUs = usBetween(ts[0], ts[1]);
                }
                ++it;
            }
            std::vector<f64> t;
            for (const Row& r : rows) t.push_back(r.rtUs);
            const f64 p50 = percentile(t, 0.5);
            LB_LOG_INFO("%s %s: p50 %.1f us = %.2f GB/s", d.name, payloadLabel(bytes).c_str(), p50,
                        static_cast<f64>(bytes) / (p50 * 1e-6) / 1e9);
            out.emplace_back(d.name, std::move(rows));
        }
        if (pcieThread.joinable()) pcieThread.join();
        if (queryPcie) {
            pcie = pcieResult;
            LB_LOG_INFO("PCIe link under load: %s", pcie.c_str());
        }
        m_g.destroyBuffer(dev);
        m_g.destroyBuffer(hostDown);
        m_g.destroyBuffer(hostUp);
    }

    bool openCsv(const std::string& pcie) {
        const std::filesystem::path path(m_cfg.outPath);
        std::error_code ec;
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
        m_csv = std::fopen(m_cfg.outPath.c_str(), "w");
        if (m_csv == nullptr) {
            LB_LOG_ERROR("cannot open %s for writing", m_cfg.outPath.c_str());
            return false;
        }
        KeyValues h;
        h.add("file", "micro.csv (docs/06 §12)");
        h.add("version", kVersion);
        h.add("git_commit", std::string(LB_GIT_COMMIT) + (LB_GIT_DIRTY ? "-dirty" : ""));
        h.add("GENERATOR_VERSION", std::to_string(kGeneratorVersion));
        h.add("tuning_hash", "n/a (microbench)");
        h.add("cli", m_cfg.commandLine);
        h.add("options", m_cfg.options);
        h.add("tag", m_cfg.tag);
        h.add("start", isoTimestampUtc());
        h.add("iterations", std::to_string(m_cfg.iterations));
        h.add("warmup", std::to_string(m_cfg.warmup));
        for (const auto& kv : collectSysInfo(m_g.caps()).items) h.add("sysinfo." + kv.first, kv.second);
        h.add("sysinfo.pcie_link", pcie);
        h.add("columns.extra", "copy_g2c_us,copy_c2g_us,gpu_write_us,gpu_read_us appended after the §12 columns "
                               "(DECISIONS #20); bw_* rows: rt_us = GPU copy time");
        for (const auto& kv : h.items) std::fprintf(m_csv, "# %s: %s\n", kv.first.c_str(), kv.second.c_str());
        std::fputs("platform,path,submit,cpuwait,payload_bytes,iter,rt_us,g2c_us,c2g_us,cpu_read_us,cpu_write_us,"
                   "copy_g2c_us,copy_c2g_us,gpu_write_us,gpu_read_us\n",
                   m_csv);
        return true;
    }

    void writeRows(const std::string& path, const char* submit, const char* wait, u64 payload, const std::vector<Row>& rows) {
        if (m_csv == nullptr) return;
        for (const Row& r : rows) {
            std::fprintf(m_csv, "%s,%s,%s,%s,%llu,%u", m_platform.c_str(), path.c_str(), submit, wait,
                         static_cast<unsigned long long>(payload), r.iter);
            writeF(m_csv, r.rtUs);
            writeF(m_csv, r.g2cUs);
            writeF(m_csv, r.c2gUs);
            writeF(m_csv, r.cpuReadUs);
            writeF(m_csv, r.cpuWriteUs);
            writeF(m_csv, r.copyG2cUs);
            writeF(m_csv, r.copyC2gUs);
            writeF(m_csv, r.gpuWriteUs);
            writeF(m_csv, r.gpuReadUs);
            std::fputc('\n', m_csv);
        }
        std::fflush(m_csv);
    }

    void printSummary() const {
        std::printf("\nround-trip p50 / p99 (us), %s\n", m_platform.c_str());
        std::printf("%-13s %-7s %-5s", "path", "submit", "wait");
        std::vector<u64> payloads{0};
        for (u64 p : m_cfg.payloads) payloads.push_back(p);
        for (u64 p : payloads) std::printf(" %17s", payloadLabel(p).c_str());
        std::printf("\n");
        std::vector<std::string> seen;
        for (const Summary& s : m_summaries) {
            const std::string key = s.path + s.submit + s.wait;
            if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
            seen.push_back(key);
            std::printf("%-13s %-7s %-5s", s.path.c_str(), s.submit.c_str(), s.wait.c_str());
            for (u64 p : payloads) {
                const Summary* found = nullptr;
                for (const Summary& t : m_summaries) {
                    if (t.path == s.path && t.submit == s.submit && t.wait == s.wait && t.payload == p) found = &t;
                }
                if (found != nullptr) std::printf(" %8.1f/%8.1f", found->rt50, found->rt99);
                else std::printf(" %17s", "-");
            }
            std::printf("\n");
        }
    }

    Gfx& m_g;
    const MicrobenchConfig& m_cfg;
    std::string m_platform;
    u64 m_maxPayload = 16;
    PipelineHandle m_write, m_read;
    BufferHandle m_checksum;
    u32* m_checksumPtr = nullptr;
    TimelineHandle m_timeline;
    TimestampPoolHandle m_ts;
    u64 m_value = 0;
    u64 m_failures = 0;
    std::FILE* m_csv = nullptr;
    std::vector<Summary> m_summaries;
};

} // namespace

std::vector<u64> defaultMicroPayloads() {
    return {4ull << 10, 64ull << 10, 256ull << 10, 1ull << 20, 4ull << 20, 16ull << 20};
}

bool parsePayloadList(const std::string& text, std::vector<u64>& out, std::string& error) {
    out.clear();
    usize start = 0;
    while (start <= text.size()) {
        const usize comma = text.find(',', start);
        const std::string item = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (item.empty()) {
            error = "empty payload entry";
            return false;
        }
        char* end = nullptr;
        const unsigned long long n = std::strtoull(item.c_str(), &end, 10);
        u64 mult = 1;
        if (end != nullptr && *end != '\0') {
            const char s = *end;
            if (s == 'K' || s == 'k') mult = 1ull << 10;
            else if (s == 'M' || s == 'm') mult = 1ull << 20;
            else if (s == 'G' || s == 'g') mult = 1ull << 30;
            else { error = "bad payload '" + item + "'"; return false; }
            if (end[1] != '\0') { error = "bad payload '" + item + "'"; return false; }
        }
        const u64 bytes = static_cast<u64>(n) * mult;
        if (bytes == 0 || bytes % 16 != 0 || bytes > (1ull << 30)) {
            error = "payload '" + item + "' must be a non-zero multiple of 16 bytes up to 1G";
            return false;
        }
        out.push_back(bytes);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return !out.empty();
}

int runMicrobench(Gfx& gfx, const MicrobenchConfig& cfg) {
    Microbench mb(gfx, cfg);
    return mb.run();
}

} // namespace lb::bench
