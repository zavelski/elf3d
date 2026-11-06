#pragma once

#include <graphics/texture_preparation.h>

namespace elf3d::graphics {
struct TextureCubeStorageDescription {
    TextureFormat format = TextureFormat::rgba16_float;
    std::uint32_t extent = 0;
    std::uint32_t mip_count = 0;
    TextureFilterMode min_filter = TextureFilterMode::linear;
    TextureFilterMode mag_filter = TextureFilterMode::linear;
};
struct CubeUploadRequest {
    TextureUploadRequest region;
    std::uint32_t mip = 0;
    std::uint32_t face = 0;
};
// Exclusive incomplete storage. Reborrow the requested face only in advance;
// neither mip descriptions nor pixels are retained by this owner.
class TextureCubePreparation {
  public:
    virtual ~TextureCubePreparation() noexcept = default;
    [[nodiscard]] virtual Result<CubeUploadRequest>
    request(std::size_t maximum_bytes = default_graphics_transfer_bytes) const noexcept = 0;
    [[nodiscard]] virtual Result<TextureUploadStep>
    advance(std::span<const std::byte> pixels = {}) noexcept = 0;
    [[nodiscard]] virtual std::uint64_t driver_calls() const noexcept = 0;
    [[nodiscard]] virtual Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<TextureCube>> take_texture() noexcept = 0;
};
[[nodiscard]] Result<std::size_t>
cube_storage_bytes(const TextureCubeStorageDescription& storage) noexcept;
[[nodiscard]] Result<TextureCubeStorageDescription>
describe_cube_storage(const TextureCubeDescription& source) noexcept;
[[nodiscard]] TextureStorageDescription
cube_face_storage(const TextureCubeStorageDescription& storage, std::uint32_t mip) noexcept;
[[nodiscard]] Result<std::unique_ptr<TextureCube>>
complete_cube_upload(TextureCubePreparation& preparation,
                     const TextureCubeDescription& source) noexcept;
[[nodiscard]] Result<std::unique_ptr<TextureCube>>
complete_cube_upload(Device& device, const TextureCubeDescription& source) noexcept;
} // namespace elf3d::graphics
