#ifndef ELF3D_VIEWPORT_H
#define ELF3D_VIEWPORT_H

#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <memory>

namespace elf3d {

class Engine;

#if defined(_MSC_VER)
#pragma warning(push)
// Exported special members keep unique_ptr operations inside the DLL.
#pragma warning(disable : 4251)
#endif
class ELF3D_API Viewport final {
  public:
    // Destruction must occur on the owning graphics thread while a compatible
    // host OpenGL context is current. The Engine must not outlive that context.
    ~Viewport();

    Viewport(const Viewport &) = delete;
    Viewport &operator=(const Viewport &) = delete;

    Viewport(Viewport &&) noexcept;
    Viewport &operator=(Viewport &&) noexcept;

    [[nodiscard]] Extent2D extent() const noexcept;
    // Resize and render are graphics-thread operations. A zero component safely
    // releases the current render target and produces no color texture.
    [[nodiscard]] Result<void> resize(Extent2D extent);

    void set_clear_color(Color4 color) noexcept;
    [[nodiscard]] Color4 clear_color() const noexcept;

    [[nodiscard]] Result<void> render();
    // The returned non-owning handle is invalidated by resize or destruction.
    [[nodiscard]] TextureHandle color_texture() const noexcept;
    [[nodiscard]] bool framebuffer_valid() const noexcept;

  private:
    friend class Engine;

    class Impl;
    explicit Viewport(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
