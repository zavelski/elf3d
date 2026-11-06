#pragma once

#include "device_pipeline_preparation.h"

namespace elf3d::backend::opengl::device_detail {
[[nodiscard]] graphics::GraphicsPipelineDescription picking_program_sources() noexcept;
[[nodiscard]] graphics::GraphicsPipelineDescription overlay_program_sources() noexcept;
[[nodiscard]] graphics::GraphicsPipelineDescription display_program_sources() noexcept;
[[nodiscard]] std::size_t auxiliary_uniform_count(ShaderPipelineKind kind) noexcept;
void query_auxiliary_uniform(SharedShaderResources& candidate, ShaderPipelineKind kind,
                             std::size_t index, GLuint program) noexcept;

// Device-owned incomplete programs may retain the native teardown state. The
// state holds only ready scalar IDs and never owns this preparation (no cycle).
class SharedGraphicsPreparation final {
  public:
    SharedGraphicsPreparation(std::shared_ptr<OpenGLDeviceState> state,
                              ShaderPipelineKind kind) noexcept;
    ~SharedGraphicsPreparation() noexcept;
    [[nodiscard]] Result<graphics::PipelinePreparationRequest> request() const noexcept;
    [[nodiscard]] Result<graphics::PipelinePreparationStep> advance() noexcept;
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept;
    [[nodiscard]] ShaderPipelineKind kind() const noexcept
    {
        return kind_;
    }
    [[nodiscard]] bool failed() const noexcept
    {
        return failed_;
    }
    [[nodiscard]] bool complete() const noexcept;

  private:
    [[nodiscard]] Result<graphics::PipelinePreparationStep> advance_program() noexcept;
    [[nodiscard]] Result<graphics::PipelinePreparationStep> advance_objects() noexcept;
    [[nodiscard]] Result<void> execute_object_unit() noexcept;
    void advance_uniform_bindings() noexcept;
    [[nodiscard]] Result<void> allocate_vertex_array() noexcept;
    [[nodiscard]] Result<void> validate_and_commit() noexcept;
    [[nodiscard]] Result<void> commit() noexcept;
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    retire_candidate(const graphics::ResourceRetirementBudget& budget) noexcept;
    [[nodiscard]] bool candidate_objects() const noexcept;
    [[nodiscard]] bool retired() const noexcept;
    void delete_candidate_object() noexcept;
    [[nodiscard]] GLuint program_name() const noexcept;
    void configure_overlay_attributes() noexcept;
    std::shared_ptr<OpenGLDeviceState> state_;
    ShaderPipelineKind kind_;
    std::unique_ptr<graphics::GraphicsPipelinePreparation> program_;
    std::unique_ptr<graphics::GraphicsPipeline> linked_;
    SharedShaderResources candidate_;
    ShaderPreparationPhase phase_ = ShaderPreparationPhase::factory;
    std::size_t cursor_ = 0;
    bool failed_ = false;
};
[[nodiscard]] Result<void> ensure_auxiliary_graphics(std::shared_ptr<OpenGLDeviceState> state,
                                                     ShaderPipelineKind kind) noexcept;
[[nodiscard]] Result<std::unique_ptr<SharedGraphicsPreparation>>
begin_shared_graphics(std::shared_ptr<OpenGLDeviceState> state, ShaderPipelineKind kind) noexcept;
} // namespace elf3d::backend::opengl::device_detail
