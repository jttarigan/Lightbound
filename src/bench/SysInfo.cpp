#include "bench/SysInfo.h"

#include <bit>
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
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#include <powrprof.h>
#else
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
