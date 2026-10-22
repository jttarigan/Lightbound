#include "bench/MicroPattern.h"
#include "bench/MicroResume.h"
#include "bench/Microbench.h"
#include "bench/SysInfo.h"
#include "frame/MemoryPolicy.h"

#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

using namespace lb;
using namespace lb::bench;

TEST_CASE("micro patterns: CPU read sum matches a written GPU pattern") {
    for (u32 elements : {0u, 1u, 3u, 64u, 1000u}) {
        for (u32 it : {0u, 1u, 199u, 2199u}) {
            std::vector<u32> words(4ull * elements);
            for (u32 i = 0; i < elements; ++i) gpuWriteElement(i, it, &words[4ull * i]);
            CHECK(sumWords(words.data(), words.size()) == expectedCpuReadSum(elements, it));
        }
    }
}

TEST_CASE("micro patterns: GPU partial-sum reduction matches the CPU expectation") {
    // Emulates micro_read's grid-stride loop and per-group partials.
    for (u32 elements : {0u, 1u, 63u, 64u, 65u, 16384u, 70000u}) {
        const u32 it = 7;
        std::vector<u32> words(4ull * elements);
        writeCpuPattern(words.data(), elements, it);
        const u32 groups = readGroups(elements);
        const u32 stride = groups * kMicroGroupSize;
        std::vector<u32> partials(groups, 0);
        for (u32 g = 0; g < groups; ++g) {
            for (u32 t = 0; t < kMicroGroupSize; ++t) {
                for (u32 i = g * kMicroGroupSize + t; i < elements; i += stride) {
                    const u32* e = &words[4ull * i];
                    partials[g] += e[0] ^ e[1] ^ e[2] ^ e[3];
                }
            }
        }
        CHECK(sumWords(partials.data(), partials.size()) == expectedGpuChecksum(elements, it));
    }
}

TEST_CASE("micro patterns: a single flipped byte is detected") {
    const u32 elements = 4096, it = 3;
    std::vector<u32> words(4ull * elements);
    for (u32 i = 0; i < elements; ++i) gpuWriteElement(i, it, &words[4ull * i]);
    words[1234] ^= 0x100u;
    CHECK(sumWords(words.data(), words.size()) != expectedCpuReadSum(elements, it));
}

TEST_CASE("micro dispatch sizes") {
    CHECK(writeGroups(0) == 1);
    CHECK(writeGroups(64) == 1);
    CHECK(writeGroups(65) == 2);
    CHECK(readGroups(1u << 20) == kMicroMaxReadGroups);
    CHECK(readGroups(100) == 2);
}

TEST_CASE("payload list parsing") {
    std::vector<u64> p;
    std::string err;
    REQUIRE(parsePayloadList("4K,64K,1M,16M", p, err));
    CHECK(p == std::vector<u64>{4096, 65536, 1u << 20, 16u << 20});
    REQUIRE(parsePayloadList("256", p, err));
    CHECK(p == std::vector<u64>{256});
    CHECK_FALSE(parsePayloadList("", p, err));
    CHECK_FALSE(parsePayloadList("4K,", p, err));
    CHECK_FALSE(parsePayloadList("100", p, err));   // not a multiple of 16
    CHECK_FALSE(parsePayloadList("4X", p, err));
    CHECK(defaultMicroPayloads().size() == 6);
}

namespace {
std::string microRows(const char* path, const char* submit, const char* wait, u64 payload, u32 count) {
    std::string s;
    for (u32 i = 0; i < count; ++i) {
        s += std::string("P-TEST,") + path + "," + submit + "," + wait + "," + std::to_string(payload) + "," +
             std::to_string(i) + ",1.000,2.000,3.000,4.000,5.000,nan,nan,6.000,7.000\r\n";
    }
    return s;
}

const char* const kMicroHeader =
    "# file: micro.csv (docs/06 §12)\r\n"
    "# git_commit: abc123\r\n"
    "# iterations: 3\r\n"
    "platform,path,submit,cpuwait,payload_bytes,iter,rt_us,g2c_us,c2g_us,cpu_read_us,cpu_write_us,"
    "copy_g2c_us,copy_c2g_us,gpu_write_us,gpu_read_us\r\n";
} // namespace

