#include <backend_opengl/backend_opengl.h>

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_pipeline_preparation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <graphics/graphics.h>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace elf3d::backend::opengl::device_detail {
namespace {

class OpenGLGraphicsPipeline final : public graphics::GraphicsPipeline {
  public:
    OpenGLGraphicsPipeline(std::shared_ptr<OpenGLDeviceState> state, GLuint program,
                           UniformLocations uniforms) noexcept
        : state_(std::move(state)), program_(program), uniforms_(uniforms)
    {
    }

    ~OpenGLGraphicsPipeline() override
    {
        if (program_ != 0 && state_->can_destroy_objects()) {
            glDeleteProgram(program_);
        }
    }

    [[nodiscard]] GLuint program() const noexcept
    {
        return program_;
    }

    [[nodiscard]] Result<GLuint> release_program() noexcept
    {
        if (const auto valid = state_->validate_thread(); !valid)
            return valid.error();
        return std::exchange(program_, 0);
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    retire(const graphics::ResourceRetirementBudget& budget) noexcept
    {
        if (const auto valid = state_->validate_thread(); !valid)
            return valid.error();
        if (program_ == 0)
            return graphics::ResourceRetirementStep{0, 0, true, false};
        if (budget.objects == 0 || budget.driver_calls < 3)
            return graphics::ResourceRetirementStep{};
        if (const auto valid = state_->validate_operation(); !valid)
            return valid.error();
        glDeleteProgram(std::exchange(program_, 0));
        if (glGetError() != GL_NO_ERROR)
            return Error{ErrorCode::shader_linking_failed, "OpenGL program retirement failed"};
        return graphics::ResourceRetirementStep{1, 3, true, true};
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
    {
        return opengl_resource_token();
    }

    [[nodiscard]] const UniformLocations& uniforms() const noexcept
    {
        return uniforms_;
    }

    [[nodiscard]] std::optional<float>& environment_intensity() noexcept
    {
        return environment_intensity_;
    }

    [[nodiscard]] std::optional<float>& environment_rotation() noexcept
    {
        return environment_rotation_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint program_ = 0;
    UniformLocations uniforms_;
    std::optional<float> environment_intensity_;
    std::optional<float> environment_rotation_;
};

[[nodiscard]] bool transform_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.model >= 0 && locations.view >= 0 && locations.projection >= 0 &&
           locations.normal >= 0 && locations.vertex_layout >= 0 &&
           locations.orientation_sign >= 0 && locations.base_color >= 0;
}

[[nodiscard]] bool direct_lighting_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.camera_world_position >= 0 && locations.light_direction >= 0 &&
           locations.light_color >= 0 && locations.ambient_intensity >= 0 &&
           locations.diffuse_intensity >= 0;
}

[[nodiscard]] bool indirect_lighting_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.environment_intensity >= 0 && locations.environment_rotation >= 0 &&
           locations.metallic_factor >= 0 && locations.roughness_factor >= 0 &&
           locations.emissive_factor >= 0 && locations.occlusion_strength >= 0 &&
           locations.normal_scale >= 0;
}

[[nodiscard]] bool lighting_locations_valid(const UniformLocations& locations) noexcept
{
    return direct_lighting_locations_valid(locations) &&
           indirect_lighting_locations_valid(locations);
}

[[nodiscard]] bool optical_locations_valid(const UniformLocations& value) noexcept
{
    const std::array<GLint, 18> locations{value.transmission_factor,
                                          value.thickness_factor,
                                          value.attenuation_distance,
                                          value.attenuation_color,
                                          value.clearcoat_factor,
                                          value.clearcoat_roughness_factor,
                                          value.clearcoat_normal_scale,
                                          value.clearcoat_uses_tangent_basis,
                                          value.has_extension_textures,
                                          value.specular_texture,
                                          value.specular_color_texture,
                                          value.transmission_texture,
                                          value.thickness_texture,
                                          value.clearcoat_texture,
                                          value.clearcoat_roughness_texture,
                                          value.clearcoat_normal_texture,
                                          value.transmission_scene,
                                          value.has_transmission_scene};
    const bool complete = std::all_of(locations.begin(), locations.end(),
                                      [](GLint location) { return location >= 0; });
    const bool core = std::all_of(locations.begin(), locations.end(),
                                  [](GLint location) { return location < 0; });
    return complete || core;
}

[[nodiscard]] bool surface_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.ior >= 0 && locations.specular_factor >= 0 &&
           locations.specular_color_factor >= 0 && locations.highlight_color >= 0 &&
           locations.highlight_strength >= 0;
}

[[nodiscard]] bool material_texture_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.has_base_color_texture >= 0 && locations.has_metallic_roughness_texture >= 0 &&
           locations.has_normal_texture >= 0 && locations.has_occlusion_texture >= 0 &&
           locations.has_emissive_texture >= 0 && locations.base_color_texture >= 0 &&
           locations.metallic_roughness_texture >= 0 && locations.normal_texture >= 0 &&
           locations.occlusion_texture >= 0 && locations.emissive_texture >= 0;
}

