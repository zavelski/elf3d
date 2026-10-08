#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <memory>
#include <utility>

namespace elf3d_examples {

// Retain both viewports in the Application and release them during stop.
struct TwoViewports {
    std::unique_ptr<elf3d::Viewport> first;
    std::unique_ptr<elf3d::Viewport> second;
};

[[nodiscard]] elf3d::Result<TwoViewports> create_two_viewports(elf3d::Engine& engine) noexcept
{
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

    first->set_environment_lighting({1.0F, 0.0F});
    first->set_display_transform({0.0F, elf3d::ToneMappingMode::pbr_neutral});
    second->set_environment_lighting({1.0F, 3.14159265359F});
    second->set_display_transform({-0.5F, elf3d::ToneMappingMode::pbr_neutral});

    return TwoViewports{std::move(first), std::move(second)};
}

// Call from Application::build_ui; queued references survive until frame completion.
[[nodiscard]] elf3d::Result<void> queue_two_viewports(elf3d::ApplicationUiContext& context,
                                                      const TwoViewports& viewports,
                                                      const elf3d::Scene& scene,
                                                      elf3d::EntityId camera_entity) noexcept
{
    const auto first_queued = context.queue_viewport_render(*viewports.first, scene, camera_entity);
    if (!first_queued) {
        return first_queued.error();
    }
    return context.queue_viewport_render(*viewports.second, scene, camera_entity);
}

} // namespace elf3d_examples
