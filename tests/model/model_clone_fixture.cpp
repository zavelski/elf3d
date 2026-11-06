#include "model_clone_test_support.h"

#include <model/imported_metadata.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace elf3d::model::tests {
namespace {

[[nodiscard]] std::string owned_name(std::string_view kind, std::size_t index)
{
    return "Independently owned clone fixture " + std::string{kind} + " " + std::to_string(index);
}

void create_images(Document& document)
{
    const std::array<std::byte, 4> pixels{std::byte{17}, std::byte{34}, std::byte{51},
                                          std::byte{255}};
    // The canonical Model validates signatures, not codecs. These payloads test
    // byte ownership only; importer/exporter tests cover file-level preservation.
    const std::array<std::byte, 8> png{std::byte{0x89}, std::byte{0x50}, std::byte{0x4e},
                                       std::byte{0x47}, std::byte{0x0d}, std::byte{0x0a},
                                       std::byte{0x1a}, std::byte{0x0a}};
    const std::array<std::byte, 8> jpeg{std::byte{0xff}, std::byte{0xd8}, std::byte{0xff}};
    const std::array<std::byte, 8> tiff{std::byte{'I'}, std::byte{'I'}, std::byte{42}};
    const std::array<std::byte, 8> exr{std::byte{0x76}, std::byte{0x2f}, std::byte{0x31},
                                       std::byte{0x01}};
    for (const auto& entry :
         {std::pair{ModelImageMimeType::png, png}, std::pair{ModelImageMimeType::jpeg, jpeg},
          std::pair{ModelImageMimeType::tiff, tiff}}) {
        const auto id = require_value(document.create_image(
            {1, 1, PixelFormat::rgba8_unorm, pixels, entry.first, entry.second}));
        ELF3D_ASSERT(id.is_valid());
    }
    const auto raw = require_value(document.create_image({1, 1, PixelFormat::rgba8_unorm, pixels}));
    const auto environment = require_value(
        document.create_image({0, 0, PixelFormat::rgba8_unorm, {}, ModelImageMimeType::exr, exr}));
    ELF3D_ASSERT(raw != environment);
}

[[nodiscard]] std::array<TextureId, 4> create_textures(Document& document)
{
    const auto a = require_value(document.create_sampler());
    const auto b = require_value(
        document.create_sampler({TextureWrap::mirrored_repeat, TextureWrap::clamp_to_edge,
                                 TextureFilter::linear_mipmap_linear, TextureFilter::nearest}));
    const auto first = require_value(document.image_at(0)).id;
    return {require_value(document.create_texture({first, a})),
            require_value(document.create_texture({first, a})),
            require_value(document.create_texture({require_value(document.image_at(1)).id, b})),
            require_value(document.create_texture({require_value(document.image_at(2)).id, a}))};
}

[[nodiscard]] ModelMaterialDescription optical_material(const std::array<TextureId, 4>& texture)
{
    ModelMaterialDescription material;
    material.base_color = {0.1F, 0.2F, 0.3F, 0.75F};
    material.double_sided = true;
    material.metallic_factor = 0.125F;
    material.roughness_factor = 0.375F;
    material.alpha_mode = AlphaMode::mask;
    material.alpha_cutoff = 0.25F;
    material.emissive_factor = {0.2F, 0.3F, 0.4F};
    material.emissive_strength = 2.5F;
    material.normal_scale = 0.75F;
    material.occlusion_strength = 0.625F;
    material.ior = 1.4F;
    material.specular_factor = 0.875F;
    material.specular_color_factor = {0.25F, 0.5F, 0.75F};
    material.base_color_texture = texture[0];
    material.metallic_roughness_texture = texture[1];
    material.normal_texture = texture[2];
    material.occlusion_texture = texture[0];
    material.emissive_texture = texture[3];
    material.specular_texture = texture[2];
    material.specular_color_texture = texture[1];
    const TextureMapping mapping{1, {{0.125F, 0.25F}, {0.5F, 0.75F}, 0.375F}};
    material.emissive_texture_mapping = mapping;
    material.specular_texture_mapping = mapping;
    material.specular_color_texture_mapping = mapping;
    material.transmission = ModelTransmissionDescription{0.5F, texture[3], mapping};
    material.volume =
        ModelVolumeDescription{0.375F, texture[2], mapping, 3.5F, {0.75F, 0.5F, 0.25F}};
    material.clearcoat = ModelClearcoatDescription{
        0.75F, 0.25F, texture[1], texture[3], texture[0], 0.625F, mapping, mapping, mapping};
    return material;
}

[[nodiscard]] std::array<MaterialId, 3> create_materials(Document& document)
{
    auto optical = optical_material(create_textures(document));
    const auto optical_name = owned_name("optical material", 0);
    optical.name = optical_name;
    const auto first = require_value(document.create_material(optical));
    ModelMaterialDescription plain;
    plain.unlit = true;
    plain.base_color = {0.125F, 0.75F, 0.5F, 0.5F};
    plain.alpha_mode = AlphaMode::blend;
    const auto plain_name = owned_name("plain material", 1);
    plain.name = plain_name;
    const auto second = require_value(document.create_material(plain));
    const auto unused_name = owned_name("unused material", 2);
    optical.name = unused_name;
    optical.volume->attenuation_distance.reset();
    return {first, second, require_value(document.create_material(optical))};
}

void create_geometry(Document& document, const std::array<MaterialId, 3>& materials)
{
    const auto a = require_value(document.create_mesh(owned_name("mesh", 0)));
    const auto b = require_value(document.create_mesh(owned_name("mesh", 1)));
    auto full = clone_triangle_data();
    full.positions.push_back({1.0F, 1.0F, 0.0F});
    full.indices = {0, 1, 2, 2, 1, 3};
    full.normals.assign(4, {0.0F, 0.0F, 1.0F});
    full.tangents.assign(4, {1.0F, 0.0F, 0.0F, -1.0F});
    full.texcoord0 = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    full.texcoord1 = {{0.125F, 0.25F}, {0.5F, 0.25F}, {0.125F, 0.5F}, {0.5F, 0.5F}};
    full.colors.assign(4, {0.1F, 0.25F, 0.5F, 1.0F});
    const auto first = require_value(document.create_primitive(a, materials[0], full.view()));
    const auto middle =
        require_value(document.create_primitive(b, materials[1], clone_triangle_data()));
    const auto last = require_value(document.create_primitive(a, materials[1], full.view()));
    ELF3D_ASSERT(first != middle && first != last);
}

void create_hierarchy(Document& document)
{
    std::array<NodeId, 5> nodes;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        nodes[i] = require_value(document.create_node(owned_name("node", i)));
    }
    // Parent order and mesh primitive order differ from global creation order.
    for (const auto child : {nodes[3], nodes[0], nodes[2]}) {
        require_success(document.set_parent(child, nodes[1]));
    }
    require_success(document.set_node_mesh(nodes[0], require_value(document.mesh_at(0)).id));
    require_success(document.set_node_mesh(nodes[2], require_value(document.mesh_at(0)).id));
    require_success(document.set_node_mesh(nodes[3], require_value(document.mesh_at(1)).id));
    Float4x4 transformed;
    transformed.elements[0] = -1.0F;
    transformed.elements[5] = 2.0F;
    transformed.elements[10] = 0.5F;
    transformed.elements[12] = 3.25F;
    require_success(document.set_node_matrix(nodes[2], transformed));
    require_success(document.set_node_perspective_camera(nodes[4], {0.75F, 0.125F, 128.0F}));
    ModelCameraPropertiesDescription properties;
    properties.name = owned_name("infinite camera", 0);
    properties.aspect_ratio = 1.5F;
    properties.infinite_far_plane = true;
    properties.image_based_lighting = ModelCameraImageBasedLightingDescription{
        require_value(document.image_at(4)).id, {10.0F, -20.0F, 30.0F}, {0.75F, 1.25F}};
    require_success(document.set_node_camera_properties(nodes[4], properties));
    const auto first = require_value(document.create_scene(owned_name("scene", 0)));
    const auto second = require_value(document.create_scene(owned_name("scene", 1)));
    require_success(document.add_scene_root(first, nodes[1]));
    require_success(document.add_scene_root(first, nodes[4]));
    require_success(document.add_scene_root(second, nodes[4]));
    require_success(document.add_scene_root(second, nodes[1]));
    require_success(document.set_default_scene(second));
}

