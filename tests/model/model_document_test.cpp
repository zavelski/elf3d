#include <elf3d/model.h>
#include <model/document_builder.h>
#include <model/document_handle_access.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

static_assert(noexcept(elf3d::Document{}));
static_assert(noexcept(std::declval<elf3d::Document&>().create_node()));
static_assert(noexcept(elf3d::validate_document(elf3d::DocumentView{})));
#include <elf3d/model.h>

#include "model_document_test_support.h"

using elf3d::model::tests::single_pixel;

namespace {
[[nodiscard]] bool verify_loading_storage();

struct SampleDocument {
    elf3d::Document document;
    elf3d::DocumentSceneId scene;
    elf3d::NodeId root;
    elf3d::MeshId mesh;
    elf3d::MaterialId material;
    elf3d::PrimitiveId primitive;
};

[[nodiscard]] elf3d::PrimitiveData triangle_data()
{
    elf3d::PrimitiveData data;
    data.positions = {
        {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
    };
    data.normals = {
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
    };
    data.texcoord0 = {
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {0.0F, 1.0F},
    };
    data.colors = {
        {1.0F, 0.0F, 0.0F, 1.0F},
        {0.0F, 1.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F, 1.0F},
    };
    data.indices = {0, 1, 2};
    return data;
}

[[nodiscard]] elf3d::PrimitiveData quad_data()
{
    elf3d::PrimitiveData data;
    data.positions = {
        {-1.0F, -1.0F, 0.0F},
        {1.0F, -1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F},
        {-1.0F, 1.0F, 0.0F},
    };
    data.normals = {
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
    };
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

[[nodiscard]] std::optional<SampleDocument> create_sample_document()
{
    SampleDocument sample;
    const auto scene = sample.document.create_scene("Default");
    const auto root = sample.document.create_node("Root");
    const auto mesh = sample.document.create_mesh("TriangleMesh");
    const auto material = sample.document.create_material({});
    if (!scene || !root || !mesh || !material) {
        return std::nullopt;
    }

    elf3d::PrimitiveData triangle = triangle_data();
    const auto primitive =
        sample.document.create_primitive(mesh.value(), material.value(), std::move(triangle));
    if (!primitive) {
        return std::nullopt;
    }
    if (!sample.document.set_node_mesh(root.value(), mesh.value())) {
        return std::nullopt;
    }
    if (!sample.document.add_scene_root(scene.value(), root.value())) {
        return std::nullopt;
    }

    sample.scene = scene.value();
    sample.root = root.value();
    sample.mesh = mesh.value();
    sample.material = material.value();
    sample.primitive = primitive.value();
    return std::move(sample);
}

[[nodiscard]] bool sample_bounds_are_valid(const elf3d::Document& document)
{
    const std::optional<elf3d::Bounds3> bounds = document.bounds();
    if (!bounds.has_value()) {
        return false;
    }
    if (bounds->minimum != elf3d::Float3{0.0F, 0.0F, 0.0F}) {
        return false;
    }
    return bounds->maximum == elf3d::Float3{1.0F, 1.0F, 0.0F};
}

[[nodiscard]] bool sample_scene_and_node_views_are_valid(const SampleDocument& sample)
{
    const elf3d::DocumentView view = sample.document.view();
    const auto scene_view = view.scene(sample.scene);
    const auto node_view = view.node(sample.root);
    if (!scene_view) {
        return false;
    }
    if (scene_view.value().name != "Default") {
        return false;
    }
    if (scene_view.value().roots.size() != 1) {
        return false;
    }
    if (view.default_scene() != std::optional<elf3d::DocumentSceneId>{sample.scene}) {
        return false;
    }
    if (!node_view) {
        return false;
    }
    if (node_view.value().name != "Root") {
        return false;
    }
    if (node_view.value().mesh != std::optional<elf3d::MeshId>{sample.mesh}) {
        return false;
    }
    return true;
}

[[nodiscard]] bool sample_mesh_and_primitive_views_are_valid(const SampleDocument& sample)
{
    const elf3d::DocumentView view = sample.document.view();
    const auto mesh_view = view.mesh(sample.mesh);
    const auto primitive_view = view.primitive(sample.primitive);
    if (!mesh_view) {
        return false;
    }
    if (!primitive_view) {
        return false;
    }
    if (mesh_view.value().primitives.size() != 1) {
        return false;
    }
    if (primitive_view.value().data.positions.size() != 3) {
        return false;
    }
    return primitive_view.value().data.indices.size() == 3;
}

[[nodiscard]] bool sample_views_are_valid(const SampleDocument& sample)
{
    return sample_scene_and_node_views_are_valid(sample) &&
           sample_mesh_and_primitive_views_are_valid(sample);
}

} // namespace

[[nodiscard]] int test_create_and_view_document()
{
    if (!verify_loading_storage())
        return 701;
    std::optional<SampleDocument> sample = create_sample_document();
    if (!sample.has_value()) {
        return 1;
    }

    const elf3d::DocumentStatistics expected_statistics{1, 1, 1, 1, 3, 3, 1, 1};
    if (sample->document.statistics() != expected_statistics) {
        return 2;
    }
    if (!sample_bounds_are_valid(sample->document)) {
        return 3;
    }
    if (!sample_views_are_valid(*sample)) {
        return 4;
    }

    return 0;
}

[[nodiscard]] bool replace_with_widened_triangle(SampleDocument& sample)
{
    elf3d::PrimitiveData widened_triangle = triangle_data();
    widened_triangle.positions[1].x = 2.0F;
    return sample.document.replace_primitive(sample.primitive, std::move(widened_triangle)) &&
           sample.document.bounds()->maximum.x == 2.0F;
}

[[nodiscard]] bool reject_invalid_replacement_without_mutation(SampleDocument& sample)
{
    elf3d::PrimitiveData invalid_quad = quad_data();
    invalid_quad.indices.back() = 99U;
    return !sample.document.replace_primitive(sample.primitive, std::move(invalid_quad)) &&
           sample.document.statistics().vertices == 3U &&
           sample.document.bounds()->maximum.x == 2.0F;
}

[[nodiscard]] bool replace_with_quad(SampleDocument& sample)
{
    elf3d::PrimitiveData quad = quad_data();
    const elf3d::DocumentStatistics replaced_statistics{1, 1, 1, 1, 4, 6, 2, 1};
    return sample.document.replace_primitive(sample.primitive, std::move(quad)) &&
           sample.document.statistics() == replaced_statistics &&
           sample.document.bounds()->minimum.x == -1.0F &&
           sample.document.bounds()->maximum.y == 1.0F;
}

[[nodiscard]] int test_mutation_and_replacement()
{
    std::optional<SampleDocument> sample = create_sample_document();
    if (!sample.has_value()) {
        return 1;
    }
    if (!replace_with_widened_triangle(*sample)) {
        return 2;
    }
    if (!reject_invalid_replacement_without_mutation(*sample)) {
        return 3;
    }
    if (!replace_with_quad(*sample)) {
        return 4;
    }

    return 0;
}

[[nodiscard]] int test_foreign_handles_and_cycles()
{
    std::optional<SampleDocument> sample = create_sample_document();
    if (!sample.has_value()) {
        return 1;
    }

    elf3d::Document foreign_document;
    const elf3d::Result<elf3d::MaterialId> foreign_material = foreign_document.create_material({});
    if (!foreign_material) {
        return 2;
    }
    elf3d::PrimitiveData invalid_triangle = triangle_data();
    const auto foreign_result = sample->document.create_primitive(
        sample->mesh, foreign_material.value(), invalid_triangle.view());
    if (foreign_result) {
        return 3;
    }
    if (foreign_result.error().code() != elf3d::ErrorCode::invalid_material_id) {
        return 4;
    }

    const elf3d::Result<elf3d::NodeId> child = sample->document.create_node("Child");
    if (!child) {
        return 5;
    }
    if (!sample->document.set_parent(child.value(), sample->root)) {
        return 6;
    }

    const elf3d::Result<void> cycle = sample->document.set_parent(sample->root, child.value());
    if (cycle) {
        return 7;
    }
    if (cycle.error().code() != elf3d::ErrorCode::hierarchy_cycle) {
        return 8;
    }

    const elf3d::DocumentValidationReport report =
        elf3d::validate_document(sample->document.view());
    if (report.has_errors()) {
        return 9;
    }

    return 0;
}

[[nodiscard]] bool has_initial_default_scene(const elf3d::Document& document,
                                             const elf3d::Result<elf3d::DocumentSceneId>& first,
                                             const elf3d::Result<elf3d::DocumentSceneId>& second)
{
    return first && second &&
           document.default_scene() == std::optional<elf3d::DocumentSceneId>{first.value()};
}

[[nodiscard]] bool has_selected_default_scene(elf3d::Document& document,
                                              elf3d::DocumentSceneId scene)
{
    return document.set_default_scene(scene) &&
           document.view().default_scene() == std::optional<elf3d::DocumentSceneId>{scene};
}

[[nodiscard]] int test_default_scene_selection()
{
    elf3d::Document document;
    const auto first = document.create_scene("First");
    const auto second = document.create_scene("Second");
    if (!has_initial_default_scene(document, first, second)) {
        return 1;
    }
    if (!has_selected_default_scene(document, second.value())) {
        return 2;
    }
    elf3d::Document foreign;
    const auto foreign_scene = foreign.create_scene();
    if (!foreign_scene || document.set_default_scene(foreign_scene.value())) {
        return 3;
    }
    if (!document.clear_default_scene() || document.default_scene().has_value()) {
        return 4;
    }
    return elf3d::validate_document(document.view()).has_errors() ? 5 : 0;
}

struct DocumentIdentityFixture final {
    elf3d::Document document;
    elf3d::DocumentSceneId scene;
    elf3d::NodeId node;
    elf3d::MeshId mesh;
    elf3d::PrimitiveId primitive;
    elf3d::MaterialId material;
    elf3d::ImageId image;
    elf3d::TextureId texture;
    elf3d::SamplerId sampler;
};

[[nodiscard]] std::optional<DocumentIdentityFixture> create_document_identity_fixture()
{
    elf3d::Document document;
    const auto scene = document.create_scene();
    const auto node = document.create_node();
    const auto mesh = document.create_mesh();
    const auto material = document.create_material();
    const std::array<std::byte, 4> pixel = single_pixel();
    const auto image = document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
    const auto sampler = document.create_sampler();
    const std::array fixture_created{static_cast<bool>(scene), static_cast<bool>(node),
                                     static_cast<bool>(mesh),  static_cast<bool>(material),
                                     static_cast<bool>(image), static_cast<bool>(sampler)};
    if (!std::all_of(fixture_created.begin(), fixture_created.end(),
                     [](bool created) noexcept { return created; })) {
        return std::nullopt;
    }

    const auto primitive =
        document.create_primitive(mesh.value(), material.value(), triangle_data());
    const auto texture = document.create_texture({image.value(), sampler.value()});
    if (!primitive || !texture) {
        return std::nullopt;
    }
    return DocumentIdentityFixture{std::move(document), scene.value(),     node.value(),
                                   mesh.value(),        primitive.value(), material.value(),
                                   image.value(),       texture.value(),   sampler.value()};
}

[[nodiscard]] int test_document_identity_errors()
{
    std::optional<DocumentIdentityFixture> fixture = create_document_identity_fixture();
    if (!fixture.has_value()) {
        return 1;
    }

    elf3d::Document foreign;
    const std::array actual_codes{
        foreign.scene(fixture->scene).error().code(),
        foreign.node(fixture->node).error().code(),
        foreign.mesh(fixture->mesh).error().code(),
        foreign.primitive(fixture->primitive).error().code(),
        foreign.material(fixture->material).error().code(),
        foreign.image(fixture->image).error().code(),
        foreign.texture(fixture->texture).error().code(),
        foreign.sampler(fixture->sampler).error().code(),
    };
    constexpr std::array expected_codes{
        elf3d::ErrorCode::invalid_document_scene_id, elf3d::ErrorCode::invalid_node_id,
        elf3d::ErrorCode::invalid_mesh_id,           elf3d::ErrorCode::invalid_primitive_id,
        elf3d::ErrorCode::invalid_material_id,       elf3d::ErrorCode::invalid_image_id,
        elf3d::ErrorCode::invalid_texture_id,        elf3d::ErrorCode::invalid_sampler_id,
    };
    if (actual_codes != expected_codes) {
        return 2;
    }

    const auto owner_token =
        elf3d::model::detail::DocumentHandleAccess::document_token(fixture->node);
    elf3d::Document moved = std::move(fixture->document);
    if (!moved.node(fixture->node)) {
        return 3;
    }
    moved = elf3d::Document{};
    const auto replacement_node = moved.create_node();
    if (!replacement_node) {
        return 4;
    }
    if (elf3d::model::detail::DocumentHandleAccess::document_token(replacement_node.value()) ==
        owner_token) {
        return 5;
    }
    if (moved.node(fixture->node).error().code() != elf3d::ErrorCode::invalid_node_id) {
        return 6;
    }
    return 0;
}

[[nodiscard]] elf3d::Result<elf3d::MaterialId>
create_document_material_with_texture(elf3d::Document& document)
{
    const std::array<std::byte, 4> pixel = single_pixel();
    const auto built_image = document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
    if (!built_image) {
        return built_image.error();
    }

    const auto built_sampler = document.create_sampler({});
    if (!built_sampler) {
        return built_sampler.error();
    }

    const auto built_texture =
        document.create_texture({built_image.value(), built_sampler.value()});
    if (!built_texture) {
        return built_texture.error();
    }
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color_texture = built_texture.value();
    return document.create_material(material_description);
}

[[nodiscard]] elf3d::Result<void> populate_document_scene(elf3d::Document& document,
                                                          elf3d::MaterialId material)
{
    const auto built_scene = document.create_scene("Built");
    if (!built_scene) {
        return built_scene.error();
    }

    const auto built_node = document.create_node("BuiltNode");
    if (!built_node) {
        return built_node.error();
    }

    const auto built_mesh = document.create_mesh("BuiltMesh");
    if (!built_mesh) {
        return built_mesh.error();
    }
    elf3d::PrimitiveData built_triangle = triangle_data();
    const auto built_primitive =
        document.create_primitive(built_mesh.value(), material, built_triangle.view());
    if (!built_primitive) {
        return built_primitive.error();
    }

    const elf3d::Result<void> mesh_result =
        document.set_node_mesh(built_node.value(), built_mesh.value());
    if (!mesh_result) {
        return mesh_result.error();
    }
    return document.add_scene_root(built_scene.value(), built_node.value());
}

[[nodiscard]] std::optional<elf3d::Document> create_built_document()
{
    elf3d::Document document;
    const auto built_material = create_document_material_with_texture(document);
    if (!built_material) {
        return std::nullopt;
    }
    if (!populate_document_scene(document, built_material.value())) {
        return std::nullopt;
    }
    if (elf3d::validate_document(document.view()).has_errors()) {
        return std::nullopt;
    }
    return std::move(document);
}

[[nodiscard]] int test_direct_document_construction()
{
    std::optional<elf3d::Document> built_document = create_built_document();
    if (!built_document.has_value()) {
        return 1;
    }

    const elf3d::DocumentStatistics statistics = built_document->statistics();
    if (statistics.primitives != 1) {
        return 2;
    }
    if (statistics.textures != 1) {
        return 3;
    }

    return 0;
}

namespace {

[[nodiscard]] elf3d::ModelJsonMetadata metadata_tag()
{
    elf3d::ModelJsonMetadata metadata;
    metadata.extras_json = R"({"label":"metadata","nested":[1,true,null]})";
    metadata.extensions.push_back({"EXT_checked", R"({"enabled":true})"});
    return metadata;
}

[[nodiscard]] elf3d::Result<elf3d::ModelDocumentMetadata> sample_metadata(SampleDocument& sample)
{
    auto& document = sample.document;
    const auto image =
        document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, single_pixel()});
    const auto sampler = document.create_sampler();
    if (!image || !sampler) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Metadata fixture resources"};
    }
    const auto texture = document.create_texture({image.value(), sampler.value()});
    if (!texture || !document.set_node_perspective_camera(sample.root, {})) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Metadata fixture camera/texture"};
    }
    elf3d::ModelDocumentMetadata metadata;
    metadata.root = metadata.asset = metadata_tag();
    metadata.required_extensions = {"EXT_checked"};
    metadata.scenes.emplace_back(sample.scene, metadata_tag());
    metadata.nodes.emplace_back(sample.root, metadata_tag());
    metadata.cameras.emplace_back(sample.root, metadata_tag());
    metadata.camera_projections.emplace_back(sample.root, metadata_tag());
    metadata.meshes.emplace_back(sample.mesh, metadata_tag());
    metadata.primitives.emplace_back(sample.primitive, metadata_tag());
    metadata.materials.emplace_back(sample.material, metadata_tag());
    metadata.images.emplace_back(image.value(), metadata_tag());
    metadata.textures.emplace_back(texture.value(), metadata_tag());
    metadata.samplers.emplace_back(sampler.value(), metadata_tag());
    return metadata;
}

