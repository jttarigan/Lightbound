#pragma once
#include "app/Cli.h"

namespace lb::app {

/// --mode=procgen-dump: generates the floor for --seed / --depth, prints hash and metrics,
/// writes a debug PNG to --out (default floor_<seed>_d<depth>.png). Returns the exit code.
int runProcgenDump(const Options& opt);

} // namespace lb::app
