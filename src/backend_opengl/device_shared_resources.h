#pragma once
#include <elf3d/graphics_preparation.h>
#include <glad/gl.h>
namespace elf3d::backend::opengl::device_detail {
struct OverlayVertex {
    float x = 0;
    float y = 0;
    float z = 0;
};
struct OverlayResources {
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    GLint color_uniform = -1;
};

void release_overlay_resources(OverlayResources& resources) noexcept;

struct PickingResources {
    GLuint program = 0;
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint object_id = -1;
    GLint primitive_index = -1;
    GLint clipping_section_plane_enabled = -1;
    GLint clipping_section_plane_normal = -1;
    GLint clipping_section_plane_offset = -1;
    GLint clipping_retain_positive_half_space = -1;
    GLint clipping_box_count = -1;
    GLint clipping_box_minimums = -1;
    GLint clipping_box_maximums = -1;
    GLint normal_matrix = -1;
    GLint camera_world_position = -1;
    GLint vertex_layout = -1;

    [[nodiscard]] bool valid() const noexcept;
};

struct DisplayResolveResources {
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLint texture_uniform = -1;
    GLint exposure_uniform = -1;
    GLint tone_mapping_uniform = -1;
    [[nodiscard]] bool valid() const noexcept;
};
struct SharedShaderResources {
    PickingResources picking;
    OverlayResources overlay;
    DisplayResolveResources display_resolve;
    [[nodiscard]] bool ready(ShaderPipelineKind kind) const noexcept;
};
// Called only by the owning context teardown; these values hold no state owner.
void release_picking_resources(PickingResources& resources) noexcept;
void release_shared_shader_resources(SharedShaderResources& resources) noexcept;
} // namespace elf3d::backend::opengl::device_detail
