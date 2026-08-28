#include "viewer_browser.hpp"
#include "viewer_commands.hpp"
#include "viewer_component_state.hpp"
#include "viewer_input_math.hpp"
#include "viewer_viewport.hpp"
#include "viewer_workflows.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>

namespace {

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 1.0e-6F;
}

[[nodiscard]] int verify_dpi_and_pointer_precision() {
    using elf3d::Extent2D;
    using elf3d::Float2;
    using elf3d::viewer::content_extent_in_pixels;
    using elf3d::viewer::pointer_delta_in_target_pixels;

    if (content_extent_in_pixels({800.0F, 600.0F}, {1.0F, 1.0F}) != Extent2D{800, 600} ||
        content_extent_in_pixels({800.0F, 600.0F}, {1.5F, 1.5F}) != Extent2D{1200, 900} ||
        content_extent_in_pixels({800.0F, 600.0F}, {2.0F, 2.0F}) != Extent2D{1600, 1200}) {
        return 1;
    }
    const Float2 scaled =
        pointer_delta_in_target_pixels({0.25F, -0.125F}, {800.0F, 600.0F}, {1600, 1200});
    if (!nearly_equal(scaled.x, 0.5F) || !nearly_equal(scaled.y, -0.25F)) {
        return 2;
    }
    const Float2 fractional =
        pointer_delta_in_target_pixels({0.1F, 0.2F}, {1000.0F, 500.0F}, {1500, 750});
    if (!nearly_equal(fractional.x, 0.15F) || !nearly_equal(fractional.y, 0.3F)) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int verify_wheel_accumulation() {
    float accumulated = 0.0F;
    for (double delta : {0.125, 0.25, -0.0625}) {
        const std::optional<float> next =
            elf3d::viewer::accumulated_wheel_delta(accumulated, delta);
        if (!next.has_value()) {
            return 4;
        }
        accumulated = *next;
    }
    if (!nearly_equal(accumulated, 0.3125F) ||
        elf3d::viewer::accumulated_wheel_delta(accumulated, 0.0).has_value() ||
        elf3d::viewer::accumulated_wheel_delta(accumulated, std::numeric_limits<double>::infinity())
            .has_value()) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int verify_retained_frame_lifecycle_invalidation() {
    using elf3d::viewer::RetainedViewportFrameKey;
    using elf3d::viewer::viewport_frame_render_required;
    RetainedViewportFrameKey key;
    key.scene_revision = 7;
    const std::optional<RetainedViewportFrameKey> missing;
    if (!viewport_frame_render_required(missing, key, true, false) ||
        !viewport_frame_render_required(key, key, false, false) ||
        !viewport_frame_render_required(key, key, true, true) ||
        viewport_frame_render_required(key, key, true, false)) {
        return 6;
    }
    RetainedViewportFrameKey changed = key;
    ++changed.scene_revision;
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 7;
    }
    changed = key;
    changed.target_extent = {1920, 1080};
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 8;
    }
    changed = key;
    changed.diagnostic_render_scale_percent = 50;
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 9;
    }
    return 0;
}

[[nodiscard]] int verify_retained_frame_mechanism_invalidation() {
    using elf3d::viewer::RetainedViewportFrameKey;
    using elf3d::viewer::viewport_frame_render_required;
    RetainedViewportFrameKey key;
    RetainedViewportFrameKey changed = key;
    ++changed.viewport_revision;
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 35;
    }
    changed = key;
    ++changed.clipping_revision;
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 36;
    }
    changed = key;
    changed.measurement_state = elf3d::viewer::DistanceMeasurementState::awaiting_second_point;
    return viewport_frame_render_required(key, changed, true, false) ? 0 : 37;
}

