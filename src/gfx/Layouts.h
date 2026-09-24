#pragma once
// C++ mirrors of shaders/shared/layouts.slang. Any change here must be made there too;
// tests/test_layouts.cpp verifies both against the generated Slang reflection.
#include "core/Math.h"

namespace lb::gfx {

struct Perception {
    float2 lit_grad;
    f32 lit;
    f32 dist_player;
    u32 los_flags;
    u32 neighbor_count;
    u32 cell;
    u32 frame;
};
static_assert(sizeof(Perception) == 32 && alignof(Perception) == 8);

constexpr u32 kLosPlayer = 1u << 0;
constexpr u32 kLosNearOccluderEdge = 1u << 1;
constexpr u32 kLosInLanternCone = 1u << 2;

struct Intent {
    float2 desired_dir;
    f32 desired_speed;
    u32 packed;
};
static_assert(sizeof(Intent) == 16 && alignof(Intent) == 8);

struct RefineRequest {
    u32 agent;
    u32 kind;
    float2 target;
};
static_assert(sizeof(RefineRequest) == 16 && alignof(RefineRequest) == 8);

struct RefineResult {
    u32 agent;
    f32 lit_ss;
    float2 lit_grad_ss;
};
static_assert(sizeof(RefineResult) == 16 && alignof(RefineResult) == 8);

struct GpuEvent {
    float2 pos;
    u32 kind;
    u32 a;
    u32 b;
    u32 pad;
};
static_assert(sizeof(GpuEvent) == 24 && alignof(GpuEvent) == 8);

constexpr u32 kGpuEventCapacity = 1024;

struct GpuEvents {
    u32 count;
    u32 pad0;
    u32 pad1;
    u32 pad2;
    GpuEvent ev[kGpuEventCapacity];
};
static_assert(sizeof(GpuEvents) == 16 + 24 * kGpuEventCapacity && alignof(GpuEvents) == 8);

struct FrameConsts {
    float4 player;
    float4 lantern;
    float4 misc;
    uint4 ctrl;
};
static_assert(sizeof(FrameConsts) == 64 && alignof(FrameConsts) == 16);

constexpr u32 kAgentGroupSize = 64;
constexpr u32 kImageGroupX = 8;
constexpr u32 kImageGroupY = 8;
constexpr u32 kMaxAgents = 131072;

} // namespace lb::gfx
