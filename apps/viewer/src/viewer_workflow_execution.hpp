#pragma once

#include "viewer_commands.hpp"
#include "viewer_workflows.hpp"

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
class ToolCoordinator;

struct ViewerWorkflowContext {
    Engine& engine;
    Viewport& viewport;
    ViewerRenderingState& rendering;
    ViewerNotificationState& notifications;
    SceneHierarchyComponentState& hierarchy;
    ViewerPreferencesState& preferences;
    SceneSession& scene;
    ToolCoordinator& tools;
    SceneReplacementWorkflow& scene_replacement;
    ModelSaveWorkflow& save;
};

[[nodiscard]] ViewerCommandCompletion execute_scene_workflow(const ViewerWorkflowContext& context,
                                                             SceneReplacementRequest request);
[[nodiscard]] ViewerCommandCompletion execute_save_workflow(const ViewerWorkflowContext& context,
                                                            ModelSaveRequest request);
void execute_external_editor_workflow(FileBrowserState& browser, ExternalEditorWorkflow& workflow);

} // namespace elf3d::viewer