[[nodiscard]] int verify_command_fifo() {
    using namespace elf3d::viewer;
    ViewerCapabilitySnapshot capabilities;
    capabilities.view_available = true;
    ViewerCommandDispatcher commands;
    commands.begin_frame(capabilities);
    commands.emit(ActivateViewerToolCommand{ViewerTool::distance_measurement});
    commands.emit(ExitViewerCommand{});

    std::optional<ViewerCommandDispatch> dispatch = commands.take_next(capabilities.scene);
    if (!dispatch.has_value() || dispatch->sequence != 1 ||
        viewer_command_kind(dispatch->command) != ViewerCommandKind::activate_tool) {
        return 10;
    }
    commands.complete(*dispatch, {});
    dispatch = commands.take_next(capabilities.scene);
    if (!dispatch.has_value() || dispatch->sequence != 2 ||
        viewer_command_kind(dispatch->command) != ViewerCommandKind::exit_viewer) {
        return 11;
    }
    commands.complete(*dispatch, {});
    if (commands.take_next(capabilities.scene).has_value() || commands.outcomes().size() != 2) {
        return 12;
    }
    return 0;
}

[[nodiscard]] int verify_command_scene_barrier() {
    using namespace elf3d::viewer;
    ViewerCapabilitySnapshot capabilities;
    capabilities.scene_imported = true;
    capabilities.view_available = true;
    capabilities.visible_content = true;
    ViewerCommandDispatcher commands;
    commands.begin_frame(capabilities);
    commands.emit(ReloadSceneCommand{});
    commands.emit(FitViewCommand{});
    commands.emit(ExitViewerCommand{});
    std::optional<ViewerCommandDispatch> dispatch = commands.take_next(capabilities.scene);
    if (!dispatch.has_value() ||
        viewer_command_kind(dispatch->command) != ViewerCommandKind::reload_scene) {
        return 13;
    }
    commands.complete(*dispatch, ViewerCommandCompletion{ViewerCommandOutcomeStatus::executed,
                                                         std::nullopt, true});
    dispatch = commands.take_next(capabilities.scene);
    if (!dispatch.has_value() || dispatch->sequence != 3 ||
        viewer_command_kind(dispatch->command) != ViewerCommandKind::exit_viewer) {
        return 14;
    }
    commands.complete(*dispatch, {});
    if (commands.take_next(capabilities.scene).has_value() || !commands.scene_replaced() ||
        commands.outcomes().size() != 3 ||
        commands.outcomes()[1].status !=
            ViewerCommandOutcomeStatus::rejected_after_scene_replacement) {
        return 15;
    }
    return 0;
}

[[nodiscard]] int verify_command_disablement() {
    using namespace elf3d::viewer;
    ViewerCapabilitySnapshot disabled;
    ViewerCommandDispatcher commands;
    commands.begin_frame(disabled);
    commands.emit(ReloadSceneCommand{});
    commands.emit(FitViewCommand{});
    commands.emit(ClearSelectionCommand{});
    if (commands.take_next(disabled.scene).has_value() || commands.outcomes().size() != 3) {
        return 16;
    }
    for (const ViewerCommandOutcome& outcome : commands.outcomes()) {
        if (outcome.status != ViewerCommandOutcomeStatus::disabled) {
            return 17;
        }
    }
    return 0;
}

[[nodiscard]] int verify_single_owner_command_failure() {
    using namespace elf3d::viewer;
    ViewerCapabilitySnapshot enabled;
    enabled.scene_imported = true;
    ViewerCommandDispatcher commands;
    commands.begin_frame(enabled);
    commands.emit(ReloadSceneCommand{});
    commands.emit(CloseSceneCommand{});
    const std::optional<ViewerCommandDispatch> reload = commands.take_next(enabled.scene);
    if (!reload.has_value()) {
        return 18;
    }
    commands.complete(*reload,
                      ViewerCommandCompletion{
                          ViewerCommandOutcomeStatus::failed,
                          elf3d::Error{elf3d::ErrorCode::invalid_argument, "Expected test failure"},
                          false});
    if (commands.take_next(enabled.scene).has_value() || commands.outcomes().size() != 2 ||
        commands.outcomes()[0].status != ViewerCommandOutcomeStatus::failed ||
        !commands.outcomes()[0].error.has_value() ||
        commands.outcomes()[0].error->code() != elf3d::ErrorCode::invalid_argument ||
        commands.outcomes()[1].status != ViewerCommandOutcomeStatus::rejected_replacement_limit ||
        commands.outcomes()[1].error.has_value()) {
        return 19;
    }
    return 0;
}

