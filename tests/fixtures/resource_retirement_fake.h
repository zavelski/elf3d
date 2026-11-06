#pragma once

#include <graphics/resource_retirement.h>

namespace elf3d::graphics::tests {
[[nodiscard]] inline Result<ResourceRetirementStep>
retire_fake_objects(std::uint32_t& remaining, const ResourceRetirementBudget& budget) noexcept
{
    if (remaining == 0)
        return ResourceRetirementStep{0, 0, true, false};
    if (budget.objects == 0 || budget.driver_calls == 0)
        return ResourceRetirementStep{};
    --remaining;
    return ResourceRetirementStep{1, 1, remaining == 0, true};
}
} // namespace elf3d::graphics::tests
