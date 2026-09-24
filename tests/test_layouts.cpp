// Verifies the C++ GPU structs (src/gfx/Layouts.h) match the Slang structs
// (shaders/shared/layouts.slang) via the header generated from slangc reflection.
#include "gfx/Layouts.h"
#include "layouts.gen.h"

#include <doctest/doctest.h>

#include <cstddef>

using namespace lb::gfx;

#define LB_CHECK_FIELD(Struct, field)                                                       \
    do {                                                                                    \
        CHECK(offsetof(Struct, field) == slang_layout::Struct::field##_offset);             \
        CHECK(sizeof(Struct::field) == slang_layout::Struct::field##_size);                 \
    } while (0)

TEST_CASE("Perception matches Slang layout") {
    CHECK(sizeof(Perception) == slang_layout::Perception::size);
    CHECK(slang_layout::Perception::field_count == 7);
    LB_CHECK_FIELD(Perception, lit_grad);
    LB_CHECK_FIELD(Perception, lit);
    LB_CHECK_FIELD(Perception, dist_player);
    LB_CHECK_FIELD(Perception, los_flags);
    LB_CHECK_FIELD(Perception, neighbor_count);
    LB_CHECK_FIELD(Perception, cell);
    LB_CHECK_FIELD(Perception, frame);
}

TEST_CASE("Intent matches Slang layout") {
    CHECK(sizeof(Intent) == slang_layout::Intent::size);
    LB_CHECK_FIELD(Intent, desired_dir);
    LB_CHECK_FIELD(Intent, desired_speed);
    LB_CHECK_FIELD(Intent, packed);
}

TEST_CASE("RefineRequest / RefineResult match Slang layout") {
    CHECK(sizeof(RefineRequest) == slang_layout::RefineRequest::size);
    LB_CHECK_FIELD(RefineRequest, agent);
    LB_CHECK_FIELD(RefineRequest, kind);
    LB_CHECK_FIELD(RefineRequest, target);
    CHECK(sizeof(RefineResult) == slang_layout::RefineResult::size);
    LB_CHECK_FIELD(RefineResult, agent);
    LB_CHECK_FIELD(RefineResult, lit_ss);
    LB_CHECK_FIELD(RefineResult, lit_grad_ss);
}

TEST_CASE("GpuEvent / GpuEvents match Slang layout") {
    CHECK(sizeof(GpuEvent) == slang_layout::GpuEvent::size);
    LB_CHECK_FIELD(GpuEvent, pos);
    LB_CHECK_FIELD(GpuEvent, kind);
    LB_CHECK_FIELD(GpuEvent, a);
    LB_CHECK_FIELD(GpuEvent, b);
    LB_CHECK_FIELD(GpuEvent, pad);
    CHECK(sizeof(GpuEvents) == slang_layout::GpuEvents::size);
    LB_CHECK_FIELD(GpuEvents, count);
    LB_CHECK_FIELD(GpuEvents, ev);
}

TEST_CASE("FrameConsts matches Slang layout") {
    CHECK(sizeof(FrameConsts) == slang_layout::FrameConsts::size);
    LB_CHECK_FIELD(FrameConsts, player);
    LB_CHECK_FIELD(FrameConsts, lantern);
    LB_CHECK_FIELD(FrameConsts, misc);
    LB_CHECK_FIELD(FrameConsts, ctrl);
}

TEST_CASE("generated field tables are self-consistent (no gaps beyond padding)") {
    // Fields must be listed in increasing offset order and not overlap.
    for (std::size_t i = 1; i < slang_layout::Perception::field_count; ++i) {
        const auto& prev = slang_layout::Perception::fields[i - 1];
        const auto& cur = slang_layout::Perception::fields[i];
        CHECK(prev.offset + prev.size <= cur.offset);
    }
}