[[nodiscard]] ModelJsonMetadata metadata(std::string_view kind, std::size_t index)
{
    ModelJsonMetadata result;
    result.extras_json = "{ \"kind\" : \"" + owned_name(kind, index) + "\", \"opaqueIndex\" : 0 }";
    result.extensions = {{"TEST_clone_owned",
                          "{ \"payload\" : \"Preserve this exact JSON spelling and storage\" }"}};
    return result;
}

void attach_metadata(Document& document)
{
    detail::ImportedDocumentMetadata imported;
    imported.root = metadata("root", 0);
    imported.asset = metadata("asset", 0);
    imported.required_extensions = {"TEST_required_a", "TEST_required_b"};
    for (std::size_t i = 0; i < document.scene_count(); ++i)
        imported.scenes.emplace_back(require_value(document.scene_at(i)).id, metadata("scene", i));
    for (std::size_t i = 0; i < document.node_count(); ++i)
        imported.nodes.emplace_back(require_value(document.node_at(i)).id, metadata("node", i));
    for (std::size_t i = 0; i < document.mesh_count(); ++i)
        imported.meshes.emplace_back(require_value(document.mesh_at(i)).id, metadata("mesh", i));
    for (std::size_t i = 0; i < document.primitive_count(); ++i)
        imported.primitives.emplace_back(require_value(document.primitive_at(i)).id,
                                         metadata("primitive", i));
    for (std::size_t i = 0; i < document.material_count(); ++i)
        imported.materials.emplace_back(require_value(document.material_at(i)).id,
                                        metadata("material", i));
    for (std::size_t i = 0; i < document.image_count(); ++i)
        imported.images.emplace_back(require_value(document.image_at(i)).id, metadata("image", i));
    for (std::size_t i = 0; i < document.texture_count(); ++i)
        imported.textures.emplace_back(require_value(document.texture_at(i)).id,
                                       metadata("texture", i));
    for (std::size_t i = 0; i < document.sampler_count(); ++i)
        imported.samplers.emplace_back(require_value(document.sampler_at(i)).id,
                                       metadata("sampler", i));
    const auto camera = require_value(document.node_at(4)).id;
    imported.cameras.emplace_back(camera, metadata("camera", 0));
    imported.camera_projections.emplace_back(camera, metadata("projection", 0));
    require_success(
        detail::DocumentMetadataAccess::attach_import_metadata(document, std::move(imported)));
}

} // namespace

PrimitiveData clone_triangle_data()
{
    PrimitiveData data;
    data.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    data.indices = {0, 1, 2};
    return data;
}

Document make_clone_fixture()
{
    Document document;
    create_images(document);
    create_geometry(document, create_materials(document));
    create_hierarchy(document);
    attach_metadata(document);
    return document;
}

} // namespace elf3d::model::tests
