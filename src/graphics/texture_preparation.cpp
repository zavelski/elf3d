#include "texture_preparation.h"

#include <algorithm>
#include <limits>

namespace elf3d::graphics {
Result<std::size_t> texture_pixel_bytes(TextureFormat format) noexcept
{
    switch (format) {
    case TextureFormat::rgba8_unorm:
    case TextureFormat::rgba8_srgb:
        return std::size_t{4};
    case TextureFormat::rgba16_float:
        return std::size_t{8};
    }
    return Error{ErrorCode::unsupported_texture_format, "Unsupported texture pixel format"};
}
Result<std::size_t> texture_storage_bytes(const TextureStorageDescription& storage) noexcept
{
    const auto pixel = texture_pixel_bytes(storage.format);
    if (!pixel)
        return pixel.error();
    const std::size_t width = storage.extent.width;
    const std::size_t height = storage.extent.height;
    if (width == 0 || height == 0)
        return Error{ErrorCode::gpu_texture_creation_failed, "Texture dimensions must be positive"};
    if (width > std::numeric_limits<std::size_t>::max() / pixel.value() ||
        height > std::numeric_limits<std::size_t>::max() / (width * pixel.value()))
        return Error{ErrorCode::size_overflow, "Texture pixel storage size overflows"};
    return width * height * pixel.value();
}
Result<TextureStorageDescription>
describe_texture_storage(const Texture2DDescription& source) noexcept
{
    TextureStorageDescription storage{source.extent, source.format,     source.wrap_u,
                                      source.wrap_v, source.min_filter, source.mag_filter};
    const auto bytes = texture_storage_bytes(storage);
    if (!bytes)
        return bytes.error();
    if (source.pixels.empty() && source.format == TextureFormat::rgba16_float) {
        storage.initialization = TextureInitialization::empty;
        return storage;
    }
    if (source.pixels.size() != bytes.value())
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "Texture upload does not contain tightly packed pixels"};
    return storage;
}
Result<TextureUploadRequest> texture_upload_region(const TextureStorageDescription& storage,
                                                   std::uint32_t x, std::uint32_t y,
                                                   std::size_t maximum_bytes) noexcept
{
    const auto total = texture_storage_bytes(storage);
    if (!total)
        return total.error();
    if (x >= storage.extent.width || y >= storage.extent.height)
        return Error{ErrorCode::invalid_argument, "Texture cursor is outside its storage"};
    const auto pixel = texture_pixel_bytes(storage.format).value();
    const auto capacity = std::min(maximum_bytes, maximum_texture_upload_chunk) / pixel;
    if (capacity == 0)
        return Error{ErrorCode::invalid_argument, "Texture upload requires a complete pixel"};
    const auto width = std::min<std::size_t>(storage.extent.width - x, capacity);
    const auto height = x == 0 && width == storage.extent.width
                            ? std::min<std::size_t>(storage.extent.height - y, capacity / width)
                            : 1;
    TextureUploadRequest next;
    next.phase = TexturePreparationPhase::pixels;
    next.x = x;
    next.y = y;
    // Both dimensions are within the input uint32 extent; total bytes were checked.
    next.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    next.offset_bytes = (static_cast<std::size_t>(y) * storage.extent.width + x) * pixel;
    next.bytes = width * height * pixel;
    return next;
}
Result<std::unique_ptr<Texture2D>>
complete_texture_upload(Texture2DPreparation& preparation,
                        std::span<const std::byte> pixels) noexcept
{
    for (;;) {
        if (const auto setup = preparation.advance_setup(GraphicsPreparationBudget{}.driver_calls);
            !setup)
            return setup.error();
        const auto next = preparation.request();
        if (!next)
            return next.error();
        if (next.value().phase == TexturePreparationPhase::ready)
            return preparation.take_texture();
        const auto offset = next.value().offset_bytes;
        const auto bytes = next.value().bytes;
        if (offset > pixels.size() || bytes > pixels.size() - offset)
            return Error{ErrorCode::gpu_texture_upload_failed,
                         "Texture source range is incomplete"};
        const auto advanced = preparation.advance(pixels.subspan(offset, bytes));
        if (!advanced)
            return advanced.error();
    }
}
Result<std::unique_ptr<Texture2D>>
complete_texture_upload(Device& device, const Texture2DDescription& source) noexcept
{
    const auto storage = describe_texture_storage(source);
    if (!storage)
        return storage.error();
    auto preparation = device.begin_texture_2d(storage.value());
    if (!preparation)
        return preparation.error();
    return complete_texture_upload(*preparation.value(), source.pixels);
}
} // namespace elf3d::graphics