[[nodiscard]] bool matches_tag(elf3d::ModelJsonMetadataView view)
{
    const auto expected = metadata_tag();
    return view.extras_json == expected.extras_json && view.extensions.size() == 1 &&
           view.extensions[0] == expected.extensions[0];
}

[[nodiscard]] bool metadata_all_scopes_match(const SampleDocument& sample)
{
    const auto& document = sample.document;
    const auto node = document.node(sample.root).value();
    const std::array matches{matches_tag(document.root_metadata()),
                             matches_tag(document.asset_metadata()),
                             matches_tag(document.scene(sample.scene).value().metadata),
                             matches_tag(node.metadata),
                             matches_tag(node.camera_properties.metadata),
                             matches_tag(node.camera_properties.projection_metadata),
                             matches_tag(document.mesh(sample.mesh).value().metadata),
                             matches_tag(document.primitive(sample.primitive).value().metadata),
                             matches_tag(document.material(sample.material).value().metadata),
                             matches_tag(document.image_at(0).value().metadata),
                             matches_tag(document.texture_at(0).value().metadata),
                             matches_tag(document.sampler_at(0).value().metadata)};
    const auto required = document.required_extensions();
    return std::all_of(matches.begin(), matches.end(), [](bool value) { return value; }) &&
           required.size() == 1 && required[0] == "EXT_checked";
}