[[nodiscard]] int verify_command_queue_limit() {
    using namespace elf3d::viewer;
    ViewerCommandDispatcher commands;
    commands.begin_frame({});
    for (std::size_t index = 0; index <= maximum_viewer_commands_per_frame; ++index) {
        commands.emit(ExitViewerCommand{});
    }
    return commands.enqueue_error().has_value() ? 0 : 20;
}

[[nodiscard]] bool
open_request_matches(const std::optional<elf3d::viewer::SceneReplacementRequest>& request) {
    using elf3d::viewer::SceneReplacementKind;
    return request.has_value() && request->kind == SceneReplacementKind::open_model &&
           request->source_path == "a.glb";
}

[[nodiscard]] bool
failed_workflow_matches(const elf3d::viewer::SceneReplacementWorkflow& workflow) {
    using namespace elf3d::viewer;
    const WorkflowSnapshot snapshot = workflow.snapshot();
    return snapshot.phase == WorkflowPhase::failed && snapshot.error.has_value() &&
           workflow.attempted_this_frame();
}

[[nodiscard]] int verify_failed_scene_replacement_workflow() {
    using namespace elf3d::viewer;
    SceneReplacementWorkflow workflow;
    workflow.begin_frame();
    if (workflow.activate(SceneReplacementRequest{SceneReplacementKind::open_model, "a.glb"}) !=
            WorkflowActivation::accepted ||
        workflow.activate(SceneReplacementRequest{SceneReplacementKind::reload_model, "b.glb"}) !=
            WorkflowActivation::busy) {
        return 21;
    }
    std::optional<SceneReplacementRequest> request = workflow.begin_execution();
    if (!open_request_matches(request) || workflow.begin_execution().has_value()) {
        return 22;
    }
    workflow.fail(elf3d::Error{elf3d::ErrorCode::invalid_argument, "Expected workflow failure"});
    if (!failed_workflow_matches(workflow) ||
        workflow.activate(SceneReplacementRequest{SceneReplacementKind::close_to_empty, {}}) !=
            WorkflowActivation::frame_limit_reached) {
        return 23;
    }
    return 0;
}

[[nodiscard]] int verify_successful_scene_replacement_workflow() {
    using namespace elf3d::viewer;
    SceneReplacementWorkflow workflow;
    workflow.begin_frame();
    if (workflow.activate(SceneReplacementRequest{SceneReplacementKind::close_to_empty, {}}) !=
        WorkflowActivation::accepted) {
        return 24;
    }
    const std::optional<SceneReplacementRequest> request = workflow.begin_execution();
    if (!request.has_value() || request->kind != SceneReplacementKind::close_to_empty) {
        return 25;
    }
    workflow.succeed();
    return workflow.snapshot().phase == WorkflowPhase::succeeded ? 0 : 26;
}

[[nodiscard]] int verify_save_and_editor_workflows() {
    using namespace elf3d::viewer;
    ModelSaveWorkflow save;
    save.begin_frame();
    if (save.activate(ModelSaveRequest{"model.glb"}) != WorkflowActivation::accepted ||
        save.activate(ModelSaveRequest{"other.glb"}) != WorkflowActivation::busy) {
        return 27;
    }
    const std::optional<ModelSaveRequest> save_request = save.begin_execution();
    if (!save_request.has_value() || save_request->target_path != "model.glb") {
        return 28;
    }
    save.succeed();
    if (save.snapshot().phase != WorkflowPhase::succeeded) {
        return 29;
    }

    ExternalEditorWorkflow editor;
    if (editor.activate(ExternalEditorLaunchRequest{"editor.exe", "model.glb", "Editor"}) !=
        WorkflowActivation::accepted) {
        return 30;
    }
    const std::optional<ExternalEditorLaunchRequest> editor_request = editor.begin_execution();
    if (!editor_request.has_value() || editor_request->editor_label != "Editor") {
        return 31;
    }
    editor.cancel();
    return editor.snapshot().phase == WorkflowPhase::cancelled ? 0 : 32;
}

