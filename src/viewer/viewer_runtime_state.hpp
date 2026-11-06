#pragma once

#include "viewer_assets.hpp"
#include "viewer_browser.hpp"
#include "viewer_components.hpp"
#include "viewer_graphics.hpp"
#include "viewer_workflow_execution.hpp"

namespace elf3d::viewer {

struct ViewerAssembly {
    elf3d::Engine* engine = nullptr;
    // Framework-owned, valid from Application::start until stop completes.
    InteractionArbiter* interaction_arbiter = nullptr;
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
    ModelWriteReport last_write_report;
    ExternalEditorWorkflow external_editor_workflow;
    ToolbarIcons toolbar_icons;
    InteractionOwnerId viewport_interaction_owner;
    InteractionRegionId viewport_interaction_region;
    bool exit_requested = false;
    bool session_replacement_allowed = true;
};

[[nodiscard]] ViewerFrameContext frame_context(ViewerAssembly& runtime) noexcept;
[[nodiscard]] ViewerWorkflowContext workflow_context(ViewerAssembly& runtime) noexcept;
[[nodiscard]] Result<void> dispatch_viewer_commands(ViewerAssembly& runtime,
                                                    ViewerCommandDispatcher& commands);
[[nodiscard]] ViewerCapabilitySnapshot viewer_capabilities(const ViewerAssembly& runtime) noexcept;
[[nodiscard]] Result<void> initialize_viewer_engine(ViewerAssembly& runtime, Engine& engine);
void begin_viewer_frame(ViewerAssembly& runtime) noexcept;
[[nodiscard]] Result<void> handle_file_dialog_result(ViewerAssembly& runtime,
                                                     const FileDialogResult& result);
[[nodiscard]] ViewerCommandCompletion execute_viewer_save_request(ViewerAssembly& runtime,
                                                                  ModelSaveRequest request);
[[nodiscard]] ViewerCommandCompletion execute_viewer_file_request(ViewerAssembly& runtime,
                                                                  SceneReplacementRequest request);

} // namespace elf3d::viewer
