#pragma once
// Resume support for an interrupted microbenchmark run (DECISIONS #28). Cells are measured
// independently and written to micro.csv one whole cell at a time, so a later process can keep
// the complete cells of an interrupted run and measure only the missing ones.
#include "core/Types.h"

#include <string>
#include <utility>
#include <vector>

namespace lb::bench {

struct ResumeCell {
    std::string path, submit, cpuwait;
    u64 payload = 0;
    u32 rows = 0;
};

struct ResumeScan {
    std::vector<std::pair<std::string, std::string>> header;  // "# key: value" lines above the column line
    std::vector<std::string> kept;   // lines to carry over: header, column line, complete cells and their notes
    std::vector<ResumeCell> cells;   // complete cells, in file order
    u64 failures = 0;                // verification/R7 failures recorded for the complete cells
    u32 droppedRows = 0;             // data rows of incomplete cells (process killed while writing)

    std::string headerValue(const std::string& key) const;
    bool has(const std::string& path, const std::string& submit, const std::string& cpuwait, u64 payload) const;
};

/// Note written in front of a cell's rows when the cell had verification/R7 failures, so the
/// count survives a process restart.
std::string cellFailureNote(const std::string& path, const std::string& submit, const std::string& cpuwait,
                            u64 payload, u64 failures);

/// Scans the text of a micro.csv left by an interrupted run. A cell is complete when it has
/// exactly `iterations` rows (`bandwidthRows` for the bw_* rows). Rows of incomplete cells,
/// malformed lines and the end-of-run footer are dropped. Returns false if the text has no
/// column line (nothing to resume from).
bool scanResumeCsv(const std::string& text, u32 iterations, u32 bandwidthRows, ResumeScan& out);

} // namespace lb::bench
