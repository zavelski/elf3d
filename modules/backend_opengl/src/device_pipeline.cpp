#include <elf3d/internal/backend_opengl.h>

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <elf3d/internal/graphics.h>
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
        if (state_->can_destroy_objects() && program_ != 0) {
            glDeleteProgram(program_);
        }
    }

    [[nodiscard]] GLuint program() const noexcept
    {
        return program_;
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

[[nodiscard]] Result<GLuint>
create_graphics_program(const graphics::GraphicsPipelineDescription& description)
{
    return create_program_from_sources(description.vertex_shader_source,
                                       description.fragment_shader_source);
}

[[nodiscard]] UniformLocations query_uniform_locations(GLuint program) noexcept
{
    return UniformLocations{glGetUniformLocation(program, "u_model"),
                            glGetUniformLocation(program, "u_view"),
                            glGetUniformLocation(program, "u_projection"),
                            glGetUniformLocation(program, "u_normal_matrix"),
                            glGetUniformLocation(program, "u_vertex_layout"),
                            glGetUniformLocation(program, "u_orientation_sign"),
                            glGetUniformLocation(program, "u_base_color"),
                            glGetUniformLocation(program, "u_camera_world_position"),
                            glGetUniformLocation(program, "u_light_direction"),
                            glGetUniformLocation(program, "u_light_color"),
                            glGetUniformLocation(program, "u_ambient_intensity"),
                            glGetUniformLocation(program, "u_diffuse_intensity"),
                            glGetUniformLocation(program, "u_environment_intensity"),
                            glGetUniformLocation(program, "u_environment_rotation"),
                            glGetUniformLocation(program, "u_metallic_factor"),
                            glGetUniformLocation(program, "u_roughness_factor"),
                            glGetUniformLocation(program, "u_emissive_factor"),
                            glGetUniformLocation(program, "u_occlusion_strength"),
                            glGetUniformLocation(program, "u_normal_scale"),
                            glGetUniformLocation(program, "u_ior"),
                            glGetUniformLocation(program, "u_specular_factor"),
                            glGetUniformLocation(program, "u_specular_color_factor"),
                            glGetUniformLocation(program, "u_highlight_color"),
                            glGetUniformLocation(program, "u_highlight_strength"),
                            glGetUniformLocation(program, "u_has_base_color_texture"),
                            glGetUniformLocation(program, "u_has_metallic_roughness_texture"),
                            glGetUniformLocation(program, "u_has_normal_texture"),
                            glGetUniformLocation(program, "u_has_occlusion_texture"),
                            glGetUniformLocation(program, "u_has_emissive_texture"),
                            glGetUniformLocation(program, "u_base_color_texture"),
                            glGetUniformLocation(program, "u_metallic_roughness_texture"),
                            glGetUniformLocation(program, "u_normal_texture"),
                            glGetUniformLocation(program, "u_occlusion_texture"),
                            glGetUniformLocation(program, "u_emissive_texture"),
                            glGetUniformLocation(program, "u_diffuse_environment"),
                            glGetUniformLocation(program, "u_specular_environment"),
                            glGetUniformLocation(program, "u_environment_brdf_lut"),
                            glGetUniformLocation(program, "u_texture_texcoord_sets[0]"),
                            glGetUniformLocation(program, "u_texture_offsets[0]"),
                            glGetUniformLocation(program, "u_texture_scales[0]"),
                            glGetUniformLocation(program, "u_texture_rotations[0]"),
                            glGetUniformLocation(program, "u_alpha_mode"),
                            glGetUniformLocation(program, "u_alpha_cutoff"),
                            glGetUniformLocation(program, "u_unlit"),
                            glGetUniformLocation(program, "u_clipping_section_plane_enabled"),
                            glGetUniformLocation(program, "u_clipping_section_plane_normal"),
                            glGetUniformLocation(program, "u_clipping_section_plane_offset"),
                            glGetUniformLocation(program, "u_clipping_retain_positive_half_space"),
                            glGetUniformLocation(program, "u_clipping_box_count"),
                            glGetUniformLocation(program, "u_clipping_box_minimums[0]"),
                            glGetUniformLocation(program, "u_clipping_box_maximums[0]"),
                            glGetUniformLocation(program, "u_transmission_factor"),
                            glGetUniformLocation(program, "u_thickness_factor"),
                            glGetUniformLocation(program, "u_attenuation_distance"),
                            glGetUniformLocation(program, "u_attenuation_color"),
                            glGetUniformLocation(program, "u_clearcoat_factor"),
                            glGetUniformLocation(program, "u_clearcoat_roughness_factor"),
                            glGetUniformLocation(program, "u_clearcoat_normal_scale"),
                            glGetUniformLocation(program, "u_clearcoat_uses_tangent_basis"),
                            glGetUniformLocation(program, "u_has_extension_textures[0]"),
                            glGetUniformLocation(program, "u_specular_texture"),
                            glGetUniformLocation(program, "u_specular_color_texture"),
                            glGetUniformLocation(program, "u_transmission_texture"),
                            glGetUniformLocation(program, "u_thickness_texture"),
                            glGetUniformLocation(program, "u_clearcoat_texture"),
                            glGetUniformLocation(program, "u_clearcoat_roughness_texture"),
                            glGetUniformLocation(program, "u_clearcoat_normal_texture"),
                            glGetUniformLocation(program, "u_transmission_scene"),
                            glGetUniformLocation(program, "u_has_transmission_scene")};
}

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

void configure_texture_sampler_uniforms(GLuint program, const UniformLocations& uniforms) noexcept
{
    const std::array<GLint, 16> locations{
        uniforms.base_color_texture,       uniforms.metallic_roughness_texture,
        uniforms.normal_texture,           uniforms.occlusion_texture,
        uniforms.emissive_texture,         uniforms.diffuse_environment,
        uniforms.specular_environment,     uniforms.environment_brdf_lut,
        uniforms.specular_texture,         uniforms.specular_color_texture,
        uniforms.transmission_texture,     uniforms.thickness_texture,
        uniforms.clearcoat_texture,        uniforms.clearcoat_roughness_texture,
        uniforms.clearcoat_normal_texture, uniforms.transmission_scene,
    };
    for (std::size_t index = 0; index < locations.size(); ++index) {
        glProgramUniform1i(program, locations[index], static_cast<GLint>(index));
    }
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
    if (description.vertex_layout !=
        graphics::VertexLayout::
            position_normal_float3_texcoord2_float2_color_float4_tangent_float4) {
        return Error{ErrorCode::unsupported_vertex_layout,
                     "The PBR pipeline requires position, normal, two UV sets, color, and tangent"};
    }

    try {
        Result<GLuint> program_result = create_graphics_program(description);
        if (!program_result) {
            return program_result.error();
        }

        const GLuint program = program_result.value();
        const UniformLocations uniforms = query_uniform_locations(program);
        if (!uniforms.valid()) {
            glDeleteProgram(program);
            return Error{ErrorCode::shader_linking_failed,
                         "The linked shader program is missing a required renderer uniform"};
        }

        configure_texture_sampler_uniforms(program, uniforms);
        return std::unique_ptr<graphics::GraphicsPipeline>{
            std::make_unique<OpenGLGraphicsPipeline>(std::move(state), program, uniforms)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
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
