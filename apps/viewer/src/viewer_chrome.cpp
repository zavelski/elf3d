#include "viewer_internal.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace elf3d::viewer {

void build_file_menu(GLFWwindow* window, ViewerState& state, const ViewerScene& scene,
                     ViewerCommands& commands) {
    if (!ImGui::BeginMenu("File")) {
        return;
    }
    if (ImGui::MenuItem("Open...")) {
        state.request_open_modal = true;
    }
    ImGui::BeginDisabled(!scene.is_imported());
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
        state.request_save_modal = true;
    }
    ImGui::Separator();
    commands.reload = ImGui::MenuItem("Reload");
    commands.close_scene = ImGui::MenuItem("Close Scene");
    ImGui::EndDisabled();
    ImGui::Separator();
    if (ImGui::MenuItem("Exit")) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    ImGui::EndMenu();
}

void build_view_menu(ViewerState& state) {
    if (!ImGui::BeginMenu("View")) {
        return;
    }
    ImGui::MenuItem("3D View", nullptr, &state.show_3d_view);
    ImGui::MenuItem("Scene Hierarchy", nullptr, &state.show_scene_hierarchy);
    ImGui::MenuItem("Model Information", nullptr, &state.show_model_information);
    ImGui::MenuItem("Rendering", nullptr, &state.show_rendering_panel);
    ImGui::MenuItem("Selection", nullptr, &state.show_selection_panel);
    ImGui::MenuItem("Measurement", nullptr, &state.show_measurement_panel);
    ImGui::MenuItem("Clipping", nullptr, &state.show_clipping_panel);
    ImGui::MenuItem("Dear ImGui Demo", nullptr, &state.show_imgui_demo);
    ImGui::MenuItem("Status Bar", nullptr, &state.show_status_bar);
    if (ImGui::MenuItem("Reset Layout")) {
        state.reset_dock_layout = true;
    }
    ImGui::EndMenu();
}

void build_tools_menu(elf3d::Viewport& viewport, ViewerCommands& commands) {
    if (!ImGui::BeginMenu("Tools")) {
        return;
    }
    const elf3d::ViewportTool active_tool = viewport.active_tool();
    commands.select_tool =
        ImGui::MenuItem("Select", "S", active_tool == elf3d::ViewportTool::selection);
    commands.measure_tool = ImGui::MenuItem(
        "Measure Distance", "M", active_tool == elf3d::ViewportTool::distance_measurement);
    commands.show_clipping_panel = ImGui::MenuItem("Clipping");
    ImGui::EndMenu();
}

void build_clipping_menu(elf3d::Viewport& viewport, ViewerCommands& commands) {
    if (!ImGui::BeginMenu("Clipping")) {
        return;
    }
    const elf3d::ClippingSnapshot clipping = viewport.clipping_snapshot();
    commands.enable_section_plane =
        ImGui::MenuItem("Enable Section Plane", nullptr, clipping.section_plane.enabled);
    ImGui::BeginDisabled(!clipping.section_plane.enabled);
    commands.flip_section_side = ImGui::MenuItem("Flip Section Side");
    ImGui::EndDisabled();
    ImGui::BeginDisabled(clipping.box_count >= elf3d::maximum_clipping_boxes);
    commands.add_clipping_box_from_bounds = ImGui::MenuItem("Add Box from Visible Bounds");
    ImGui::EndDisabled();
    commands.clear_clipping = ImGui::MenuItem("Clear Clipping");
    commands.toggle_clipping_helpers =
        ImGui::MenuItem("Show Helpers", nullptr, clipping.helpers.visible);
    commands.fit_to_clipped_content = ImGui::MenuItem("Fit to Clipped Content");
    ImGui::EndMenu();
}

void build_camera_menu(ViewerState& state, const ViewerScene& scene, elf3d::Viewport& viewport,
                       ViewerCommands& commands) {
    if (!ImGui::BeginMenu("Camera")) {
        return;
    }
    const elf3d::Result<std::optional<elf3d::Bounds3>> visible_bounds =
        viewport.visible_bounds(*scene.scene);
    ImGui::BeginDisabled(!visible_bounds || !visible_bounds.value().has_value() ||
                         !has_nonzero_extent(state.view_dimensions) || !state.show_3d_view);
    commands.fit_to_scene = ImGui::MenuItem("Fit to Scene", "F");
    commands.reset_view = ImGui::MenuItem("Reset View", "Home");
    ImGui::EndDisabled();
    ImGui::Separator();
    const bool navigation_enabled = viewport.navigation_enabled();
    if (ImGui::MenuItem("Enable Navigation", nullptr, navigation_enabled)) {
        viewport.set_navigation_enabled(!navigation_enabled);
    }
    if (ImGui::MenuItem("Navigation Settings...")) {
        state.show_navigation_settings = true;
    }
    ImGui::EndMenu();
}

