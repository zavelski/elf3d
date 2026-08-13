#include "viewer_chrome.hpp"

#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace elf3d::viewer {

void build_file_menu(const ViewerCapabilitySnapshot& capabilities,
                     ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("File")) {
        return;
    }
    if (ImGui::MenuItem("Open...")) {
        commands.emit(ShowOpenDialogCommand{});
    }
    ImGui::BeginDisabled(!capabilities.scene_imported);
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        commands.emit(ShowSaveDialogCommand{});
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reload")) {
        commands.emit(ReloadSceneCommand{});
    }
    if (ImGui::MenuItem("Close Scene")) {
        commands.emit(CloseSceneCommand{});
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    if (ImGui::MenuItem("Exit")) {
        commands.emit(ExitViewerCommand{});
    }
    ImGui::EndMenu();
}

void build_view_menu(ViewerFrameContext& state, ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("View")) {
        return;
    }
    ImGui::MenuItem("3D View", nullptr, &state.shell.show_3d_view);
    ImGui::MenuItem("Scene Hierarchy", nullptr, &state.shell.show_scene_hierarchy);
    ImGui::MenuItem("Model Information", nullptr, &state.shell.show_model_information);
    ImGui::MenuItem("Rendering", nullptr, &state.shell.show_rendering_panel);
    ImGui::MenuItem("Selection", nullptr, &state.shell.show_selection_panel);
    ImGui::MenuItem("Measurement", nullptr, &state.shell.show_measurement_panel);
    ImGui::MenuItem("Clipping", nullptr, &state.shell.show_clipping_panel);
    ImGui::MenuItem("Dear ImGui Demo", nullptr, &state.shell.show_imgui_demo);
    ImGui::MenuItem("Status Bar", nullptr, &state.shell.show_status_bar);
    if (ImGui::MenuItem("Reset Layout")) {
        commands.emit(ResetViewerLayoutCommand{});
    }
    ImGui::EndMenu();
}

void build_tools_menu(const ToolCoordinator& tools, ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("Tools")) {
        return;
    }
    const ViewerTool active_tool = tools.active_tool();
    if (ImGui::MenuItem("Select", "S", active_tool == ViewerTool::selection)) {
        commands.emit(ActivateViewerToolCommand{ViewerTool::selection});
    }
    if (ImGui::MenuItem("Measure Distance", "M", active_tool == ViewerTool::distance_measurement)) {
        commands.emit(ActivateViewerToolCommand{ViewerTool::distance_measurement});
    }
    if (ImGui::MenuItem("Clipping")) {
        commands.emit(ShowViewerPanelCommand{ViewerPanel::clipping});
    }
    ImGui::EndMenu();
}

void build_clipping_menu(elf3d::Viewport& viewport, const ClippingTool& clipping_tool,
                         const ViewerCapabilitySnapshot& capabilities,
                         ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("Clipping")) {
        return;
    }
    const elf3d::ClippingSnapshot clipping = viewport.clipping_snapshot();
    if (ImGui::MenuItem("Enable Section Plane", nullptr, clipping.section_plane.enabled)) {
        commands.emit(ToggleSectionPlaneCommand{});
    }
    ImGui::BeginDisabled(!capabilities.section_plane_enabled);
    if (ImGui::MenuItem("Flip Section Side")) {
        commands.emit(FlipSectionPlaneCommand{});
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!capabilities.can_add_clipping_box ||
                         !capabilities.unclipped_visible_content);
    if (ImGui::MenuItem("Add Box from Visible Bounds")) {
        commands.emit(AddClippingBoxFromBoundsCommand{});
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!capabilities.has_clipping);
    if (ImGui::MenuItem("Clear Clipping")) {
        commands.emit(ClearClippingCommand{});
    }
    ImGui::EndDisabled();
    if (ImGui::MenuItem("Show Helpers", nullptr, clipping_tool.helpers_visible())) {
        commands.emit(ToggleClippingHelpersCommand{});
    }
    ImGui::BeginDisabled(!capabilities.visible_content);
    if (ImGui::MenuItem("Fit to Clipped Content")) {
        commands.emit(FitClippedContentCommand{});
    }
    ImGui::EndDisabled();
    ImGui::EndMenu();
}

