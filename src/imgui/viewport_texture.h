#ifndef ELF3D_IMGUI_VIEWPORT_TEXTURE_H
#define ELF3D_IMGUI_VIEWPORT_TEXTURE_H

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>

#include "engine_access.h"

namespace elf3d::imgui::detail {

[[nodiscard]] Result<void> draw_viewport_image(const elf3d::detail::NativeTextureView& texture,
                                               Float2 top_left_screen_position,
                                               Float2 display_size) noexcept;

} // namespace elf3d::imgui::detail

#endif
