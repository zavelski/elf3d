#include <elf3d/internal/backend_opengl.h>

#include <elf3d/graphics.h>
#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
namespace elf3d::backend::opengl::device_detail {
namespace {
class PixelPackGuard final {
  public:
    PixelPackGuard() noexcept
    {
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer_);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
        for (std::size_t index = 0; index < names_.size(); ++index) {
            glGetIntegerv(names_[index], &values_[index]);
            glPixelStorei(names_[index], index == 0 ? 1 : 0);
        }
        // PBO binding and pack offsets must never reinterpret the CPU span.
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    ~PixelPackGuard() noexcept
    {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(buffer_));
        for (std::size_t index = 0; index < names_.size(); ++index) {
            glPixelStorei(names_[index], values_[index]);
        }
    }
    PixelPackGuard(const PixelPackGuard&) = delete;
    PixelPackGuard& operator=(const PixelPackGuard&) = delete;

  private:
    static constexpr std::array<GLenum, 6> names_{GL_PACK_ALIGNMENT,    GL_PACK_ROW_LENGTH,
                                                  GL_PACK_SKIP_ROWS,    GL_PACK_SKIP_PIXELS,
                                                  GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_IMAGES};
    std::array<GLint, 6> values_{};
    GLint buffer_ = 0;
    GLint texture_ = 0;
};
} // namespace

Result<void> read_display_pixels(GLuint texture, Extent2D extent,
                                 std::span<std::uint8_t> pixels) noexcept
{
    if (extent.width > std::numeric_limits<std::size_t>::max() / 4U / extent.height) {
        return Error{ErrorCode::size_overflow, "Viewport image byte count overflows"};
    }
    const std::size_t row_bytes = static_cast<std::size_t>(extent.width) * 4U;
    if (pixels.size() != row_bytes * extent.height) {
        return Error{ErrorCode::invalid_argument, "RGBA8 storage must match the viewport extent"};
    }
    const PixelPackGuard guard;
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::gpu_texture_readback_failed, "OpenGL viewport readback failed"};
    }
    for (std::size_t row = 0; row < extent.height / 2U; ++row) {
        const std::size_t opposite = extent.height - 1U - row;
        for (std::size_t byte = 0; byte < row_bytes; ++byte) {
            std::swap(pixels[row * row_bytes + byte], pixels[opposite * row_bytes + byte]);
        }
    }
    return {};
}
} // namespace elf3d::backend::opengl::device_detail