void build_camera_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                       const ViewerCapabilitySnapshot& capabilities,
                       ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("Camera")) {
        return;
    }
    ImGui::BeginDisabled(!capabilities.view_available || !capabilities.visible_content);
    if (ImGui::MenuItem("Fit to Scene", "F")) {
        commands.emit(FitViewCommand{});
    }
    if (ImGui::MenuItem("Reset View", "Home")) {
        commands.emit(ResetViewCommand{});
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    const bool navigation_enabled = viewport.navigation_enabled();
    if (ImGui::MenuItem("Enable Navigation", nullptr, navigation_enabled)) {
        viewport.set_navigation_enabled(!navigation_enabled);
    }
    if (ImGui::MenuItem("Navigation Settings...")) {
        state.shell.show_navigation_settings = true;
    }
    ImGui::EndMenu();
}

void build_selected_entity_menu(const ViewerCapabilitySnapshot& capabilities,
                                ViewerCommandDispatcher& commands) {
    ImGui::BeginDisabled(!capabilities.selected_entity.has_value());
    if (ImGui::MenuItem("Clear Selection")) {
        commands.emit(ClearSelectionCommand{});
    }
    if (ImGui::MenuItem("Hide Selected") && capabilities.selected_entity.has_value()) {
        commands.emit(SetEntityVisibilityCommand{*capabilities.selected_entity, false});
    }
    if (ImGui::MenuItem("Show Selected") && capabilities.selected_entity.has_value()) {
        commands.emit(SetEntityVisibilityCommand{*capabilities.selected_entity, true});
    }
    if (ImGui::MenuItem("Isolate Selected") && capabilities.selected_entity.has_value()) {
        commands.emit(IsolateEntityCommand{*capabilities.selected_entity});
    }
    ImGui::EndDisabled();
}

void build_scene_visibility_menu(const ViewerCapabilitySnapshot& capabilities,
                                 ViewerCommandDispatcher& commands) {
    ImGui::Separator();
    if (ImGui::MenuItem("Show All")) {
        commands.emit(ShowAllEntitiesCommand{});
    }
    ImGui::BeginDisabled(!capabilities.isolating);
    if (ImGui::MenuItem("Exit Isolation")) {
        commands.emit(ExitIsolationCommand{});
    }
    ImGui::EndDisabled();
}

void build_selection_options_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                                  ToolCoordinator& tools) {
    ImGui::Separator();
    const bool selection_enabled = tools.selection().enabled();
    if (ImGui::MenuItem("Enable Selection", nullptr, selection_enabled)) {
        tools.selection().set_enabled(!selection_enabled);
        if (selection_enabled) {
            viewport.clear_selection();
        }
    }
    if (ImGui::MenuItem("Selection Settings...")) {
        state.shell.show_selection_panel = true;
    }
}

void build_selection_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                          ToolCoordinator& tools, const ViewerCapabilitySnapshot& capabilities,
                          ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("Selection")) {
        return;
    }
    build_selected_entity_menu(capabilities, commands);
    build_scene_visibility_menu(capabilities, commands);
    build_selection_options_menu(state, viewport, tools);
    ImGui::EndMenu();
}

void build_measurement_menu(ViewerFrameContext& state, const ViewerCapabilitySnapshot& capabilities,
                            ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMenu("Measurement")) {
        return;
    }
    ImGui::BeginDisabled(!capabilities.measurement_incomplete);
    if (ImGui::MenuItem("Cancel Current", "Escape")) {
        commands.emit(CancelMeasurementCommand{});
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!capabilities.has_measurement);
    if (ImGui::MenuItem("Clear Measurement", "Delete")) {
        commands.emit(ClearMeasurementCommand{});
    }
    ImGui::EndDisabled();
    if (ImGui::MenuItem("Measurement Settings...")) {
        state.shell.show_measurement_panel = true;
    }
    ImGui::EndMenu();
}

