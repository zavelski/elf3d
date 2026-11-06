#include "viewer_graphics.hpp"
#include "viewer_runtime_state.hpp"

#include <algorithm>
#include <cmath>

namespace elf3d::viewer {
bool viewer_scene_graphics_ready(const ViewerRenderingState& rendering,
                                 const SceneSession& scene) noexcept
{
    return scene.scene && rendering.graphics_scene == scene.scene->id() &&
           !rendering.graphics_error &&
           rendering.graphics_report.status.state == GraphicsPreparationState::ready;
}
void advance_viewer_graphics(ViewerAssembly& runtime)
{
    if (!runtime.scene.scene)
        return;
    auto& rendering = runtime.rendering;
    rendering.service_microseconds = 0;
    const auto scene = runtime.scene.scene->id();
    if (rendering.graphics_scene != scene) {
        rendering.graphics_scene = scene;
        rendering.graphics_error.reset();
        rendering.graphics_report = {};
    }
    auto budget = rendering.graphics_budget;
    const auto advanced = runtime.engine->advance_scene_graphics(*runtime.scene.scene, budget);
    if (!advanced) {
        rendering.graphics_error = advanced.error();
        const auto status = runtime.engine->scene_graphics_status(*runtime.scene.scene);
        if (status)
            rendering.graphics_report.status = status.value();
        const auto retired = runtime.engine->graphics_retirement_status();
        if (retired)
            rendering.graphics_report.retirement.status = retired.value();
        rendering.graphics_report.status.state = GraphicsPreparationState::failed;
        runtime.notifications.viewport_error = advanced.error().message();
        return;
    }
    rendering.graphics_report = advanced.value();
    const double consumed =
        advanced.value().activity.milliseconds + advanced.value().retirement.activity.milliseconds;
    rendering.service_microseconds =
        static_cast<std::int64_t>(std::max(0.0, std::floor((2.0 - consumed) * 1000.0)));
    // No retained-frame invalidation for progress alone. The replacement path
    // already invalidates its old frame; immutable ready Scenes remain cached.
}
} // namespace elf3d::viewer
