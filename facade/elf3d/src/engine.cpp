#include <elf3d/elf3d.h>

#include <elf3d/backend/opengl/device_factory.h>
#include <elf3d/core/version_data.h>
#include <elf3d/graphics/device.h>
#include <elf3d/viewport/offscreen_viewport.h>

#include <memory>
#include <utility>

namespace elf3d {

class Engine::Impl final {
  public:
    Impl() = default;

    Impl(GraphicsBackend backend, std::shared_ptr<graphics::Device> device) noexcept
        : backend(backend), device(std::move(device)) {}

    GraphicsBackend backend = GraphicsBackend::opengl;
    std::shared_ptr<graphics::Device> device;
};

class Viewport::Impl final {
  public:
    explicit Impl(std::unique_ptr<viewport::OffscreenViewport> viewport) noexcept
        : viewport(std::move(viewport)) {}

    std::unique_ptr<viewport::OffscreenViewport> viewport;
};

Version version() noexcept {
    const core::VersionData current = core::version_data();
    return Version{current.major, current.minor, current.patch};
}

const char *version_string() noexcept {
    return core::version_string();
}

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::Engine(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Engine::~Engine() = default;

Engine::Engine(Engine &&) noexcept = default;

Engine &Engine::operator=(Engine &&) noexcept = default;

Result<std::unique_ptr<Engine>> Engine::create(const EngineConfiguration &configuration) noexcept {
    try {
        Result<std::shared_ptr<graphics::Device>> device_result =
            Error{ErrorCode::invalid_argument, "The requested graphics backend is unsupported"};

        switch (configuration.graphics_backend) {
        case GraphicsBackend::opengl:
            device_result = backend::opengl::create_device(configuration.opengl);
            break;
        default:
            return Error{ErrorCode::invalid_argument,
                         "The requested graphics backend is unsupported"};
        }

        if (!device_result) {
            return device_result.error();
        }

        auto impl = std::make_unique<Impl>(configuration.graphics_backend,
                                           std::move(device_result).value());
        return std::unique_ptr<Engine>{new Engine{std::move(impl)}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Elf3D engine creation threw an exception"};
    }
}

GraphicsBackend Engine::graphics_backend() const noexcept {
    return impl_ != nullptr ? impl_->backend : GraphicsBackend::opengl;
}

bool Engine::graphics_initialized() const noexcept {
    return impl_ != nullptr && impl_->device != nullptr;
}

Result<std::unique_ptr<Viewport>> Engine::create_viewport(Extent2D initial_extent) {
    if (impl_ == nullptr || impl_->device == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport creation requires an initialized graphics backend"};
    }

    try {
        Result<std::unique_ptr<viewport::OffscreenViewport>> viewport_result =
            viewport::OffscreenViewport::create(impl_->device, initial_extent);
        if (!viewport_result) {
            return viewport_result.error();
        }

        auto viewport_impl = std::make_unique<Viewport::Impl>(std::move(viewport_result).value());
        return std::unique_ptr<Viewport>{new Viewport{std::move(viewport_impl)}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Elf3D viewport facade creation threw an exception"};
    }
}

Result<NativeTextureView> Engine::native_texture_view(TextureHandle texture) const {
    if (impl_ == nullptr || impl_->device == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Native texture access requires an initialized graphics backend"};
    }

    try {
        return impl_->device->native_texture_view(texture);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Native texture access threw an exception"};
    }
}

Viewport::Viewport(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Viewport::~Viewport() = default;

Viewport::Viewport(Viewport &&) noexcept = default;

Viewport &Viewport::operator=(Viewport &&) noexcept = default;

Extent2D Viewport::extent() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->extent() : Extent2D{};
}

Result<void> Viewport::resize(Extent2D extent) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->resize(extent);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport resize threw an exception"};
    }
}

void Viewport::set_clear_color(Color4 color) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_clear_color(color);
    }
}

Color4 Viewport::clear_color() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clear_color()
                                                          : Color4{};
}

Result<void> Viewport::render() {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->render();
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport rendering threw an exception"};
    }
}

TextureHandle Viewport::color_texture() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->color_texture()
                                                          : TextureHandle{};
}

bool Viewport::framebuffer_valid() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->framebuffer_valid();
}

} // namespace elf3d