void build_help_menu(ViewerFrameContext& state) {
    if (!ImGui::BeginMenu("Help")) {
        return;
    }
    if (ImGui::MenuItem("About Elf3D")) {
        state.shell.show_about = true;
    }
    ImGui::EndMenu();
}

void build_main_menu(ViewerFrameContext& state, elf3d::Viewport& engine_viewport,
                     ToolCoordinator& tools, const ViewerCapabilitySnapshot& capabilities,
                     ViewerCommandDispatcher& commands) {
    if (!ImGui::BeginMainMenuBar()) {
        state.shell.main_menu_height = ImGui::GetFrameHeight();
        return;
    }
    state.shell.main_menu_height = ImGui::GetWindowSize().y;
    build_file_menu(capabilities, commands);
    build_view_menu(state, commands);
    build_tools_menu(tools, commands);
    build_clipping_menu(engine_viewport, tools.clipping(), capabilities, commands);
    build_camera_menu(state, engine_viewport, capabilities, commands);
    build_selection_menu(state, engine_viewport, tools, capabilities, commands);
    build_measurement_menu(state, capabilities, commands);
    build_help_menu(state);
    ImGui::EndMainMenuBar();
}

void tooltip(const char* text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("%s", text);
    }
}

constexpr float toolbar_button_frame_padding = 3.0F;
constexpr float toolbar_button_vertical_margin = 7.0F;
constexpr float toolbar_button_visual_center_offset = -0.5F;

struct ToolbarButtonPalette {
    ImVec4 button;
    ImVec4 hovered;
    ImVec4 active;
};

struct ToolbarButtonDescription {
    const char* id = nullptr;
    const char* tooltip = nullptr;
    float icon_size = 0.0F;
    bool active = false;
    bool enabled = true;
};

[[nodiscard]] ToolbarButtonPalette toolbar_button_palette(bool enabled, bool selected) noexcept {
    if (!enabled) {
        return ToolbarButtonPalette{
            ImVec4{0.70F, 0.76F, 0.76F, 0.30F},
            ImVec4{0.70F, 0.76F, 0.76F, 0.30F},
            ImVec4{0.70F, 0.76F, 0.76F, 0.30F},
        };
    }

    if (selected) {
        return ToolbarButtonPalette{
            ImVec4{0.75F, 1.00F, 1.00F, 0.95F},
            ImVec4{0.84F, 0.96F, 0.96F, 1.00F},
            ImVec4{0.66F, 0.90F, 0.94F, 1.00F},
        };
    }

    return ToolbarButtonPalette{
        ImVec4{0.76F, 0.84F, 0.84F, 0.48F},
        ImVec4{0.84F, 0.94F, 0.94F, 0.90F},
        ImVec4{0.70F, 0.90F, 0.93F, 1.00F},
    };
}

bool toolbar_button(const ToolbarIcons& icons, ToolbarIcon icon,
                    const ToolbarButtonDescription& description) {
    const ToolbarTexture& texture = icons.texture(icon);
    const ImVec2 image_size{description.icon_size, description.icon_size};
    const ImVec2 fallback_size{description.icon_size + 6.0F, description.icon_size + 6.0F};
    const ImVec4 transparent{0.0F, 0.0F, 0.0F, 0.0F};
    const ToolbarButtonPalette palette =
        toolbar_button_palette(description.enabled, description.active);

    ImGui::PushID(description.id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2{toolbar_button_frame_padding, toolbar_button_frame_padding});
    ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0F);
    ImGui::PushStyleColor(ImGuiCol_Button, palette.button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.active);
    if (!description.enabled) {
        ImGui::BeginDisabled();
    }
    const bool pressed =
        texture.is_valid() ? ImGui::ImageButton("image", texture.texture_ref(), image_size,
                                                ImVec2{0.0F, 0.0F}, ImVec2{1.0F, 1.0F}, transparent)
                           : ImGui::Button("?", fallback_size);
    if (!description.enabled) {
        ImGui::EndDisabled();
    }
    tooltip(description.tooltip);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return description.enabled && pressed;
}

