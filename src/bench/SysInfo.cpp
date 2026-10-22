#include "bench/SysInfo.h"

#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <pthread/qos.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#include <powrprof.h>
#else
#include <sys/resource.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace lb::bench {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    usize i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

bool contains(const char* haystack, const char* needle) { return std::strstr(haystack, needle) != nullptr; }

std::string bytesToGiB(u64 bytes) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1f GiB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

#if defined(__APPLE__)
std::string sysctlString(const char* name) {
    char buf[256] = {};
    size_t len = sizeof(buf) - 1;
    if (sysctlbyname(name, buf, &len, nullptr, 0) != 0) return {};
    return trim(buf);
}

u64 sysctlU64(const char* name) {
    u64 v = 0;
    size_t len = sizeof(v);
    if (sysctlbyname(name, &v, &len, nullptr, 0) != 0) return 0;
    if (len == sizeof(u32)) return static_cast<u64>(*reinterpret_cast<u32*>(&v));
    return v;
}

// NSProcessInfo via the Objective-C runtime (this TU is C++; no metal-cpp dependency so the
// Vulkan/MoltenVK build can use it too).
id processInfo() {
    Class cls = objc_getClass("NSProcessInfo");
    if (cls == nullptr) return nullptr;
    using Fn = id (*)(Class, SEL);
    return reinterpret_cast<Fn>(objc_msgSend)(cls, sel_registerName("processInfo"));
}

long processInfoLong(const char* selector) {
    id pi = processInfo();
    if (pi == nullptr) return -1;
    using Fn = long (*)(id, SEL);
    return reinterpret_cast<Fn>(objc_msgSend)(pi, sel_registerName(selector));
}

std::string macGpuCores() {
    io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AGXAccelerator"));
    if (svc == 0) return "unknown";
    std::string out = "unknown";
    CFTypeRef v = IORegistryEntryCreateCFProperty(svc, CFSTR("gpu-core-count"), kCFAllocatorDefault, 0);
    if (v != nullptr) {
        long n = 0;
        if (CFGetTypeID(v) == CFNumberGetTypeID() && CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberLongType, &n)) {
            out = std::to_string(n);
        }
        CFRelease(v);
    }
    IOObjectRelease(svc);
    return out;
}

std::string macPowerSource() {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info == nullptr) return "unknown";
    std::string out = "unknown";
    CFStringRef type = IOPSGetProvidingPowerSourceType(info);
    if (type != nullptr) {
        char buf[64] = {};
        if (CFStringGetCString(type, buf, sizeof(buf), kCFStringEncodingUTF8)) out = buf;
    }
    CFRelease(info);
    return out;
}
#endif

#if defined(_WIN32)
std::string windowsCpuBrand() {
    int regs[4] = {};
    char brand[49] = {};
    __cpuid(regs, std::bit_cast<int>(0x80000000u));
    if (std::bit_cast<unsigned>(regs[0]) < 0x80000004u) return "unknown";
    for (unsigned i = 0; i < 3; ++i) {
        __cpuid(regs, std::bit_cast<int>(0x80000002u + i));
        std::memcpy(brand + 16 * i, regs, 16);
    }
    return trim(brand);
}

std::string windowsVersion() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) return "Windows";
    auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
    if (fn == nullptr) return "Windows";
    RTL_OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (fn(&v) != 0) return "Windows";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Windows %lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    return buf;
}

std::string windowsPowerPlan() {
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS || scheme == nullptr) return "unknown";
    std::string out = "unknown";
    wchar_t name[256] = {};
    DWORD size = static_cast<DWORD>(sizeof(name));
    if (PowerReadFriendlyName(nullptr, scheme, nullptr, nullptr, reinterpret_cast<UCHAR*>(name), &size) == ERROR_SUCCESS) {
        char utf8[512] = {};
        WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, static_cast<int>(sizeof(utf8) - 1), nullptr, nullptr);
        out = utf8;
    }
    LocalFree(scheme);
    return out;
}
#endif

} // namespace

std::string runCommand(const char* command) {
#if defined(_WIN32)
    FILE* p = _popen(command, "r");
#else
    FILE* p = popen(command, "r");
#endif
    if (p == nullptr) return {};
    std::string out;
    char buf[512];
    while (std::fgets(buf, sizeof(buf), p) != nullptr) out += buf;
#if defined(_WIN32)
    const int rc = _pclose(p);
#else
    const int rc = pclose(p);
#endif
    if (rc != 0) return {};
    return trim(out);
}

