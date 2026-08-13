#ifndef ELF3D_IMGUI_TEXTURE_H
#define ELF3D_IMGUI_TEXTURE_H

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace elf3d::imgui {

struct UiTextureDescription {
    std::span<const std::byte> rgba8;
    Extent2D extent;
};

// Integration-owned UI texture. No native graphics value escapes this boundary.
class UiTexture final {
  private:
    struct ConstructionKey final {};

  public:
    ~UiTexture() noexcept;

    UiTexture(const UiTexture&) = delete;
    UiTexture& operator=(const UiTexture&) = delete;
    UiTexture(UiTexture&&) = delete;
    UiTexture& operator=(UiTexture&&) = delete;

    [[nodiscard]] static Result<std::unique_ptr<UiTexture>>
    create(const UiTextureDescription& description) noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] ImTextureRef texture_ref() const noexcept;

    explicit UiTexture(ConstructionKey) noexcept {}

  private:
    std::uint32_t texture_ = 0;
    Extent2D extent_;
};

} // namespace elf3d::imgui

#endif
