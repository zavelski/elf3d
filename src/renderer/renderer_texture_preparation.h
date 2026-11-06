#pragma once
#include <elf3d/graphics_preparation.h>

#include <graphics/texture_preparation.h>
#include <optional>

namespace elf3d::renderer {
struct TextureWorkBudget {
    std::size_t upload_bytes = default_graphics_transfer_bytes;
    std::uint32_t driver_calls = GraphicsPreparationBudget{}.driver_calls;
    std::size_t transfer_bytes = default_graphics_transfer_bytes;
};
struct TextureWorkStep {
    graphics::TexturePreparationPhase phase = graphics::TexturePreparationPhase::object;
    std::size_t uploaded_bytes = 0;
    std::size_t allocated_bytes = 0;
    std::uint32_t driver_calls = 0;
    bool progressed = false;
    bool ready = false;
    std::size_t texture_index = 0;
    std::uint32_t completed_textures = 0;
};
struct TextureImagePreparation {
    graphics::TextureStorageDescription storage;
    std::unique_ptr<graphics::Texture2DPreparation> pending;
    std::unique_ptr<graphics::Texture2D> texture;
    std::optional<Error> failure;
    GraphicsPreparationFailure attempt;
    std::size_t allocated_bytes = 0;
    bool initialized = false;
};
// Source pixels are borrowed only during this call. No interleaved image or
// staging copy is needed: each requested rectangle is a contiguous source slice.
[[nodiscard]] Result<TextureWorkStep>
advance_texture_image(graphics::Device& device, const graphics::TextureStorageDescription& storage,
                      std::span<const std::byte> pixels, TextureImagePreparation& preparation,
                      const TextureWorkBudget& budget = {});
} // namespace elf3d::renderer
