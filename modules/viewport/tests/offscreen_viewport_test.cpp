#include <elf3d/graphics/texture_handle_access.h>
#include <elf3d/viewport/offscreen_viewport.h>

#include <limits>
#include <memory>
#include <utility>

namespace {

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    explicit FakeRenderTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {
        update_handle();
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        update_handle();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4 color) override {
        last_clear_color = color;
        ++clear_count;
        return {};
    }

    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override {
        return texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return texture_handle_.is_valid();
    }

    int resize_count = 0;
    int clear_count = 0;
    elf3d::Color4 last_clear_color;

  private:
    void update_handle() noexcept {
        texture_handle_ = extent_.width != 0 && extent_.height != 0
                              ? elf3d::detail::TextureHandleAccess::create(1)
                              : elf3d::TextureHandle{};
    }

    elf3d::Extent2D extent_;
    elf3d::TextureHandle texture_handle_;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::opengl;
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D initial_extent) override {
        auto target = std::make_unique<FakeRenderTarget>(initial_extent);
        last_target = target.get();
        return std::unique_ptr<elf3d::graphics::RenderTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle texture) const override {
        if (!texture.is_valid()) {
            return elf3d::Error{elf3d::ErrorCode::texture_unavailable,
                                "Fake texture is unavailable"};
        }
        return elf3d::NativeTextureView{elf3d::NativeGraphicsApi::opengl, 1, last_target->extent()};
    }

    FakeRenderTarget *last_target = nullptr;
};

} // namespace

int main() {
    auto device = std::make_shared<FakeDevice>();
    elf3d::Result<std::unique_ptr<elf3d::viewport::OffscreenViewport>> create_result =
        elf3d::viewport::OffscreenViewport::create(device, elf3d::Extent2D{});
    if (!create_result) {
        return 1;
    }

    std::unique_ptr<elf3d::viewport::OffscreenViewport> viewport = std::move(create_result).value();
    if (viewport->framebuffer_valid() || viewport->color_texture().is_valid()) {
        return 2;
    }
    if (!viewport->render() || device->last_target->clear_count != 0) {
        return 3;
    }

    if (!viewport->resize(elf3d::Extent2D{640, 360}) ||
        !viewport->resize(elf3d::Extent2D{640, 360}) || device->last_target->resize_count != 1 ||
        !viewport->framebuffer_valid()) {
        return 4;
    }

    viewport->set_clear_color(elf3d::Color4{-1.0F, 2.0F, std::numeric_limits<float>::quiet_NaN(),
                                            std::numeric_limits<float>::infinity()});
    const elf3d::Color4 expected{0.0F, 1.0F, 0.0F, 1.0F};
    if (viewport->clear_color() != expected || !viewport->render() ||
        device->last_target->clear_count != 1 ||
        device->last_target->last_clear_color != expected) {
        return 5;
    }

    elf3d::viewport::OffscreenViewport moved{std::move(*viewport)};
    if (!moved.framebuffer_valid() || viewport->render()) {
        return 6;
    }

    if (!moved.resize(elf3d::Extent2D{0, 360}) || moved.framebuffer_valid() ||
        moved.color_texture().is_valid()) {
        return 7;
    }

    return 0;
}