[[nodiscard]] bool texture_unit_locations_valid(const UniformLocations& locations) noexcept
{
    return material_texture_locations_valid(locations) && locations.diffuse_environment >= 0 &&
           locations.specular_environment >= 0 && locations.environment_brdf_lut >= 0;
}

[[nodiscard]] bool texture_parameter_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.texture_texcoord_sets >= 0 && locations.texture_offsets >= 0 &&
           locations.texture_scales >= 0 && locations.texture_rotations >= 0 &&
           locations.alpha_mode >= 0 && locations.alpha_cutoff >= 0 && locations.unlit >= 0;
}

[[nodiscard]] bool clipping_locations_valid(const UniformLocations& locations) noexcept
{
    return locations.clipping_section_plane_enabled >= 0 &&
           locations.clipping_section_plane_normal >= 0 &&
           locations.clipping_section_plane_offset >= 0 &&
           locations.clipping_retain_positive_half_space >= 0 &&
           locations.clipping_box_count >= 0 && locations.clipping_box_minimums >= 0 &&
           locations.clipping_box_maximums >= 0;
}

} // namespace

bool UniformLocations::valid() const noexcept
{
    return transform_locations_valid(*this) && lighting_locations_valid(*this) &&
           surface_locations_valid(*this) && texture_unit_locations_valid(*this) &&
           texture_parameter_locations_valid(*this) && clipping_locations_valid(*this) &&
           optical_locations_valid(*this);
}

Result<std::unique_ptr<graphics::GraphicsPipeline>>
create_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                         const graphics::GraphicsPipelineDescription& description) noexcept
{
    auto preparation = begin_graphics_pipeline(std::move(state), description);
    if (!preparation)
        return preparation.error();
    return graphics::complete_pipeline_preparation(*preparation.value());
}

std::unique_ptr<graphics::GraphicsPipeline>
adopt_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state, GLuint program,
                        UniformLocations uniforms) noexcept
{
    try {
        return std::make_unique<OpenGLGraphicsPipeline>(std::move(state), program, uniforms);
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<GLuint> release_graphics_program(graphics::GraphicsPipeline& pipeline) noexcept
{
    if (pipeline.backend_resource_token() != opengl_resource_token())
        return Error{ErrorCode::backend_mismatch, "Shader program does not belong to OpenGL"};
    return static_cast<OpenGLGraphicsPipeline&>(pipeline).release_program();
}
Result<graphics::ResourceRetirementStep>
retire_graphics_pipeline(graphics::GraphicsPipeline& pipeline,
                         const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (pipeline.backend_resource_token() != opengl_resource_token())
        return Error{ErrorCode::backend_mismatch, "Shader program does not belong to OpenGL"};
    return static_cast<OpenGLGraphicsPipeline&>(pipeline).retire(budget);
}
Result<PipelineView> pipeline_view(graphics::GraphicsPipeline& pipeline) noexcept
{
    if (pipeline.backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch,
                     "The graphics pipeline does not belong to OpenGL"};
    }
    auto& opengl_pipeline = static_cast<OpenGLGraphicsPipeline&>(pipeline);
    return PipelineView{opengl_pipeline.program(), opengl_pipeline.uniforms(),
                        &opengl_pipeline.environment_intensity(),
                        &opengl_pipeline.environment_rotation()};
}

} // namespace elf3d::backend::opengl::device_detail
