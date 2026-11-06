#include "viewer_runtime_state.hpp"

#include "viewer_browser.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <optional>
#include <variant>

namespace elf3d::viewer {
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
    bool exit_ready = true;
    runtime.exit_requested = exit_ready;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowOpenDialogCommand&) noexcept
{
    runtime.browser.request_open_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const SaveModelCommand&)
{
    if (!runtime.scene.save_target) {
        runtime.browser.request_save_modal = true;
        return {};
    }
    return execute_viewer_save_request(
        runtime, {path_to_utf8(*runtime.scene.save_target), runtime.scene.save_options, false});
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowSaveDialogCommand&) noexcept
{
    if (!runtime.scene.is_imported()) {
        return command_failed(
            Error{ErrorCode::invalid_argument, "Save As requires a loaded model"});
    }
    runtime.browser.request_save_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResetViewerLayoutCommand&) noexcept
{
    runtime.shell.reset_dock_layout = true;
    runtime.shell.layout_read_only = false;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ReloadSceneCommand&)
{
    return execute_viewer_file_request(
        runtime, SceneReplacementRequest{SceneReplacementKind::reload_model,
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

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FrameSelectionCommand&) noexcept
{
    const auto resolved = runtime.tools.selection().resolve_browser_resources(
        *runtime.scene.scene, *runtime.viewport, runtime.hierarchy.browser.index);
    if (!resolved)
        return command_result(resolved);
    return command_result(runtime.tools.selection().frame_selected(
        *runtime.scene.scene, *runtime.viewport, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const SetShadingModeCommand& command) noexcept
{
    runtime.rendering.shading_mode = command.mode;
    return {};
}
[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const SetBackfaceCullingCommand& command) noexcept
{
    runtime.rendering.backface_culling_enabled = command.enabled;
    return {};
}
[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ActivateViewerToolCommand& command) noexcept
{
    runtime.tools.activate(command.tool);
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearSelectionCommand&) noexcept
{
    runtime.tools.selection().clear(*runtime.viewport);
    return {};
}
[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const SelectAllEntitiesCommand&) noexcept
{
    return command_result(
        runtime.tools.selection().select_all(*runtime.scene.scene, *runtime.viewport));
}
[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const SetSelectionVisibilityCommand& command) noexcept
{
    auto& selection = runtime.tools.selection();
    const auto resolved = selection.resolve_browser_resources(
        *runtime.scene.scene, *runtime.viewport, runtime.hierarchy.browser.index);
    if (!resolved)
        return command_result(resolved);
    const auto changed =
        selection.set_selected_visibility(*runtime.scene.scene, *runtime.viewport, command.visible);

    return command_result(changed);
}
[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const IsolateSelectionCommand&) noexcept
{
    const auto resolved = runtime.tools.selection().resolve_browser_resources(
        *runtime.scene.scene, *runtime.viewport, runtime.hierarchy.browser.index);
    if (!resolved)
        return command_result(resolved);
    return command_result(
        runtime.tools.selection().isolate_selected(*runtime.scene.scene, *runtime.viewport));
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
    return command_result(runtime.tools.selection().select_entity(
        *runtime.scene.scene, *runtime.viewport, command.entity, command.additive));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const SetEntityVisibilityCommand& command) noexcept
{
    const Result<void> result =
        command.visible && command.scope == EntityVisibilityScope::entity_and_ancestors
            ? runtime.scene.scene->show_entity_and_ancestors(command.entity)
            : runtime.scene.scene->set_entity_local_visibility(command.entity, command.visible);

    return command_result(result);
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowAllEntitiesCommand&) noexcept
{
    const Result<void> result = runtime.scene.scene->show_all_entities();
    if (result) {
        runtime.viewport->clear_isolation();
        runtime.viewport->clear_primitive_visibility();
    }
    return command_result(result);
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const IsolateEntityCommand& command) noexcept
{
    const auto shown = runtime.scene.scene->show_entity_and_ancestors(command.entity);
    if (!shown) {
        return command_result(shown);
    }
    return command_result(runtime.viewport->isolate_entity(*runtime.scene.scene, command.entity));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ExitIsolationCommand&) noexcept
{
    runtime.viewport->clear_isolation();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResourceActionCommand& command)
{
    return command_result(apply_browser_resource_action(
        *runtime.scene.scene, *runtime.viewport, runtime.hierarchy.browser.index,
        {runtime.scene.camera, command.key, command.action}));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ViewerCommand& command)
{
    return std::visit([&runtime](const auto& value) { return execute_command(runtime, value); },
                      command);
}

Result<void> dispatch_viewer_commands(ViewerAssembly& runtime, ViewerCommandDispatcher& commands)
{
    std::optional<ViewerCommandDispatch> dispatch = commands.take_next(runtime.scene.scene->id());
    while (dispatch.has_value()) {
        const ViewerCommandCompletion completion =
            viewer_command_enabled(dispatch->command, viewer_capabilities(runtime))
                ? execute_command(runtime, dispatch->command)
                : ViewerCommandCompletion{ViewerCommandOutcomeStatus::disabled};
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
        commands.complete(*dispatch, completion);
        if (!runtime.scene.scene || !runtime.viewport) {
            return completion.error.value_or(
                Error{ErrorCode::graphics_context_unavailable, "Viewer session creation failed"});
        }
        dispatch = commands.take_next(runtime.scene.scene->id());
    }
    if (commands.enqueue_error().has_value()) {
        set_viewport_error(frame_context(runtime), *commands.enqueue_error());
    }
    return {};
}

} // namespace elf3d::viewer
