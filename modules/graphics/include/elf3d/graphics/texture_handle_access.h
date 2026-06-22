#ifndef ELF3D_GRAPHICS_TEXTURE_HANDLE_ACCESS_H
#define ELF3D_GRAPHICS_TEXTURE_HANDLE_ACCESS_H

#include <elf3d/graphics.h>

#include <cstdint>

namespace elf3d::detail {

class TextureHandleAccess final {
  public:
    [[nodiscard]] static constexpr TextureHandle create(std::uint64_t value) noexcept {
        return TextureHandle{value};
    }

    [[nodiscard]] static constexpr std::uint64_t value(TextureHandle handle) noexcept {
        return handle.value_;
    }
};

} // namespace elf3d::detail

#endif
