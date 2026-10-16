#pragma once
// Debug picture of a floor for --mode=procgen-dump (docs/02 §7): tiles, regions, lights,
// props, critical path, spawn zones. Debug colors only, not the art palette.
#include "procgen/FloorData.h"

#include <vector>

namespace lb::procgen {

struct FloorImage {
    u32 width = 0, height = 0;
    std::vector<u8> rgba;  // width * height * 4
};

/// Renders `f` at `pxPerTile` image pixels per tile.
FloorImage renderFloorImage(const FloorData& f, u32 pxPerTile = 4);

} // namespace lb::procgen