template <class Id>
[[nodiscard]] bool reject_duplicate(elf3d::Document& document,
                                    elf3d::ModelDocumentMetadata& metadata,
                                    std::vector<std::pair<Id, elf3d::ModelJsonMetadata>>& entries)
{
    entries.push_back(entries.front());
    const bool rejected = !document.attach_verified_metadata(metadata);
    entries.pop_back();
    return rejected && !document.root_metadata().extras_json &&
           document.required_extensions().empty();
}

template <class Id>
[[nodiscard]] bool
reject_invalid_target(elf3d::Document& document, elf3d::ModelDocumentMetadata& metadata,
                      std::vector<std::pair<Id, elf3d::ModelJsonMetadata>>& entries)
{
    const Id valid = entries[0].first;
    entries[0].first = {};
    const bool rejected = !document.attach_verified_metadata(metadata);
    entries[0].first = valid;
    return rejected && !document.root_metadata().extras_json &&
           document.required_extensions().empty();
}

[[nodiscard]] bool reject_invalid_resources(elf3d::Document& document,
                                            elf3d::ModelDocumentMetadata& metadata)
{
    const std::array rejected{reject_invalid_target(document, metadata, metadata.materials),
                              reject_invalid_target(document, metadata, metadata.images),
                              reject_invalid_target(document, metadata, metadata.textures),
                              reject_invalid_target(document, metadata, metadata.samplers)};
    return std::all_of(rejected.begin(), rejected.end(), [](bool value) { return value; });
}

