#include "app/Cli.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

namespace lb::app {

namespace {

bool ieq(const char* a, const char* b) {
    for (;; ++a, ++b) {
        const int ca = std::tolower(static_cast<unsigned char>(*a));
        const int cb = std::tolower(static_cast<unsigned char>(*b));
        if (ca != cb) return false;
        if (ca == 0) return true;
    }
}

bool parseU64(const std::string& s, u64& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const unsigned long long v = std::strtoull(s.c_str(), &end, 0);
    if (end == nullptr || *end != '\0') return false;
    out = static_cast<u64>(v);
    return true;
}

bool parseU32(const std::string& s, u32& out) {
    u64 v = 0;
    if (!parseU64(s, v) || v > 0xFFFFFFFFull) return false;
    out = static_cast<u32>(v);
    return true;
}

bool parseOnOff(const std::string& s, bool& out) {
    if (ieq(s.c_str(), "on") || ieq(s.c_str(), "1") || ieq(s.c_str(), "true") || ieq(s.c_str(), "yes")) {
        out = true;
        return true;
    }
    if (ieq(s.c_str(), "off") || ieq(s.c_str(), "0") || ieq(s.c_str(), "false") || ieq(s.c_str(), "no")) {
        out = false;
        return true;
    }
    return false;
}

template <class E>
struct EnumName { const char* name; E value; };

template <class E, usize N>
bool parseEnum(const std::string& s, const EnumName<E> (&table)[N], E& out) {
    for (const auto& e : table) {
        if (ieq(s.c_str(), e.name)) {
            out = e.value;
            return true;
        }
    }
    return false;
}

constexpr EnumName<Mode> kModes[] = {
    {"play", Mode::Play}, {"bench", Mode::Bench}, {"microbench", Mode::Microbench},
    {"audit", Mode::Audit}, {"procgen-dump", Mode::ProcgenDump},
};
constexpr EnumName<Strategy> kStrategies[] = {
    {"S0", Strategy::S0}, {"S1", Strategy::S1}, {"S2", Strategy::S2}, {"S3", Strategy::S3},
    {"0", Strategy::S0}, {"1", Strategy::S1}, {"2", Strategy::S2}, {"3", Strategy::S3},
};
constexpr EnumName<Submit> kSubmits[] = {{"chain", Submit::Chain}, {"perpass", Submit::PerPass}};
constexpr EnumName<CpuWait> kCpuWaits[] = {{"spin", CpuWait::Spin}, {"block", CpuWait::Block}};
constexpr EnumName<MemVariant> kMemVariants[] = {
    {"default", MemVariant::Default}, {"rebar", MemVariant::Rebar},
    {"hostcached", MemVariant::HostCached}, {"coherent", MemVariant::Coherent},
};
constexpr EnumName<AudioMode> kAudioModes[] = {
    {"on", AudioMode::On}, {"null", AudioMode::Null}, {"off", AudioMode::Off}};
constexpr EnumName<Preset> kPresets[] = {
    {"none", Preset::None}, {"quick", Preset::Quick}, {"core", Preset::Core}, {"audit", Preset::Audit}};

struct FlagSpec { const char* name; bool takesValue; };
constexpr FlagSpec kFlags[] = {
    {"mode", true}, {"strategy", true}, {"k", true}, {"delay", true}, {"agents", true}, {"seed", true},
    {"frames", true}, {"warmup", true}, {"submit", true}, {"cpuwait", true}, {"threads", true},
    {"memvariant", true}, {"audit", true}, {"audio", true}, {"preset", true}, {"out", true},
    {"tag", true}, {"power", true},
    {"vsync", true}, {"validation", true}, {"width", true}, {"height", true}, {"exit-after", true},
    {"log", true}, {"micro-paths", true}, {"micro-payloads", true}, {"help", false}, {"version", false},
};

const FlagSpec* findFlag(const std::string& name) {
    for (const auto& f : kFlags) {
        if (name == f.name) return &f;
    }
    return nullptr;
}

} // namespace

ParseStatus parseCommandLine(int argc, const char* const* argv, Options& out, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strncmp(arg, "--", 2) != 0) {
            error = std::string("unexpected argument '") + arg + "' (flags start with --)";
            return ParseStatus::Error;
        }
        std::string name(arg + 2);
        std::string value;
        bool hasValue = false;
        const usize eq = name.find('=');
        if (eq != std::string::npos) {
            value = name.substr(eq + 1);
            name = name.substr(0, eq);
            hasValue = true;
        }
        const FlagSpec* spec = findFlag(name);
        if (spec == nullptr) {
            error = "unknown flag '--" + name + "'";
            return ParseStatus::Error;
        }
        if (spec->takesValue && !hasValue) {
            if (i + 1 >= argc) {
                error = "flag '--" + name + "' needs a value";
                return ParseStatus::Error;
            }
            value = argv[++i];
            hasValue = true;
        }
        if (!spec->takesValue && hasValue) {
            error = "flag '--" + name + "' takes no value";
            return ParseStatus::Error;
        }

