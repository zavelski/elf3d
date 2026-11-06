#include "viewer_workflow_execution.hpp"

#include "viewer_application.hpp"
#include "viewer_assets.hpp"
#include "viewer_browser.hpp"
#include "viewer_component_state.hpp"
#include "viewer_scene_session.hpp"
#include "viewer_tools.hpp"
#include "viewer_ui.hpp"

#include <filesystem>
#include <iostream>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace elf3d::viewer {
namespace {

struct SceneReplacementOutcome {
    bool replaced = false;
    std::optional<Error> error;
};

void report_load_failure(ViewerNotificationState& notifications, const std::string& path,
                         const Error& error)
{
    notifications.load_failure = LoadFailure{path, error};
    notifications.request_error_modal = true;
    std::cerr << "Failed to load '" << path << "' [" << error_category(error.code())
              << "]: " << error.message() << '\n';
}

[[nodiscard]] ViewerCommandCompletion command_failed(const Error& error) noexcept
{
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::failed, error, false};
}

[[nodiscard]] Error workflow_activation_error(WorkflowActivation activation)
{
    const char* message = activation == WorkflowActivation::busy
                              ? "A viewer workflow is already active"
                              : "The workflow operation limit was reached for this frame";
    return Error{ErrorCode::invalid_argument, message};
}

void clear_session_observers(const ViewerWorkflowContext& context)
{
    context.arbiter.release(context.interaction_owner);
    context.interaction_region = {};
    context.viewport->cancel_interaction();
    context.tools.clear_scene(context.scene.scene->id());
    context.tools.measurement().clear();
    context.rendering.retained_viewport_frame.reset();
    context.rendering.graphics_scene = {};
    context.rendering.graphics_report = {};
    context.rendering.graphics_error.reset();
    context.rendering.framebuffer_valid = false;
    context.rendering.viewport_rendered_this_frame = false;
    context.rendering.statistics = {};
    context.rendering.view_dimensions = {};
    context.rendering.render_target_dimensions = {};
    context.notifications = {};
    context.save = {};
    cancel_model_save(context.browser);
    context.performance.frame_samples.clear();
    context.performance.captured_frame_count = 0;
    context.performance.rendered_3d_frame_count = 0;
    context.performance.reused_3d_frame_count = 0;
    context.performance.sampled_picking_gpu_requests = 0;
    context.performance.sampled_picking_cpu_fallbacks = 0;
}

void clear_session_state(const ViewerWorkflowContext& context)
{
    clear_session_observers(context);
    context.hierarchy = {};
}

[[nodiscard]] Result<void> replace_viewport_and_scene(const ViewerWorkflowContext& context)
{
    const OrbitNavigationSettings navigation = context.viewport->navigation_settings();
    const bool navigation_enabled = context.viewport->navigation_enabled();
    // Release the current Viewport before its Scene/Document.
    context.viewport.reset();
    context.scene = {};
    Result<std::unique_ptr<Viewport>> viewport = context.engine.create_viewport({});
    if (!viewport) {
        return viewport.error();
    }
    context.viewport = std::move(viewport).value();
    context.viewport->set_navigation_enabled(navigation_enabled);
    return context.viewport->set_navigation_settings(navigation);
}

[[nodiscard]] Result<void> release_session(const ViewerWorkflowContext& context)
{
    clear_session_state(context);
    return replace_viewport_and_scene(context);
}

[[nodiscard]] Result<void> make_empty_scene(const ViewerWorkflowContext& context)
{
    Result<SceneSession> scene = create_empty_scene(context.engine);
    if (!scene) {
        return scene.error();
    }
    context.scene = std::move(scene).value();
    return {};
}

[[nodiscard]] SceneReplacementOutcome replace_with_empty_scene(const ViewerWorkflowContext& context)
{
    const Result<void> empty = make_empty_scene(context);
    return SceneReplacementOutcome{true,
                                   empty ? std::nullopt : std::optional<Error>{empty.error()}};
}

[[nodiscard]] SceneReplacementOutcome load_before_replacement(const ViewerWorkflowContext& context,
                                                              const std::string& source_path)
{
    const auto path = path_from_utf8(source_path);
    if (!path) {
        report_load_failure(context.notifications, source_path, path.error());
        return {false, path.error()};
    }
    auto loaded = load_model_scene(context.engine, path.value());
    if (!loaded) {
        report_load_failure(context.notifications, source_path, loaded.error());
        return {false, loaded.error()};
    }
    if (const auto released = release_session(context); !released) {
        return {true, released.error()};
    }
    context.scene = std::move(loaded).value();
    context.scene.saved_document_revision = context.scene.scene->document()->revision();
    remember_model_directory(context.preferences, context.scene.source_path);
    return {true, std::nullopt};
}

