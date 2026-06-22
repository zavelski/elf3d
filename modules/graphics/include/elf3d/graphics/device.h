#ifndef ELF3D_GRAPHICS_DEVICE_H
#define ELF3D_GRAPHICS_DEVICE_H

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <memory>

namespace elf3d::graphics {

class RenderTarget {
  public:
    virtual ~RenderTarget();

    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&) = delete;
    RenderTarget &operator=(RenderTarget &&) = delete;

    [[nodiscard]] virtual Extent2D extent() const noexcept = 0;
    [[nodiscard]] virtual Result<void> resize(Extent2D extent) = 0;
    [[nodiscard]] virtual Result<void> clear(Color4 color) = 0;
    [[nodiscard]] virtual TextureHandle color_texture() const noexcept = 0;
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;

  protected:
    RenderTarget() = default;
};

class Device {
  public:
    virtual ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    [[nodiscard]] virtual GraphicsBackend backend() const noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<RenderTarget>>
    create_render_target(Extent2D initial_extent) = 0;
    [[nodiscard]] virtual Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const = 0;

  protected:
    Device() = default;
};

} // namespace elf3d::graphics

#endif
