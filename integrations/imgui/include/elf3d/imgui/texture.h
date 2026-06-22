#ifndef ELF3D_IMGUI_TEXTURE_H
#define ELF3D_IMGUI_TEXTURE_H

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

namespace elf3d::imgui {

// Draws a non-owning Elf3D OpenGL texture in the current Dear ImGui window.
// UVs are flipped vertically to account for OpenGL framebuffer orientation.
[[nodiscard]] Result<void> image(const NativeTextureView &texture, Float2 display_size) noexcept;

} // namespace elf3d::imgui

#endif
