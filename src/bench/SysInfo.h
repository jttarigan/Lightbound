#pragma once
// System description written into every bench CSV header (docs/06 §2, §12).
#include "gfx/Gfx.h"

#include <string>
#include <utility>
#include <vector>

namespace lb::bench {

struct KeyValues {
    std::vector<std::pair<std::string, std::string>> items;
    void add(const std::string& key, const std::string& value) { items.emplace_back(key, value); }
    std::string get(const std::string& key) const {
        for (const auto& kv : items) {
            if (kv.first == key) return kv.second;
        }
        return {};
    }
};

/// Platform id from docs/06 §2 (P-M5, P-M5-VK, P-3060, P-4060), or "<backend>-<device>" otherwise.
std::string platformId(const gfx::GfxCaps& caps);

/// OS, CPU, RAM, power/thermal state and GPU description (no PCIe link; see below).
KeyValues collectSysInfo(const gfx::GfxCaps& caps);

/// Current thermal state ("nominal", "fair", "serious", "critical", or "n/a").
std::string thermalState();

/// PCIe link reported by nvidia-smi (call while the GPU is under load: links downshift at
/// idle). Returns e.g. "gen4 x16 (max gen4 x16)", or an explanation when unavailable.
std::string queryNvidiaPcieLink();

/// QoS class the calling thread has requested (macOS: "user-interactive", "user-initiated", "default",
/// "utility", "background", "unspecified"), or "n/a" where the OS has no QoS classes.
std::string threadQosClass();

/// Scheduler priority of the calling thread, "cur=<n> base=<n> max=<n>" (macOS, proc_pidinfo; the QoS
/// classes map to base 47 / 37 / 31 / 20 / 4), or "n/a".
std::string threadPriority();

/// Nice value of the process (POSIX), or "n/a".
std::string processNice();

/// Requests QOS_CLASS_USER_INTERACTIVE for the calling thread (macOS). `note` says what happened.
/// Returns false where unsupported or refused. A diagnostic for the M5 spin-wait shift (DECISIONS #36).
bool requestThreadQosInteractive(std::string& note);

/// Raises the calling thread's base priority to `target` (macOS THREAD_PRECEDENCE_POLICY; 47 is what a
/// user-interactive thread has in an app, 31 the default of a command-line process). No root needed.
/// `note` says what happened; returns false where unsupported or the read-back misses the target.
bool requestThreadPriority(int target, std::string& note);

/// UTC timestamp, ISO 8601 (header only; never used by simulation logic, R4).
std::string isoTimestampUtc();

/// Runs a shell command and returns its stdout (trimmed), or empty on failure.
std::string runCommand(const char* command);

} // namespace lb::bench