void toolbar_group_gap() {
    ImGui::SameLine(0.0F, ImGui::GetStyle().ItemSpacing.x + 10.0F);
}

struct ToolbarContext {
    ViewerFrameContext* state = nullptr;
    const ToolbarIcons* icons = nullptr;
    elf3d::Viewport* viewport = nullptr;
    ToolCoordinator* tools = nullptr;
    const ViewerCapabilitySnapshot* capabilities = nullptr;
    ViewerCommandDispatcher* commands = nullptr;
    float icon_size = 0.0F;
};

void draw_toolbar_file_group(const ToolbarContext& context) {
    if (toolbar_button(*context.icons, ToolbarIcon::open,
                       {"open", "Open glTF or GLB", context.icon_size})) {
        context.commands->emit(ShowOpenDialogCommand{});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::save_as,
                       {"save-as", "Save model as glTF or GLB", context.icon_size, false,
                        context.capabilities->scene_imported})) {
        context.commands->emit(ShowSaveDialogCommand{});
    }
}

void draw_toolbar_camera_group(const ToolbarContext& context) {
    if (toolbar_button(
            *context.icons, ToolbarIcon::fit_view,
            {"fit-view", "Fit visible content", context.icon_size, false,
             context.capabilities->view_available && context.capabilities->visible_content})) {
        context.commands->emit(FitViewCommand{});
    }
    ImGui::SameLine();
    if (toolbar_button(
            *context.icons, ToolbarIcon::reset_camera,
            {"reset-camera", "Reset camera", context.icon_size, false,
             context.capabilities->view_available && context.capabilities->visible_content})) {
        context.commands->emit(ResetViewCommand{});
    }
}

void draw_toolbar_tool_group(const ToolbarContext& context) {
    const ViewerTool active_tool = context.tools->active_tool();
    if (toolbar_button(*context.icons, ToolbarIcon::select,
                       {"select", "Selection tool", context.icon_size,
                        active_tool == ViewerTool::selection})) {
        context.commands->emit(ActivateViewerToolCommand{ViewerTool::selection});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::measure,
                       {"measure", "Distance measurement tool", context.icon_size,
                        active_tool == ViewerTool::distance_measurement})) {
        context.commands->emit(ActivateViewerToolCommand{ViewerTool::distance_measurement});
    }
}

void draw_toolbar_clipping_group(const ToolbarContext& context) {
    if (toolbar_button(*context.icons, ToolbarIcon::clipping_panel,
                       {"clipping-panel", "Clipping panel", context.icon_size})) {
        context.commands->emit(ShowViewerPanelCommand{ViewerPanel::clipping});
    }
    ImGui::SameLine();
    const elf3d::ClippingSnapshot clipping = context.viewport->clipping_snapshot();
    if (toolbar_button(*context.icons, ToolbarIcon::section_plane,
                       {"section-plane", "Toggle section plane", context.icon_size,
                        clipping.section_plane.enabled})) {
        context.commands->emit(ToggleSectionPlaneCommand{});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::add_clipping_box,
                       {"add-clipping-box", "Add clipping box", context.icon_size, false,
                        context.capabilities->can_add_clipping_box &&
                            context.capabilities->unclipped_visible_content})) {
        context.commands->emit(AddClippingBoxFromBoundsCommand{});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::clear_clipping,
                       {"clear-clipping", "Clear clipping", context.icon_size, false,
                        context.capabilities->has_clipping})) {
        context.commands->emit(ClearClippingCommand{});
    }
}

void draw_toolbar_visibility_group(const ToolbarContext& context) {
    const std::optional<EntityId>& selected = context.capabilities->selected_entity;
    if (toolbar_button(*context.icons, ToolbarIcon::hide_selected,
                       {"hide-selected", "Hide selected entity", context.icon_size, false,
                        selected.has_value()})) {
        context.commands->emit(SetEntityVisibilityCommand{*selected, false});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::show_selected,
                       {"show-selected", "Show selected entity", context.icon_size, false,
                        selected.has_value()})) {
        context.commands->emit(SetEntityVisibilityCommand{*selected, true});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::isolate_selected,
                       {"isolate-selected", "Isolate selected entity", context.icon_size, false,
                        selected.has_value()})) {
        context.commands->emit(IsolateEntityCommand{*selected});
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::show_all,
                       {"show-all", "Show all entities", context.icon_size})) {
        context.commands->emit(ShowAllEntitiesCommand{});
    }
}

