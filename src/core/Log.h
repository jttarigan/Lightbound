#pragma once
// Tiny logger. Use LB_LOG_* everywhere except bench CSV writers (which use FILE* directly).
#include "core/Types.h"

#include <cstdarg>

namespace lb {

enum class LogLevel : u8 { Trace = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

void logSetLevel(LogLevel level);
LogLevel logGetLevel();

/// Number of Error-level messages logged since start (used to fail runs on backend errors).
u64 logErrorCount();

#if defined(__GNUC__) || defined(__clang__)
#define LB_PRINTF_LIKE(fmtIdx, firstArg) __attribute__((format(printf, fmtIdx, firstArg)))
#else
#define LB_PRINTF_LIKE(fmtIdx, firstArg)
#endif

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...) LB_PRINTF_LIKE(4, 5);
void logMessageV(LogLevel level, const char* file, int line, const char* fmt, va_list args) LB_PRINTF_LIKE(4, 0);

[[noreturn]] void fatalAbort(const char* file, int line, const char* cond, const char* msg);

} // namespace lb

#define LB_LOG_TRACE(...) ::lb::logMessage(::lb::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
#define LB_LOG_INFO(...)  ::lb::logMessage(::lb::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LB_LOG_WARN(...)  ::lb::logMessage(::lb::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LB_LOG_ERROR(...) ::lb::logMessage(::lb::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

/// Always-on check: aborts with a message. Use for invariants whose violation would
/// invalidate an experiment (e.g. R7 hidden-copy counters).
#define LB_CHECK(cond, msg) \
    do { if (!(cond)) ::lb::fatalAbort(__FILE__, __LINE__, #cond, (msg)); } while (0)

#if defined(NDEBUG)
#define LB_ASSERT(cond, msg) do { (void)sizeof(cond); } while (0)
#else
#define LB_ASSERT(cond, msg) LB_CHECK(cond, msg)
#endif
