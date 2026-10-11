#include <elf3d/core/error.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

namespace elf3d::gltf::importer_detail {

Result<void> attach_camera_properties(ImportState& state, const cgltf_camera& camera, NodeId node)
{
    ModelCameraPropertiesDescription properties;
    properties.name = camera.name != nullptr ? camera.name : "";
    properties.infinite_far_plane = camera.data.perspective.has_zfar == 0;
    if (camera.data.perspective.has_aspect_ratio) {
        properties.aspect_ratio = camera.data.perspective.aspect_ratio;
    }
    return state.builder.set_node_camera_properties(node, properties);
}

} // namespace elf3d::gltf::importer_detail