std::string platformId(const gfx::GfxCaps& caps) {
    if (caps.backend == gfx::Backend::Metal && contains(caps.deviceName, "Apple M5")) return "P-M5";
    if (caps.backend == gfx::Backend::Vulkan) {
        if (contains(caps.deviceName, "Apple M5")) return "P-M5-VK";
        if (contains(caps.deviceName, "3060 Ti")) return "P-3060";
        if (contains(caps.deviceName, "4060 Ti")) return "P-4060";
    }
    std::string s = std::string(gfx::backendName(caps.backend)) + "-" + caps.deviceName;
    for (char& c : s) {
        if (c == ' ' || c == ',') c = '_';
    }
    return s;
}

std::string thermalState() {
#if defined(__APPLE__)
    switch (processInfoLong("thermalState")) {
    case 0: return "nominal";
    case 1: return "fair";
    case 2: return "serious";
    case 3: return "critical";
    default: return "unknown";
    }
#else
    return "n/a";
#endif
}

#if defined(__APPLE__)
namespace {
const char* qosName(qos_class_t c) {
    switch (c) {
    case QOS_CLASS_USER_INTERACTIVE: return "user-interactive";
    case QOS_CLASS_USER_INITIATED: return "user-initiated";
    case QOS_CLASS_DEFAULT: return "default";
    case QOS_CLASS_UTILITY: return "utility";
    case QOS_CLASS_BACKGROUND: return "background";
    case QOS_CLASS_UNSPECIFIED: return "unspecified";
    }
    return "?";
}
} // namespace
#endif

std::string threadQosClass() {
#if defined(__APPLE__)
    qos_class_t cls = QOS_CLASS_UNSPECIFIED;
    int rel = 0;
    if (pthread_get_qos_class_np(pthread_self(), &cls, &rel) != 0) return "unknown";
    std::string s = qosName(cls);
    if (rel != 0) s += " (" + std::to_string(rel) + ")";
    return s;
#else
    return "n/a";
#endif
}

std::string threadPriority() {
#if defined(__APPLE__)
    u64 tid = 0;
    if (pthread_threadid_np(nullptr, &tid) != 0) return "unknown";
    proc_threadinfo ti{};
    const int n = proc_pidinfo(getpid(), PROC_PIDTHREADID64INFO, tid, &ti, static_cast<int>(sizeof(ti)));
    if (n < static_cast<int>(sizeof(ti))) return "unknown";
    return "cur=" + std::to_string(ti.pth_curpri) + " base=" + std::to_string(ti.pth_priority) +
           " max=" + std::to_string(ti.pth_maxpriority);
#else
    return "n/a";
#endif
}

std::string processNice() {
#if defined(_WIN32)
    return "n/a";
#else
    errno = 0;
    const int nice = getpriority(PRIO_PROCESS, 0);
    if (nice == -1 && errno != 0) return "unknown";
    return std::to_string(nice);
#endif
}

bool requestThreadQosInteractive(std::string& note) {
#if defined(__APPLE__)
    const std::string before = threadQosClass() + ", priority " + threadPriority();
    const int rc = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    if (rc != 0) {
        note = "pthread_set_qos_class_self_np failed (" + std::string(std::strerror(rc)) + "); thread stays " + before;
        return false;
    }
    note = "QOS_CLASS_USER_INTERACTIVE requested; thread was " + before + ", now " + threadQosClass() +
           ", priority " + threadPriority();
    return true;
#else
    note = "no QoS classes on this OS (no-op); thread " + threadQosClass();
    return false;
#endif
}

#if defined(__APPLE__)
namespace {
int threadBasePriority() {
    u64 tid = 0;
    if (pthread_threadid_np(nullptr, &tid) != 0) return -1;
    proc_threadinfo ti{};
    const int n = proc_pidinfo(getpid(), PROC_PIDTHREADID64INFO, tid, &ti, static_cast<int>(sizeof(ti)));
    return n < static_cast<int>(sizeof(ti)) ? -1 : ti.pth_priority;
}
} // namespace
#endif

