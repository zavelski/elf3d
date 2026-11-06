#include "exporter_file_writer.h"

#include "exporter_file_validation.h"
#include <model/document_storage.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] Result<void> validate_camera(const NodeView& node, const ExportData& document)
{
    if (node.perspective_camera && !valid_perspective_camera(*node.perspective_camera))
        return Error{ErrorCode::invalid_camera_configuration,
                     "Invalid exported perspective camera"};
    const auto& properties = node.camera_properties;
    if (properties.aspect_ratio &&
        (!std::isfinite(*properties.aspect_ratio) || *properties.aspect_ratio <= 0))
        return Error{ErrorCode::invalid_camera_configuration,
                     "Invalid exported camera aspect ratio"};
    if (!properties.image_based_lighting)
        return {};
    const auto& lighting = *properties.image_based_lighting;
    if (!export_table_index(document.images, lighting.image) || !finite(lighting.rotation) ||
        !finite(lighting.scale))
        return Error{ErrorCode::invalid_camera_configuration,
                     "Exported camera lighting requires a current image and finite transforms"};
    return {};
}
[[nodiscard]] Result<void> validate_node(const NodeView& node, const ExportData& document)
{
    if (!export_table_index(document.nodes, node.id) || !finite(node.local_matrix))
        return Error{ErrorCode::invalid_node_id, "Invalid exported node identity or transform"};
    if (node.mesh && !export_table_index(document.meshes, *node.mesh))
        return Error{ErrorCode::invalid_mesh_id, "Exported node references an invalid mesh"};
    if (node.parent && !export_table_index(document.nodes, *node.parent))
        return Error{ErrorCode::invalid_node_id, "Exported node references an invalid parent"};
    for (const auto id : node.children) {
        const auto child = export_table_index(document.nodes, id);
        if (!child || document.nodes[*child].parent != node.id)
            return Error{ErrorCode::invalid_node_id,
                         "Exported node child relationship is inconsistent"};
    }
    return validate_camera(node, document);
}
[[nodiscard]] Result<void> validate_scenes(const ExportData& document)
{
    if (document.default_scene && !export_table_index(document.scenes, *document.default_scene))
        return Error{ErrorCode::invalid_document_scene_id, "Invalid exported default scene"};
    for (const auto& scene : document.scenes) {
        if (!export_table_index(document.scenes, scene.id))
            return Error{ErrorCode::invalid_document_scene_id, "Invalid exported scene identity"};
        for (const auto root : scene.roots) {
            const auto node = export_table_index(document.nodes, root);
            if (!node || document.nodes[*node].parent)
                return Error{ErrorCode::invalid_node_id,
                             "Exported scene root is invalid or has a parent"};
        }
    }
    return {};
}
[[nodiscard]] Result<void> validate_hierarchy(const ExportData& document)
{
    std::vector<std::uint8_t> states(document.nodes.size(), 0);
    std::vector<std::size_t> path;
    for (std::size_t start = 0; start < document.nodes.size(); ++start) {
        if (states[start] != 0)
            continue;
        path.clear();
        auto current = start;
        while (states[current] == 0) {
            states[current] = 1;
            path.push_back(current);
            const auto parent = document.nodes[current].parent;
            if (!parent)
                break;
            const auto index = export_table_index(document.nodes, *parent);
            if (!index)
                return Error{ErrorCode::invalid_node_id, "Exported node parent is invalid"};
            current = *index;
            if (states[current] == 1)
                return Error{ErrorCode::hierarchy_cycle,
                             "Exported node hierarchy contains a cycle"};
        }
        for (const auto index : path)
            states[index] = 2;
    }
    return {};
}
[[nodiscard]] Result<void> validate_meshes(const ExportData& document)
{
    for (const auto& mesh : document.meshes) {
        if (!export_table_index(document.meshes, mesh.id))
            return Error{ErrorCode::invalid_mesh_id, "Invalid exported mesh identity"};
        for (const auto id : mesh.primitives) {
            const auto primitive = export_table_index(document.primitives, id);
            if (!primitive || document.primitives[*primitive].mesh != mesh.id)
                return Error{ErrorCode::invalid_primitive_id,
                             "Exported primitive owner does not match its containing mesh"};
        }
    }
    for (const auto& primitive : document.primitives) {
        if (!export_table_index(document.primitives, primitive.id) ||
            !export_table_index(document.meshes, primitive.mesh) ||
            !export_table_index(document.materials, primitive.material))
            return Error{ErrorCode::invalid_primitive_id,
                         "Exported primitive has an invalid identity, mesh or material"};
    }
    return {};
}
} // namespace
Result<void> validate_owned_export(const ExportData& document)
{
    if (const auto scenes = validate_scenes(document); !scenes)
        return scenes.error();
    for (const auto& node : document.nodes) {
        if (const auto valid = validate_node(node, document); !valid)
            return valid.error();
    }
    if (const auto hierarchy = validate_hierarchy(document); !hierarchy)
        return hierarchy.error();
    if (const auto meshes = validate_meshes(document); !meshes)
        return meshes.error();
    if (const auto assets = validate_export_assets(document); !assets)
        return assets.error();
    return {};
}
} // namespace elf3d::gltf::exporter_detail
