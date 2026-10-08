#include "viewer_runtime_state.hpp"

#include "viewer_browser.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <optional>
#include <variant>

namespace elf3d::viewer {

[[nodiscard]] Result<void> toggle_section_plane(ViewerAssembly& runtime)
{
    elf3d::SectionPlane plane = runtime.viewport->clipping_snapshot().section_plane;
    plane.enabled = !plane.enabled;
    if (plane.enabled) {
        const elf3d::Result<std::optional<elf3d::Bounds3>> bounds =
            runtime.viewport->visible_bounds(*runtime.scene.scene);
        if (bounds && bounds.value().has_value()) {
            plane.point = bounds_center(*bounds.value());
        }
    }
    return runtime.viewport->set_section_plane(plane);
}

[[nodiscard]] Result<void> flip_section_plane(ViewerAssembly& runtime)
{
    elf3d::SectionPlane plane = runtime.viewport->clipping_snapshot().section_plane;
    plane.retained_half_space = plane.retained_half_space == elf3d::PlaneHalfSpace::positive
                                    ? elf3d::PlaneHalfSpace::negative
                                    : elf3d::PlaneHalfSpace::positive;
    return runtime.viewport->set_section_plane(plane);
}

[[nodiscard]] ViewerCommandCompletion command_failed(const Error& error) noexcept
{
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::failed, error, false};
}

[[nodiscard]] ViewerCommandCompletion command_result(const Result<void>& result) noexcept
{
    return result ? ViewerCommandCompletion{} : command_failed(result.error());
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ExitViewerCommand&) noexcept
{
    runtime.exit_requested = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowOpenDialogCommand&) noexcept
{
    runtime.browser.request_open_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowSaveDialogCommand&) noexcept
{
    runtime.browser.request_save_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResetViewerLayoutCommand&) noexcept
{
    runtime.shell.reset_dock_layout = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ReloadSceneCommand&)
{
    return execute_scene_workflow(workflow_context(runtime),
                                  SceneReplacementRequest{SceneReplacementKind::reload_model,
                                                          path_to_utf8(runtime.scene.source_path)});
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const CloseSceneCommand&)
{
    return execute_scene_workflow(
        workflow_context(runtime),
        SceneReplacementRequest{SceneReplacementKind::close_to_empty, {}});
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FitViewCommand&) noexcept
{
    return command_result(
        runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResetViewCommand&) noexcept
{
    return command_result(runtime.viewport->reset_view(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ShowViewerPanelCommand& command) noexcept
{
    if (command.panel == ViewerPanel::clipping) {
        runtime.shell.show_clipping_panel = true;
    }
    return {};
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ActivateViewerToolCommand& command) noexcept
{
    runtime.tools.activate(command.tool);
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ToggleSectionPlaneCommand&) noexcept
{
    return command_result(toggle_section_plane(runtime));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FlipSectionPlaneCommand&) noexcept
{
    return command_result(flip_section_plane(runtime));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const AddClippingBoxFromBoundsCommand&) noexcept
{
    const Result<std::uint32_t> result = runtime.tools.clipping().add_box_from_visible_bounds(
        *runtime.scene.scene, *runtime.viewport);
    return result ? ViewerCommandCompletion{} : command_failed(result.error());
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearClippingCommand&) noexcept
{
    runtime.viewport->clear_clipping();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ToggleClippingHelpersCommand&) noexcept
{
    runtime.tools.clipping().set_helpers_visible(!runtime.tools.clipping().helpers_visible());
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FitClippedContentCommand&) noexcept
{
    return command_result(
        runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearSelectionCommand&) noexcept
{
    runtime.viewport->clear_selection();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const CancelMeasurementCommand&) noexcept
{
    runtime.tools.measurement().cancel_incomplete();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearMeasurementCommand&) noexcept
{
    runtime.tools.measurement().clear();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const SelectEntityCommand& command) noexcept
{
    return command_result(
        runtime.viewport->set_selected_entity(*runtime.scene.scene, command.entity));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const SetEntityVisibilityCommand& command) noexcept
{
    const Result<void> result =
        command.visible && command.scope == EntityVisibilityScope::entity_and_ancestors
            ? runtime.scene.scene->show_entity_and_ancestors(command.entity)
            : runtime.scene.scene->set_entity_local_visibility(command.entity, command.visible);
    if (result) {
        invalidate_hierarchy_snapshot(runtime.scene);
    }
    return command_result(result);
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowAllEntitiesCommand&) noexcept
{
    const Result<void> result = runtime.scene.scene->show_all_entities();
    if (result) {
        invalidate_hierarchy_snapshot(runtime.scene);
    }
    return command_result(result);
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const IsolateEntityCommand& command) noexcept
{
    return command_result(runtime.viewport->isolate_entity(*runtime.scene.scene, command.entity));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ExitIsolationCommand&) noexcept
{
    runtime.viewport->clear_isolation();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ViewerCommand& command)
{
    return std::visit([&runtime](const auto& value) { return execute_command(runtime, value); },
                      command);
}

void dispatch_viewer_commands(ViewerAssembly& runtime, ViewerCommandDispatcher& commands)
{
    std::optional<ViewerCommandDispatch> dispatch = commands.take_next(runtime.scene.scene->id());
    while (dispatch.has_value()) {
        const ViewerCommandCompletion completion = execute_command(runtime, dispatch->command);
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
        commands.complete(*dispatch, completion);
        dispatch = commands.take_next(runtime.scene.scene->id());
    }
    if (commands.enqueue_error().has_value()) {
        set_viewport_error(frame_context(runtime), *commands.enqueue_error());
    }
}

} // namespace elf3d::viewer
