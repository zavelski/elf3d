#include <elf3d/imgui/texture.h>

#include <imgui.h>

#include <cstdint>
#include <type_traits>

namespace elf3d::imgui {
namespace {

template <typename TextureId> TextureId to_imgui_texture_id(std::uintptr_t value) noexcept {
    if constexpr (std::is_pointer_v<TextureId>) {
        return reinterpret_cast<TextureId>(value);
    } else {
        return static_cast<TextureId>(value);
    }
}

} // namespace

Result<void> image(const NativeTextureView &texture, Float2 display_size) noexcept {
    if (texture.api != NativeGraphicsApi::opengl) {
        return Error{ErrorCode::backend_mismatch,
                     "The Dear ImGui integration requires an OpenGL native texture"};
    }
    if (!texture.is_valid()) {
        return Error{ErrorCode::texture_unavailable,
                     "The native texture view is invalid or unavailable"};
    }
    if (display_size.x <= 0.0F || display_size.y <= 0.0F) {
        return {};
    }

    const ImTextureID texture_id = to_imgui_texture_id<ImTextureID>(texture.value);
    ImGui::Image(texture_id, ImVec2{display_size.x, display_size.y}, ImVec2{0.0F, 1.0F},
                 ImVec2{1.0F, 0.0F});
    return {};
}

} // namespace elf3d::imgui
