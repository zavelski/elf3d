#include "device_pipeline_preparation.h"

#include <array>
#include <elf3d/core/assert.h>

namespace elf3d::backend::opengl::device_detail {
namespace {
struct UniformBinding {
    const char* name; // Immutable string literals consumed by the native C API.
    GLint UniformLocations::* location;
};
constexpr std::array<UniformBinding, 69> bindings{{
    {"u_model", &UniformLocations::model},
    {"u_view", &UniformLocations::view},
    {"u_projection", &UniformLocations::projection},
    {"u_normal_matrix", &UniformLocations::normal},
    {"u_vertex_layout", &UniformLocations::vertex_layout},
    {"u_orientation_sign", &UniformLocations::orientation_sign},
    {"u_base_color", &UniformLocations::base_color},
    {"u_camera_world_position", &UniformLocations::camera_world_position},
    {"u_light_direction", &UniformLocations::light_direction},
    {"u_light_color", &UniformLocations::light_color},
    {"u_ambient_intensity", &UniformLocations::ambient_intensity},
    {"u_diffuse_intensity", &UniformLocations::diffuse_intensity},
    {"u_environment_intensity", &UniformLocations::environment_intensity},
    {"u_environment_rotation", &UniformLocations::environment_rotation},
    {"u_metallic_factor", &UniformLocations::metallic_factor},
    {"u_roughness_factor", &UniformLocations::roughness_factor},
    {"u_emissive_factor", &UniformLocations::emissive_factor},
    {"u_occlusion_strength", &UniformLocations::occlusion_strength},
    {"u_normal_scale", &UniformLocations::normal_scale},
    {"u_ior", &UniformLocations::ior},
    {"u_specular_factor", &UniformLocations::specular_factor},
    {"u_specular_color_factor", &UniformLocations::specular_color_factor},
    {"u_highlight_color", &UniformLocations::highlight_color},
    {"u_highlight_strength", &UniformLocations::highlight_strength},
    {"u_has_base_color_texture", &UniformLocations::has_base_color_texture},
    {"u_has_metallic_roughness_texture", &UniformLocations::has_metallic_roughness_texture},
    {"u_has_normal_texture", &UniformLocations::has_normal_texture},
    {"u_has_occlusion_texture", &UniformLocations::has_occlusion_texture},
    {"u_has_emissive_texture", &UniformLocations::has_emissive_texture},
    {"u_base_color_texture", &UniformLocations::base_color_texture},
    {"u_metallic_roughness_texture", &UniformLocations::metallic_roughness_texture},
    {"u_normal_texture", &UniformLocations::normal_texture},
    {"u_occlusion_texture", &UniformLocations::occlusion_texture},
    {"u_emissive_texture", &UniformLocations::emissive_texture},
    {"u_diffuse_environment", &UniformLocations::diffuse_environment},
    {"u_specular_environment", &UniformLocations::specular_environment},
    {"u_environment_brdf_lut", &UniformLocations::environment_brdf_lut},
    {"u_texture_texcoord_sets[0]", &UniformLocations::texture_texcoord_sets},
    {"u_texture_offsets[0]", &UniformLocations::texture_offsets},
    {"u_texture_scales[0]", &UniformLocations::texture_scales},
    {"u_texture_rotations[0]", &UniformLocations::texture_rotations},
    {"u_alpha_mode", &UniformLocations::alpha_mode},
    {"u_alpha_cutoff", &UniformLocations::alpha_cutoff},
    {"u_unlit", &UniformLocations::unlit},
    {"u_clipping_section_plane_enabled", &UniformLocations::clipping_section_plane_enabled},
    {"u_clipping_section_plane_normal", &UniformLocations::clipping_section_plane_normal},
    {"u_clipping_section_plane_offset", &UniformLocations::clipping_section_plane_offset},
    {"u_clipping_retain_positive_half_space",
     &UniformLocations::clipping_retain_positive_half_space},
    {"u_clipping_box_count", &UniformLocations::clipping_box_count},
    {"u_clipping_box_minimums[0]", &UniformLocations::clipping_box_minimums},
    {"u_clipping_box_maximums[0]", &UniformLocations::clipping_box_maximums},
    {"u_transmission_factor", &UniformLocations::transmission_factor},
    {"u_thickness_factor", &UniformLocations::thickness_factor},
    {"u_attenuation_distance", &UniformLocations::attenuation_distance},
    {"u_attenuation_color", &UniformLocations::attenuation_color},
    {"u_clearcoat_factor", &UniformLocations::clearcoat_factor},
    {"u_clearcoat_roughness_factor", &UniformLocations::clearcoat_roughness_factor},
    {"u_clearcoat_normal_scale", &UniformLocations::clearcoat_normal_scale},
    {"u_clearcoat_uses_tangent_basis", &UniformLocations::clearcoat_uses_tangent_basis},
    {"u_has_extension_textures[0]", &UniformLocations::has_extension_textures},
    {"u_specular_texture", &UniformLocations::specular_texture},
    {"u_specular_color_texture", &UniformLocations::specular_color_texture},
    {"u_transmission_texture", &UniformLocations::transmission_texture},
    {"u_thickness_texture", &UniformLocations::thickness_texture},
    {"u_clearcoat_texture", &UniformLocations::clearcoat_texture},
    {"u_clearcoat_roughness_texture", &UniformLocations::clearcoat_roughness_texture},
    {"u_clearcoat_normal_texture", &UniformLocations::clearcoat_normal_texture},
    {"u_transmission_scene", &UniformLocations::transmission_scene},
    {"u_has_transmission_scene", &UniformLocations::has_transmission_scene},
}};
constexpr std::array<GLint UniformLocations::*, 16> samplers{
    &UniformLocations::base_color_texture,       &UniformLocations::metallic_roughness_texture,
    &UniformLocations::normal_texture,           &UniformLocations::occlusion_texture,
    &UniformLocations::emissive_texture,         &UniformLocations::diffuse_environment,
    &UniformLocations::specular_environment,     &UniformLocations::environment_brdf_lut,
    &UniformLocations::specular_texture,         &UniformLocations::specular_color_texture,
    &UniformLocations::transmission_texture,     &UniformLocations::thickness_texture,
    &UniformLocations::clearcoat_texture,        &UniformLocations::clearcoat_roughness_texture,
    &UniformLocations::clearcoat_normal_texture, &UniformLocations::transmission_scene,
};
} // namespace
std::size_t pipeline_uniform_count() noexcept
{
    return bindings.size();
}
void query_pipeline_uniform(GLuint program, UniformLocations& uniforms, std::size_t index) noexcept
{
    ELF3D_ASSERT(index < bindings.size());
    const auto& binding = bindings[index];
    uniforms.*(binding.location) = glGetUniformLocation(program, binding.name);
}
void set_pipeline_sampler(GLuint program, const UniformLocations& uniforms,
                          std::size_t index) noexcept
{
    // The closed sampler table contains sixteen entries, all representable as GLint.
    ELF3D_ASSERT(index < samplers.size());
    glProgramUniform1i(program, uniforms.*samplers[index], static_cast<GLint>(index));
}
} // namespace elf3d::backend::opengl::device_detail
