#include "app/Cli.h"
#include "app/Version.h"
#include "bench/Microbench.h"
#include "core/Log.h"
#include "core/Time.h"
#include "gfx/Gfx.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int runPlay(const lb::app::Options& opt) {
    using namespace lb;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        LB_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    flags |= (gfx::compiledBackend() == gfx::Backend::Metal) ? SDL_WINDOW_METAL : SDL_WINDOW_VULKAN;
    SDL_Window* window = SDL_CreateWindow("Lightbound", static_cast<int>(opt.width),
                                          static_cast<int>(opt.height), flags);
    if (window == nullptr) {
        LB_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const std::string shaderDir = std::string(SDL_GetBasePath()) + "shaders/";
    gfx::GfxDesc desc;
    desc.window = window;
    desc.vsync = opt.vsync;
    desc.validation = opt.validation;
    desc.shaderDir = shaderDir.c_str();

    std::unique_ptr<gfx::Gfx> g = gfx::createGfx();
    if (!g->init(desc)) {
        LB_LOG_ERROR("graphics init failed (backend %s)", gfx::backendName(gfx::compiledBackend()));
        g->shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    LB_LOG_INFO("backend=%s device=\"%s\" backbuffer=%ux%u", gfx::backendName(g->caps().backend),
                g->caps().deviceName, g->caps().backbufferWidth, g->caps().backbufferHeight);

    bool running = true;
    u32 frames = 0;
    Stopwatch clock;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_ESCAPE) running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                g->resize(static_cast<u32>(ev.window.data1 > 0 ? ev.window.data1 : 1),
                          static_cast<u32>(ev.window.data2 > 0 ? ev.window.data2 : 1));
                break;
            default: break;
            }
        }
        if (!running) break;

        if (g->beginFrame()) {
            g->clearBackbuffer(gfx::kClearCave);
            g->endFrame();
            ++frames;
        }
        if (opt.exitAfter != 0 && frames >= opt.exitAfter) running = false;
    }

    const f64 seconds = clock.elapsedSec();
    const u64 gfxErrors = g->errorCount();
    LB_LOG_INFO("rendered %u frames in %.2f s (%.2f ms/frame avg), backend errors: %llu", frames, seconds,
                frames > 0 ? seconds * 1000.0 / static_cast<f64>(frames) : 0.0,
                static_cast<unsigned long long>(gfxErrors));

    g->shutdown();
    g.reset();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return gfxErrors == 0 ? 0 : 1;
}

std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    lb::usize start = 0;
    while (start < s.size()) {
        const lb::usize comma = s.find(',', start);
        out.push_back(s.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

int runMicrobench(const lb::app::Options& opt, int argc, char** argv) {
    using namespace lb;
    bench::MicrobenchConfig cfg;
    cfg.iterations = opt.frames;
    cfg.warmup = opt.warmup;
    if (opt.submitExplicit) cfg.chainModes = {opt.submit == app::Submit::Chain};
    else cfg.chainModes = {true, false};
    if (opt.cpuwaitExplicit) {
        cfg.waitModes = {opt.cpuwait == app::CpuWait::Spin ? gfx::CpuWaitMode::Spin : gfx::CpuWaitMode::Block};
    } else {
        cfg.waitModes = {gfx::CpuWaitMode::Spin, gfx::CpuWaitMode::Block};
    }
    cfg.paths = splitList(opt.microPaths);
    cfg.payloads = bench::defaultMicroPayloads();
    if (!opt.microPayloads.empty()) {
        std::string err;
        if (!bench::parsePayloadList(opt.microPayloads, cfg.payloads, err)) {
            std::fprintf(stderr, "lightbound: --micro-payloads: %s\n", err.c_str());
            return 2;
        }
    }
    cfg.outPath = opt.out.empty() ? std::string("results/micro.csv") : opt.out;
    for (int i = 0; i < argc; ++i) cfg.commandLine += (i > 0 ? " " : "") + std::string(argv[i]);
    cfg.options = app::describeOptions(opt);
    cfg.tag = opt.tag;

    // Headless: no window or swapchain, only the compute/copy queue (DECISIONS #19).
    const char* base = SDL_GetBasePath();
    const std::string shaderDir = std::string(base != nullptr ? base : "") + "shaders/";
    gfx::GfxDesc desc;
    desc.window = nullptr;
    desc.vsync = false;
    desc.validation = opt.validation;
    desc.shaderDir = shaderDir.c_str();
    std::unique_ptr<gfx::Gfx> g = gfx::createGfx();
    if (!g->init(desc)) {
        LB_LOG_ERROR("graphics init failed (backend %s)", gfx::backendName(gfx::compiledBackend()));
        return 2;
    }
    const int rc = bench::runMicrobench(*g, cfg);
    g->shutdown();
    return rc;
}

} // namespace

int main(int argc, char** argv) {
    using namespace lb::app;

    Options opt;
    std::string error;
    switch (parseCommandLine(argc, argv, opt, error)) {
    case ParseStatus::Help: printUsage(stdout); return 0;
    case ParseStatus::Error:
        std::fprintf(stderr, "lightbound: %s\n\n", error.c_str());
        printUsage(stderr);
        return 2;
    case ParseStatus::Ok: break;
    }
    if (opt.version) {
        std::printf("lightbound %s (backend %s, generator v%u)\n", lb::kVersion,
                    lb::gfx::backendName(lb::gfx::compiledBackend()), lb::kGeneratorVersion);
        return 0;
    }

    lb::logSetLevel(static_cast<lb::LogLevel>(opt.logLevel));
    const std::string note = normalizeOptions(opt);
    if (!note.empty()) LB_LOG_WARN("%s", note.c_str());
    LB_LOG_INFO("lightbound %s | %s", lb::kVersion, describeOptions(opt).c_str());

    switch (opt.mode) {
    case Mode::Play: return runPlay(opt);
    case Mode::Microbench: return runMicrobench(opt, argc, argv);
    case Mode::ProcgenDump:
        std::fprintf(stderr, "mode '%s' is not implemented yet (milestone M2)\n", cliName(opt.mode));
        return 2;
    case Mode::Bench:
        std::fprintf(stderr, "mode '%s' is not implemented yet (milestone M4)\n", cliName(opt.mode));
        return 2;
    case Mode::Audit:
        std::fprintf(stderr, "mode '%s' is not implemented yet (milestone M8)\n", cliName(opt.mode));
        return 2;
    }
    return 2;
}
