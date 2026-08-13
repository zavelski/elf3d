#include "viewer_workflows.hpp"

#include <utility>

namespace elf3d::viewer {
namespace {

[[nodiscard]] bool workflow_busy(WorkflowPhase phase) noexcept {
    return phase == WorkflowPhase::ready || phase == WorkflowPhase::executing;
}

void complete_success(WorkflowPhase& phase, std::optional<Error>& error) noexcept {
    phase = WorkflowPhase::succeeded;
    error.reset();
}

void complete_failure(WorkflowPhase& phase, std::optional<Error>& target, const Error& error) {
    phase = WorkflowPhase::failed;
    target = error;
}

void cancel_workflow(WorkflowPhase& phase, std::optional<Error>& error) noexcept {
    if (workflow_busy(phase)) {
        phase = WorkflowPhase::cancelled;
        error.reset();
    }
}

[[nodiscard]] WorkflowSnapshot workflow_snapshot(WorkflowPhase phase,
                                                 const std::optional<Error>& error) {
    return WorkflowSnapshot{phase, error};
}

} // namespace

void SceneReplacementWorkflow::begin_frame() noexcept {
    attempted_this_frame_ = false;
}

WorkflowActivation SceneReplacementWorkflow::activate(SceneReplacementRequest request) {
    if (workflow_busy(phase_)) {
        return WorkflowActivation::busy;
    }
    if (attempted_this_frame_) {
        return WorkflowActivation::frame_limit_reached;
    }
    request_ = std::move(request);
    error_.reset();
    phase_ = WorkflowPhase::ready;
    return WorkflowActivation::accepted;
}

std::optional<SceneReplacementRequest> SceneReplacementWorkflow::begin_execution() {
    if (phase_ != WorkflowPhase::ready || !request_.has_value() || attempted_this_frame_) {
        return std::nullopt;
    }
    attempted_this_frame_ = true;
    phase_ = WorkflowPhase::executing;
    std::optional<SceneReplacementRequest> result = std::move(request_);
    request_.reset();
    return result;
}

void SceneReplacementWorkflow::succeed() noexcept {
    complete_success(phase_, error_);
}

void SceneReplacementWorkflow::fail(const Error& error) {
    complete_failure(phase_, error_, error);
}

void SceneReplacementWorkflow::cancel() noexcept {
    request_.reset();
    cancel_workflow(phase_, error_);
}

WorkflowSnapshot SceneReplacementWorkflow::snapshot() const {
    return workflow_snapshot(phase_, error_);
}

bool SceneReplacementWorkflow::attempted_this_frame() const noexcept {
    return attempted_this_frame_;
}

void ModelSaveWorkflow::begin_frame() noexcept {
    attempted_this_frame_ = false;
}

WorkflowActivation ModelSaveWorkflow::activate(ModelSaveRequest request) {
    if (workflow_busy(phase_)) {
        return WorkflowActivation::busy;
    }
    if (attempted_this_frame_) {
        return WorkflowActivation::frame_limit_reached;
    }
    request_ = std::move(request);
    error_.reset();
    phase_ = WorkflowPhase::ready;
    return WorkflowActivation::accepted;
}

std::optional<ModelSaveRequest> ModelSaveWorkflow::begin_execution() {
    if (phase_ != WorkflowPhase::ready || !request_.has_value() || attempted_this_frame_) {
        return std::nullopt;
    }
    attempted_this_frame_ = true;
    phase_ = WorkflowPhase::executing;
    std::optional<ModelSaveRequest> result = std::move(request_);
    request_.reset();
    return result;
}

void ModelSaveWorkflow::succeed() noexcept {
    complete_success(phase_, error_);
}

void ModelSaveWorkflow::fail(const Error& error) {
    complete_failure(phase_, error_, error);
}

void ModelSaveWorkflow::cancel() noexcept {
    request_.reset();
    cancel_workflow(phase_, error_);
}

WorkflowSnapshot ModelSaveWorkflow::snapshot() const {
    return workflow_snapshot(phase_, error_);
}

WorkflowActivation ExternalEditorWorkflow::activate(ExternalEditorLaunchRequest request) {
    if (workflow_busy(phase_)) {
        return WorkflowActivation::busy;
    }
    request_ = std::move(request);
    error_.reset();
    phase_ = WorkflowPhase::ready;
    return WorkflowActivation::accepted;
}

std::optional<ExternalEditorLaunchRequest> ExternalEditorWorkflow::begin_execution() {
    if (phase_ != WorkflowPhase::ready || !request_.has_value()) {
        return std::nullopt;
    }
    phase_ = WorkflowPhase::executing;
    std::optional<ExternalEditorLaunchRequest> result = std::move(request_);
    request_.reset();
    return result;
}

void ExternalEditorWorkflow::succeed() noexcept {
    complete_success(phase_, error_);
}

void ExternalEditorWorkflow::fail(const Error& error) {
    complete_failure(phase_, error_, error);
}

void ExternalEditorWorkflow::cancel() noexcept {
    request_.reset();
    cancel_workflow(phase_, error_);
}

WorkflowSnapshot ExternalEditorWorkflow::snapshot() const {
    return workflow_snapshot(phase_, error_);
}

} // namespace elf3d::viewer
