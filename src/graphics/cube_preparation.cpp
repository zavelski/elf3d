#include "cube_preparation.h"

#include <algorithm>
#include <limits>

namespace elf3d::graphics {
TextureStorageDescription cube_face_storage(const TextureCubeStorageDescription& storage,
                                            std::uint32_t mip) noexcept
{
    TextureStorageDescription face;
    const auto extent = mip < 32 ? std::max(storage.extent >> mip, 1U) : 1U;
    face.extent = {extent, extent};
    face.format = storage.format;
    return face;
}
Result<std::size_t> cube_storage_bytes(const TextureCubeStorageDescription& storage) noexcept
{
    if (storage.format != TextureFormat::rgba16_float)
        return Error{ErrorCode::unsupported_texture_format, "Cubemaps require RGBA16F storage"};
    if (storage.extent == 0 || storage.mip_count == 0 || storage.mip_count > 32)
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Cubemap dimensions/mip count are invalid"};
    std::size_t bytes = 0;
    for (std::uint32_t mip = 0; mip < storage.mip_count; ++mip) {
        if (mip != 0 && (storage.extent >> (mip - 1)) == 1)
            return Error{ErrorCode::gpu_texture_creation_failed, "Cubemap has excess mip levels"};
        const auto face = texture_storage_bytes(cube_face_storage(storage, mip));
        if (!face)
            return face.error();
        if (face.value() > (std::numeric_limits<std::size_t>::max() - bytes) / 6)
            return Error{ErrorCode::size_overflow, "Cubemap storage size overflows"};
        bytes += face.value() * 6;
    }
    return bytes;
}
Result<TextureCubeStorageDescription>
describe_cube_storage(const TextureCubeDescription& source) noexcept
{
    if (source.mips.empty() || source.mips.size() > 32)
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Cubemap requires a bounded mip chain"};
    const TextureCubeStorageDescription storage{source.format, source.mips.front().extent,
                                                static_cast<std::uint32_t>(source.mips.size()),
                                                source.min_filter, source.mag_filter};
    if (const auto bytes = cube_storage_bytes(storage); !bytes)
        return bytes.error();
    for (std::uint32_t mip = 0; mip < storage.mip_count; ++mip) {
        const auto face = cube_face_storage(storage, mip);
        const auto bytes = texture_storage_bytes(face).value();
        if (source.mips[mip].extent != face.extent.width)
            return Error{ErrorCode::gpu_texture_creation_failed,
                         "Cubemap mip dimensions are inconsistent"};
        for (const auto pixels : source.mips[mip].faces) {
            if (pixels.size() != bytes)
                return Error{ErrorCode::gpu_texture_upload_failed,
                             "Cubemap requires six tightly packed faces per mip"};
        }
    }
    return storage;
}
Result<std::unique_ptr<TextureCube>>
complete_cube_upload(TextureCubePreparation& preparation,
                     const TextureCubeDescription& source) noexcept
{
    for (;;) {
        const auto next = preparation.request();
        if (!next)
            return next.error();
        const auto& region = next.value().region;
        if (region.phase == TexturePreparationPhase::ready)
            return preparation.take_texture();
        std::span<const std::byte> payload;
        if (region.bytes != 0) {
            if (next.value().mip >= source.mips.size() || next.value().face >= 6)
                return Error{ErrorCode::invalid_argument,
                             "Cubemap request exceeds the source mip chain"};
            const auto pixels = source.mips[next.value().mip].faces[next.value().face];
            if (region.offset_bytes > pixels.size() ||
                region.bytes > pixels.size() - region.offset_bytes)
                return Error{ErrorCode::gpu_texture_upload_failed,
                             "Cubemap source face is incomplete"};
            payload = pixels.subspan(region.offset_bytes, region.bytes);
        }
        if (const auto step = preparation.advance(payload); !step)
            return step.error();
    }
}
Result<std::unique_ptr<TextureCube>>
complete_cube_upload(Device& device, const TextureCubeDescription& source) noexcept
{
    const auto storage = describe_cube_storage(source);
    if (!storage)
        return storage.error();
    auto prepared = device.begin_texture_cube(storage.value());
    if (!prepared)
        return prepared.error();
    return complete_cube_upload(*prepared.value(), source);
}
} // namespace elf3d::graphics
