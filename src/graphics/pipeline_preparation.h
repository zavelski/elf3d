#pragma once

#include <graphics/graphics.h>

namespace elf3d::graphics {
using PipelinePreparationPhase = ShaderPreparationPhase;
struct PipelinePreparationRequest {
    PipelinePreparationPhase phase = PipelinePreparationPhase::vertex_object;
    // Includes context validation and, for status queries, bounded diagnostics.
    std::uint32_t driver_call_limit = 0;
};
struct PipelinePreparationStep {
    std::uint32_t driver_calls = 0;
    bool ready = false;
};
struct AuxiliaryGraphicsStatus {
    bool picking_ready = false;
    bool overlay_ready = false;
    bool display_resolve_ready = false;
    bool retirement_pending = false;
    std::optional<ShaderPipelineKind> preparing;
    [[nodiscard]] bool complete() const noexcept
    {
        return picking_ready && overlay_ready && display_resolve_ready && !retirement_pending &&
               !preparing;
    }
};
// Context-thread exclusive shader/program owner. Small shader strings are owned;
// no Scene, Document, graphics observer or asynchronous source borrow is retained.
class GraphicsPipelinePreparation {
  public:
    virtual ~GraphicsPipelinePreparation() noexcept = default;
    [[nodiscard]] virtual Result<PipelinePreparationRequest> request() const noexcept = 0;
    [[nodiscard]] virtual Result<PipelinePreparationStep> advance() noexcept = 0;
    [[nodiscard]] virtual std::uint64_t driver_calls() const noexcept = 0;
    [[nodiscard]] virtual Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<GraphicsPipeline>> take_pipeline() noexcept = 0;
};
[[nodiscard]] Result<std::unique_ptr<GraphicsPipeline>>
complete_pipeline_preparation(GraphicsPipelinePreparation& preparation) noexcept;
} // namespace elf3d::graphics
