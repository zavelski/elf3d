#pragma once

#include "viewer_assets.hpp"
#include "viewer_browser.hpp"
#include "viewer_components.hpp"
#include "viewer_workflow_execution.hpp"

namespace elf3d::viewer {

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

[[nodiscard]] ViewerFrameContext frame_context(ViewerAssembly& runtime) noexcept;
[[nodiscard]] ViewerWorkflowContext workflow_context(ViewerAssembly& runtime) noexcept;
void dispatch_viewer_commands(ViewerAssembly& runtime, ViewerCommandDispatcher& commands);

} // namespace elf3d::viewer