[[nodiscard]] int verify_component_state_ownership() {
    using namespace elf3d::viewer;
    ViewerShellState shell;
    ViewerRenderingState rendering;
    ViewerPerformanceState performance;
    ViewerGraphicsDiagnosticsState diagnostics;
    ViewerNotificationState notifications;
    ViewerInteractionFrameState interaction;
    SceneHierarchyComponentState hierarchy;
    ViewerPresentationResources presentation;
    PendingFileInputState pending_files;
    constexpr std::array<float, 4> expected_clear_color{213.0F / 255.0F, 227.0F / 255.0F,
                                                        240.0F / 255.0F, 1.0F};
    if (rendering.clear_color != expected_clear_color) {
        return 43;
    }
    ViewerFrameContext frame{shell,       rendering, performance,  diagnostics,  notifications,
                             interaction, hierarchy, presentation, pending_files};
    frame.shell.show_3d_view = false;
    frame.rendering.diagnostic_render_scale_percent = 50;
    frame.performance.captured_frame_count = 7;
    frame.notifications.viewport_error = "test";
    if (shell.show_3d_view || rendering.diagnostic_render_scale_percent != 50 ||
        performance.captured_frame_count != 7 || notifications.viewport_error != "test") {
        return 33;
    }

    ViewerPreferencesState preferences;
    FileBrowserState browser;
    preferences.last_model_directory = "preferences";
    browser.directory = "browser";
    return preferences.last_model_directory != browser.directory ? 0 : 34;
}

class TemporaryViewerState final {
  public:
    TemporaryViewerState()
        : path_(std::filesystem::path{ELF3D_VIEWER_TEST_BINARY_DIR} / "viewer_behavior") {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_ / "first" / "nested", error);
        std::filesystem::create_directories(path_ / "second", error);
    }

    ~TemporaryViewerState() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryViewerState(const TemporaryViewerState&) = delete;
    TemporaryViewerState& operator=(const TemporaryViewerState&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] bool write_file(const std::filesystem::path& path, std::string_view content) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output << content;
    return output.good();
}

[[nodiscard]] bool browser_entries_are_filtered(const elf3d::viewer::FileBrowserState& browser) {
    return browser.entries.size() == 3 && browser.entries[0].directory &&
           browser.entries[0].label == "nested" && !browser.entries[1].directory &&
           browser.entries[1].label == "model.gltf" && !browser.entries[2].directory &&
           browser.entries[2].label == "UPPER.GLB";
}

[[nodiscard]] int verify_preferences_persistence(const TemporaryViewerState& temporary,
                                                 const std::filesystem::path& first) {
    using namespace elf3d::viewer;
    ViewerPreferencesState preferences;
    preferences.storage_path = temporary.path() / "settings" / "viewer-state.ini";
    remember_model_directory(preferences, first / "model.gltf");
    ViewerPreferencesState loaded;
    loaded.storage_path = preferences.storage_path;
    load_viewer_preferences(loaded);
    if (!same_path_key(preferences.last_model_directory, first) ||
        !same_path_key(loaded.last_model_directory, first)) {
        return 39;
    }
    return 0;
}

[[nodiscard]] int verify_browser_history(const std::filesystem::path& first,
                                         const std::filesystem::path& second,
                                         elf3d::viewer::FileBrowserState& browser) {
    using namespace elf3d::viewer;
    set_file_browser_directory(browser, first);
    set_file_browser_directory(browser, second);
    navigate_file_browser_history(browser, -1);
    refresh_file_browser_entries(browser);
    if (!same_path_key(browser.directory, first) || browser.history.size() != 2 ||
        browser.recents.size() != 2 || !browser_entries_are_filtered(browser)) {
        return 40;
    }
    return 0;
}

