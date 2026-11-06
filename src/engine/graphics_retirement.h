#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics_preparation.h>
#include <elf3d/graphics_retirement.h>

namespace elf3d::renderer {
class Renderer;
}
namespace elf3d::picking {
class PickingService;
}
namespace elf3d::scene {
class Storage;
}
namespace elf3d::detail {
[[nodiscard]] GraphicsRetirementStatus
graphics_retirement_status(const renderer::Renderer& renderer,
                           const picking::PickingService& picking) noexcept;
[[nodiscard]] Result<GraphicsRetirementReport>
advance_graphics_retirement(renderer::Renderer& renderer, picking::PickingService& picking,
                            const GraphicsRetirementBudget& budget);
[[nodiscard]] Result<SceneGraphicsPreparationReport>
advance_scene_graphics(renderer::Renderer& renderer, picking::PickingService& picking,
                       const scene::Storage& scene, const GraphicsPreparationBudget& budget);
} // namespace elf3d::detail
