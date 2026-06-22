#include <elf3d/viewport/offscreen_viewport.h>

#include <elf3d/math/conventions.h>

#include <memory>
#include <utility>

namespace elf3d::viewport {

Result<std::unique_ptr<OffscreenViewport>>
OffscreenViewport::create(std::shared_ptr<graphics::Device> device,
                          Extent2D initial_extent) noexcept {
    if (!device) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport creation requires an active graphics device"};
    }

    try {
        Result<std::unique_ptr<graphics::RenderTarget>> target_result =
            device->create_render_target(initial_extent);
        if (!target_result) {
            return target_result.error();
        }

        return std::unique_ptr<OffscreenViewport>{
            new OffscreenViewport{std::move(device), std::move(target_result).value()}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport creation threw an exception"};
    }
}

OffscreenViewport::OffscreenViewport(std::shared_ptr<graphics::Device> device,
                                     std::unique_ptr<graphics::RenderTarget> render_target) noexcept
    : device_(std::move(device)), render_target_(std::move(render_target)) {}

Extent2D OffscreenViewport::extent() const noexcept {
    return render_target_ != nullptr ? render_target_->extent() : Extent2D{};
}

Result<void> OffscreenViewport::resize(Extent2D extent) {
    if (render_target_ == nullptr || device_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (extent == render_target_->extent()) {
        return {};
    }
    return render_target_->resize(extent);
}

void OffscreenViewport::set_clear_color(Color4 color) noexcept {
    clear_color_ = math::clamp_color(color);
}

Color4 OffscreenViewport::clear_color() const noexcept {
    return clear_color_;
}

Result<void> OffscreenViewport::render() {
    if (render_target_ == nullptr || device_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (render_target_->extent().width == 0 || render_target_->extent().height == 0) {
        return {};
    }
    return render_target_->clear(clear_color_);
}

TextureHandle OffscreenViewport::color_texture() const noexcept {
    return render_target_ != nullptr ? render_target_->color_texture() : TextureHandle{};
}

bool OffscreenViewport::framebuffer_valid() const noexcept {
    return render_target_ != nullptr && render_target_->is_valid();
}

} // namespace elf3d::viewport
