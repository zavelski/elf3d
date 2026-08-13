#pragma once

#include <elf3d/elf3d.h>

#include <imgui.h>

#include <string>

#include "viewer_commands.hpp"
#include "viewer_component_state.hpp"
#include "viewer_scene_session.hpp"
#include "viewer_tools.hpp"

namespace elf3d::viewer {

[[nodiscard]] std::string entity_label(const SceneSession& scene, EntityId entity);
[[nodiscard]] std::string selected_entity_label(const SceneSession& scene,
                                                const SelectionSnapshot& selection);
void build_model_information(ImGuiID dockspace_id, ViewerFrameContext& state,
                             const SceneSession& scene);
void build_rendering_panel(ImGuiID dockspace_id, ViewerFrameContext& state, SceneSession& scene);
void build_navigation_settings_window(ImGuiID dockspace_id, ViewerFrameContext& state,
                                      Viewport& viewport);
void build_selection_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                           const SceneSession& scene, Viewport& viewport, ToolCoordinator& tools);
void build_measurement_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                             const SceneSession& scene, Viewport& viewport, ToolCoordinator& tools);
void build_clipping_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                          const SceneSession& scene, Viewport& viewport, ToolCoordinator& tools);
void build_scene_hierarchy_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                                 SceneSession& scene, Viewport& viewport,
                                 ViewerCommandDispatcher& commands);
void invalidate_hierarchy_snapshot(SceneSession& scene) noexcept;

void build_error_modal(ViewerFrameContext& state);
void build_save_error_modal(ViewerFrameContext& state);
void build_status_bar(const ViewerFrameContext& state, const Engine& engine,
                      const SceneSession& scene, const Viewport& viewport,
                      const ToolCoordinator& tools);
void build_about_window(ViewerFrameContext& state);

} // namespace elf3d::viewer
