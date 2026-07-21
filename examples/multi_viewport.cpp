#include <elf3d/elf3d.h>

#include <memory>
#include <utility>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<void> render_two_viewports(elf3d::Engine& engine,
                                                       const elf3d::Scene& scene,
                                                       elf3d::EntityId camera_entity) noexcept {
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> first_result =
        engine.create_viewport({640, 480});
    if (!first_result) {
        return first_result.error();
    }
    std::unique_ptr<elf3d::Viewport> first = std::move(first_result).value();

    elf3d::Result<std::unique_ptr<elf3d::Viewport>> second_result =
        engine.create_viewport({320, 240});
    if (!second_result) {
        return second_result.error();
    }
    std::unique_ptr<elf3d::Viewport> second = std::move(second_result).value();

    const elf3d::Result<void> first_render = first->render(scene, camera_entity);
    if (!first_render) {
        return first_render.error();
    }
    const elf3d::Result<void> second_render = second->render(scene, camera_entity);
    if (!second_render) {
        return second_render.error();
    }
    return {};
}

} // namespace elf3d_examples
