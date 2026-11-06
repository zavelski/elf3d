#pragma once

#include <elf3d/core/result.h>

namespace elf3d {
class Engine;
namespace test {
[[nodiscard]] Result<void> verify_runtime_hierarchy_contract(Engine& engine) noexcept;
}
} // namespace elf3d
