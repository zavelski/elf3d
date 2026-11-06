#pragma once

#include <cstdint>
#include <elf3d/core/result.h>

namespace elf3d::graphics {
struct ResourceRetirementBudget {
    std::uint32_t objects = 1;
    std::uint32_t driver_calls = 3;
};
struct ResourceRetirementStep {
    std::uint32_t deleted_objects = 0;
    std::uint32_t driver_calls = 0;
    bool complete = false;
    bool progressed = false;
};
} // namespace elf3d::graphics
