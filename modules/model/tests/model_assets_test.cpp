#include <elf3d/model.h>
#include <elf3d/model/detail/document_handle_access.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>

#include "model_document_test_support.h"

using elf3d::model::tests::single_pixel;

namespace {

struct SampleAssets {
    elf3d::Document document;
    elf3d::ImageId image;
    elf3d::SamplerId sampler;
    elf3d::TextureId texture;
    elf3d::MaterialId material;
    elf3d::SamplerDescription sampler_description;
    elf3d::ModelMaterialDescription material_description;
};

} // namespace

[[nodiscard]] std::optional<SampleAssets> create_sample_assets()
{
    SampleAssets sample;
    const std::array<std::byte, 4> pixel = single_pixel();
    constexpr std::array<std::byte, 8> png_source{std::byte{0x89}, std::byte{0x50}, std::byte{0x4e},
                                                  std::byte{0x47}, std::byte{0x0d}, std::byte{0x0a},
                                                  std::byte{0x1a}, std::byte{0x0a}};
    const elf3d::Result<elf3d::ImageId> image = sample.document.create_image(
        {1, 1, elf3d::PixelFormat::rgba8_unorm, pixel, elf3d::ModelImageMimeType::png, png_source});
    if (!image) {
        return std::nullopt;
    }

    sample.sampler_description = elf3d::SamplerDescription{
        elf3d::TextureWrap::clamp_to_edge, elf3d::TextureWrap::mirrored_repeat,
        elf3d::TextureFilter::linear_mipmap_linear, elf3d::TextureFilter::nearest};
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

[[nodiscard]] bool sample_image_view_is_valid(const SampleAssets& sample)
{
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

[[nodiscard]] bool sample_sampler_texture_material_views_are_valid(const SampleAssets& sample)
{
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

[[nodiscard]] bool sample_asset_statistics_are_valid(const SampleAssets& sample)
{
    const elf3d::DocumentStatistics expected_statistics{0, 0, 0, 0, 0, 0, 0, 1, 1,
                                                        1, 1, 0, 4, 1, 0, 1, 0, 0};
    if (sample.document.statistics() != expected_statistics) {
        return false;
    }
    return !elf3d::validate_document(sample.document.view()).has_errors();
}

[[nodiscard]] bool sample_stale_texture_is_rejected(const SampleAssets& sample)
{
    const elf3d::TextureId stale_texture =
        elf3d::model::detail::DocumentHandleAccess::create_texture(
            elf3d::model::detail::DocumentHandleAccess::document_token(sample.texture),
            sample.texture.debug_value() + 1U);
    return sample.document.texture(stale_texture).error().code() ==
           elf3d::ErrorCode::invalid_texture_id;
}

[[nodiscard]] int test_model_asset_views_and_statistics()
{
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

[[nodiscard]] int test_model_image_rejections()
{
    elf3d::Document document;
    const std::array<std::byte, 4> pixel = single_pixel();
    const std::array<std::byte, 1> short_pixel{std::byte{0}};

    if (document.create_image({0, 1, elf3d::PixelFormat::rgba8_unorm, pixel}).error().code() !=
        elf3d::ErrorCode::zero_image_dimensions) {
        return 1;
    }
    if (document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, short_pixel})
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
            .create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel,
                           elf3d::ModelImageMimeType::none, jpeg_source})
            .error()
            .code() != elf3d::ErrorCode::invalid_argument) {
        return 4;
    }
    if (document
            .create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel,
                           elf3d::ModelImageMimeType::png, jpeg_source})
            .error()
            .code() != elf3d::ErrorCode::invalid_argument) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int test_model_sampler_rejections()
{
    elf3d::Document document;
    elf3d::SamplerDescription invalid_sampler;
    invalid_sampler.mag_filter = elf3d::TextureFilter::linear_mipmap_linear;
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

[[nodiscard]] std::optional<TextureRejectionInputs> create_texture_rejection_inputs()
{
    TextureRejectionInputs inputs;
    const std::array<std::byte, 4> pixel = single_pixel();
    const elf3d::Result<elf3d::ImageId> image =
        inputs.document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
    const elf3d::Result<elf3d::SamplerId> sampler = inputs.document.create_sampler({});
    const elf3d::Result<elf3d::ImageId> foreign_image =
        inputs.foreign_document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
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

[[nodiscard]] int test_model_texture_rejections()
{
    std::optional<TextureRejectionInputs> inputs = create_texture_rejection_inputs();
    if (!inputs.has_value()) {
        return 1;
    }
    if (inputs->document.create_texture({inputs->foreign_image, inputs->sampler}).error().code() !=
        elf3d::ErrorCode::invalid_image_id) {
        return 2;
    }
    if (inputs->document.create_texture({inputs->image, inputs->foreign_sampler}).error().code() !=
        elf3d::ErrorCode::invalid_sampler_id) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_model_material_rejections()
{
    std::optional<TextureRejectionInputs> inputs = create_texture_rejection_inputs();
    if (!inputs.has_value()) {
        return 1;
    }
    elf3d::ModelMaterialDescription material_with_foreign_texture;
    material_with_foreign_texture.base_color_texture = inputs->foreign_texture;
    if (inputs->document.create_material(material_with_foreign_texture).error().code() !=
        elf3d::ErrorCode::invalid_texture_id) {
        return 2;
    }
    elf3d::ModelMaterialDescription invalid_mapping;
    invalid_mapping.base_color_texture_mapping.texcoord_set =
        elf3d::maximum_texture_coordinate_sets;
    if (inputs->document.create_material(invalid_mapping).error().code() !=
        elf3d::ErrorCode::invalid_material_description) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_model_asset_rejections()
{
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
