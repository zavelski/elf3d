#pragma once

#include <elf3d/elf3d.h>

#include <filesystem>
#include <optional>
#include <string>

namespace elf3d::viewer {

enum class WorkflowPhase {
    idle,
    ready,
    executing,
    succeeded,
    failed,
    cancelled,
};

enum class WorkflowActivation {
    accepted,
    busy,
    frame_limit_reached,
};

enum class SceneReplacementKind {
    open_model,
    dropped_file,
    reload_model,
    close_to_demo,
    create_demo,
};

struct SceneReplacementRequest {
    SceneReplacementKind kind = SceneReplacementKind::open_model;
    std::string source_path;
};

struct WorkflowSnapshot {
    WorkflowPhase phase = WorkflowPhase::idle;
    std::optional<Error> error;
};

class SceneReplacementWorkflow final {
  public:
    void begin_frame() noexcept;
    [[nodiscard]] WorkflowActivation activate(SceneReplacementRequest request);
    [[nodiscard]] std::optional<SceneReplacementRequest> begin_execution();
    void succeed() noexcept;
    void fail(const Error& error);
    void cancel() noexcept;
    [[nodiscard]] WorkflowSnapshot snapshot() const;
    [[nodiscard]] bool attempted_this_frame() const noexcept;

  private:
    WorkflowPhase phase_ = WorkflowPhase::idle;
    std::optional<SceneReplacementRequest> request_;
    std::optional<Error> error_;
    bool attempted_this_frame_ = false;
};

struct ModelSaveRequest {
    std::string target_path;
};

class ModelSaveWorkflow final {
  public:
    void begin_frame() noexcept;
    [[nodiscard]] WorkflowActivation activate(ModelSaveRequest request);
    [[nodiscard]] std::optional<ModelSaveRequest> begin_execution();
    void succeed() noexcept;
    void fail(const Error& error);
    void cancel() noexcept;
    [[nodiscard]] WorkflowSnapshot snapshot() const;

  private:
    WorkflowPhase phase_ = WorkflowPhase::idle;
    std::optional<ModelSaveRequest> request_;
    std::optional<Error> error_;
    bool attempted_this_frame_ = false;
};

struct ExternalEditorLaunchRequest {
    std::filesystem::path editor;
    std::filesystem::path file;
    std::string editor_label;
};

class ExternalEditorWorkflow final {
  public:
    [[nodiscard]] WorkflowActivation activate(ExternalEditorLaunchRequest request);
    [[nodiscard]] std::optional<ExternalEditorLaunchRequest> begin_execution();
    void succeed() noexcept;
    void fail(const Error& error);
    void cancel() noexcept;
    [[nodiscard]] WorkflowSnapshot snapshot() const;

  private:
    WorkflowPhase phase_ = WorkflowPhase::idle;
    std::optional<ExternalEditorLaunchRequest> request_;
    std::optional<Error> error_;
};

} // namespace elf3d::viewer