void draw_toolbar_content(const ToolbarContext& context, float button_size) {
    ImGui::SetCursorPosY((context.state->shell.toolbar_height - button_size) * 0.5F +
                         toolbar_button_visual_center_offset);
    draw_toolbar_file_group(context);
    toolbar_group_gap();
    draw_toolbar_camera_group(context);
    toolbar_group_gap();
    draw_toolbar_tool_group(context);
    toolbar_group_gap();
    draw_toolbar_clipping_group(context);
    toolbar_group_gap();
    draw_toolbar_visibility_group(context);
    toolbar_group_gap();
    if (toolbar_button(*context.icons, ToolbarIcon::reset_layout,
                       {"reset-layout", "Reset dock layout", context.icon_size})) {
        context.commands->emit(ResetViewerLayoutCommand{});
    }
}

void build_toolbar(const ToolbarBuildContext& build) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float base_toolbar_height =
        std::max(ImGui::GetFrameHeight() * 1.5F, build.state.shell.main_menu_height * 1.6F);
    const float icon_size = base_toolbar_height * 45.0F / 55.0F;
    const float button_size = icon_size + 2.0F * toolbar_button_frame_padding;
    build.state.shell.toolbar_height =
        std::max(base_toolbar_height, button_size + 2.0F * toolbar_button_vertical_margin);
    ImGui::SetNextWindowPos(
        ImVec2{viewport->Pos.x, viewport->Pos.y + build.state.shell.main_menu_height});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, build.state.shell.toolbar_height});
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{6.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{3.0F, 0.0F});
    if (ImGui::Begin("##Elf3DToolbar", nullptr, flags)) {
        draw_toolbar_content(ToolbarContext{&build.state, &build.icons, &build.viewport,
                                            &build.tools, &build.capabilities, &build.commands,
                                            icon_size},
                             button_size);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

struct DockLayoutNodes {
    ImGuiID center = 0;
    ImGuiID right = 0;
    ImGuiID right_bottom = 0;
};

[[nodiscard]] DockLayoutNodes initialize_default_dock_layout(ImGuiID dockspace_id,
                                                             ImVec2 dockspace_size) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

    ImGuiID center = dockspace_id;
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, side_dock_width_fraction,
                                                nullptr, &center);
    ImGuiID right_bottom =
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.28F, nullptr, &right);

    ImGui::DockBuilderDockWindow("3D View", center);
    ImGui::DockBuilderDockWindow("Model Information", center);
    ImGui::DockBuilderDockWindow("Scene Hierarchy", right);
    ImGui::DockBuilderDockWindow("Selection", right);
    ImGui::DockBuilderDockWindow("Rendering", right_bottom);
    ImGui::DockBuilderDockWindow("Measurement", right_bottom);
    ImGui::DockBuilderDockWindow("Clipping", right_bottom);
    ImGui::DockBuilderDockWindow("Navigation Settings", right_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
    return DockLayoutNodes{center, right, right_bottom};
}