TEST_CASE("resume scan keeps complete cells and drops an interrupted one") {
    const std::string text = std::string(kMicroHeader) + microRows("bw_d2h", "na", "na", 4096, 2) +
                             microRows("empty", "chain", "spin", 0, 3) +
                             cellFailureNote("s1_copy", "chain", "spin", 4096, 2) + "\r\n" +
                             microRows("s1_copy", "chain", "spin", 4096, 3) +
                             "# resumed: earlier\r\n" +
                             cellFailureNote("s1_copy", "chain", "spin", 65536, 7) + "\r\n" +
                             microRows("s1_copy", "chain", "spin", 65536, 2) +   // killed after 2 of 3 rows
                             "P-TEST,s1_copy,chain,spin,65536,2,1.0";            // ... in the middle of a row
    ResumeScan scan;
    REQUIRE(scanResumeCsv(text, 3, 2, scan));
    CHECK(scan.headerValue("git_commit") == "abc123");
    CHECK(scan.headerValue("iterations") == "3");
    CHECK(scan.headerValue("missing").empty());
    CHECK(scan.cells.size() == 3);
    CHECK(scan.has("bw_d2h", "na", "na", 4096));
    CHECK(scan.has("empty", "chain", "spin", 0));
    CHECK(scan.has("s1_copy", "chain", "spin", 4096));
    CHECK_FALSE(scan.has("s1_copy", "chain", "spin", 65536));
    CHECK_FALSE(scan.has("s1_copy", "chain", "block", 4096));
    CHECK(scan.failures == 2);      // the dropped cell's 7 failures are measured again
    CHECK(scan.droppedRows == 2);
    // 4 header lines, 2 + 3 + 3 rows, one failure note, one earlier "resumed" note.
    CHECK(scan.kept.size() == 4 + 8 + 2);
    for (const std::string& line : scan.kept) CHECK(line.find('\r') == std::string::npos);

    // A cell with too many rows is not trusted either; a finished run's footer is not carried over.
    const std::string done = std::string(kMicroHeader) + microRows("empty", "chain", "spin", 0, 4) +
                             microRows("empty", "chain", "block", 0, 3) +
                             "# thermal_state_end: n/a\r\n# failures: 0\r\n";
    REQUIRE(scanResumeCsv(done, 3, 2, scan));
    CHECK(scan.cells.size() == 1);
    CHECK(scan.has("empty", "chain", "block", 0));
    CHECK(scan.droppedRows == 4);
    CHECK(scan.kept.size() == 4 + 3);

    CHECK_FALSE(scanResumeCsv("", 3, 2, scan));
    CHECK_FALSE(scanResumeCsv("# file: micro.csv\r\n# git_commit: abc", 3, 2, scan));  // killed inside the header
}

TEST_CASE("hot-GPU path names and load notes (DECISIONS #32)") {
    CHECK(hotPathName("s2_direct") == "s2_direct_hot");
    CHECK(isHotPath("s2_direct_hot"));
    CHECK(isHotPath("empty_hot"));
    CHECK_FALSE(isHotPath("s2_direct"));
    CHECK_FALSE(isHotPath("_hot"));
    CHECK(std::string(gpuLoadName(GpuLoad::Hot)) == "hot");
    CHECK(cellLoadNote("empty_hot", "chain", "spin", 0, 1234, 0.98765) ==
          "# cell_load: empty_hot,chain,spin,0,dispatches=1234,duty=0.988");

    // A load note stays with a complete cell and goes with an interrupted one.
    const std::string text = std::string(kMicroHeader) + cellLoadNote("empty_hot", "chain", "spin", 0, 10, 1.0) + "\r\n" +
                             microRows("empty_hot", "chain", "spin", 0, 3) +
                             cellLoadNote("empty_hot", "chain", "block", 0, 4, 0.9) + "\r\n" +
                             microRows("empty_hot", "chain", "block", 0, 1);
    ResumeScan scan;
    REQUIRE(scanResumeCsv(text, 3, 2, scan));
    CHECK(scan.cells.size() == 1);
    CHECK(scan.has("empty_hot", "chain", "spin", 0));
    CHECK(scan.droppedRows == 1);
    CHECK(scan.kept.size() == 4 + 1 + 3);   // header, the kept cell's note and rows; nothing of the other cell
    CHECK(scan.kept[4] == cellLoadNote("empty_hot", "chain", "spin", 0, 10, 1.0));
}