        bool ok = true;
        if (name == "mode") ok = parseEnum(value, kModes, out.mode);
        else if (name == "strategy") ok = parseEnum(value, kStrategies, out.strategy);
        else if (name == "k") ok = parseU32(value, out.k) && out.k >= 1 && out.k <= 4;
        else if (name == "delay") ok = parseU32(value, out.delay) && out.delay >= 1 && out.delay <= 2;
        else if (name == "agents") ok = parseU32(value, out.agents) && out.agents >= 1 && out.agents <= 131072;
        else if (name == "seed") { ok = parseU64(value, out.seed); out.seedExplicit = ok; }
        else if (name == "frames") { ok = parseU32(value, out.frames) && out.frames >= 1; out.framesExplicit = ok; }
        else if (name == "warmup") { ok = parseU32(value, out.warmup); out.warmupExplicit = ok; }
        else if (name == "submit") { ok = parseEnum(value, kSubmits, out.submit); out.submitExplicit = ok; }
        else if (name == "cpuwait") { ok = parseEnum(value, kCpuWaits, out.cpuwait); out.cpuwaitExplicit = ok; }
        else if (name == "threads") ok = parseU32(value, out.threads) && out.threads >= 1 && out.threads <= 64;
        else if (name == "memvariant") ok = parseEnum(value, kMemVariants, out.memvariant);
        else if (name == "audit") ok = parseOnOff(value, out.audit);
        else if (name == "audio") ok = parseEnum(value, kAudioModes, out.audio);
        else if (name == "preset") ok = parseEnum(value, kPresets, out.preset);
        else if (name == "out") out.out = value;
        else if (name == "tag") out.tag = value;
        else if (name == "power") ok = parseOnOff(value, out.power);
        else if (name == "vsync") { ok = parseOnOff(value, out.vsync); out.vsyncExplicit = ok; }
        else if (name == "validation") ok = parseOnOff(value, out.validation);
        else if (name == "width") ok = parseU32(value, out.width) && out.width >= 64;
        else if (name == "height") ok = parseU32(value, out.height) && out.height >= 64;
        else if (name == "exit-after") ok = parseU32(value, out.exitAfter);
        else if (name == "micro-paths") out.microPaths = value;
        else if (name == "micro-payloads") out.microPayloads = value;
        else if (name == "log") {
            u32 lvl = 0;
            if (ieq(value.c_str(), "trace")) lvl = 0;
            else if (ieq(value.c_str(), "info")) lvl = 1;
            else if (ieq(value.c_str(), "warn")) lvl = 2;
            else if (ieq(value.c_str(), "error")) lvl = 3;
            else ok = parseU32(value, lvl) && lvl <= 3;
            out.logLevel = static_cast<u8>(lvl);
        }
        else if (name == "help") { out.help = true; return ParseStatus::Help; }
        else if (name == "version") out.version = true;

        if (!ok) {
            error = "invalid value '" + value + "' for flag '--" + name + "'";
            return ParseStatus::Error;
        }
    }
    return ParseStatus::Ok;
}

std::string normalizeOptions(Options& opt) {
    std::string note;
    if ((opt.strategy == Strategy::S0 || opt.strategy == Strategy::S3) && opt.k != 1) {
        note += "K forced to 1 for " + std::string(cliName(opt.strategy)) + " (docs/05 §3.1). ";
        opt.k = 1;
    }
    if (!opt.vsyncExplicit) {
        opt.vsync = (opt.mode == Mode::Play);
    }
    if (opt.mode == Mode::Audit) opt.audit = true;
    if (opt.mode == Mode::Microbench) {
        // Protocol §6: 2,000 iterations after 200 warm-up per cell.
        if (!opt.framesExplicit) opt.frames = 2000;
        if (!opt.warmupExplicit) opt.warmup = 200;
    }
    return note;
}

