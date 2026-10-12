#include "renderer_cache.hpp"
#include "renderer_detail.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace elf3d::renderer {
namespace {

[[nodiscard]] bool has_zero_component(Extent2D extent) noexcept
{
    return extent.width == 0 || extent.height == 0;
}

[[nodiscard]] bool uses_mipmaps(graphics::TextureFilterMode filter) noexcept
{
    return filter != graphics::TextureFilterMode::nearest &&
           filter != graphics::TextureFilterMode::linear;
}

[[nodiscard]] Result<std::uint64_t>
estimated_texture_resident_bytes(Extent2D extent, graphics::TextureFilterMode min_filter)
{
    if (has_zero_component(extent)) {
        return std::uint64_t{0};
    }

    std::uint32_t width = extent.width;
    std::uint32_t height = extent.height;
    std::uint64_t bytes = 0;
    while (true) {
        const std::uint64_t pixels =
            static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        if (pixels > std::numeric_limits<std::uint64_t>::max() / 4U) {
            return Error{ErrorCode::size_overflow,
                         "Texture mip residency exceeds the statistics range"};
        }

        const std::uint64_t level_bytes = pixels * 4U;
        if (bytes > std::numeric_limits<std::uint64_t>::max() - level_bytes) {
            return Error{ErrorCode::size_overflow,
                         "Texture mip residency exceeds the statistics range"};
        }
        bytes += level_bytes;
        if (!uses_mipmaps(min_filter) || (width == 1U && height == 1U)) {
            return bytes;
        }
        width = std::max(width / 2U, 1U);
        height = std::max(height / 2U, 1U);
    }
}

[[nodiscard]] graphics::Texture2DDescription
texture_description(const scene::RuntimeTextureView& texture, bool srgb)
{
    graphics::Texture2DDescription description;
    description.extent = Extent2D{texture.width, texture.height};
    description.format =
        srgb ? graphics::TextureFormat::rgba8_srgb : graphics::TextureFormat::rgba8_unorm;
    description.pixels = texture.pixels;
    description.wrap_u = runtime_address_mode(texture.sampler.wrap_u);
    description.wrap_v = runtime_address_mode(texture.sampler.wrap_v);
    description.min_filter = runtime_filter_mode(texture.sampler.min_filter);
    description.mag_filter = runtime_filter_mode(texture.sampler.mag_filter);
    return description;
}

} // namespace

Result<std::size_t> Renderer::cached_texture(SceneId scene_id,
                                             const scene::RuntimeTextureView& texture,
                                             TextureColorSpace color_space,
                                             std::uint64_t& upload_count)
{
    if (texture.image_identity >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The texture cache identity exceeds addressable storage"};
    }
    CacheState::SceneEntry& scene_cache = cache_->scene(scene_id);
    std::vector<std::optional<CacheState::ImageEntry>>& images =
        texture.document_image ? scene_cache.document_images : scene_cache.images;
    const std::size_t image_index = static_cast<std::size_t>(texture.image_identity);
    if (image_index >= images.size()) {
        images.resize(image_index + 1U);
    }

    std::optional<CacheState::ImageEntry>& image = images[image_index];
    if (!image.has_value()) {
        image.emplace();
    }

    const auto existing =
        std::find_if(image->variants.begin(), image->variants.end(),
                     [color_space, &texture](const CacheState::TextureVariant& variant) noexcept {
                         return variant.matches(color_space, texture);
                     });
    if (existing != image->variants.end()) {
        return static_cast<std::size_t>(existing - image->variants.begin());
    }

    const auto description = texture_description(texture, color_space == TextureColorSpace::srgb);
    Result<std::unique_ptr<graphics::Texture2D>> gpu_result =
        device_->create_texture_2d(description);
    if (!gpu_result) {
        return gpu_result.error();
    }

    const Result<std::uint64_t> resident_bytes =
        estimated_texture_resident_bytes(description.extent, description.min_filter);
    if (!resident_bytes) {
        return resident_bytes.error();
    }
    image->variants.push_back(CacheState::TextureVariant{
        color_space, texture.sampler, std::move(gpu_result).value(), resident_bytes.value()});
    scene_cache.resident_texture_bytes += resident_bytes.value();
    cache_->resident_texture_bytes += resident_bytes.value();
    ++scene_cache.texture_count;
    ++cache_->texture_count;
    ++upload_count;
    return image->variants.size() - 1U;
}

} // namespace elf3d::renderer
