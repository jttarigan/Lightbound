#pragma once
#include "gfx/Gfx.h"

namespace lb::gfx {
std::unique_ptr<Gfx> createVulkanGfx();
}