void printUsage(FILE* to) {
    std::fputs(
        "Lightbound — survival-stealth roguelite & CPU–GPU cooperation research instrument\n"
        "\n"
        "usage: lightbound [--flag=value | --flag value]...\n"
        "\n"
        "Protocol flags (docs/06_RESEARCH_PROTOCOL.md §11):\n"
        "  --mode=play|bench|microbench|audit|procgen-dump   (default play)\n"
        "  --strategy=S0|S1|S2|S3        transfer strategy   (default S2)\n"
        "  --k=1..4                      round trips/frame   (default 1; S0/S3 force 1)\n"
        "  --delay=1..2                  S0 readback delay   (default 2)\n"
        "  --agents=N                    swarm size, ≤131072 (default 20000)\n"
        "  --seed=S                      run seed, dec or 0x (default 1234)\n"
        "  --frames=N                    measured frames     (default 3600)\n"
        "  --warmup=N                    discarded frames    (default 600)\n"
        "  --submit=chain|perpass        GPU submission mode (default chain)\n"
        "  --cpuwait=spin|block          CPU wait mode       (default spin)\n"
        "  --threads=W                   worker threads      (default 4)\n"
        "  --memvariant=default|rebar|hostcached|coherent    (default default)\n"
        "  --audit=on|off                decision oracle     (default off)\n"
        "  --audio=on|null|off           audio device        (default on)\n"
        "  --preset=quick|core|audit     condition preset\n"
        "  --out=path.csv                CSV output path\n"
        "  --tag=text                    free-text run tag\n"
        "  --power=on|off                power capture       (default off)\n"
        "\n"
        "Development flags:\n"
        "  --vsync=on|off                (default: on in play, off otherwise)\n"
        "  --validation=on|off           API validation layers (default off)\n"
        "  --width=W --height=H          window size (default 1920x1080)\n"
        "  --exit-after=N                play mode: quit after N frames\n"
        "  --micro-paths=a,b             microbench: only these paths (empty,s1_copy,s2_direct,\n"
        "                                s2_hostcached,s2_rebar,s2_coherent)\n"
        "  --micro-payloads=4K,1M        microbench: only these payload sizes\n"
        "  --log=trace|info|warn|error   log level (default info)\n"
        "  --help  --version\n",
        to);
}

std::string describeOptions(const Options& o) {
    std::string s;
    s += "mode=" + std::string(cliName(o.mode));
    s += " strategy=" + std::string(cliName(o.strategy));
    s += " k=" + std::to_string(o.k);
    s += " delay=" + std::to_string(o.delay);
    s += " agents=" + std::to_string(o.agents);
    s += " seed=" + std::to_string(o.seed);
    s += " frames=" + std::to_string(o.frames);
    s += " warmup=" + std::to_string(o.warmup);
    s += " submit=" + std::string(cliName(o.submit));
    s += " cpuwait=" + std::string(cliName(o.cpuwait));
    s += " threads=" + std::to_string(o.threads);
    s += " memvariant=" + std::string(cliName(o.memvariant));
    s += std::string(" audit=") + (o.audit ? "on" : "off");
    s += " audio=" + std::string(cliName(o.audio));
    s += " preset=" + std::string(cliName(o.preset));
    s += " out=" + o.out;
    s += " tag=" + o.tag;
    s += std::string(" power=") + (o.power ? "on" : "off");
    s += std::string(" vsync=") + (o.vsync ? "on" : "off");
    s += std::string(" validation=") + (o.validation ? "on" : "off");
    if (!o.microPaths.empty()) s += " micro-paths=" + o.microPaths;
    if (!o.microPayloads.empty()) s += " micro-payloads=" + o.microPayloads;
    return s;
}

const char* cliName(Mode v) {
    switch (v) {
    case Mode::Play: return "play";
    case Mode::Bench: return "bench";
    case Mode::Microbench: return "microbench";
    case Mode::Audit: return "audit";
    case Mode::ProcgenDump: return "procgen-dump";
    }
    return "?";
}
const char* cliName(Strategy v) {
    switch (v) {
    case Strategy::S0: return "S0";
    case Strategy::S1: return "S1";
    case Strategy::S2: return "S2";
    case Strategy::S3: return "S3";
    }
    return "?";
}
const char* cliName(Submit v) { return v == Submit::Chain ? "chain" : "perpass"; }
const char* cliName(CpuWait v) { return v == CpuWait::Spin ? "spin" : "block"; }
const char* cliName(MemVariant v) {
    switch (v) {
    case MemVariant::Default: return "default";
    case MemVariant::Rebar: return "rebar";
    case MemVariant::HostCached: return "hostcached";
    case MemVariant::Coherent: return "coherent";
    }
    return "?";
}
const char* cliName(AudioMode v) {
    switch (v) {
    case AudioMode::On: return "on";
    case AudioMode::Null: return "null";
    case AudioMode::Off: return "off";
    }
    return "?";
}
const char* cliName(Preset v) {
    switch (v) {
    case Preset::None: return "none";
    case Preset::Quick: return "quick";
    case Preset::Core: return "core";
    case Preset::Audit: return "audit";
    }
    return "?";
}

} // namespace lb::app
