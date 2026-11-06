#include "viewer_runtime_state.hpp"
#include "viewer_viewport.hpp"
#include <optional>
#include <utility>
namespace elf3d::viewer {
[[nodiscard]] ViewerFrameContext frame_context(ViewerAssembly& runtime) noexcept
{
    ViewerFrameContext result{runtime.shell,       runtime.rendering,     runtime.performance,
                              runtime.diagnostics, runtime.notifications, runtime.interaction,
                              runtime.hierarchy,   runtime.presentation,  runtime.pending_files};
    return result;
}

[[nodiscard]] ViewerWorkflowContext workflow_context(ViewerAssembly& runtime) noexcept
{
    ViewerWorkflowContext result{*runtime.engine,
                                 runtime.viewport,
                                 runtime.rendering,
                                 runtime.notifications,
                                 runtime.hierarchy,
                                 runtime.preferences,
                                 runtime.scene,
                                 runtime.tools,
                                 runtime.scene_workflow,
                                 runtime.save_workflow,
                                 runtime.last_write_report,
                                 runtime.performance,
                                 runtime.browser,
                                 *runtime.interaction_arbiter,
                                 runtime.viewport_interaction_owner,
                                 runtime.viewport_interaction_region,
                                 runtime.session_replacement_allowed};
    return result;
}

[[nodiscard]] bool semantic_selection_available(const SelectionTool& selection,
                                                const std::optional<EntityId>& primary) noexcept
{
    return !selection.browser_selection().empty() || primary.has_value();
}

[[nodiscard]] ViewerCapabilitySnapshot viewer_capabilities(const ViewerAssembly& runtime) noexcept
{
    ViewerCapabilitySnapshot result;
    result.scene = runtime.scene.scene->id();
    result.selected_entity = runtime.viewport->selected_entity();
    result.has_selection =
        semantic_selection_available(runtime.tools.selection(), result.selected_entity);
    result.has_spatial_selection = runtime.viewport->has_selection();
    result.scene_imported = runtime.scene.is_imported();
    result.view_available =
        runtime.shell.show_3d_view && has_nonzero_extent(runtime.rendering.view_dimensions);
    result.spatial_available =
        result.spatial_available && viewer_scene_graphics_ready(runtime.rendering, runtime.scene);
    const Result<std::optional<Bounds3>> visible =
        runtime.viewport->visible_bounds(*runtime.scene.scene);
    result.visible_content = visible && visible.value().has_value();
    result.isolating = runtime.viewport->is_isolating();
    const DistanceMeasurementSnapshot measurement = runtime.tools.measurement().snapshot(
        *runtime.scene.scene, *runtime.viewport,
        runtime.tools.active_tool() == ViewerTool::distance_measurement);
    result.measurement_incomplete =
        measurement.state == DistanceMeasurementState::awaiting_second_point;
    result.has_measurement = measurement.first_point.has_value() ||
                             measurement.second_point.has_value() ||
                             measurement.preview_point.has_value();
    return result;
}

[[nodiscard]] Result<void> initialize_viewer_engine(ViewerAssembly& runtime, Engine& engine)
{
    runtime.engine = &engine;
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport = runtime.engine->create_viewport({});
    if (!viewport) {
        return viewport.error();
    }
    runtime.viewport = std::move(viewport).value();
    elf3d::Result<SceneSession> scene = create_empty_scene(*runtime.engine);
    if (!scene) {
        return scene.error();
    }
    runtime.scene = std::move(scene).value();
    return {};
}

} // namespace elf3d::viewer
