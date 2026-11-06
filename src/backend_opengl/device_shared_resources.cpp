#include "device_shared_resources.h"

namespace elf3d::backend::opengl::device_detail {
bool DisplayResolveResources::valid() const noexcept
{
    return program != 0 && vertex_array != 0 && texture_uniform >= 0 && exposure_uniform >= 0 &&
           tone_mapping_uniform >= 0;
}
bool SharedShaderResources::ready(ShaderPipelineKind kind) const noexcept
{
    switch (kind) {
    case ShaderPipelineKind::picking:
        return picking.valid();
    case ShaderPipelineKind::overlay:
        return overlay.program != 0 && overlay.vertex_array != 0 && overlay.vertex_buffer != 0 &&
               overlay.color_uniform >= 0;
    case ShaderPipelineKind::display_resolve:
        return display_resolve.valid();
    default:
        return false;
    }
}
void release_shared_shader_resources(SharedShaderResources& resources) noexcept
{
    release_picking_resources(resources.picking);
    release_overlay_resources(resources.overlay);
    if (resources.display_resolve.program != 0)
        glDeleteProgram(resources.display_resolve.program);
    if (resources.display_resolve.vertex_array != 0)
        glDeleteVertexArrays(1, &resources.display_resolve.vertex_array);
    resources = {};
}
} // namespace elf3d::backend::opengl::device_detail
