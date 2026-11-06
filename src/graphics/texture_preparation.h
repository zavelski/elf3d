#pragma once

#include <graphics/graphics.h>

namespace elf3d::graphics {
inline constexpr std::size_t maximum_texture_upload_chunk = 256U * 1024U * 1024U;
enum class TextureInitialization : std::uint8_t { pixels, empty };
struct TextureStorageDescription {
    Extent2D extent;
    TextureFormat format = TextureFormat::rgba8_unorm;
    TextureAddressMode wrap_u = TextureAddressMode::repeat;
    TextureAddressMode wrap_v = TextureAddressMode::repeat;
    TextureFilterMode min_filter = TextureFilterMode::linear;
    TextureFilterMode mag_filter = TextureFilterMode::linear;
    TextureInitialization initialization = TextureInitialization::pixels;
    bool operator==(const TextureStorageDescription&) const = default;
};
enum class TexturePreparationPhase : std::uint8_t {
    object,
    sampler,
    storage,
    pixels,
    mipmaps,
    validation,
    ready,
    failed,
    consumed
};
struct TextureUploadRequest {
    TexturePreparationPhase phase = TexturePreparationPhase::object;
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    Extent2D extent;
    std::size_t offset_bytes = 0;
    std::size_t bytes = 0;
    // Includes owning-context validation and capture/restoration of touched state.
    std::uint32_t driver_call_limit = 0;
};
struct TextureUploadStep {
    std::size_t uploaded_bytes = 0;
    std::uint32_t driver_calls = 0;
    bool ready = false;
    std::size_t allocated_bytes = 0;
};
// Context-thread owner of incomplete GPU storage and scalar progress. No pixels,
// source views or callbacks are retained. Payload borrows expire in advance.
class Texture2DPreparation {
  public:
    virtual ~Texture2DPreparation() noexcept = default;
    [[nodiscard]] virtual Result<TextureUploadRequest>
    request(std::size_t maximum_bytes = default_graphics_transfer_bytes) const noexcept = 0;
    [[nodiscard]] virtual Result<TextureUploadStep>
    advance(std::span<const std::byte> payload = {}) noexcept = 0;
    // Optional setup group; zero calls means the caller should use scalar steps.
    [[nodiscard]] virtual Result<TextureUploadStep> advance_setup(std::uint32_t) noexcept
    {
        return TextureUploadStep{};
    }
    [[nodiscard]] virtual std::uint64_t driver_calls() const noexcept = 0;
    [[nodiscard]] virtual Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<Texture2D>> take_texture() noexcept = 0;
};
[[nodiscard]] Result<std::size_t> texture_pixel_bytes(TextureFormat format) noexcept;
[[nodiscard]] Result<std::size_t>
texture_storage_bytes(const TextureStorageDescription& storage) noexcept;
[[nodiscard]] Result<TextureStorageDescription>
describe_texture_storage(const Texture2DDescription& source) noexcept;
[[nodiscard]] Result<TextureUploadRequest>
texture_upload_region(const TextureStorageDescription& storage, std::uint32_t x, std::uint32_t y,
                      std::size_t maximum_bytes) noexcept;
// The same completion driver serves the Device and backend-internal target path.
[[nodiscard]] Result<std::unique_ptr<Texture2D>>
complete_texture_upload(Texture2DPreparation& preparation,
                        std::span<const std::byte> pixels) noexcept;
[[nodiscard]] Result<std::unique_ptr<Texture2D>>
complete_texture_upload(Device& device, const Texture2DDescription& source) noexcept;
} // namespace elf3d::graphics
