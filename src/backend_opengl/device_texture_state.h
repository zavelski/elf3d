#pragma once

#include "device_internal.hpp"

#include <array>

namespace elf3d::backend::opengl::device_detail {
// Three native calls: capture, bind, restore. Only the two texture targets used
// by the concrete 2D/cubemap preparation boundaries are admitted by their callers.
class TextureBinding final {
  public:
    TextureBinding(GLenum target, GLuint texture) noexcept : target_(target)
    {
        glGetIntegerv(target == GL_TEXTURE_CUBE_MAP ? GL_TEXTURE_BINDING_CUBE_MAP
                                                    : GL_TEXTURE_BINDING_2D,
                      &previous_);
        glBindTexture(target_, texture);
    }
    ~TextureBinding() noexcept
    {
        glBindTexture(target_, static_cast<GLuint>(previous_));
    }

  private:
    GLenum target_;
    GLint previous_ = 0;
};
// Eighteen native calls including restoration, independent of the source size.
class PixelUnpack final {
  public:
    PixelUnpack() noexcept
    {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer_);
        for (std::size_t i = 0; i < names.size(); ++i)
            glGetIntegerv(names[i], &values_[i]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        for (std::size_t i = 0; i < names.size(); ++i)
            glPixelStorei(names[i], i == 0 ? 1 : 0);
    }
    ~PixelUnpack() noexcept
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(buffer_));
        for (std::size_t i = 0; i < names.size(); ++i)
            glPixelStorei(names[i], values_[i]);
    }

  private:
    static constexpr std::array<GLenum, 5> names{GL_UNPACK_ALIGNMENT, GL_UNPACK_ROW_LENGTH,
                                                 GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS,
                                                 GL_UNPACK_SWAP_BYTES};
    GLint buffer_ = 0;
    std::array<GLint, 5> values_{};
};
} // namespace elf3d::backend::opengl::device_detail
