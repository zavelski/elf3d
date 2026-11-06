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

Renderer::TextureColorSpace Renderer::texture_color_space(std::size_t slot) noexcept
{
    return slot == 0 || slot == 4 || slot == 6 ? TextureColorSpace::srgb
                                               : TextureColorSpace::linear;
}
Result<TextureWorkStep> Renderer::CacheState::TextureVariant::prepare_step(
    graphics::Device& device, const graphics::TextureStorageDescription& storage,
    std::span<const std::byte> pixels, const TextureWorkBudget& budget)
{
    TextureWorkStep ready;
    ready.ready = static_cast<bool>(texture);
    if (ready.ready)
        return ready;
    if (!preparation)
        preparation = std::make_unique<TextureImagePreparation>();
    auto step = advance_texture_image(device, storage, pixels, *preparation, budget);
    if (!step)
        return step.error();
    accounted_bytes += step.value().allocated_bytes;
    if (!step.value().ready)
        return step;
    texture = std::move(preparation->texture);
    preparation.reset();
    accounted_bytes = resident_bytes;
    step.value().completed_textures = 1;
    return step;
}
Result<TextureWorkStep> Renderer::prepare_texture_step(SceneId scene_id,
                                                       const scene::RuntimeTextureView& texture,
                                                       TextureColorSpace color_space,
                                                       const TextureWorkBudget& budget)
{
    if (texture.image_identity >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The texture cache identity exceeds addressable storage"};
    }
    CacheState::SceneEntry& scene_cache = cache_->scene(scene_id);
    auto& images = texture.document_image ? scene_cache.document_images : scene_cache.images;
    const std::size_t image_index = static_cast<std::size_t>(texture.image_identity);
    auto& image = images[image_index];
    const auto existing =
        std::find_if(image.variants.begin(), image.variants.end(),
                     [color_space, &texture](const CacheState::TextureVariant& variant) noexcept {
                         return variant.matches(color_space, texture);
                     });
    const auto description = texture_description(texture, color_space == TextureColorSpace::srgb);
    const auto storage = graphics::describe_texture_storage(description);
    if (!storage)
        return storage.error();
    const Result<std::uint64_t> resident_bytes =
        estimated_texture_resident_bytes(description.extent, description.min_filter);
    if (!resident_bytes) {
        return resident_bytes.error();
    }
    const auto index = static_cast<std::size_t>(existing - image.variants.begin());
    if (existing == image.variants.end()) {
        CacheState::TextureVariant variant;
        variant.color_space = color_space;
        variant.sampler = texture.sampler;
        variant.resident_bytes = resident_bytes.value();
        variant.extent = description.extent;
        image.variants.push_back(std::move(variant));
        ++scene_cache.texture_entries;
    }
    auto& variant = image.variants[index];
    if (!variant.texture && !retirement_status().empty())
        return TextureWorkStep{};
    const auto accounted_before = variant.accounted_bytes;
    auto step = variant.prepare_step(*device_, storage.value(), texture.pixels, budget);
    if (!step) {
        ELF3D_ASSERT(variant.preparation);
        cache_->preparation_attempt = variant.preparation->attempt;
        return step.error();
    }
    scene_cache.estimated_texture_bytes += variant.accounted_bytes - accounted_before;
    step.value().texture_index = index;
    if (step.value().completed_textures == 0)
        return step;
    scene_cache.resident_texture_bytes += variant.resident_bytes;
    cache_->resident_texture_bytes += variant.resident_bytes;
    ++scene_cache.texture_count;
    ++cache_->texture_count;
    return step;
}
Result<std::size_t> Renderer::cached_texture(SceneId scene_id,
                                             const scene::RuntimeTextureView& texture,
                                             TextureColorSpace color_space,
                                             std::uint64_t& upload_count)
{
    for (;;) {
        const auto step = prepare_texture_step(scene_id, texture, color_space, {});
        if (!step)
            return step.error();
        upload_count += step.value().completed_textures;
        if (step.value().ready)
            return step.value().texture_index;
        if (!retirement_status().empty()) {
            if (const auto retired = complete_retirement(); !retired)
                return retired.error();
            continue;
        }
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous texture preparation could not advance"};
    }
}

} // namespace elf3d::renderer
