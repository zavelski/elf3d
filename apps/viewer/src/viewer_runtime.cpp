#include "viewer_application.hpp"

#include "viewer_assets.hpp"
#include "viewer_browser.hpp"
#include "viewer_chrome.hpp"
#include "viewer_components.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"
#include "viewer_workflow_execution.hpp"

#include <elf3d/imgui/context.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace elf3d::viewer {
namespace {

void retain_frame_sample(ViewerFrameContext& state, const ViewerFrameSample& sample) {
    ++state.performance.captured_frame_count;
    state.performance.frame_samples.push_back(sample);
    const std::size_t maximum_samples = state.performance.capture_csv ? 10000U : 600U;
    if (state.performance.frame_samples.size() > maximum_samples) {
        state.performance.frame_samples.erase(
            state.performance.frame_samples.begin(),
            state.performance.frame_samples.begin() +
                static_cast<std::ptrdiff_t>(state.performance.frame_samples.size() -
                                            maximum_samples));
    }
}

} // namespace

[[noreturn]] void fatal_viewer_allocation_failure() noexcept {
    fatal_error("Elf3D viewer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_viewer_exception() noexcept {
    fatal_error("Elf3D viewer encountered an unexpected exception");
}

struct ViewerAssembly {
    elf3d::Engine* engine = nullptr;
    std::unique_ptr<elf3d::Viewport> viewport;
    SceneSession scene;
    ToolCoordinator tools;
    ViewerShellState shell;
    ViewerRenderingState rendering;
    ViewerPerformanceState performance;
    ViewerGraphicsDiagnosticsState diagnostics;
    ViewerNotificationState notifications;
    ViewerInteractionFrameState interaction;
    SceneHierarchyComponentState hierarchy;
    ViewerPresentationResources presentation;
    PendingFileInputState pending_files;
    FileBrowserState browser;
    ViewerPreferencesState preferences;
    SceneReplacementWorkflow scene_workflow;
    ModelSaveWorkflow save_workflow;
    ExternalEditorWorkflow external_editor_workflow;
    ToolbarIcons toolbar_icons;
    InteractionOwnerId viewport_interaction_owner;
    InteractionRegionId viewport_interaction_region;
    bool exit_requested = false;
};

[[nodiscard]] ViewerFrameContext frame_context(ViewerAssembly& runtime) noexcept {
    return ViewerFrameContext{runtime.shell,       runtime.rendering,     runtime.performance,
                              runtime.diagnostics, runtime.notifications, runtime.interaction,
                              runtime.hierarchy,   runtime.presentation,  runtime.pending_files};
}

[[nodiscard]] ViewerWorkflowContext workflow_context(ViewerAssembly& runtime) noexcept {
    return ViewerWorkflowContext{*runtime.engine,       *runtime.viewport, runtime.rendering,
                                 runtime.notifications, runtime.hierarchy, runtime.preferences,
                                 runtime.scene,         runtime.tools,     runtime.scene_workflow,
                                 runtime.save_workflow};
}

[[nodiscard]] ViewerCapabilitySnapshot viewer_capabilities(const ViewerAssembly& runtime) noexcept {
    ViewerCapabilitySnapshot result;
    result.scene = runtime.scene.scene->id();
    result.selected_entity = runtime.viewport->selected_entity();
    result.scene_imported = runtime.scene.is_imported();
    result.view_available =
        runtime.shell.show_3d_view && has_nonzero_extent(runtime.rendering.view_dimensions);
    const Result<std::optional<Bounds3>> visible =
        runtime.viewport->visible_bounds(*runtime.scene.scene);
    result.visible_content = visible && visible.value().has_value();
    const Result<std::optional<Bounds3>> unclipped =
        runtime.viewport->unclipped_visible_bounds(*runtime.scene.scene);
    result.unclipped_visible_content = unclipped && unclipped.value().has_value();
    const ClippingSnapshot clipping = runtime.viewport->clipping_snapshot();
    result.section_plane_enabled = clipping.section_plane.enabled;
    result.has_clipping = clipping.section_plane.enabled || clipping.box_count != 0;
    result.can_add_clipping_box = clipping.box_count < maximum_clipping_boxes;
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

[[nodiscard]] Result<void> initialize_viewer_engine(ViewerAssembly& runtime, Engine& engine) {
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

void initialize_viewer_presentation(ViewerAssembly& runtime,
                                    const std::filesystem::path& asset_root, float dpi_scale) {
    const std::string font_path = path_to_utf8(asset_root / "font" / "DroidSans.ttf");
    runtime.toolbar_icons = load_toolbar_icons(asset_root);
    runtime.presentation.main_font =
        elf3d::imgui::load_font(font_path, viewer_ui_font_size_pixels, dpi_scale);
    ImGui::GetIO().FontDefault = runtime.presentation.main_font;
    runtime.presentation.panel_title_font =
        elf3d::imgui::load_font(font_path, panel_title_font_size_pixels, dpi_scale);
    runtime.presentation.panel_content_font =
        elf3d::imgui::load_font(font_path, panel_content_font_size_pixels, dpi_scale);
}

void capture_context_diagnostics(const GraphicsContextSnapshot& graphics,
                                 ViewerGraphicsDiagnosticsState& diagnostics) {
    diagnostics.gl_vendor = graphics.vendor_name;
    diagnostics.gl_renderer = graphics.device_name;
    diagnostics.gl_version = graphics.api_version;
    diagnostics.glsl_version_report = graphics.shading_language_version;
    diagnostics.default_red_bits = graphics.red_bits;
    diagnostics.default_green_bits = graphics.green_bits;
    diagnostics.default_blue_bits = graphics.blue_bits;
    diagnostics.default_alpha_bits = graphics.alpha_bits;
    diagnostics.default_depth_bits = graphics.depth_bits;
    diagnostics.default_stencil_bits = graphics.stencil_bits;
    diagnostics.default_samples = graphics.samples;
    diagnostics.default_srgb_capable = graphics.default_framebuffer_srgb ? 1 : 0;
    diagnostics.maximum_texture_size = graphics.maximum_texture_extent;
}

void collect_save_shortcut(const InputSnapshot& input, const ViewerCapabilitySnapshot& capabilities,
                           ViewerCommandDispatcher& commands) {
    if (capabilities.scene_imported && !navigation_blocked_by_modal() && input.modifiers.control &&
        input.modifiers.shift && input.key(InputKey::s).pressed && !input.text_input_owned) {
        commands.emit(ShowSaveDialogCommand{});
    }
}

void collect_camera_shortcuts(const InputSnapshot& input,
                              const ViewerCapabilitySnapshot& capabilities,
                              ViewerCommandDispatcher& commands) {
    if (!capabilities.view_available || !capabilities.visible_content || input.text_input_owned ||
        navigation_blocked_by_modal()) {
        return;
    }
    if (input.key(InputKey::f).pressed) {
        commands.emit(FitViewCommand{});
    }
    if (input.key(InputKey::home).pressed) {
        commands.emit(ResetViewCommand{});
    }
}

[[nodiscard]] bool tool_shortcuts_available(const ViewerAssembly& runtime,
                                            const InputSnapshot& input) noexcept {
    return runtime.shell.show_3d_view && has_nonzero_extent(runtime.rendering.view_dimensions) &&
           !input.text_input_owned && !navigation_blocked_by_modal();
}

[[nodiscard]] bool selection_shortcut_pressed(const InputSnapshot& input) noexcept {
    return !input.button(InputButton::left).down && !input.button(InputButton::right).down &&
           !input.modifiers.control && !input.modifiers.shift && input.key(InputKey::s).pressed;
}

void collect_tool_shortcuts(ViewerAssembly& runtime, const InputSnapshot& input,
                            const ViewerCapabilitySnapshot& capabilities,
                            ViewerCommandDispatcher& commands) {
    if (!capabilities.view_available || !tool_shortcuts_available(runtime, input)) {
        return;
    }
    if (selection_shortcut_pressed(input)) {
        commands.emit(ActivateViewerToolCommand{ViewerTool::selection});
    }
    if (input.key(InputKey::m).pressed) {
        commands.emit(ActivateViewerToolCommand{ViewerTool::distance_measurement});
    }
    if (input.key(InputKey::delete_key).pressed) {
        commands.emit(ClearMeasurementCommand{});
    }
}

void collect_escape_shortcut(ViewerAssembly& runtime, const InputSnapshot& input,
                             const ViewerCapabilitySnapshot& capabilities,
                             ViewerCommandDispatcher& commands) {
    if (!runtime.shell.show_3d_view || input.text_input_owned || navigation_blocked_by_modal() ||
        !input.key(InputKey::escape).pressed) {
        return;
    }
    if (capabilities.measurement_incomplete) {
        commands.emit(CancelMeasurementCommand{});
    } else {
        commands.emit(ClearSelectionCommand{});
    }
}

void collect_viewer_shortcuts(ViewerAssembly& runtime, const InputSnapshot& input,
                              const ViewerCapabilitySnapshot& capabilities,
                              ViewerCommandDispatcher& commands) {
    collect_save_shortcut(input, capabilities, commands);
    collect_camera_shortcuts(input, capabilities, commands);
    collect_tool_shortcuts(runtime, input, capabilities, commands);
    collect_escape_shortcut(runtime, input, capabilities, commands);
}

[[nodiscard]] Result<void> toggle_section_plane(ViewerAssembly& runtime) {
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

[[nodiscard]] Result<void> flip_section_plane(ViewerAssembly& runtime) {
    elf3d::SectionPlane plane = runtime.viewport->clipping_snapshot().section_plane;
    plane.retained_half_space = plane.retained_half_space == elf3d::PlaneHalfSpace::positive
                                    ? elf3d::PlaneHalfSpace::negative
                                    : elf3d::PlaneHalfSpace::positive;
    return runtime.viewport->set_section_plane(plane);
}

[[nodiscard]] ViewerCommandCompletion command_failed(const Error& error) noexcept {
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::failed, error, false};
}

[[nodiscard]] ViewerCommandCompletion command_result(const Result<void>& result) noexcept {
    return result ? ViewerCommandCompletion{} : command_failed(result.error());
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ExitViewerCommand&) noexcept {
    runtime.exit_requested = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowOpenDialogCommand&) noexcept {
    runtime.browser.request_open_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ShowSaveDialogCommand&) noexcept {
    runtime.browser.request_save_modal = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResetViewerLayoutCommand&) noexcept {
    runtime.shell.reset_dock_layout = true;
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ReloadSceneCommand&) {
    return execute_scene_workflow(workflow_context(runtime),
                                  SceneReplacementRequest{SceneReplacementKind::reload_model,
                                                          path_to_utf8(runtime.scene.source_path)});
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const CloseSceneCommand&) {
    return execute_scene_workflow(
        workflow_context(runtime),
        SceneReplacementRequest{SceneReplacementKind::close_to_empty, {}});
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FitViewCommand&) noexcept {
    return command_result(
        runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ResetViewCommand&) noexcept {
    return command_result(runtime.viewport->reset_view(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ShowViewerPanelCommand& command) noexcept {
    if (command.panel == ViewerPanel::clipping) {
        runtime.shell.show_clipping_panel = true;
    }
    return {};
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ActivateViewerToolCommand& command) noexcept {
    runtime.tools.activate(command.tool);
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ToggleSectionPlaneCommand&) noexcept {
    return command_result(toggle_section_plane(runtime));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FlipSectionPlaneCommand&) noexcept {
    return command_result(flip_section_plane(runtime));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const AddClippingBoxFromBoundsCommand&) noexcept {
    const Result<std::uint32_t> result = runtime.tools.clipping().add_box_from_visible_bounds(
        *runtime.scene.scene, *runtime.viewport);
    return result ? ViewerCommandCompletion{} : command_failed(result.error());
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearClippingCommand&) noexcept {
    runtime.viewport->clear_clipping();
    return {};
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const ToggleClippingHelpersCommand&) noexcept {
    runtime.tools.clipping().set_helpers_visible(!runtime.tools.clipping().helpers_visible());
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const FitClippedContentCommand&) noexcept {
    return command_result(
        runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearSelectionCommand&) noexcept {
    runtime.viewport->clear_selection();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const CancelMeasurementCommand&) noexcept {
    runtime.tools.measurement().cancel_incomplete();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ClearMeasurementCommand&) noexcept {
    runtime.tools.measurement().clear();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const SelectEntityCommand& command) noexcept {
    return command_result(
        runtime.viewport->set_selected_entity(*runtime.scene.scene, command.entity));
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const SetEntityVisibilityCommand& command) noexcept {
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
                                                      const ShowAllEntitiesCommand&) noexcept {
    const Result<void> result = runtime.scene.scene->show_all_entities();
    if (result) {
        invalidate_hierarchy_snapshot(runtime.scene);
    }
    return command_result(result);
}

[[nodiscard]] ViewerCommandCompletion
execute_command(ViewerAssembly& runtime, const IsolateEntityCommand& command) noexcept {
    return command_result(runtime.viewport->isolate_entity(*runtime.scene.scene, command.entity));
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ExitIsolationCommand&) noexcept {
    runtime.viewport->clear_isolation();
    return {};
}

[[nodiscard]] ViewerCommandCompletion execute_command(ViewerAssembly& runtime,
                                                      const ViewerCommand& command) {
    return std::visit([&runtime](const auto& value) { return execute_command(runtime, value); },
                      command);
}

void dispatch_viewer_commands(ViewerAssembly& runtime, ViewerCommandDispatcher& commands) {
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

void handle_pending_model_files(ViewerAssembly& runtime) {
    if (runtime.pending_files.dropped_path.has_value()) {
        std::string path = std::move(*runtime.pending_files.dropped_path);
        runtime.pending_files.dropped_path.reset();
        const ViewerCommandCompletion completion = execute_scene_workflow(
            workflow_context(runtime),
            SceneReplacementRequest{SceneReplacementKind::dropped_file, std::move(path)});
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
    }
    if (runtime.pending_files.drop_copy_failed) {
        runtime.pending_files.drop_copy_failed = false;
        const Error error{ErrorCode::invalid_argument,
                          "The viewer could not copy the dropped UTF-8 path"};
        runtime.notifications.load_failure = LoadFailure{"Dropped file", error};
        runtime.notifications.request_error_modal = true;
    }
    const FileBrowserFrameInput browser_input{runtime.interaction.escape_pressed,
                                              runtime.interaction.primary_double_clicked};
    const std::optional<FileDialogResult> open_result =
        build_open_modal(runtime.browser, runtime.preferences, runtime.scene, browser_input,
                         runtime.external_editor_workflow);
    if (open_result.has_value()) {
        const ViewerCommandCompletion completion = execute_scene_workflow(
            workflow_context(runtime),
            SceneReplacementRequest{SceneReplacementKind::open_model, open_result->path});
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
    }
    const std::optional<FileDialogResult> save_result =
        build_save_modal(runtime.browser, runtime.preferences, runtime.scene, browser_input,
                         runtime.external_editor_workflow);
    execute_external_editor_workflow(runtime.browser, runtime.external_editor_workflow);
    if (!save_result.has_value()) {
        return;
    }
    if (save_result->action == FileDialogAction::open) {
        const ViewerCommandCompletion completion = execute_scene_workflow(
            workflow_context(runtime),
            SceneReplacementRequest{SceneReplacementKind::open_model, save_result->path});
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
    } else {
        const ViewerCommandCompletion completion =
            execute_save_workflow(workflow_context(runtime), ModelSaveRequest{save_result->path});
        if (completion.error.has_value()) {
            set_viewport_error(frame_context(runtime), *completion.error);
        }
    }
}

void build_viewer_panels(ViewerAssembly& runtime, ApplicationUiContext& application,
                         ImGuiID dockspace_id, ViewerCommandDispatcher& commands) {
    ViewerFrameContext state = frame_context(runtime);
    SceneSession& scene = runtime.scene;
    build_rendering_panel(dockspace_id, state, scene, *runtime.viewport);
    build_3d_view(ViewPanelContext{dockspace_id, state, *runtime.viewport, scene, runtime.tools,
                                   application, runtime.viewport_interaction_owner,
                                   runtime.viewport_interaction_region});
    build_scene_hierarchy_panel(dockspace_id, state, scene, *runtime.viewport, commands);
    build_model_information(dockspace_id, state, scene);
    build_navigation_settings_window(dockspace_id, state, *runtime.viewport);
    build_selection_panel(dockspace_id, state, scene, *runtime.viewport, runtime.tools);
    build_measurement_panel(dockspace_id, state, scene, *runtime.viewport, runtime.tools);
    build_clipping_panel(dockspace_id, state, scene, *runtime.viewport, runtime.tools);
    build_status_bar(state, *runtime.engine, scene, *runtime.viewport, runtime.tools);
    build_about_window(state);
    build_error_modal(state);
    build_save_error_modal(state);
    if (state.shell.show_imgui_demo) {
        ImGui::ShowDemoWindow(&state.shell.show_imgui_demo);
    }
    state.shell.apply_dock_layout = false;
}

void build_viewer_frame_ui(ViewerAssembly& runtime, ApplicationUiContext& application) {
    runtime.scene_workflow.begin_frame();
    runtime.save_workflow.begin_frame();
    ViewerFrameContext state = frame_context(runtime);
    const ViewerCapabilitySnapshot capabilities = viewer_capabilities(runtime);
    ViewerCommandDispatcher commands;
    commands.begin_frame(capabilities);
    build_main_menu(state, *runtime.viewport, runtime.tools, capabilities, commands);
    build_toolbar(ToolbarBuildContext{state, runtime.toolbar_icons, *runtime.viewport,
                                      runtime.tools, capabilities, commands});
    collect_viewer_shortcuts(runtime, application.input(), capabilities, commands);
    dispatch_viewer_commands(runtime, commands);
    const ImGuiID dockspace_id = build_main_dockspace(state);
    handle_pending_model_files(runtime);
    build_viewer_panels(runtime, application, dockspace_id, commands);
    dispatch_viewer_commands(runtime, commands);
}

void capture_frame_sample(ViewerAssembly& runtime, const ApplicationUpdateContext& context) {
    ViewerFrameContext state = frame_context(runtime);
    if (state.rendering.viewport_rendered_this_frame) {
        state.rendering.statistics = runtime.viewport->render_statistics();
        state.rendering.framebuffer_valid = runtime.viewport->framebuffer_valid();
    }
    ViewerFrameSample sample;
    sample.frame_milliseconds = context.elapsed_seconds() * 1000.0;
    sample.render_milliseconds = state.rendering.viewport_rendered_this_frame
                                     ? state.rendering.statistics.cpu_total_milliseconds
                                     : 0.0;
    sample.navigation_scene_milliseconds =
        std::max(0.0, sample.frame_milliseconds - sample.render_milliseconds);
    sample.input_to_present_proxy_milliseconds = sample.frame_milliseconds;
    if (state.rendering.viewport_rendered_this_frame) {
        sample.render = state.rendering.statistics;
    }
    const Result<PickingStatistics> picking = runtime.viewport->picking_statistics();
    if (picking &&
        (picking.value().lifetime_gpu_requests != state.performance.sampled_picking_gpu_requests ||
         picking.value().lifetime_cpu_fallbacks !=
             state.performance.sampled_picking_cpu_fallbacks)) {
        sample.picking = picking.value();
        state.performance.sampled_picking_gpu_requests = picking.value().lifetime_gpu_requests;
        state.performance.sampled_picking_cpu_fallbacks = picking.value().lifetime_cpu_fallbacks;
    }
    sample.window_dimensions = context.window_extent();
    sample.framebuffer_dimensions = context.framebuffer_extent();
    sample.view_dimensions = state.rendering.view_dimensions;
    sample.target_dimensions = state.rendering.render_target_dimensions;
    sample.render_scale_percent = state.rendering.diagnostic_render_scale_percent;
    sample.vsync_enabled = state.rendering.vsync_enabled;
    sample.standard_shading = state.rendering.shading_mode == RenderShadingMode::standard;
    sample.rendered_3d = state.rendering.viewport_rendered_this_frame;
    retain_frame_sample(state, sample);
    state.rendering.viewport_rendered_this_frame = false;
}

void update_input_state(ViewerAssembly& runtime, const ApplicationUpdateContext& context) {
    runtime.interaction.frame_delta_seconds = context.elapsed_seconds();
    runtime.interaction.application_focused = context.focused();
    runtime.interaction.escape_pressed = context.input().key(InputKey::escape).pressed;
    const InputTransition& primary = context.input().button(InputButton::left);
    if (primary.click_count == 2) {
        runtime.interaction.primary_double_clicked = true;
    } else if (!primary.down && !primary.released) {
        runtime.interaction.primary_double_clicked = false;
    }
    runtime.notifications.viewport_error.clear();
    if (!context.focused()) {
        runtime.viewport->cancel_interaction();
    }
    if (context.dropped_file_count() != 0) {
        runtime.pending_files.dropped_path = std::string{context.dropped_file(0)};
    }
}

void synchronize_presentation_mode(ViewerAssembly& runtime,
                                   ApplicationUpdateContext& context) noexcept {
    if (runtime.rendering.vsync_enabled == runtime.rendering.vsync_applied) {
        return;
    }
    context.set_presentation_mode(runtime.rendering.vsync_enabled ? PresentationMode::synchronized
                                                                  : PresentationMode::immediate);
    runtime.rendering.vsync_applied = runtime.rendering.vsync_enabled;
}

class ViewerApplication final : public Application {
  public:
    ViewerApplication(std::filesystem::path asset_root,
                      std::optional<std::string> initial_model_path, bool smoke_mode)
        : asset_root_(std::move(asset_root)), initial_model_path_(std::move(initial_model_path)),
          smoke_mode_(smoke_mode) {}

    [[nodiscard]] Result<void> start(ApplicationContext& context) noexcept override {
        try {
            return start_impl(context);
        } catch (const std::bad_alloc&) {
            fatal_viewer_allocation_failure();
        } catch (...) {
            fatal_unexpected_viewer_exception();
        }
    }

    [[nodiscard]] Result<void> update(ApplicationUpdateContext& context) noexcept override {
        try {
            return update_impl(context);
        } catch (const std::bad_alloc&) {
            fatal_viewer_allocation_failure();
        } catch (...) {
            fatal_unexpected_viewer_exception();
        }
    }

    [[nodiscard]] Result<void> build_ui(ApplicationUiContext& context) noexcept override {
        try {
            build_viewer_frame_ui(runtime_, context);
            initial_ui_frame_built_ = true;
            return {};
        } catch (const std::bad_alloc&) {
            fatal_viewer_allocation_failure();
        } catch (...) {
            fatal_unexpected_viewer_exception();
        }
    }

    void stop(ApplicationContext& context) noexcept override {
        if (runtime_.viewport_interaction_owner.is_valid()) {
            context.interaction_arbiter().destroy_owner(runtime_.viewport_interaction_owner);
        }
        runtime_.toolbar_icons = {};
        runtime_.presentation = {};
        runtime_.viewport.reset();
        runtime_.scene = {};
        runtime_.engine = nullptr;
    }

  private:
    [[nodiscard]] Result<void> start_impl(ApplicationContext& context) {
        const Result<void> initialized = initialize_viewer_engine(runtime_, context.engine());
        if (!initialized) {
            return initialized.error();
        }
        load_viewer_preferences(runtime_.preferences);
        capture_context_diagnostics(context.graphics_context(), runtime_.diagnostics);
        initialize_viewer_presentation(runtime_, asset_root_, context.dpi_scale());
        const Result<InteractionOwnerId> owner =
            context.interaction_arbiter().create_owner(InteractionPriority::normal);
        if (!owner) {
            return owner.error();
        }
        runtime_.viewport_interaction_owner = owner.value();
        return {};
    }

    void complete_deferred_startup() {
        if (!initial_ui_frame_built_) {
            return;
        }
        if (!initial_model_path_.has_value()) {
            return;
        }
        std::string path = std::move(*initial_model_path_);
        initial_model_path_.reset();
        static_cast<void>(execute_scene_workflow(
            workflow_context(runtime_),
            SceneReplacementRequest{SceneReplacementKind::open_model, std::move(path)}));
    }

    [[nodiscard]] Result<void> validate_smoke_frame() const {
        if (smoke_mode_ && (runtime_.presentation.main_font == nullptr ||
                            ImGui::GetIO().FontDefault != runtime_.presentation.main_font ||
                            std::string_view{runtime_.presentation.main_font->GetDebugName()} !=
                                "DroidSans.ttf")) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "Viewer DroidSans.ttf asset is not the default presentation font"};
        }
        if (smoke_mode_ && update_count_ >= 4 && !runtime_.notifications.viewport_error.empty()) {
            return Error{ErrorCode::invalid_interaction_region,
                         runtime_.notifications.viewport_error};
        }
        return {};
    }

    void advance_smoke_frame(ApplicationUpdateContext& context) {
        if (!smoke_mode_) {
            return;
        }
        if (update_count_ == 2) {
            runtime_.shell.show_3d_view = false;
        } else if (update_count_ == 3) {
            runtime_.shell.show_3d_view = true;
        }
        if (update_count_ >= 4) {
            context.request_exit();
        }
    }

    [[nodiscard]] Result<void> update_impl(ApplicationUpdateContext& context) {
        ++update_count_;
        const Result<void> smoke_frame = validate_smoke_frame();
        if (!smoke_frame) {
            return smoke_frame.error();
        }
        capture_frame_sample(runtime_, context);
        update_input_state(runtime_, context);
        synchronize_presentation_mode(runtime_, context);
        complete_deferred_startup();
        advance_smoke_frame(context);
        if (runtime_.exit_requested) {
            context.request_exit();
        }
        return {};
    }

    std::filesystem::path asset_root_;
    std::optional<std::string> initial_model_path_;
    bool smoke_mode_ = false;
    bool initial_ui_frame_built_ = false;
    std::uint32_t update_count_ = 0;
    ViewerAssembly runtime_;
};

[[nodiscard]] bool environment_cannot_create_context(ErrorCode code) noexcept {
    return code == ErrorCode::graphics_initialization_failed ||
           code == ErrorCode::graphics_context_unavailable ||
           code == ErrorCode::unsupported_graphics_version;
}

[[nodiscard]] const char* first_viewer_argument(int argument_count, char** arguments) noexcept {
    return argument_count >= 2 && arguments != nullptr ? arguments[1] : nullptr;
}

int run_viewer_entry(int argument_count, char** arguments) {
    try {
        const char* first_argument = first_viewer_argument(argument_count, arguments);
        const bool smoke_mode =
            first_argument != nullptr && std::string_view{first_argument} == "--smoke";
        std::optional<std::string> initial_model_path;
        if (first_argument != nullptr && !smoke_mode) {
            initial_model_path = first_argument;
        }
        ViewerApplication application{viewer_asset_root(argument_count, arguments),
                                      std::move(initial_model_path), smoke_mode};
        ApplicationOptions options;
        options.title = "Elf3D Viewer";
        options.initial_window_extent = {1600, 900};
        options.presentation_mode = PresentationMode::synchronized;
        options.initial_visibility =
            smoke_mode ? ApplicationWindowVisibility::hidden : ApplicationWindowVisibility::visible;
        const Result<int> result = run_application(options, application);
        if (result) {
            return result.value();
        }
        if (smoke_mode && environment_cannot_create_context(result.error().code())) {
            return 77;
        }
        std::cerr << "Elf3D Viewer failed [" << error_category(result.error().code())
                  << "]: " << result.error().message() << '\n';
        return 1;
    } catch (const std::bad_alloc&) {
        fatal_viewer_allocation_failure();
    } catch (...) {
        fatal_unexpected_viewer_exception();
    }
}

} // namespace elf3d::viewer
