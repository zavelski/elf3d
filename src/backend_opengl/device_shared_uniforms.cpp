#include "device_shared_preparation.h"
#include <array>
#include <elf3d/core/assert.h>
namespace elf3d::backend::opengl::device_detail {
namespace {
struct PickingBinding {
    const char* name;
    GLint PickingResources::* member;
};
constexpr std::array<PickingBinding, 15> picking_bindings{{
    {"u_model", &PickingResources::model},
    {"u_view", &PickingResources::view},
    {"u_projection", &PickingResources::projection},
    {"u_pick_object_id", &PickingResources::object_id},
    {"u_pick_primitive_index", &PickingResources::primitive_index},
    {"u_clipping_section_plane_enabled", &PickingResources::clipping_section_plane_enabled},
    {"u_clipping_section_plane_normal", &PickingResources::clipping_section_plane_normal},
    {"u_clipping_section_plane_offset", &PickingResources::clipping_section_plane_offset},
    {"u_clipping_retain_positive_half_space",
     &PickingResources::clipping_retain_positive_half_space},
    {"u_clipping_box_count", &PickingResources::clipping_box_count},
    {"u_clipping_box_minimums[0]", &PickingResources::clipping_box_minimums},
    {"u_clipping_box_maximums[0]", &PickingResources::clipping_box_maximums},
    {"u_normal_matrix", &PickingResources::normal_matrix},
    {"u_camera_world_position", &PickingResources::camera_world_position},
    {"u_vertex_layout", &PickingResources::vertex_layout},
}};
struct ResolveBinding {
    const char* name;
    GLint DisplayResolveResources::* member;
};
constexpr std::array<ResolveBinding, 3> resolve_bindings{
    {{"u_linear_color_texture", &DisplayResolveResources::texture_uniform},
     {"u_exposure_ev", &DisplayResolveResources::exposure_uniform},
     {"u_tone_mapping", &DisplayResolveResources::tone_mapping_uniform}}};
} // namespace
std::size_t auxiliary_uniform_count(ShaderPipelineKind kind) noexcept
{
    if (kind == ShaderPipelineKind::picking)
        return picking_bindings.size();
    return kind == ShaderPipelineKind::overlay ? 1 : resolve_bindings.size();
}
void query_auxiliary_uniform(SharedShaderResources& candidate, ShaderPipelineKind kind,
                             std::size_t index, GLuint program) noexcept
{
    ELF3D_ASSERT(index < auxiliary_uniform_count(kind));
    if (kind == ShaderPipelineKind::picking) {
        const auto& binding = picking_bindings[index];
        candidate.picking.*(binding.member) = glGetUniformLocation(program, binding.name);
    } else if (kind == ShaderPipelineKind::overlay) {
        candidate.overlay.color_uniform = glGetUniformLocation(program, "u_color");
    } else {
        const auto& binding = resolve_bindings[index];
        candidate.display_resolve.*(binding.member) = glGetUniformLocation(program, binding.name);
    }
}
} // namespace elf3d::backend::opengl::device_detail
