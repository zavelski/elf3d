#pragma once

#include "viewer_commands.hpp"
#include "viewer_workflows.hpp"

#include <elf3d/app/interaction.h>
#include <elf3d/scene.h>
#include <memory>

namespace elf3d {
class Engine;
class Viewport;
} // namespace elf3d

namespace elf3d::viewer {

struct FileBrowserState;
struct SceneHierarchyComponentState;
struct SceneSession;
struct ViewerNotificationState;
struct ViewerPreferencesState;
struct ViewerRenderingState;
struct ViewerPerformanceState;
class ToolCoordinator;

struct ViewerWorkflowContext {
    Engine& engine;
    std::unique_ptr<Viewport>& viewport;
    ViewerRenderingState& rendering;
    ViewerNotificationState& notifications;
    SceneHierarchyComponentState& hierarchy;
    ViewerPreferencesState& preferences;
    SceneSession& scene;
    ToolCoordinator& tools;
    SceneReplacementWorkflow& scene_replacement;
    ModelSaveWorkflow& save;
    ModelWriteReport& write_report;
    ViewerPerformanceState& performance;
    FileBrowserState& browser;
    InteractionArbiter& arbiter;
    InteractionOwnerId interaction_owner;
    InteractionRegionId& interaction_region;
    bool replacement_allowed = true;
    bool model_busy = false;
};

[[nodiscard]] ViewerCommandCompletion execute_scene_workflow(const ViewerWorkflowContext& context,
                                                             SceneReplacementRequest request);
[[nodiscard]] ViewerCommandCompletion execute_save_workflow(const ViewerWorkflowContext& context,
                                                            ModelSaveRequest request);
void execute_external_editor_workflow(FileBrowserState& browser, ExternalEditorWorkflow& workflow);

} // namespace elf3d::viewer