void build_selection_menu(ViewerState& state, elf3d::Viewport& viewport, ViewerCommands& commands) {
    if (!ImGui::BeginMenu("Selection")) {
        return;
    }
    ImGui::BeginDisabled(!viewport.has_selection());
    commands.clear_selection = ImGui::MenuItem("Clear Selection");
    commands.hide_selected = ImGui::MenuItem("Hide Selected");
    commands.show_selected = ImGui::MenuItem("Show Selected");
    commands.isolate_selected = ImGui::MenuItem("Isolate Selected");
    ImGui::EndDisabled();
    ImGui::Separator();
    commands.show_all = ImGui::MenuItem("Show All");
    ImGui::BeginDisabled(!viewport.is_isolating());
    commands.exit_isolation = ImGui::MenuItem("Exit Isolation");
    ImGui::EndDisabled();
    ImGui::Separator();
    const bool selection_enabled = viewport.selection_enabled();
    if (ImGui::MenuItem("Enable Selection", nullptr, selection_enabled)) {
        viewport.set_selection_enabled(!selection_enabled);
    }
    if (ImGui::MenuItem("Selection Settings...")) {
        state.show_selection_panel = true;
    }
    ImGui::EndMenu();
}

void build_measurement_menu(ViewerState& state, const ViewerScene& scene, elf3d::Viewport& viewport,
                            ViewerCommands& commands) {
    if (!ImGui::BeginMenu("Measurement")) {
        return;
    }
    const elf3d::DistanceMeasurementSnapshot measurement =
        viewport.distance_measurement_snapshot(*scene.scene);
    const bool incomplete =
        measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point;
    const bool has_measurement = measurement.first_point.has_value() ||
                                 measurement.second_point.has_value() ||
                                 measurement.preview_point.has_value();
    ImGui::BeginDisabled(!incomplete);
    commands.cancel_measurement = ImGui::MenuItem("Cancel Current", "Escape");
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!has_measurement);
    commands.clear_measurement = ImGui::MenuItem("Clear Measurement", "Delete");
    ImGui::EndDisabled();
    if (ImGui::MenuItem("Measurement Settings...")) {
        state.show_measurement_panel = true;
    }
    ImGui::EndMenu();
}

void build_help_menu(ViewerState& state) {
    if (!ImGui::BeginMenu("Help")) {
        return;
    }
    if (ImGui::MenuItem("About Elf3D")) {
        state.show_about = true;
    }
    ImGui::EndMenu();
}

void build_main_menu(GLFWwindow* window, ViewerState& state, const ViewerScene& scene,
                     elf3d::Viewport& engine_viewport, ViewerCommands& commands) {
    if (!ImGui::BeginMainMenuBar()) {
        state.main_menu_height = ImGui::GetFrameHeight();
        return;
    }
    state.main_menu_height = ImGui::GetWindowSize().y;
    build_file_menu(window, state, scene, commands);
    build_view_menu(state);
    build_tools_menu(engine_viewport, commands);
    build_clipping_menu(engine_viewport, commands);
    build_camera_menu(state, scene, engine_viewport, commands);
    build_selection_menu(state, engine_viewport, commands);
    build_measurement_menu(state, scene, engine_viewport, commands);
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
    ViewerState* state = nullptr;
    const ToolbarIcons* icons = nullptr;
    const ViewerScene* scene = nullptr;
    elf3d::Viewport* viewport = nullptr;
    ViewerCommands* commands = nullptr;
    float icon_size = 0.0F;
};

void draw_toolbar_file_group(const ToolbarContext& context) {
    if (toolbar_button(*context.icons, ToolbarIcon::open,
                       {"open", "Open glTF or GLB", context.icon_size})) {
        context.state->request_open_modal = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::save_as,
                       {"save-as", "Save model as glTF or GLB", context.icon_size, false,
                        context.scene->is_imported()})) {
        context.state->request_save_modal = true;
    }
}

void draw_toolbar_camera_group(const ToolbarContext& context) {
    if (toolbar_button(*context.icons, ToolbarIcon::fit_view,
                       {"fit-view", "Fit visible content", context.icon_size, false,
                        has_nonzero_extent(context.state->view_dimensions)})) {
        context.commands->fit_to_scene = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::reset_camera,
                       {"reset-camera", "Reset camera", context.icon_size, false,
                        has_nonzero_extent(context.state->view_dimensions)})) {
        context.commands->reset_view = true;
    }
}

void draw_toolbar_tool_group(const ToolbarContext& context) {
    const elf3d::ViewportTool active_tool = context.viewport->active_tool();
    if (toolbar_button(*context.icons, ToolbarIcon::select,
                       {"select", "Selection tool", context.icon_size,
                        active_tool == elf3d::ViewportTool::selection})) {
        context.commands->select_tool = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::measure,
                       {"measure", "Distance measurement tool", context.icon_size,
                        active_tool == elf3d::ViewportTool::distance_measurement})) {
        context.commands->measure_tool = true;
    }
}

