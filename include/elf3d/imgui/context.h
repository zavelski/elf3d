#ifndef ELF3D_IMGUI_CONTEXT_H
#define ELF3D_IMGUI_CONTEXT_H

#include <string_view>

struct ImFont;

namespace elf3d::imgui {

// Loads a presentation font into the current integration-owned context. The
// returned pointer is borrowed until that context is destroyed.
[[nodiscard]] ImFont* load_font(std::string_view path_utf8, float logical_size_pixels,
                                float dpi_scale) noexcept;

} // namespace elf3d::imgui

#endif