[[nodiscard]] SceneReplacementOutcome
perform_scene_replacement(const ViewerWorkflowContext& context,
                          const SceneReplacementRequest& request)
{
    switch (request.kind) {
    case SceneReplacementKind::close_to_empty: {
        const auto released = release_session(context);
        return released ? replace_with_empty_scene(context)
                        : SceneReplacementOutcome{true, released.error()};
    }
    case SceneReplacementKind::open_model:
    case SceneReplacementKind::dropped_file:
    case SceneReplacementKind::reload_model:
        break;
    default:
        return {false, Error{ErrorCode::invalid_argument, "The scene workflow request is invalid"}};
    }
    return load_before_replacement(context, request.source_path);
}

[[nodiscard]] ViewerCommandCompletion command_replacement(SceneReplacementOutcome outcome)
{
    if (outcome.error.has_value()) {
        return ViewerCommandCompletion{ViewerCommandOutcomeStatus::failed, outcome.error,
                                       outcome.replaced};
    }
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::executed, std::nullopt,
                                   outcome.replaced};
}

[[nodiscard]] std::optional<Error> save_model(const ViewerWorkflowContext& context,
                                              const ModelSaveRequest& request)
{
    if (!context.scene.scene || !context.scene.is_imported()) {
        return Error{ErrorCode::invalid_argument, "Saving requires a current loaded model"};
    }
    Result<std::filesystem::path> path = path_from_utf8(request.target_path);

    const auto document = context.scene.scene->document();
    if (!document) {
        return Error{ErrorCode::invalid_argument, "Saving requires the accepted Document"};
    }
    auto saved = path ? save_document(request.target_path, *document, request.options)
                      : Result<ModelWriteReport>{path.error()};
    if (!saved) {
        context.notifications.save_failure = LoadFailure{request.target_path, saved.error()};
        context.notifications.request_save_error_modal = true;
        return saved.error();
    }
    context.scene.source_path = std::move(path).value();
    context.scene.dirty = false;
    context.scene.saved_document_revision = document->revision();
    context.write_report = std::move(saved).value();
    context.scene.save_target = context.scene.source_path;
    context.scene.save_options = request.options;
    remember_model_directory(context.preferences, context.scene.source_path);
    return std::nullopt;
}

} // namespace

ViewerCommandCompletion execute_scene_workflow(const ViewerWorkflowContext& context,
                                               SceneReplacementRequest request)
{
    if (context.model_busy) {
        return command_failed(Error{ErrorCode::invalid_argument,
                                    "Model actions are unavailable while work is active"});
    }
    if (!context.replacement_allowed) {
        return command_failed(Error{ErrorCode::invalid_argument,
                                    "Scene replacement must precede viewport submission"});
    }
    const WorkflowActivation activation = context.scene_replacement.activate(std::move(request));
    if (activation != WorkflowActivation::accepted) {
        return command_failed(workflow_activation_error(activation));
    }

    const std::optional<SceneReplacementRequest> active =
        context.scene_replacement.begin_execution();
    if (!active.has_value()) {
        const Error error{ErrorCode::invalid_argument,
                          "The scene workflow did not enter execution"};
        context.scene_replacement.fail(error);
        return command_failed(error);
    }
    SceneReplacementOutcome outcome = perform_scene_replacement(context, *active);
    if (outcome.error.has_value()) {
        context.scene_replacement.fail(*outcome.error);
    } else {
        context.scene_replacement.succeed();
    }
    return command_replacement(std::move(outcome));
}

ViewerCommandCompletion execute_save_workflow(const ViewerWorkflowContext& context,
                                              ModelSaveRequest request)
{
    if (context.model_busy) {
        return command_failed(
            Error{ErrorCode::invalid_argument, "Saving is unavailable while model work is active"});
    }
    const WorkflowActivation activation = context.save.activate(std::move(request));
    if (activation != WorkflowActivation::accepted) {
        return command_failed(workflow_activation_error(activation));
    }

    const std::optional<ModelSaveRequest> active = context.save.begin_execution();
    if (!active.has_value()) {
        const Error error{ErrorCode::invalid_argument, "The save workflow did not enter execution"};
        context.save.fail(error);
        return command_failed(error);
    }

    const std::optional<Error> error = save_model(context, *active);
    if (error.has_value()) {
        context.save.fail(*error);
        return command_failed(*error);
    }

    context.save.succeed();
    return {};
}

void execute_external_editor_workflow(FileBrowserState& browser, ExternalEditorWorkflow& workflow)
{
    const std::optional<ExternalEditorLaunchRequest> request = workflow.begin_execution();
    if (!request.has_value()) {
        return;
    }
    if (launch_external_editor(request->editor, request->file)) {
        workflow.succeed();
        return;
    }

    const Error error{ErrorCode::invalid_argument,
                      "The viewer could not launch the selected external editor"};
    workflow.fail(error);
    browser.error = "Could not launch " + request->editor_label + ".";
}

} // namespace elf3d::viewer