void draw_toolbar_clipping_group(const ToolbarContext& context) {
    if (toolbar_button(*context.icons, ToolbarIcon::clipping_panel,
                       {"clipping-panel", "Clipping panel", context.icon_size})) {
        context.commands->show_clipping_panel = true;
    }
    ImGui::SameLine();
    const elf3d::ClippingSnapshot clipping = context.viewport->clipping_snapshot();
    if (toolbar_button(*context.icons, ToolbarIcon::section_plane,
                       {"section-plane", "Toggle section plane", context.icon_size,
                        clipping.section_plane.enabled})) {
        context.commands->enable_section_plane = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::add_clipping_box,
                       {"add-clipping-box", "Add clipping box", context.icon_size})) {
        context.commands->add_clipping_box_from_bounds = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::clear_clipping,
                       {"clear-clipping", "Clear clipping", context.icon_size})) {
        context.commands->clear_clipping = true;
    }
}

void draw_toolbar_visibility_group(const ToolbarContext& context) {
    const bool has_selection = context.viewport->has_selection();
    if (toolbar_button(
            *context.icons, ToolbarIcon::hide_selected,
            {"hide-selected", "Hide selected entity", context.icon_size, false, has_selection})) {
        context.commands->hide_selected = true;
    }
    ImGui::SameLine();
    if (toolbar_button(
            *context.icons, ToolbarIcon::show_selected,
            {"show-selected", "Show selected entity", context.icon_size, false, has_selection})) {
        context.commands->show_selected = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::isolate_selected,
                       {"isolate-selected", "Isolate selected entity", context.icon_size, false,
                        has_selection})) {
        context.commands->isolate_selected = true;
    }
    ImGui::SameLine();
    if (toolbar_button(*context.icons, ToolbarIcon::show_all,
                       {"show-all", "Show all entities", context.icon_size})) {
        context.commands->show_all = true;
    }
}

void draw_toolbar_content(const ToolbarContext& context, float button_size) {
    ImGui::SetCursorPosY((context.state->toolbar_height - button_size) * 0.5F +
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
        context.state->reset_dock_layout = true;
    }
}

void build_toolbar(ViewerState& state, const ToolbarIcons& icons, const ViewerScene& scene,
                   elf3d::Viewport& engine_viewport, ViewerCommands& commands) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float base_toolbar_height =
        std::max(ImGui::GetFrameHeight() * 1.5F, state.main_menu_height * 1.6F);
    const float icon_size = base_toolbar_height * 45.0F / 55.0F;
    const float button_size = icon_size + 2.0F * toolbar_button_frame_padding;
    state.toolbar_height =
        std::max(base_toolbar_height, button_size + 2.0F * toolbar_button_vertical_margin);
    ImGui::SetNextWindowPos(ImVec2{viewport->Pos.x, viewport->Pos.y + state.main_menu_height});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, state.toolbar_height});
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{6.0F, 0.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{3.0F, 0.0F});
    if (ImGui::Begin("##Elf3DToolbar", nullptr, flags)) {
        draw_toolbar_content(
            ToolbarContext{&state, &icons, &scene, &engine_viewport, &commands, icon_size},
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
    const bool hovered = ImGui::IsMouseHoveringRect(tab_rect.Min, tab_rect.Max);
    const bool selected = tab.ID == context.tab_bar->SelectedTabId;
    if (!selected && !hovered) {
        return;
    }
    const ImVec2 button_pos{
        std::max(tab_rect.Min.x, tab_rect.Max.x - context.frame_padding.x - context.button_size),
        tab_rect.Min.y + context.frame_padding.y};
    const ImRect button_rect{
        button_pos, ImVec2{button_pos.x + context.button_size, button_pos.y + context.button_size}};
    const bool button_hovered = ImGui::IsMouseHoveringRect(button_rect.Min, button_rect.Max, false);
    const ImU32 background = button_hovered
                                 ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
                                 : ImGui::GetColorU32(selected ? ImGuiCol_TabActive : ImGuiCol_Tab);
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

ImGuiID build_main_dockspace(ViewerState& state) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float status_height = state.show_status_bar ? ImGui::GetFrameHeight() : 0.0F;
    const float top = state.main_menu_height + state.toolbar_height;
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
    if (state.reset_dock_layout || !state.dock_layout_initialized ||
        ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        const ImVec2 dockspace_size = ImGui::GetContentRegionAvail();
        if (dockspace_size.x > 0.0F && dockspace_size.y > 0.0F) {
            const DockLayoutNodes nodes =
                initialize_default_dock_layout(dockspace_id, dockspace_size);
            state.dock_center_id = nodes.center;
            state.dock_right_id = nodes.right;
            state.dock_right_bottom_id = nodes.right_bottom;
            state.reset_dock_layout = false;
            state.dock_layout_initialized = true;
            state.apply_dock_layout = true;
        }
    }
    {
        const ScopedFont panel_title_font{state.panel_title_font};
        ImGui::DockSpace(dockspace_id, ImVec2{0.0F, 0.0F}, ImGuiDockNodeFlags_None);
        draw_compact_tab_close_buttons(ImGui::DockBuilderGetNode(dockspace_id));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return dockspace_id;
}

} // namespace elf3d::viewer
