// ---------------------------------------------------------------------------
// Lightbound — shared GPU-visible struct layouts (single source of truth).
//
// The matching C++ definitions live in src/gfx/Layouts.h. tools/gen_layouts.py
// reflects this file for every shader target and generates layouts.gen.h; a unit
// test (tests/test_layouts.cpp) asserts the C++ structs match byte-for-byte.
//
// Layout rules (so that std430 (SPIR-V) and Slang's Metal "natural" layout agree):
//   * no 3-component vectors, ever
//   * every float2/uint2 field sits at an 8-byte-aligned offset
//   * every float4/uint4 field sits at a 16-byte-aligned offset
//   * struct sizes are multiples of their largest field alignment
//   * constant-buffer structs use only 16-byte fields (std140 == natural)
// See docs/05_ARCHITECTURE.md §5 and docs/DECISIONS.md (2026-09-23).
// ---------------------------------------------------------------------------

/// Per-agent perception, GPU → CPU research buffer. 32 B.
struct Perception
{
    float2 lit_grad;        // gradient of luminance (per px), from half-res mip
    float  lit;             // luminance at agent position (0..~3)
    float  dist_player;     // px
    uint   los_flags;       // bit0 LOS to player (coarse), bit1 near-occluder-edge, bit2 in lantern cone
    uint   neighbor_count;  // agents in 3x3 hash cells within 48 px (capped 255)
    uint   cell;            // spatial hash cell index
    uint   frame;           // frame index written (staleness / audit checks)
};

/// Per-agent intent, CPU → GPU research buffer. 16 B.
struct Intent
{
    float2 desired_dir;     // normalized or zero
    float  desired_speed;   // px/s
    uint   packed;          // bits 0-2 state, 3 alarmed, 4 speed_boost, 5 despawn, 8-31 frame (low 24 bits)
};

/// Refinement request, CPU → GPU. 16 B.
struct RefineRequest
{
    uint   agent;
    uint   kind;            // 0 = supersampled light, 1 = LOS ray to target
    float2 target;
};

/// Refinement result, GPU → CPU. 16 B.
struct RefineResult
{
    uint   agent;
    float  lit_ss;          // kind 0: supersampled luminance; kind 1: 1/0 visibility
    float2 lit_grad_ss;     // kind 0: gradient; kind 1: hit point
};

/// Gameplay event emitted by the GPU (contacts, beam→sigil hits). 24 B.
struct GpuEvent
{
    float2 pos;
    uint   kind;
    uint   a;
    uint   b;
    uint   pad;
};

static const uint LB_GPU_EVENT_CAPACITY = 1024;

/// GPU event queue, GPU → CPU. 16 + 1024*24 B.
struct GpuEvents
{
    uint     count;
    uint     pad0;
    uint     pad1;
    uint     pad2;
    GpuEvent ev[LB_GPU_EVENT_CAPACITY];
};

/// Per-frame constants, CPU → GPU (ring ×3). 16-byte fields only. 64 B for now;
/// grows in M4 (player, lantern, dynamic lights, dt, frame, K, strategy).
struct FrameConsts
{
    float4 player;          // xy = player pos (px), zw = lantern dir
    float4 lantern;         // x = range, y = cos(half-angle), z = intensity, w = ambient
    float4 misc;            // x = dt, y = time (s), z,w reserved
    uint4  ctrl;            // x = frame, y = K, z = strategy, w = agent count N
};

// Thread-group sizes (identical on both backends, R1).
static const uint LB_AGENT_GROUP_SIZE = 64;
static const uint LB_IMAGE_GROUP_X    = 8;
static const uint LB_IMAGE_GROUP_Y    = 8;
