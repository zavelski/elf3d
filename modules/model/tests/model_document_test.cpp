#include <elf3d/model.h>

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

import elf.model;

namespace {

struct SampleDocument {
    elf3d::Document document;
    elf3d::DocumentSceneId scene;
    elf3d::NodeId root;
    elf3d::MeshId mesh;
    elf3d::MaterialId material;
    elf3d::PrimitiveId primitive;
};

struct SampleAssets {
    elf3d::Document document;
    elf3d::ImageId image;
    elf3d::SamplerId sampler;
    elf3d::TextureId texture;
    elf3d::MaterialId material;
    elf3d::ModelSamplerDescription sampler_description;
    elf3d::ModelMaterialDescription material_description;
};

[[nodiscard]] elf3d::PrimitiveData triangle_data() {
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

[[nodiscard]] elf3d::PrimitiveData quad_data() {
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

[[nodiscard]] std::array<std::byte, 4> single_pixel() {
    return {std::byte{17}, std::byte{34}, std::byte{51}, std::byte{255}};
}

[[nodiscard]] std::optional<SampleDocument> create_sample_document() {
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

[[nodiscard]] bool sample_bounds_are_valid(const elf3d::Document& document) {
    const std::optional<elf3d::Bounds3> bounds = document.bounds();
    if (!bounds.has_value()) {
        return false;
    }
    if (bounds->minimum != elf3d::Float3{0.0F, 0.0F, 0.0F}) {
        return false;
    }
    return bounds->maximum == elf3d::Float3{1.0F, 1.0F, 0.0F};
}

[[nodiscard]] bool sample_scene_and_node_views_are_valid(const SampleDocument& sample) {
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

[[nodiscard]] bool sample_mesh_and_primitive_views_are_valid(const SampleDocument& sample) {
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

[[nodiscard]] bool sample_views_are_valid(const SampleDocument& sample) {
    return sample_scene_and_node_views_are_valid(sample) &&
           sample_mesh_and_primitive_views_are_valid(sample);
}

} // namespace

[[nodiscard]] int test_create_and_view_document() {
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

[[nodiscard]] int test_mutation_and_replacement() {
    std::optional<SampleDocument> sample = create_sample_document();
    if (!sample.has_value()) {
        return 1;
    }

    elf3d::Result<std::span<elf3d::Float3>> positions =
        sample->document.mutable_positions(sample->primitive);
    if (!positions) {
        return 2;
    }
    positions.value()[1].x = 2.0F;
    if (!sample->document.update_primitive_bounds(sample->primitive)) {
        return 3;
    }
    if (sample->document.bounds()->maximum.x != 2.0F) {
        return 4;
    }

    elf3d::PrimitiveData quad = quad_data();
    if (!sample->document.replace_primitive(sample->primitive, std::move(quad))) {
        return 5;
    }
    const elf3d::DocumentStatistics replaced_statistics{1, 1, 1, 1, 4, 6, 2, 1};
    if (sample->document.statistics() != replaced_statistics) {
        return 6;
    }
    if (sample->document.bounds()->minimum.x != -1.0F) {
        return 7;
    }
    if (sample->document.bounds()->maximum.y != 1.0F) {
        return 8;
    }

    return 0;
}

[[nodiscard]] int test_foreign_handles_and_cycles() {
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
    if (foreign_result.error().code() != elf3d::ErrorCode::invalid_material_handle) {
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

[[nodiscard]] int test_default_scene_selection() {
    elf3d::Document document;
    const auto first = document.create_scene("First");
    const auto second = document.create_scene("Second");
    if (!first || !second ||
        document.default_scene() != std::optional<elf3d::DocumentSceneId>{first.value()}) {
        return 1;
    }
    if (!document.set_default_scene(second.value()) ||
        document.view().default_scene() != std::optional<elf3d::DocumentSceneId>{second.value()}) {
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

[[nodiscard]] std::optional<SampleAssets> create_sample_assets() {
    SampleAssets sample;
    const std::array<std::byte, 4> pixel = single_pixel();
    constexpr std::array<std::byte, 8> png_source{std::byte{0x89}, std::byte{0x50}, std::byte{0x4e},
                                                  std::byte{0x47}, std::byte{0x0d}, std::byte{0x0a},
                                                  std::byte{0x1a}, std::byte{0x0a}};
    const elf3d::Result<elf3d::ImageId> image =
        sample.document.create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel,
                                      elf3d::ModelImageMimeType::png, png_source});
    if (!image) {
        return std::nullopt;
    }

    sample.sampler_description = elf3d::ModelSamplerDescription{
        elf3d::ModelTextureWrap::clamp_to_edge, elf3d::ModelTextureWrap::mirrored_repeat,
        elf3d::ModelTextureFilter::linear_mipmap_linear, elf3d::ModelTextureFilter::nearest};
    const elf3d::Result<elf3d::SamplerId> sampler =
        sample.document.create_sampler(sample.sampler_description);
    if (!sampler) {
        return std::nullopt;
    }

    const elf3d::Result<elf3d::TextureId> texture =
        sample.document.create_texture({image.value(), sampler.value()});
    if (!texture) {
        return std::nullopt;
    }

    sample.material_description.base_color_texture = texture.value();
    sample.material_description.normal_texture = texture.value();
    sample.material_description.base_color_texture_mapping.texcoord_set = 1;
    const elf3d::Result<elf3d::MaterialId> material =
        sample.document.create_material(sample.material_description);
    if (!material) {
        return std::nullopt;
    }

    sample.image = image.value();
    sample.sampler = sampler.value();
    sample.texture = texture.value();
    sample.material = material.value();
    return std::move(sample);
}

[[nodiscard]] bool sample_image_view_is_valid(const SampleAssets& sample) {
    const std::array<std::byte, 4> pixel = single_pixel();
    const elf3d::Result<elf3d::ImageView> image_view = sample.document.view().image(sample.image);
    if (!image_view) {
        return false;
    }
    if (image_view.value().width != 1 || image_view.value().height != 1 ||
        image_view.value().pixels.size() != pixel.size() ||
        image_view.value().pixels.front() != pixel.front() ||
        image_view.value().source_mime_type != elf3d::ModelImageMimeType::png ||
        image_view.value().source_bytes.size() != 8U ||
        image_view.value().source_bytes.front() != std::byte{0x89}) {
        return false;
    }
    return true;
}

[[nodiscard]] bool sample_sampler_texture_material_views_are_valid(const SampleAssets& sample) {
    const elf3d::DocumentView view = sample.document.view();
    const elf3d::Result<elf3d::SamplerView> sampler_view = view.sampler(sample.sampler);
    if (!sampler_view || sampler_view.value().description != sample.sampler_description) {
        return false;
    }
    const elf3d::Result<elf3d::TextureView> texture_view = view.texture(sample.texture);
    if (!texture_view || texture_view.value().description.image != sample.image ||
        texture_view.value().description.sampler != sample.sampler) {
        return false;
    }
    const elf3d::Result<elf3d::MaterialView> material_view = view.material(sample.material);
    if (!material_view || material_view.value().description != sample.material_description) {
        return false;
    }
    return true;
}

[[nodiscard]] bool sample_asset_statistics_are_valid(const SampleAssets& sample) {
    const elf3d::DocumentStatistics expected_statistics{0, 0, 0, 0, 0, 0, 0, 1, 1,
                                                        1, 1, 0, 4, 1, 0, 1, 0, 0};
    if (sample.document.statistics() != expected_statistics) {
        return false;
    }
    return !elf3d::validate_document(sample.document.view()).has_errors();
}

[[nodiscard]] bool sample_stale_texture_is_rejected(const SampleAssets& sample) {
    const elf3d::TextureId stale_texture =
        elf3d::model::detail::DocumentHandleAccess::create_texture(
            elf3d::model::detail::DocumentHandleAccess::document(sample.texture),
            sample.texture.debug_value() + 1U);
    return sample.document.texture(stale_texture).error().code() ==
           elf3d::ErrorCode::invalid_texture_asset_handle;
}

[[nodiscard]] int test_model_asset_views_and_statistics() {
    std::optional<SampleAssets> sample = create_sample_assets();
    if (!sample.has_value()) {
        return 1;
    }
    if (!sample_image_view_is_valid(*sample)) {
        return 2;
    }
    if (!sample_sampler_texture_material_views_are_valid(*sample)) {
        return 3;
    }
    if (!sample_asset_statistics_are_valid(*sample)) {
        return 4;
    }
    if (!sample_stale_texture_is_rejected(*sample)) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int test_model_image_rejections() {
    elf3d::Document document;
    const std::array<std::byte, 4> pixel = single_pixel();
    const std::array<std::byte, 1> short_pixel{std::byte{0}};

    if (document.create_image({0, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel}).error().code() !=
        elf3d::ErrorCode::zero_image_dimensions) {
        return 1;
    }
    if (document.create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, short_pixel})
            .error()
            .code() != elf3d::ErrorCode::invalid_argument) {
        return 2;
    }
    elf3d::ModelImageDescription oversized_image;
    oversized_image.width = 67'108'865U;
    oversized_image.height = 1;
    if (document.create_image(oversized_image).error().code() !=
        elf3d::ErrorCode::decoded_image_size_overflow) {
        return 3;
    }
    const std::array<std::byte, 3> jpeg_source{std::byte{0xff}, std::byte{0xd8}, std::byte{0xff}};
    if (document
            .create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel,
                           elf3d::ModelImageMimeType::none, jpeg_source})
            .error()
            .code() != elf3d::ErrorCode::invalid_argument) {
        return 4;
    }
    if (document
            .create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel,
                           elf3d::ModelImageMimeType::png, jpeg_source})
            .error()
            .code() != elf3d::ErrorCode::invalid_argument) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int test_model_sampler_rejections() {
    elf3d::Document document;
    elf3d::ModelSamplerDescription invalid_sampler;
    invalid_sampler.mag_filter = elf3d::ModelTextureFilter::linear_mipmap_linear;
    if (document.create_sampler(invalid_sampler).error().code() !=
        elf3d::ErrorCode::invalid_sampler_description) {
        return 1;
    }
    return 0;
}

struct TextureRejectionInputs {
    elf3d::Document document;
    elf3d::Document foreign_document;
    elf3d::ImageId image;
    elf3d::SamplerId sampler;
    elf3d::ImageId foreign_image;
    elf3d::SamplerId foreign_sampler;
    elf3d::TextureId foreign_texture;
};

[[nodiscard]] std::optional<TextureRejectionInputs> create_texture_rejection_inputs() {
    TextureRejectionInputs inputs;
    const std::array<std::byte, 4> pixel = single_pixel();
    const elf3d::Result<elf3d::ImageId> image =
        inputs.document.create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel});
    const elf3d::Result<elf3d::SamplerId> sampler = inputs.document.create_sampler({});
    const elf3d::Result<elf3d::ImageId> foreign_image =
        inputs.foreign_document.create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel});
    const elf3d::Result<elf3d::SamplerId> foreign_sampler =
        inputs.foreign_document.create_sampler({});
    if (!image || !sampler || !foreign_image || !foreign_sampler) {
        return std::nullopt;
    }
    const elf3d::Result<elf3d::TextureId> foreign_texture =
        inputs.foreign_document.create_texture({foreign_image.value(), foreign_sampler.value()});
    if (!foreign_texture) {
        return std::nullopt;
    }
    inputs.image = image.value();
    inputs.sampler = sampler.value();
    inputs.foreign_image = foreign_image.value();
    inputs.foreign_sampler = foreign_sampler.value();
    inputs.foreign_texture = foreign_texture.value();
    return std::move(inputs);
}