bool requestThreadPriority(int target, std::string& note) {
#if defined(__APPLE__)
    const std::string before = threadPriority();
    // The policy takes an importance relative to the task priority (31 minus nice), which the first
    // read-back may not reflect yet; so set, read back and correct, up to three times.
    int importance = 0;
    int base = threadBasePriority();
    for (int pass = 0; pass < 3 && base >= 0 && base != target; ++pass) {
        importance += target - base;
        thread_precedence_policy_data_t policy{importance};
        const kern_return_t kr = thread_policy_set(mach_thread_self(), THREAD_PRECEDENCE_POLICY,
                                                   reinterpret_cast<thread_policy_t>(&policy),
                                                   THREAD_PRECEDENCE_POLICY_COUNT);
        if (kr != KERN_SUCCESS) {
            note = "thread_policy_set(THREAD_PRECEDENCE_POLICY) failed (kr=" + std::to_string(kr) + "); thread stays " + before;
            return false;
        }
        base = threadBasePriority();
    }
    note = "base priority " + std::to_string(target) + " requested (THREAD_PRECEDENCE_POLICY importance " +
           std::to_string(importance) + "); thread was " + before + ", now " + threadPriority();
    return base == target;
#else
    (void)target;
    note = "no thread precedence policy on this OS (no-op); thread " + threadPriority();
    return false;
#endif
}

std::string isoTimestampUtc() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string queryNvidiaPcieLink() {
    const std::string out = runCommand(
        "nvidia-smi --query-gpu=pcie.link.gen.current,pcie.link.width.current,pcie.link.gen.max,pcie.link.width.max "
        "--format=csv,noheader,nounits");
    if (out.empty()) return "unknown (nvidia-smi unavailable)";
    unsigned gen = 0, width = 0, genMax = 0, widthMax = 0;
    if (std::sscanf(out.c_str(), "%u , %u , %u , %u", &gen, &width, &genMax, &widthMax) != 4) {
        return "unparsed: " + out;
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf), "gen%u x%u (max gen%u x%u)", gen, width, genMax, widthMax);
    return buf;
}

KeyValues collectSysInfo(const gfx::GfxCaps& caps) {
    KeyValues kv;
    kv.add("platform", platformId(caps));
#if defined(__APPLE__)
    kv.add("os", "macOS " + sysctlString("kern.osproductversion") + " (" + sysctlString("kern.osversion") + ")");
    kv.add("machine", sysctlString("hw.model"));
    kv.add("cpu", sysctlString("machdep.cpu.brand_string"));
    kv.add("cpu_cores", std::to_string(sysctlU64("hw.perflevel0.physicalcpu")) + "P+" +
                            std::to_string(sysctlU64("hw.perflevel1.physicalcpu")) + "E");
    kv.add("gpu_cores", macGpuCores());
    kv.add("ram", bytesToGiB(sysctlU64("hw.memsize")));
    kv.add("power_source", macPowerSource());
    kv.add("low_power_mode", processInfoLong("isLowPowerModeEnabled") == 1 ? "on" : "off");
#elif defined(_WIN32)
    kv.add("os", windowsVersion());
    kv.add("cpu", windowsCpuBrand());
    kv.add("cpu_logical", std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    kv.add("ram", GlobalMemoryStatusEx(&mem) ? bytesToGiB(mem.ullTotalPhys) : "unknown");
    kv.add("power_plan", windowsPowerPlan());
#else
    utsname u{};
    kv.add("os", uname(&u) == 0 ? std::string(u.sysname) + " " + u.release : "unknown");
    kv.add("cpu_logical", std::to_string(sysconf(_SC_NPROCESSORS_ONLN)));
    kv.add("ram", bytesToGiB(static_cast<u64>(sysconf(_SC_PHYS_PAGES)) * static_cast<u64>(sysconf(_SC_PAGE_SIZE))));
#endif
    kv.add("thread_qos", threadQosClass());
    kv.add("thread_priority", threadPriority());
    kv.add("process_nice", processNice());
    kv.add("thermal_state_start", thermalState());
    kv.add("backend", gfx::backendName(caps.backend));
    kv.add("gpu", caps.deviceName);
    kv.add("api", caps.apiVersion);
    kv.add("driver", caps.driverInfo);
    kv.add("unified_memory", caps.unifiedMemory ? "yes" : "no");
    kv.add("rebar", caps.rebar ? "on" : "off");
    kv.add("bar_heap_mb", std::to_string(caps.deviceLocalHostVisibleHeapBytes >> 20));
    kv.add("gpu_timestamps", caps.gpuTimestamps ? "yes" : "no");
    kv.add("clock_domain", caps.hostTimeDomain);
    for (u32 c = 0; c < gfx::kMemoryClassCount; ++c) {
        if (!caps.memoryClassSupported[c]) continue;
        kv.add(std::string("memclass.") + gfx::memoryClassName(static_cast<gfx::MemoryClass>(c)), caps.memoryClassInfo[c]);
    }
    return kv;
}

} // namespace lb::bench
