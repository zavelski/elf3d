#pragma once

#include <array>
#include <glad/gl.h>

namespace elf3d::test::native_texture {
constexpr std::array<GLenum, 6> unpack_names{GL_PIXEL_UNPACK_BUFFER_BINDING, GL_UNPACK_ALIGNMENT,
                                             GL_UNPACK_ROW_LENGTH,           GL_UNPACK_SKIP_ROWS,
                                             GL_UNPACK_SKIP_PIXELS,          GL_UNPACK_SWAP_BYTES};
struct ForeignState {
    GLint active = 0;
    GLint texture = 0;
    std::array<GLint, 6> unpack{};
    bool operator==(const ForeignState&) const = default;
};
[[nodiscard]] inline ForeignState foreign_state()
{
    ForeignState result;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &result.active);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &result.texture);
    for (std::size_t i = 0; i < unpack_names.size(); ++i)
        glGetIntegerv(unpack_names[i], &result.unpack[i]);
    return result;
}
class ForeignUnpack final {
  public:
    ForeignUnpack() noexcept : original_(foreign_state())
    {
        glActiveTexture(GL_TEXTURE5);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &unit_texture_);
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_2D, texture_);
        glGenBuffers(1, &buffer_);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer_);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, nullptr, GL_STATIC_DRAW);
        constexpr std::array<GLint, 5> values{8, 2048, 1, 3, 1};
        for (std::size_t i = 1; i < unpack_names.size(); ++i)
            glPixelStorei(unpack_names[i], values[i - 1]);
    }
    ~ForeignUnpack() noexcept
    {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(unit_texture_));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(original_.unpack[0]));
        for (std::size_t i = 1; i < unpack_names.size(); ++i)
            glPixelStorei(unpack_names[i], original_.unpack[i]);
        glDeleteBuffers(1, &buffer_);
        glDeleteTextures(1, &texture_);
        glActiveTexture(static_cast<GLenum>(original_.active));
    }

  private:
    ForeignState original_;
    GLint unit_texture_ = 0;
    GLuint texture_ = 0;
    GLuint buffer_ = 0;
};
class ReadbackTarget final {
  public:
    explicit ReadbackTarget(GLuint texture, GLenum target = GL_TEXTURE_2D, GLint mip = 0) noexcept
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_framebuffer_);
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, texture, mip);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previous_buffer_);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        for (std::size_t i = 0; i < names.size(); ++i) {
            glGetIntegerv(names[i], &previous_[i]);
            glPixelStorei(names[i], i == 0 ? 1 : 0);
        }
    }
    ~ReadbackTarget() noexcept
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer_));
        glDeleteFramebuffers(1, &framebuffer_);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previous_buffer_));
        for (std::size_t i = 0; i < names.size(); ++i)
            glPixelStorei(names[i], previous_[i]);
    }

  private:
    static constexpr std::array<GLenum, 5> names{GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH,
                                                 GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS,
                                                 GL_PACK_SWAP_BYTES};
    GLint previous_framebuffer_ = 0;
    GLint previous_buffer_ = 0;
    std::array<GLint, 5> previous_{};
    GLuint framebuffer_ = 0;
};
} // namespace elf3d::test::native_texture