[[nodiscard]] int test_model_texture_rejections() {
    std::optional<TextureRejectionInputs> inputs = create_texture_rejection_inputs();
    if (!inputs.has_value()) {
        return 1;
    }
    if (inputs->document.create_texture({inputs->foreign_image, inputs->sampler}).error().code() !=
        elf3d::ErrorCode::invalid_image_handle) {
        return 2;
    }
    if (inputs->document.create_texture({inputs->image, inputs->foreign_sampler}).error().code() !=
        elf3d::ErrorCode::invalid_sampler_description) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_model_material_rejections() {
    std::optional<TextureRejectionInputs> inputs = create_texture_rejection_inputs();
    if (!inputs.has_value()) {
        return 1;
    }
    elf3d::ModelMaterialDescription material_with_foreign_texture;
    material_with_foreign_texture.base_color_texture = inputs->foreign_texture;
    if (inputs->document.create_material(material_with_foreign_texture).error().code() !=
        elf3d::ErrorCode::invalid_texture_asset_handle) {
        return 2;
    }
    elf3d::ModelMaterialDescription invalid_mapping;
    invalid_mapping.base_color_texture_mapping.texcoord_set =
        elf3d::model_maximum_texture_coordinate_sets;
    if (inputs->document.create_material(invalid_mapping).error().code() !=
        elf3d::ErrorCode::invalid_material_handle) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_model_asset_rejections() {
    if (const int result = test_model_image_rejections(); result != 0) {
        return result;
    }
    if (const int result = test_model_sampler_rejections(); result != 0) {
        return 100 + result;
    }
    if (const int result = test_model_texture_rejections(); result != 0) {
        return 200 + result;
    }
    if (const int result = test_model_material_rejections(); result != 0) {
        return 300 + result;
    }
    return 0;
}

[[nodiscard]] elf3d::Result<elf3d::MaterialId>
create_builder_material_with_texture(elf3d::DocumentBuilder& builder) {
    const std::array<std::byte, 4> pixel = single_pixel();
    const auto built_image =
        builder.create_image({1, 1, elf3d::ModelPixelFormat::rgba8_unorm, pixel});
    if (!built_image) {
        return built_image.error();
    }
    const auto built_sampler = builder.create_sampler({});
    if (!built_sampler) {
        return built_sampler.error();
    }
    const auto built_texture = builder.create_texture({built_image.value(), built_sampler.value()});
    if (!built_texture) {
        return built_texture.error();
    }
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color_texture = built_texture.value();
    return builder.create_material(material_description);
}

[[nodiscard]] elf3d::Result<void> populate_builder_scene(elf3d::DocumentBuilder& builder,
                                                         elf3d::MaterialId material) {
    const auto built_scene = builder.create_scene("Built");
    if (!built_scene) {
        return built_scene.error();
    }
    const auto built_node = builder.create_node("BuiltNode");
    if (!built_node) {
        return built_node.error();
    }
    const auto built_mesh = builder.create_mesh("BuiltMesh");
    if (!built_mesh) {
        return built_mesh.error();
    }
    elf3d::PrimitiveData built_triangle = triangle_data();
    const auto built_primitive =
        builder.create_primitive(built_mesh.value(), material, built_triangle.view());
    if (!built_primitive) {
        return built_primitive.error();
    }
    const elf3d::Result<void> mesh_result =
        builder.set_node_mesh(built_node.value(), built_mesh.value());
    if (!mesh_result) {
        return mesh_result.error();
    }
    return builder.add_scene_root(built_scene.value(), built_node.value());
}

[[nodiscard]] std::optional<elf3d::Document> create_built_document() {
    elf3d::DocumentBuilder builder;
    const auto built_material = create_builder_material_with_texture(builder);
    if (!built_material) {
        return std::nullopt;
    }
    if (!populate_builder_scene(builder, built_material.value())) {
        return std::nullopt;
    }
    elf3d::Result<elf3d::Document> built_document = builder.finish();
    if (!built_document) {
        return std::nullopt;
    }
    return std::move(built_document).value();
}

[[nodiscard]] int test_builder_finish() {
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

int elf3d_model_document_test() {
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
    if (const int result = test_builder_finish(); result != 0) {
        return 500 + result;
    }
    return 0;
}
