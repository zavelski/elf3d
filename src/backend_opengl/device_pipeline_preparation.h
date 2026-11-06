#pragma once

#include "device_internal.hpp"
#include <graphics/pipeline_preparation.h>

namespace elf3d::backend::opengl::device_detail {
enum class ShaderUniformInterface : std::uint8_t { pbr, unbound };
[[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
begin_shader_program(std::shared_ptr<OpenGLDeviceState> state,
                     const graphics::GraphicsPipelineDescription& description,
                     ShaderUniformInterface uniforms) noexcept;
[[nodiscard]] Result<GLuint>
release_graphics_program(graphics::GraphicsPipeline& pipeline) noexcept;
[[nodiscard]] Result<graphics::ResourceRetirementStep>
retire_graphics_pipeline(graphics::GraphicsPipeline& pipeline,
                         const graphics::ResourceRetirementBudget& budget) noexcept;
[[nodiscard]] Result<PipelineView> pipeline_view(graphics::GraphicsPipeline& pipeline) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
begin_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                        const graphics::GraphicsPipelineDescription& description) noexcept;
[[nodiscard]] std::unique_ptr<graphics::GraphicsPipeline>
adopt_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state, GLuint program,
                        UniformLocations uniforms) noexcept;
[[nodiscard]] std::size_t pipeline_uniform_count() noexcept;
void query_pipeline_uniform(GLuint program, UniformLocations& uniforms, std::size_t index) noexcept;
void set_pipeline_sampler(GLuint program, const UniformLocations& uniforms,
                          std::size_t index) noexcept;
} // namespace elf3d::backend::opengl::device_detail