[[nodiscard]] bool reject_duplicate_metadata(elf3d::Document& document,
                                             elf3d::ModelDocumentMetadata& metadata)
{
    const std::array rejected{reject_duplicate(document, metadata, metadata.scenes),
                              reject_duplicate(document, metadata, metadata.nodes),
                              reject_duplicate(document, metadata, metadata.cameras),
                              reject_duplicate(document, metadata, metadata.camera_projections),
                              reject_duplicate(document, metadata, metadata.meshes),
                              reject_duplicate(document, metadata, metadata.primitives),
                              reject_duplicate(document, metadata, metadata.materials),
                              reject_duplicate(document, metadata, metadata.images),
                              reject_duplicate(document, metadata, metadata.textures),
                              reject_duplicate(document, metadata, metadata.samplers)};
    return std::all_of(rejected.begin(), rejected.end(), [](bool value) { return value; });
}

[[nodiscard]] bool reject_camera_metadata(elf3d::Document& document,
                                          elf3d::ModelDocumentMetadata& metadata)
{
    const auto node = document.create_node("no camera");
    if (!node) {
        return false;
    }
    const auto camera = metadata.cameras[0].first;
    metadata.cameras[0].first = node.value();
    const bool rejected_camera = !document.attach_verified_metadata(metadata);
    metadata.cameras[0].first = camera;
    metadata.camera_projections[0].first = node.value();
    const bool rejected_projection = !document.attach_verified_metadata(metadata);
    metadata.camera_projections[0].first = camera;
    return rejected_camera && rejected_projection;
}