void set_default_dock(ImGuiID dock_id, bool force) {
    if (dock_id != 0) {
        ImGui::SetNextWindowDockID(dock_id, force ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    }
}

struct CompactTabCloseContext {
    ImGuiTabBar* tab_bar = nullptr;
    ImDrawList* draw_list = nullptr;
    float button_size = 0.0F;
    ImVec2 frame_padding;
};

void draw_compact_tab_close_button(const CompactTabCloseContext& context, const ImGuiTabItem& tab) {
    if (tab.Window == nullptr || !tab.Window->HasCloseButton || tab.Width <= 0.0F) {
        return;
    }
    const ImRect tab_rect{
        ImVec2{context.tab_bar->BarRect.Min.x + tab.Offset, context.tab_bar->BarRect.Min.y},
        ImVec2{context.tab_bar->BarRect.Min.x + tab.Offset + tab.Width,
               context.tab_bar->BarRect.Max.y}};
    const bool selected = tab.ID == context.tab_bar->SelectedTabId;
    if (!selected) {
        return;
    }
    const ImVec2 button_pos{
        std::max(tab_rect.Min.x, tab_rect.Max.x - context.frame_padding.x - context.button_size),
        tab_rect.Min.y + context.frame_padding.y};
    const ImRect button_rect{
        button_pos, ImVec2{button_pos.x + context.button_size, button_pos.y + context.button_size}};
    const ImU32 background = ImGui::GetColorU32(ImGuiCol_TabActive);
    context.draw_list->AddRectFilled(button_rect.Min, button_rect.Max, background);

    const ImVec2 raw_center = button_rect.GetCenter();
    const ImVec2 center{raw_center.x - 0.5F, raw_center.y - 0.5F};
    const float extent = context.button_size * 0.18F;
    const float thickness = std::max(1.0F, context.button_size * 0.055F);
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    context.draw_list->AddLine(ImVec2{center.x + extent, center.y + extent},
                               ImVec2{center.x - extent, center.y - extent}, color, thickness);
    context.draw_list->AddLine(ImVec2{center.x + extent, center.y - extent},
                               ImVec2{center.x - extent, center.y + extent}, color, thickness);
}

void draw_compact_tab_close_buttons(ImGuiDockNode* node) {
    if (node == nullptr) {
        return;
    }
    draw_compact_tab_close_buttons(node->ChildNodes[0]);
    draw_compact_tab_close_buttons(node->ChildNodes[1]);

    ImGuiTabBar* tab_bar = node->TabBar;
    if (tab_bar == nullptr || node->HostWindow == nullptr) {
        return;
    }

    const CompactTabCloseContext context{tab_bar, node->HostWindow->DrawList, ImGui::GetFontSize(),
                                         tab_bar->FramePadding};
    for (int index = 0; index < tab_bar->Tabs.Size; ++index) {
        draw_compact_tab_close_button(context, tab_bar->Tabs[index]);
    }
}

ImGuiID build_main_dockspace(ViewerFrameContext& state) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float status_height = state.shell.show_status_bar ? ImGui::GetFrameHeight() : 0.0F;
    const float top = state.shell.main_menu_height + state.shell.toolbar_height;
    const float height = std::max(1.0F, viewport->Size.y - top - status_height);
    ImGui::SetNextWindowPos(ImVec2{viewport->Pos.x, viewport->Pos.y + top});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, height});
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0F, 0.0F});
    ImGui::Begin("##Elf3DDockspaceHost", nullptr, window_flags);
    const ImGuiID dockspace_id = ImGui::GetID("Elf3DDockspace");
    if (state.shell.reset_dock_layout || !state.shell.dock_layout_initialized ||
        ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        const ImVec2 dockspace_size = ImGui::GetContentRegionAvail();
        if (dockspace_size.x > 0.0F && dockspace_size.y > 0.0F) {
            const DockLayoutNodes nodes =
                initialize_default_dock_layout(dockspace_id, dockspace_size);
            state.shell.dock_center_id = nodes.center;
            state.shell.dock_right_id = nodes.right;
            state.shell.dock_right_bottom_id = nodes.right_bottom;
            state.shell.reset_dock_layout = false;
            state.shell.dock_layout_initialized = true;
            state.shell.apply_dock_layout = true;
        }
    }
    {
        const ScopedFont panel_title_font{state.presentation.panel_title_font};
        ImGui::DockSpace(dockspace_id, ImVec2{0.0F, 0.0F}, ImGuiDockNodeFlags_None);
        draw_compact_tab_close_buttons(ImGui::DockBuilderGetNode(dockspace_id));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return dockspace_id;
}

} // namespace elf3d::viewer
