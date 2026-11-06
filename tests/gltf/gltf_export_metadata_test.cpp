#include <elf3d/model.h>
#include <gltf/exporter_internal.hpp>
#include <gltf/exporter_source_metadata.h>
#include <model/document_write_source.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace elf3d;
using namespace elf3d::gltf::exporter_detail;
using namespace elf3d::model::detail;

[[nodiscard]] Document make_source()
{
    Document document;
    const auto scene = document.create_scene("Scene name").value();
    const auto node = document.create_node(std::string(131073, 'n') + "\xE2\x82\xAC").value();
    const auto child = document.create_node("Child").value();
    const auto mesh = document.create_mesh("Mesh name").value();
    ModelMaterialDescription description;
    description.name = "Material name";
    const auto material = document.create_material(description).value();
    PrimitiveData data;
    data.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    data.indices = {0, 1, 2};
    const auto primitive = document.create_primitive(mesh, material, std::move(data)).value();
    const auto connected = document.set_node_mesh(node, mesh);
    const auto rooted = document.add_scene_root(scene, node);
    const auto parented = document.set_parent(child, node);
    const auto camera = document.set_node_perspective_camera(node, {0.8F, 0.1F, 100});
    if (!connected || !rooted || !parented || !camera)
        return Document{};
    ModelCameraPropertiesDescription properties;
    properties.name = "Camera name";
    properties.aspect_ratio = 1.5F;
    if (!document.set_node_camera_properties(node, properties))
        return Document{};
    ModelDocumentMetadata metadata;
    metadata.root.extras_json = "{\"data\":\"" + std::string(131077, 'r') + "\"}";
    metadata.asset.extras_json = R"({"scope":"asset"})";
    metadata.root.extensions.push_back({"EXT_metadata_owner", R"({"scope":"root"})"});
    metadata.required_extensions.push_back("EXT_metadata_owner");
    metadata.scenes.push_back({scene, {R"({"scope":"scene"})", {}}});
    metadata.nodes.push_back({node, {R"({"scope":"node"})", {}}});
    metadata.meshes.push_back({mesh, {R"({"scope":"mesh"})", {}}});
    metadata.primitives.push_back({primitive, {R"({"scope":"primitive"})", {}}});
    metadata.materials.push_back({material, {R"({"scope":"material"})", {}}});
    metadata.cameras.push_back({node, {R"({"scope":"camera"})", {}}});
    metadata.camera_projections.push_back({node, {R"({"scope":"projection"})", {}}});
    if (!document.attach_verified_metadata(std::move(metadata)))
        return Document{};
    return document;
}

[[nodiscard]] bool valid_counts(const ExportData& data)
{
    return data.scenes.size() == 1 && data.nodes.size() == 2 && data.meshes.size() == 1 &&
           data.primitives.size() == 1 && data.materials.size() == 1;
}

[[nodiscard]] bool structural_values(const ExportData& data)
{
    if (!valid_counts(data))
        return false;
    const auto& node = data.nodes[0];
    return data.scenes[0].roots.size() == 1 && data.scenes[0].roots[0] == node.id &&
           node.children.size() == 1 && node.children[0] == data.nodes[1].id &&
           node.mesh == data.meshes[0].id &&
           data.meshes[0].primitives[0] == data.primitives[0].id &&
           data.primitives[0].data.positions.empty() && data.primitives[0].data.indices.empty();
}

[[nodiscard]] bool text_values(const ExportData& data)
{
    const auto& camera = data.nodes[0].camera_properties;
    const auto root = data.root_metadata;
    if (!root.extras_json || root.extras_json->size() != 131088 || root.extensions.size() != 1)
        return false;
    return data.scenes[0].name == "Scene name" &&
           data.nodes[0].name == std::string(131073, 'n') + "\xE2\x82\xAC" &&
           data.meshes[0].name == "Mesh name" &&
           data.materials[0].description.name == "Material name" && camera.name == "Camera name" &&
           camera.aspect_ratio == 1.5F && data.required_extensions[0] == "EXT_metadata_owner";
}

[[nodiscard]] bool metadata_values(const ExportData& data)
{
    const std::array expected{
        std::pair{data.asset_metadata, R"({"scope":"asset"})"},
        std::pair{data.scenes[0].metadata, R"({"scope":"scene"})"},
        std::pair{data.nodes[0].metadata, R"({"scope":"node"})"},
        std::pair{data.meshes[0].metadata, R"({"scope":"mesh"})"},
        std::pair{data.primitives[0].metadata, R"({"scope":"primitive"})"},
        std::pair{data.materials[0].metadata, R"({"scope":"material"})"},
        std::pair{data.nodes[0].camera_properties.metadata, R"({"scope":"camera"})"},
        std::pair{data.nodes[0].camera_properties.projection_metadata,
                  R"({"scope":"projection"})"}};
    for (const auto& item : expected) {
        if (item.first.extras_json != item.second)
            return false;
    }
    return true;
}

[[nodiscard]] bool owned_after_disposal()
{
    auto source = make_source();
    auto metadata = ExportSourceMetadata::capture(source.view());
    if (!metadata || source.primitive_count() != 1)
        return false;
    source = Document{};
    ExportData data;
    data.metadata_owner = std::move(metadata).value();
    if (!data.metadata_owner->bind(data))
        return false;
    return structural_values(data) && text_values(data) && metadata_values(data);
}

[[nodiscard]] bool rejected_packets()
{
    Document document;
    const auto source = DocumentWriteSource::create(document.view()).value();
    auto metadata = ExportSourceMetadata::create(source.description());
    if (!metadata)
        return false;
    const auto root = source.record(document.view(), {}).value();
    auto borrowed = root;
    borrowed.material_description.name = "borrowed";
    auto foreign = root;
    foreign.address.object = WriteSourceObject::asset;
    if (metadata.value()->accept_record(borrowed) || metadata.value()->accept_record(foreign) ||
        metadata.value()->accept_text({}, {}))
        return false;
    return metadata.value()->accept_record(root).has_value() &&
           !metadata.value()->accept_links({}, {});
}
} // namespace

bool elf3d_export_metadata_tests()
{
    const bool passed = owned_after_disposal() && rejected_packets();
    if (!passed)
        std::fprintf(
            stderr,
            "FAIL owned export metadata after source disposal or invalid packet rejection\n");
    return passed;
}