[[nodiscard]] int verify_browser_selection(const std::filesystem::path& model,
                                           elf3d::viewer::FileBrowserState& browser) {
    using namespace elf3d::viewer;
    select_file_browser_file(browser, model);
    if (browser.selected_path.empty() ||
        std::string_view{browser.file_path.data()} != path_to_utf8(model)) {
        return 41;
    }
    clear_file_browser_selection(browser);
    return 0;
}

[[nodiscard]] int verify_invalid_browser_directory(const TemporaryViewerState& temporary,
                                                   elf3d::viewer::FileBrowserState& browser) {
    using namespace elf3d::viewer;
    const std::filesystem::path previous_directory = browser.directory;
    set_file_browser_directory(browser, temporary.path() / "missing");
    if (!browser.selected_path.empty() || browser.file_path[0] != '\0' || browser.error.empty() ||
        browser.directory != previous_directory) {
        return 42;
    }
    return supported_model_path("sample.GLTf") && !supported_model_path("sample.obj") ? 0 : 43;
}

[[nodiscard]] int verify_browser_navigation(const TemporaryViewerState& temporary,
                                            const std::filesystem::path& first,
                                            const std::filesystem::path& second) {
    elf3d::viewer::FileBrowserState browser;
    const int history = verify_browser_history(first, second, browser);
    if (history != 0) {
        return history;
    }
    const int selection = verify_browser_selection(first / "model.gltf", browser);
    if (selection != 0) {
        return selection;
    }
    return verify_invalid_browser_directory(temporary, browser);
}

[[nodiscard]] int verify_preferences_and_browser() {
    TemporaryViewerState temporary;
    const std::filesystem::path first = temporary.path() / "first";
    const std::filesystem::path second = temporary.path() / "second";
    if (!write_file(first / "model.gltf", "{}") || !write_file(first / "UPPER.GLB", "glb") ||
        !write_file(first / "ignored.txt", "ignored")) {
        return 38;
    }
    const int preferences = verify_preferences_persistence(temporary, first);
    if (preferences != 0) {
        return preferences;
    }
    return verify_browser_navigation(temporary, first, second);
}

[[nodiscard]] int verify_input_and_frame_behavior() {
    const int dpi = verify_dpi_and_pointer_precision();
    if (dpi != 0) {
        return dpi;
    }
    const int wheel = verify_wheel_accumulation();
    if (wheel != 0) {
        return wheel;
    }
    const int retained = verify_retained_frame_lifecycle_invalidation();
    if (retained != 0) {
        return retained;
    }
    return verify_retained_frame_mechanism_invalidation();
}

[[nodiscard]] int verify_command_behavior() {
    const int fifo = verify_command_fifo();
    if (fifo != 0) {
        return fifo;
    }
    const int barrier = verify_command_scene_barrier();
    if (barrier != 0) {
        return barrier;
    }
    const int disablement = verify_command_disablement();
    if (disablement != 0) {
        return disablement;
    }
    const int failure = verify_single_owner_command_failure();
    if (failure != 0) {
        return failure;
    }
    const int queue_limit = verify_command_queue_limit();
    if (queue_limit != 0) {
        return queue_limit;
    }
    return 0;
}

[[nodiscard]] int verify_workflow_and_state_behavior() {
    const int failed_workflow = verify_failed_scene_replacement_workflow();
    if (failed_workflow != 0) {
        return failed_workflow;
    }
    const int successful_workflow = verify_successful_scene_replacement_workflow();
    if (successful_workflow != 0) {
        return successful_workflow;
    }
    const int workflows = verify_save_and_editor_workflows();
    if (workflows != 0) {
        return workflows;
    }
    const int state = verify_component_state_ownership();
    if (state != 0) {
        return state;
    }
    return verify_preferences_and_browser();
}

} // namespace

int main() {
    const int input = verify_input_and_frame_behavior();
    if (input != 0) {
        return input;
    }
    const int commands = verify_command_behavior();
    if (commands != 0) {
        return commands;
    }
    return verify_workflow_and_state_behavior();
}
