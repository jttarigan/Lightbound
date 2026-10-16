// --mode=procgen-dump (docs/02 §7): generate one floor, print its hash and metrics, write a PNG.
#include "app/ProcgenDump.h"

#include "core/Log.h"
#include "procgen/FloorData.h"
#include "procgen/FloorImage.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cinttypes>
#include <cstdio>
#include <string>

namespace lb::app {

int runProcgenDump(const Options& opt) {
    using namespace lb::procgen;
    FloorRequest req;
    req.runSeed = opt.seed;
    req.depth = opt.depth;
    FloorData f;
    const GenResult r = generateFloor(req, f);
    if (r.status != GenStatus::Ok) {
        LB_LOG_ERROR("procgen: seed %" PRIu64 " depth %u failed after 16 attempts (last: %s)", opt.seed, opt.depth,
                     r.lastFailure);
        return 1;
    }
    const Metrics& m = f.metrics;
    // One machine-readable result line (like the bench CSV writers, printed directly).
    std::printf("floor seed=%" PRIu64 " depth=%u size=%ux%u biome=%s hash=%016" PRIx64 " attempts=%u gen_ms=%.1f "
                "floor_tiles=%u rooms=%u loops=%u critical_path_len=%u lit_area_fraction=%.3f dark_pocket_area=%u "
                "nest_count=%u mirror_count=%u lights=%zu\n",
                f.runSeed, f.depth, f.w, f.h, biomeName(f.biome), floorHash(f), m.attempts, m.genMs, m.floorTiles,
                m.rooms, m.loops, m.criticalPathLen, static_cast<f64>(m.litAreaFraction), m.darkPocketArea,
                m.nestCount, m.mirrorCount, f.staticLights.size());

    const std::string out = opt.out.empty()
        ? "floor_" + std::to_string(opt.seed) + "_d" + std::to_string(opt.depth) + ".png"
        : opt.out;
    const FloorImage img = renderFloorImage(f);
    if (stbi_write_png(out.c_str(), static_cast<int>(img.width), static_cast<int>(img.height), 4, img.rgba.data(),
                       static_cast<int>(img.width * 4u)) == 0) {
        LB_LOG_ERROR("procgen: cannot write %s", out.c_str());
        return 1;
    }
    LB_LOG_INFO("procgen: wrote %s (%ux%u)", out.c_str(), img.width, img.height);
    return 0;
}

} // namespace lb::app
