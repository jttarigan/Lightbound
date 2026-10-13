#include "bench/MicroResume.h"

#include <cstdlib>

namespace lb::bench {

namespace {

constexpr const char* kColumnsPrefix = "platform,path,submit,cpuwait,payload_bytes,iter,";
constexpr const char* kFailureNotePrefix = "# cell_failures: ";
constexpr usize kDataColumns = 15;

bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    usize start = 0;
    while (true) {
        const usize pos = s.find(sep, start);
        out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return out;
}

bool parseU64(const std::string& s, u64& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtoull(s.c_str(), &end, 10);
    return end != nullptr && *end == '\0';
}

struct Line {
    enum Kind { Data, FailureNote, Other } kind = Other;
    std::string text;
    usize cell = 0;      // Data, FailureNote: index into the cell table
    u64 failures = 0;    // FailureNote
};

usize cellIndex(std::vector<ResumeCell>& cells, const std::string& path, const std::string& submit,
                const std::string& cpuwait, u64 payload) {
    for (usize i = 0; i < cells.size(); ++i) {
        const ResumeCell& c = cells[i];
        if (c.payload == payload && c.path == path && c.submit == submit && c.cpuwait == cpuwait) return i;
    }
    cells.push_back({path, submit, cpuwait, payload, 0});
    return cells.size() - 1;
}

} // namespace

std::string ResumeScan::headerValue(const std::string& key) const {
    for (const auto& kv : header) {
        if (kv.first == key) return kv.second;
    }
    return {};
}

bool ResumeScan::has(const std::string& path, const std::string& submit, const std::string& cpuwait, u64 payload) const {
    for (const ResumeCell& c : cells) {
        if (c.payload == payload && c.path == path && c.submit == submit && c.cpuwait == cpuwait) return true;
    }
    return false;
}

std::string cellFailureNote(const std::string& path, const std::string& submit, const std::string& cpuwait,
                            u64 payload, u64 failures) {
    return kFailureNotePrefix + path + "," + submit + "," + cpuwait + "," + std::to_string(payload) + "," +
           std::to_string(failures);
}

bool scanResumeCsv(const std::string& text, u32 iterations, u32 bandwidthRows, ResumeScan& out) {
    out = ResumeScan{};
    // Only newline-terminated lines count: an unterminated tail is a partially written row.
    std::vector<std::string> raw;
    usize start = 0;
    while (true) {
        const usize nl = text.find('\n', start);
        if (nl == std::string::npos) break;
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        raw.push_back(std::move(line));
        start = nl + 1;
    }

    usize body = 0;
    bool haveColumns = false;
    for (; body < raw.size(); ++body) {
        const std::string& line = raw[body];
        out.kept.push_back(line);
        if (startsWith(line, kColumnsPrefix)) {
            haveColumns = true;
            ++body;
            break;
        }
        const usize sep = line.find(": ");
        if (startsWith(line, "# ") && sep != std::string::npos) {
            out.header.emplace_back(line.substr(2, sep - 2), line.substr(sep + 2));
        }
    }
    if (!haveColumns) {
        out = ResumeScan{};
        return false;
    }

    std::vector<ResumeCell> seen;
    std::vector<Line> lines;
    for (; body < raw.size(); ++body) {
        Line l;
        l.text = raw[body];
        if (startsWith(l.text, kFailureNotePrefix)) {
            const std::vector<std::string> f = split(l.text.substr(std::string(kFailureNotePrefix).size()), ',');
            u64 payload = 0;
            if (f.size() != 5 || !parseU64(f[3], payload) || !parseU64(f[4], l.failures)) continue;
            l.kind = Line::FailureNote;
            l.cell = cellIndex(seen, f[0], f[1], f[2], payload);
        } else if (startsWith(l.text, "#")) {
            // The footer belongs to the process that wrote it; the resuming process writes its own.
            if (startsWith(l.text, "# thermal_state_end: ") || startsWith(l.text, "# failures: ")) continue;
        } else {
            const std::vector<std::string> f = split(l.text, ',');
            u64 payload = 0;
            if (f.size() != kDataColumns || !parseU64(f[4], payload)) {
                ++out.droppedRows;
                continue;
            }
            l.kind = Line::Data;
            l.cell = cellIndex(seen, f[1], f[2], f[3], payload);
            ++seen[l.cell].rows;
        }
        lines.push_back(std::move(l));
    }

    std::vector<bool> complete(seen.size());
    for (usize i = 0; i < seen.size(); ++i) {
        const u32 want = startsWith(seen[i].path, "bw_") ? bandwidthRows : iterations;
        complete[i] = seen[i].rows == want;
        if (complete[i]) out.cells.push_back(seen[i]);
    }
    for (Line& l : lines) {
        if (l.kind != Line::Other && !complete[l.cell]) {
            if (l.kind == Line::Data) ++out.droppedRows;
            continue;
        }
        if (l.kind == Line::FailureNote) out.failures += l.failures;
        out.kept.push_back(std::move(l.text));
    }
    return true;
}

} // namespace lb::bench
