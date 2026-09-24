#pragma once
// Command-line options. Every flag from docs/06_RESEARCH_PROTOCOL.md §11 is parsed here.
#include "core/Types.h"

#include <cstdio>
#include <string>

namespace lb::app {

enum class Mode : u8 { Play, Bench, Microbench, Audit, ProcgenDump };
enum class Strategy : u8 { S0, S1, S2, S3 };
enum class Submit : u8 { Chain, PerPass };
enum class CpuWait : u8 { Spin, Block };
enum class MemVariant : u8 { Default, Rebar, HostCached, Coherent };
enum class AudioMode : u8 { On, Null, Off };
enum class Preset : u8 { None, Quick, Core, Audit };

struct Options {
    // Protocol §11 flags
    Mode mode = Mode::Play;
    Strategy strategy = Strategy::S2;
    u32 k = 1;
    u32 delay = 2;
    u32 agents = 20000;
    u64 seed = 1234;
    bool seedExplicit = false;
    u32 frames = 3600;          // bench: measured frames; microbench: measured iterations per cell
    u32 warmup = 600;           // bench: discarded frames; microbench: warm-up iterations per cell
    Submit submit = Submit::Chain;
    CpuWait cpuwait = CpuWait::Spin;
    bool framesExplicit = false;
    bool warmupExplicit = false;
    bool submitExplicit = false;  // microbench sweeps both modes unless given
    bool cpuwaitExplicit = false;
    u32 threads = 4;
    MemVariant memvariant = MemVariant::Default;
    bool audit = false;
    AudioMode audio = AudioMode::On;
    Preset preset = Preset::None;
    std::string out;
    std::string tag;
    bool power = false;

    // Development / harness flags (not part of the protocol)
    bool vsync = true;          // default on in play, off in every other mode
    bool vsyncExplicit = false;
    bool validation = false;    // API validation layers
    u32 width = 1920;
    u32 height = 1080;
    u32 exitAfter = 0;          // play mode: quit after N rendered frames (0 = never)
    std::string microPaths;     // microbench: comma list of paths to run (default all)
    std::string microPayloads;  // microbench: comma list of payload sizes, e.g. 4K,1M (default protocol set)
    u8 logLevel = 1;            // 0 trace, 1 info, 2 warn, 3 error
    bool help = false;
    bool version = false;
};

enum class ParseStatus { Ok, Help, Error };

/// Parses argv[1..]. Accepts --flag=value and --flag value. On Error, `error` says why.
ParseStatus parseCommandLine(int argc, const char* const* argv, Options& out, std::string& error);

/// Applies protocol rules that depend on several flags (S0/S3 force K=1, vsync default
/// per mode). Returns a human-readable note for each adjustment made (may be empty).
std::string normalizeOptions(Options& opt);

void printUsage(FILE* to);

/// Canonical one-line rendering of all options (for CSV headers and logs).
std::string describeOptions(const Options& opt);

const char* cliName(Mode v);
const char* cliName(Strategy v);
const char* cliName(Submit v);
const char* cliName(CpuWait v);
const char* cliName(MemVariant v);
const char* cliName(AudioMode v);
const char* cliName(Preset v);

} // namespace lb::app
