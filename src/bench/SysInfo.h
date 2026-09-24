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

/// UTC timestamp, ISO 8601 (header only; never used by simulation logic, R4).
std::string isoTimestampUtc();

/// Runs a shell command and returns its stdout (trimmed), or empty on failure.
std::string runCommand(const char* command);

} // namespace lb::bench
