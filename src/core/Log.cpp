#include "core/Log.h"
#include "core/Time.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace lb {

namespace {
std::atomic<LogLevel> g_level{LogLevel::Info};
std::atomic<u64> g_errorCount{0};
std::mutex g_mutex;
const u64 g_startNs = nowNs();

const char* levelTag(LogLevel l) {
    switch (l) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Off:   return "OFF  ";
    }
    return "?????";
}

const char* baseName(const char* path) {
    const char* s = std::strrchr(path, '/');
#if defined(_WIN32)
    const char* b = std::strrchr(path, '\\');
    if (b != nullptr && (s == nullptr || b > s)) s = b;
#endif
    return s != nullptr ? s + 1 : path;
}
} // namespace

void logSetLevel(LogLevel level) { g_level.store(level, std::memory_order_relaxed); }
LogLevel logGetLevel() { return g_level.load(std::memory_order_relaxed); }
u64 logErrorCount() { return g_errorCount.load(std::memory_order_relaxed); }

void logMessageV(LogLevel level, const char* file, int line, const char* fmt, va_list args) {
    if (level == LogLevel::Error) g_errorCount.fetch_add(1, std::memory_order_relaxed);
    if (level < g_level.load(std::memory_order_relaxed)) return;

    char msg[2048];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    const f64 t = nsToSec(nowNs() - g_startNs);
    std::lock_guard<std::mutex> lock(g_mutex);
    std::fprintf(stderr, "[%9.3f] %s %s:%d: %s\n", t, levelTag(level), baseName(file), line, msg);
    std::fflush(stderr);
}

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logMessageV(level, file, line, fmt, args);
    va_end(args);
}

void fatalAbort(const char* file, int line, const char* cond, const char* msg) {
    logMessage(LogLevel::Error, file, line, "CHECK FAILED: %s — %s", cond, msg);
    std::fflush(stderr);
    std::abort();
}

} // namespace lb