[[nodiscard]] bool reject_metadata_inputs(SampleDocument& sample,
                                          elf3d::ModelDocumentMetadata metadata)
{
    auto& document = sample.document;
    if (!reject_duplicate_metadata(document, metadata) ||
        !reject_camera_metadata(document, metadata) ||
        !reject_invalid_resources(document, metadata)) {
        return false;
    }
    elf3d::Document foreign;
    const auto foreign_node = foreign.create_node();
    if (!foreign_node) {
        return false;
    }
    const auto local = metadata.nodes[0].first;
    metadata.nodes[0].first = foreign_node.value();
    if (document.attach_verified_metadata(metadata)) {
        return false;
    }
    metadata.nodes[0].first = local;
    metadata.required_extensions.push_back(metadata.required_extensions.front());
    if (document.attach_verified_metadata(metadata)) {
        return false;
    }
    metadata.required_extensions.pop_back();
    metadata.root.extras_json = '"' + std::string(1024U * 1024U, 'x') + '"';
    return !document.attach_verified_metadata(std::move(metadata)) &&
           !document.root_metadata().extras_json && document.required_extensions().empty();
}

[[nodiscard]] bool metadata_stale_after_edit(elf3d::Document& document)
{
    const auto edited = document.create_node("structural edit");
    return edited && document.preserved_metadata_stale() && !document.attach_verified_metadata({});
}

