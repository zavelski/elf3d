#pragma once

#include "renderer_scene_preparation.h"
#include <array>
#include <graphics/pipeline_preparation.h>

namespace elf3d::renderer {
struct PipelineResources {
    std::array<std::unique_ptr<graphics::GraphicsPipeline>, 2> ready;
    std::unique_ptr<graphics::GraphicsPipelinePreparation> pending;
    ShaderPipelineKind active = ShaderPipelineKind::core;
    bool failed = false;
    GraphicsPreparationFailure attempt;
    [[nodiscard]] Result<SceneResourceWork> advance(graphics::Device& device,
                                                    ShaderPipelineKind kind,
                                                    const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<SceneResourceWork>
    advance_auxiliary(graphics::Device& device, const GraphicsPreparationBudget& budget);
    [[nodiscard]] bool complete() const noexcept;
    [[nodiscard]] Result<SceneResourceWork>
    advance_pending(const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<void> accept_pending();
    [[nodiscard]] bool needs_retirement() const noexcept;
    [[nodiscard]] Result<GraphicsRetirementStep>
    retire_unit(const graphics::ResourceRetirementBudget& budget);
};
} // namespace elf3d::renderer
