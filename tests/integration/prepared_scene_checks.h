#pragma once

#include <elf3d/elf3d.h>

namespace elf3d::test {
[[nodiscard]] Result<void> verify_prepared_scene_contract(Engine& engine) noexcept;
[[nodiscard]] Result<PreparedSceneDocument> prepare_scene_survivor(Engine& engine) noexcept;
[[nodiscard]] int verify_foreign_scene_preparation(PreparedSceneDocument&& prepared);
} // namespace elf3d::test
