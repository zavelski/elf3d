#include <elf3d/core/assert.h>
#include <elf3d/imgui/texture.h>

#include "viewport_texture.h"

#include <imgui.h>
#include <imgui_impl_opengl3_loader.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <new>
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

[[nodiscard]] bool has_valid_rgba8_size(const UiTextureDescription& description) noexcept {
    constexpr std::size_t channels = 4;
    const std::size_t width = description.extent.width;
    const std::size_t height = description.extent.height;
    if (width == 0 || height == 0 || width > (std::numeric_limits<std::size_t>::max)() / channels) {
        return false;
    }
    const std::size_t row_bytes = width * channels;
    return height <= (std::numeric_limits<std::size_t>::max)() / row_bytes &&
           description.rgba8.size() == row_bytes * height;
}

[[nodiscard]] bool has_supported_gl_extent(Extent2D extent) noexcept {
    const auto maximum_extent = static_cast<std::uint32_t>((std::numeric_limits<GLsizei>::max)());
    return extent.width <= maximum_extent && extent.height <= maximum_extent;
}

} // namespace

UiTexture::~UiTexture() noexcept {
    if (texture_ != 0U) {
        glDeleteTextures(1, &texture_);
    }
}

Result<std::unique_ptr<UiTexture>>
UiTexture::create(const UiTextureDescription& description) noexcept {
    try {
        if (!has_valid_rgba8_size(description) || !has_supported_gl_extent(description.extent)) {
            return Error{ErrorCode::invalid_argument, "The UI texture RGBA8 payload is invalid"};
        }

        std::unique_ptr<UiTexture> texture = std::make_unique<UiTexture>(ConstructionKey{});
        glGenTextures(1, &texture->texture_);
        if (texture->texture_ == 0U) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "The UI texture could not be created"};
        }
        glBindTexture(GL_TEXTURE_2D, texture->texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(description.extent.width),
                     static_cast<GLsizei>(description.extent.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     description.rgba8.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        texture->extent_ = description.extent;
        return texture;
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D Dear ImGui texture allocation failed");
    } catch (...) {
        fatal_error("Elf3D Dear ImGui texture integration encountered an unexpected exception");
    }
}

bool UiTexture::is_valid() const noexcept {
    return texture_ != 0U && extent_.width != 0 && extent_.height != 0;
}

ImTextureRef UiTexture::texture_ref() const noexcept {
    return ImTextureRef{to_imgui_texture_id<ImTextureID>(texture_)};
}

Result<void> detail::draw_viewport_image(const elf3d::detail::NativeTextureView& texture,
                                         Float2 top_left_screen_position,
                                         Float2 display_size) noexcept {
    if (texture.api != elf3d::detail::NativeGraphicsApi::opengl) {
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
    ImGui::GetWindowDrawList()->AddImage(
        texture_id, ImVec2{top_left_screen_position.x, top_left_screen_position.y},
        ImVec2{top_left_screen_position.x + display_size.x,
               top_left_screen_position.y + display_size.y},
        ImVec2{0.0F, 1.0F}, ImVec2{1.0F, 0.0F});
    return {};
}

} // namespace elf3d::imgui