[[nodiscard]] bool reject_after_metadata_construction(SampleDocument& sample)
{
    if (sample.document.attach_verified_metadata({}) || !metadata_all_scopes_match(sample)) {
        return false;
    }
    if (!metadata_stale_after_edit(sample.document)) {
        return false;
    }
    elf3d::Document empty;
    if (!empty.attach_verified_metadata({})) {
        return false;
    }
    elf3d::Document transferred{std::move(empty)};
    return !empty.attach_verified_metadata({}) && transferred.attach_verified_metadata({});
}

[[nodiscard]] int test_owned_metadata_construction()
{
    auto sample = create_sample_document();
    if (!sample) {
        return 1;
    }
    auto metadata = sample_metadata(*sample);
    if (!metadata || !reject_metadata_inputs(*sample, metadata.value())) {
        return 2;
    }
    const auto before = sample->document.statistics();
    if (!sample->document.attach_verified_metadata(std::move(metadata).value()) ||
        !metadata_all_scopes_match(*sample) || sample->document.statistics() != before) {
        return 3;
    }
    return reject_after_metadata_construction(*sample) ? 0 : 4;
}

} // namespace

namespace {
[[nodiscard]] bool verify_validation_after_mutation(elf3d::Document& document)
{
    // A cached successful validation must not hide warnings introduced by mutation.
    elf3d::ModelDocumentMetadata metadata;
    metadata.root.extras_json = "{\"loading\":true}";
    if (!document.attach_verified_metadata(std::move(metadata)) ||
        elf3d::validate_document(document.view()).has_errors())
        return false;
    const auto node = document.create_node("edit");
    const auto changed = elf3d::validate_document(document.view());
    if (!node || changed.diagnostics.empty() || changed.has_errors())
        return false;
    auto clone = elf3d::clone_document(document.view());
    return clone && !elf3d::validate_document(clone.value().view()).has_errors() &&
           !elf3d::validate_document(clone.value().view()).diagnostics.empty();
}
[[nodiscard]] bool verify_loading_storage()
{
    elf3d::model::detail::DocumentBuilder builder;
    std::vector<std::byte> pixels{std::byte{17}, std::byte{34}, std::byte{51}, std::byte{255}};
    std::vector<std::byte> source;
    const auto* original = pixels.data();
    const auto image = builder.create_image_owned({1, 1}, std::move(pixels), std::move(source));
    if (!image)
        return false;
    auto document = builder.finish();
    if (!document || document.value().image(image.value()).value().pixels.data() != original)
        return false;
    if (elf3d::validate_document(document.value().view()).has_errors())
        return false;
    return verify_validation_after_mutation(document.value());
}
} // namespace

int elf3d_model_document_test()
{
    if (const int result = test_create_and_view_document(); result != 0) {
        return result;
    }
    if (const int result = test_mutation_and_replacement(); result != 0) {
        return 100 + result;
    }
    if (const int result = test_foreign_handles_and_cycles(); result != 0) {
        return 200 + result;
    }
    if (const int result = test_default_scene_selection(); result != 0) {
        return 250 + result;
    }
    if (const int result = test_model_asset_views_and_statistics(); result != 0) {
        return 300 + result;
    }
    if (const int result = test_model_asset_rejections(); result != 0) {
        return 400 + result;
    }
    if (const int result = test_document_identity_errors(); result != 0) {
        return 450 + result;
    }
    if (const int result = test_direct_document_construction(); result != 0) {
        return 500 + result;
    }
    if (const int result = test_owned_metadata_construction(); result != 0) {
        return 600 + result;
    }
    return 0;
}
