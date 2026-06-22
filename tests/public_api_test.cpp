#include <elf3d/elf3d.h>

#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

int main() {
    static_assert(std::is_standard_layout_v<elf3d::Float2>);
    static_assert(std::is_standard_layout_v<elf3d::Color4>);
    static_assert(std::is_standard_layout_v<elf3d::Extent2D>);
    static_assert(std::is_move_constructible_v<elf3d::Viewport>);
    static_assert(std::is_move_assignable_v<elf3d::Viewport>);
    static_assert(!std::is_copy_constructible_v<elf3d::Viewport>);

    const elf3d::Version current = elf3d::version();
    if (current.major != 0 || current.minor != 1 || current.patch != 0) {
        return 1;
    }
    if (std::strcmp(elf3d::version_string(), "0.1.0") != 0) {
        return 2;
    }

    elf3d::Engine engine;
    elf3d::Engine moved_engine{std::move(engine)};
    elf3d::Engine assigned_engine;
    assigned_engine = std::move(moved_engine);

    const elf3d::Float2 position{4.0F, 8.0F};
    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 1.0F};
    const elf3d::Extent2D extent{800, 600};
    if (position != elf3d::Float2{4.0F, 8.0F} || color != elf3d::Color4{0.1F, 0.2F, 0.3F, 1.0F} ||
        extent != elf3d::Extent2D{800, 600}) {
        return 3;
    }

    const elf3d::TextureHandle texture;
    const elf3d::NativeTextureView native_view;
    if (texture.is_valid() || native_view.is_valid()) {
        return 4;
    }

    const elf3d::EngineConfiguration missing_loader;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> create_result =
        elf3d::Engine::create(missing_loader);
    if (create_result ||
        create_result.error().code() != elf3d::ErrorCode::missing_graphics_procedure_loader) {
        return 5;
    }

    return 0;
}