TEST_CASE("bench-thread QoS diagnostics (DECISIONS #36)") {
    CHECK(std::string(threadQosName(ThreadQos::Default)) == "default");
    CHECK(std::string(threadQosName(ThreadQos::Interactive)) == "interactive");
    CHECK(std::string(threadQosName(ThreadQos::High)) == "high");
    CHECK_FALSE(threadQosClass().empty());
    CHECK_FALSE(threadPriority().empty());
    CHECK_FALSE(processNice().empty());
    std::string note;
    const bool applied = requestThreadQosInteractive(note);
    CHECK_FALSE(note.empty());
#if defined(__APPLE__)
    CHECK(applied);
    CHECK(threadQosClass() == "user-interactive");
    CHECK(threadPriority().rfind("cur=", 0) == 0);
    std::string prio;
    CHECK(requestThreadPriority(47, prio));   // precedence policy needs no root
    CHECK(threadPriority().find("base=47") != std::string::npos);
    CHECK(requestThreadPriority(31, prio));   // and back, so later tests run as before
#else
    CHECK_FALSE(applied);
    CHECK(threadQosClass() == "n/a");
    std::string prio;
    CHECK_FALSE(requestThreadPriority(47, prio));
#endif
}

TEST_CASE("memory policy follows docs/05 §4.2") {
    gfx::GfxCaps metal;
    metal.backend = gfx::Backend::Metal;
    metal.memoryClassSupported[static_cast<u32>(gfx::MemoryClass::DeviceLocal)] = true;
    metal.memoryClassSupported[static_cast<u32>(gfx::MemoryClass::Shared)] = true;
    frame::PathMemory m;
    REQUIRE(frame::pathMemory(frame::TransferPath::S2Direct, metal, m));
    CHECK_FALSE(m.copy);
    CHECK(m.down == gfx::MemoryClass::Shared);
    CHECK(m.up == gfx::MemoryClass::Shared);
    REQUIRE(frame::pathMemory(frame::TransferPath::S1Copy, metal, m));
    CHECK(m.copy);
    CHECK(m.down == gfx::MemoryClass::DeviceLocal);
    CHECK(m.downStaging == gfx::MemoryClass::Shared);
    CHECK_FALSE(frame::pathMemory(frame::TransferPath::S2Rebar, metal, m)); // Vulkan-only variant

    gfx::GfxCaps vk;
    vk.backend = gfx::Backend::Vulkan;
    for (u32 c = 0; c < gfx::kMemoryClassCount; ++c) vk.memoryClassSupported[c] = true;
    vk.memoryClassSupported[static_cast<u32>(gfx::MemoryClass::Shared)] = false;
    vk.rebar = true;
    REQUIRE(frame::pathMemory(frame::TransferPath::S2Direct, vk, m));
    CHECK(m.down == gfx::MemoryClass::HostVisibleCached);          // CPU never reads ReBAR by default
    CHECK(m.up == gfx::MemoryClass::DeviceLocalHostVisible);
    vk.rebar = false;
    REQUIRE(frame::pathMemory(frame::TransferPath::S2Direct, vk, m));
    CHECK(m.up == gfx::MemoryClass::HostVisibleCoherent);
    REQUIRE(frame::pathMemory(frame::TransferPath::S1Copy, vk, m));
    CHECK(m.downStaging == gfx::MemoryClass::HostVisibleCached);
    CHECK(m.upStaging == gfx::MemoryClass::HostVisibleCoherent);
    vk.memoryClassSupported[static_cast<u32>(gfx::MemoryClass::DeviceLocalHostVisible)] = false;
    CHECK_FALSE(frame::pathMemory(frame::TransferPath::S2Rebar, vk, m)); // never substituted
}
