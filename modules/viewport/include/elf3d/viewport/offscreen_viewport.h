#ifndef ELF3D_VIEWPORT_OFFSCREEN_VIEWPORT_H
#define ELF3D_VIEWPORT_OFFSCREEN_VIEWPORT_H

#include <elf3d/core/result.h>
#include <elf3d/graphics/device.h>

#include <memory>

namespace elf3d::viewport {

class OffscreenViewport final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<OffscreenViewport>>
    create(std::shared_ptr<graphics::Device> device, Extent2D initial_extent) noexcept;

    ~OffscreenViewport() = default;

    OffscreenViewport(const OffscreenViewport &) = delete;
    OffscreenViewport &operator=(const OffscreenViewport &) = delete;
    OffscreenViewport(OffscreenViewport &&) noexcept = default;
    OffscreenViewport &operator=(OffscreenViewport &&) noexcept = default;

    [[nodiscard]] Extent2D extent() const noexcept;
    [[nodiscard]] Result<void> resize(Extent2D extent);

    void set_clear_color(Color4 color) noexcept;
    [[nodiscard]] Color4 clear_color() const noexcept;

    [[nodiscard]] Result<void> render();
    [[nodiscard]] TextureHandle color_texture() const noexcept;
    [[nodiscard]] bool framebuffer_valid() const noexcept;

  private:
    OffscreenViewport(std::shared_ptr<graphics::Device> device,
                      std::unique_ptr<graphics::RenderTarget> render_target) noexcept;

    std::shared_ptr<graphics::Device> device_;
    std::unique_ptr<graphics::RenderTarget> render_target_;
    Color4 clear_color_{0.08F, 0.16F, 0.28F, 1.0F};
};

} // namespace elf3d::viewport

#endif
