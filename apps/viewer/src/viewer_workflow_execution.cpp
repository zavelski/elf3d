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
                         const Error& error) {
    notifications.load_failure = LoadFailure{path, error};
    notifications.request_error_modal = true;
    std::cerr << "Failed to load '" << path << "' [" << error_category(error.code())
              << "]: " << error.message() << '\n';
}

[[nodiscard]] ViewerCommandCompletion command_failed(const Error& error) noexcept {
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::failed, error, false};
}

[[nodiscard]] Error workflow_activation_error(WorkflowActivation activation) {
    const char* message = activation == WorkflowActivation::busy
                              ? "A viewer workflow is already active"
                              : "The workflow operation limit was reached for this frame";
    return Error{ErrorCode::invalid_argument, message};
}

[[nodiscard]] SceneReplacementOutcome load_model(const ViewerWorkflowContext& context,
                                                 const std::string& source_path) {
    try {
        Result<SceneSession> result = load_model_scene(context.engine, path_from_utf8(source_path));
        if (!result) {
            report_load_failure(context.notifications, source_path, result.error());
            return SceneReplacementOutcome{false, result.error()};
        }
        context.viewport.cancel_interaction();
        context.viewport.clear_selection();
        context.viewport.clear_isolation();
        context.viewport.clear_clipping();
        context.tools.clear_scene(context.scene.scene->id());
        context.scene = std::move(result).value();
        remember_model_directory(context.preferences, context.scene.source_path);
        context.rendering.rotation_angle = 0.0F;
        context.rendering.statistics = {};
        context.hierarchy.last_revealed_selection.reset();
        return SceneReplacementOutcome{true, std::nullopt};
    } catch (const std::bad_alloc&) {
        fatal_viewer_allocation_failure();
    } catch (const std::filesystem::filesystem_error&) {
        const Error error{ErrorCode::invalid_argument,
                          "The viewer could not convert the UTF-8 source path"};
        report_load_failure(context.notifications, source_path, error);
        return SceneReplacementOutcome{false, error};
    } catch (...) {
        fatal_unexpected_viewer_exception();
    }
}

[[nodiscard]] SceneReplacementOutcome replace_with_demo(const ViewerWorkflowContext& context) {
    Result<SceneSession> replacement = create_demo_scene(context.engine);
    if (!replacement) {
        report_load_failure(context.notifications, "Procedural cube demo", replacement.error());
        return SceneReplacementOutcome{false, replacement.error()};
    }
    context.viewport.cancel_interaction();
    context.viewport.clear_selection();
    context.viewport.clear_isolation();
    context.viewport.clear_clipping();
    context.tools.clear_scene(context.scene.scene->id());
    context.scene = std::move(replacement).value();
    context.rendering.statistics = {};
    context.hierarchy.last_revealed_selection.reset();
    return SceneReplacementOutcome{true, std::nullopt};
}

[[nodiscard]] SceneReplacementOutcome
perform_scene_replacement(const ViewerWorkflowContext& context,
                          const SceneReplacementRequest& request) {
    switch (request.kind) {
    case SceneReplacementKind::open_model:
    case SceneReplacementKind::dropped_file:
    case SceneReplacementKind::reload_model:
        return load_model(context, request.source_path);
    case SceneReplacementKind::close_to_demo:
    case SceneReplacementKind::create_demo:
        return replace_with_demo(context);
    }
    return SceneReplacementOutcome{
        false, Error{ErrorCode::invalid_argument, "The scene workflow request is invalid"}};
}

[[nodiscard]] ViewerCommandCompletion command_replacement(SceneReplacementOutcome outcome) {
    if (outcome.error.has_value()) {
        return command_failed(*outcome.error);
    }
    return ViewerCommandCompletion{ViewerCommandOutcomeStatus::executed, std::nullopt,
                                   outcome.replaced};
}

[[nodiscard]] std::optional<Error> save_model(const ViewerWorkflowContext& context,
                                              const std::string& target_path) {
    const Result<void> saved = context.scene.scene->export_loaded_document(target_path);
    if (!saved) {
        context.notifications.save_failure = LoadFailure{target_path, saved.error()};
        context.notifications.request_save_error_modal = true;
        return saved.error();
    }
    context.scene.source_path = path_from_utf8(target_path);
    remember_model_directory(context.preferences, context.scene.source_path);
    return std::nullopt;
}

} // namespace

ViewerCommandCompletion execute_scene_workflow(const ViewerWorkflowContext& context,
                                               SceneReplacementRequest request) {
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
                                              ModelSaveRequest request) {
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
    const std::optional<Error> error = save_model(context, active->target_path);
    if (error.has_value()) {
        context.save.fail(*error);
        return command_failed(*error);
    }
    context.save.succeed();
    return {};
}

void execute_external_editor_workflow(FileBrowserState& browser, ExternalEditorWorkflow& workflow) {
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
