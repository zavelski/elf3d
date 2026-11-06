#include "viewer_chrome.hpp"
#include "viewer_layout.hpp"

#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace elf3d::viewer {

void command_menu_item(const char* label, const char* shortcut, const ViewerCommand& command,
                       ViewerCommandDispatcher& commands, bool selected = false)
{
    const auto& capabilities = commands.capabilities();
    const bool enabled = viewer_command_enabled(command, capabilities);
    if (ImGui::MenuItem(label, shortcut, selected, enabled))
        commands.emit(command);
    if (!enabled)
        tooltip(viewer_command_disabled_reason(command, capabilities));
}

void build_file_menu(ViewerCommandDispatcher& commands)
{
    if (!ImGui::BeginMenu("File"))
        return;
    command_menu_item("Open...", nullptr, ShowOpenDialogCommand{}, commands);
    command_menu_item("Save", "Ctrl+S", SaveModelCommand{}, commands);
    command_menu_item("Save As...", "Ctrl+Shift+S", ShowSaveDialogCommand{}, commands);
    ImGui::Separator();
    command_menu_item("Reload", nullptr, ReloadSceneCommand{}, commands);
    command_menu_item("Close Scene", nullptr, CloseSceneCommand{}, commands);
    ImGui::Separator();
    command_menu_item("Exit", nullptr, ExitViewerCommand{}, commands);
    ImGui::EndMenu();
}

void build_window_menu(ViewerFrameContext& state, ViewerCommandDispatcher& commands,
                       ViewerPreferencesState& preferences)
{
    if (!ImGui::BeginMenu("Window")) {
        return;
    }
    ImGui::MenuItem("3D View", nullptr, &state.shell.show_3d_view);
    ImGui::MenuItem("Scene Browser", nullptr, &state.shell.show_scene_hierarchy);
    ImGui::MenuItem("Model Information", nullptr, &state.shell.show_model_information);
    ImGui::MenuItem("Rendering", nullptr, &state.shell.show_rendering_panel);
    ImGui::MenuItem("Properties", nullptr, &state.shell.show_selection_panel);
    ImGui::MenuItem("Measurement", nullptr, &state.shell.show_measurement_panel);
    ImGui::MenuItem("Status Bar", nullptr, &state.shell.show_status_bar);
    build_viewer_appearance_menu(preferences);
    if (ImGui::MenuItem("Reset Layout")) {
        commands.emit(ResetViewerLayoutCommand{});
    }
    ImGui::EndMenu();
}

void build_tools_menu(const ToolCoordinator& tools, ViewerCommandDispatcher& commands)
{
    const auto active = tools.active_tool();
    command_menu_item("Select", "S", ActivateViewerToolCommand{ViewerTool::selection}, commands,
                      active == ViewerTool::selection);
    command_menu_item("Measure Distance", "M",
                      ActivateViewerToolCommand{ViewerTool::distance_measurement}, commands,
                      active == ViewerTool::distance_measurement);
}

void build_camera_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                       ViewerCommandDispatcher& commands)
{
    if (!ImGui::BeginMenu("Camera")) {
        return;
    }
    command_menu_item("Fit to Scene", "F", FitViewCommand{}, commands);
    command_menu_item("Frame Selected", "Shift+F", FrameSelectionCommand{}, commands);
    command_menu_item("Reset View", "Home", ResetViewCommand{}, commands);
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

void build_selected_entity_menu(ViewerCommandDispatcher& commands)
{
    command_menu_item("Select All Objects", nullptr, SelectAllEntitiesCommand{}, commands);
    command_menu_item("Clear Selection", nullptr, ClearSelectionCommand{}, commands);
    command_menu_item("Frame Selected", "Shift+F", FrameSelectionCommand{}, commands);
    command_menu_item("Hide Selected", nullptr, SetSelectionVisibilityCommand{false}, commands);
    command_menu_item("Show Selected", nullptr, SetSelectionVisibilityCommand{true}, commands);
    command_menu_item("Isolate Selected", nullptr, IsolateSelectionCommand{}, commands);
}

void build_scene_visibility_menu(ViewerCommandDispatcher& commands)
{
    ImGui::Separator();
    command_menu_item("Show All", nullptr, ShowAllEntitiesCommand{}, commands);
    command_menu_item("Exit Isolation", nullptr, ExitIsolationCommand{}, commands);
}

void build_selection_options_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                                  ToolCoordinator& tools)
{
    ImGui::Separator();
    const bool selection_enabled = tools.selection().enabled();
    if (ImGui::MenuItem("Enable Viewport Selection", nullptr, selection_enabled)) {
        tools.selection().set_enabled(!selection_enabled);
        if (selection_enabled) {
            tools.selection().clear(viewport);
        }
    }
    if (ImGui::MenuItem("Selection Settings...")) {
        state.shell.show_selection_panel = true;
        state.shell.expand_selection_settings = true;
    }
}

void build_selection_menu(ViewerFrameContext& state, elf3d::Viewport& viewport,
                          ToolCoordinator& tools, ViewerCommandDispatcher& commands)
{
    if (!ImGui::BeginMenu("Selection")) {
        return;
    }

    build_selected_entity_menu(commands);
    build_scene_visibility_menu(commands);
    build_selection_options_menu(state, viewport, tools);
    ImGui::EndMenu();
}

void build_measurement_menu(ViewerFrameContext& state, const ViewerCapabilitySnapshot& capabilities,
                            ViewerCommandDispatcher& commands)
{
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

void build_help_menu(ViewerFrameContext& state)
{
    if (!ImGui::BeginMenu("Help")) {
        return;
    }
    if (ImGui::MenuItem("Shortcuts"))
        state.shell.help_topic = ViewerHelpTopic::shortcuts;
    if (ImGui::MenuItem("Supported Model Limitations"))
        state.shell.help_topic = ViewerHelpTopic::model_limits;
    if (ImGui::MenuItem("Diagnostics"))
        state.shell.help_topic = ViewerHelpTopic::diagnostics;
    ImGui::Separator();
    if (ImGui::MenuItem("About Elf3D")) {
        state.shell.show_about = true;
    }
    ImGui::EndMenu();
}

void build_render_view_menu(ViewerFrameContext& state, Viewport& viewport,
                            ViewerCommandDispatcher& commands)
{
    if (!ImGui::BeginMenu("View")) {
        return;
    }
    command_menu_item("Standard PBR", nullptr, SetShadingModeCommand{RenderShadingMode::standard},
                      commands, state.rendering.shading_mode == RenderShadingMode::standard);
    command_menu_item("Unlit", nullptr, SetShadingModeCommand{RenderShadingMode::unlit}, commands,
                      state.rendering.shading_mode == RenderShadingMode::unlit);
    command_menu_item("Backface Culling", nullptr,
                      SetBackfaceCullingCommand{!state.rendering.backface_culling_enabled},
                      commands, state.rendering.backface_culling_enabled);
    build_camera_menu(state, viewport, commands);
    ImGui::EndMenu();
}

void build_main_menu(const ToolbarBuildContext& build)
{
    auto& state = build.state;
    auto& engine_viewport = build.viewport;
    auto& tools = build.tools;
    const auto& capabilities = build.capabilities;
    auto& commands = build.commands;
    if (!ImGui::BeginMainMenuBar()) {
        state.shell.main_menu_height = ImGui::GetFrameHeight();
        return;
    }
    state.shell.main_menu_height = ImGui::GetWindowSize().y;
    build_file_menu(commands);
    build_selection_menu(state, engine_viewport, tools, commands);
    build_render_view_menu(state, engine_viewport, commands);
    if (ImGui::BeginMenu("Tools")) {
        build_tools_menu(tools, commands);
        build_measurement_menu(state, capabilities, commands);
        ImGui::EndMenu();
    }
    build_window_menu(state, commands, build.preferences);
    build_help_menu(state);
    ImGui::EndMainMenuBar();
}

void tooltip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) {
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

[[nodiscard]] ToolbarButtonPalette toolbar_button_palette(bool enabled, bool selected) noexcept
{
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
                    const ToolbarButtonDescription& description)
{
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

void toolbar_group_gap()
{
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

void toolbar_command_button(const ToolbarContext& context, ToolbarIcon icon,
                            ToolbarButtonDescription description, ViewerCommand command)
{
    description.enabled = viewer_command_enabled(command, *context.capabilities);
    if (toolbar_button(*context.icons, icon, description))
        context.commands->emit(command);
    if (!description.enabled &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s\n%s", description.tooltip,
                          viewer_command_disabled_reason(command, *context.capabilities));
}
void draw_toolbar_file_group(const ToolbarContext& context)
{
    toolbar_command_button(context, ToolbarIcon::open,
                           {"open", "Open glTF or GLB", context.icon_size},
                           ShowOpenDialogCommand{});
    ImGui::SameLine();
    toolbar_command_button(context, ToolbarIcon::save_as,
                           {"save", "Save current Document (Ctrl+S)", context.icon_size},
                           SaveModelCommand{});
}

enum class ViewModeGlyph { lit, unlit, house };
void draw_view_glyph(ViewModeGlyph glyph, ImVec2 minimum, float size)
{
    auto* draw = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_Text);
    const float x = minimum.x + size * 0.23F, y = minimum.y + size * 0.23F;
    const float edge = size * 0.54F;
    if (glyph == ViewModeGlyph::lit) {
        draw->AddCircle({x + edge / 2, y + edge / 2}, edge / 2, color, 16, 1.5F);
        draw->AddLine({x, y + edge / 2}, {x + edge, y + edge / 2}, color);
        draw->AddLine({x + edge / 2, y}, {x + edge / 2, y + edge}, color);
    } else if (glyph == ViewModeGlyph::unlit) {
        draw->AddRectFilled({x, y}, {x + edge, y + edge}, color, 2);
    } else {
        draw->AddTriangle({x, y + edge / 2}, {x + edge / 2, y}, {x + edge, y + edge / 2}, color);
        draw->AddRect({x + edge * 0.15F, y + edge / 2}, {x + edge * 0.85F, y + edge}, color);
    }
}
void view_mode_button(const ToolbarContext& context, const ToolbarButtonDescription& description,
                      ViewerCommand command, ViewModeGlyph glyph)
{
    const bool enabled = viewer_command_enabled(command, *context.capabilities);
    const auto palette = toolbar_button_palette(enabled, description.active);
    ImGui::PushID(description.id);
    ImGui::PushStyleColor(ImGuiCol_Button, palette.button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, palette.hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, palette.active);
    ImGui::BeginDisabled(!enabled);
    const float size = context.icon_size + 2 * toolbar_button_frame_padding;
    const bool pressed = ImGui::Button("##view-mode", {size, size});
    const auto minimum = ImGui::GetItemRectMin();
    draw_view_glyph(glyph, minimum, size);
    if (description.active)
        ImGui::GetWindowDrawList()->AddRect(minimum, ImGui::GetItemRectMax(),
                                            ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0F, 2.0F);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s%s%s", description.tooltip, enabled ? "" : "\n",
                          enabled ? ""
                                  : viewer_command_disabled_reason(command, *context.capabilities));
    if (pressed && enabled)
        context.commands->emit(std::move(command));
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}
void draw_toolbar_view_group(const ToolbarContext& context)
{
    view_mode_button(context,
                     {"view-pbr", "Standard PBR", context.icon_size,
                      context.state->rendering.shading_mode == RenderShadingMode::standard},
                     SetShadingModeCommand{RenderShadingMode::standard}, ViewModeGlyph::lit);
    ImGui::SameLine();
    view_mode_button(context,
                     {"view-unlit", "Unlit", context.icon_size,
                      context.state->rendering.shading_mode == RenderShadingMode::unlit},
                     SetShadingModeCommand{RenderShadingMode::unlit}, ViewModeGlyph::unlit);
}
void draw_toolbar_camera_group(const ToolbarContext& context)
{
    toolbar_command_button(context, ToolbarIcon::fit_view,
                           {"fit-view", "Fit visible content", context.icon_size},
                           FitViewCommand{});
    ImGui::SameLine();
    toolbar_command_button(context, ToolbarIcon::reset_camera,
                           {"reset-camera", "Reset camera", context.icon_size}, ResetViewCommand{});
}

void draw_toolbar_tool_group(const ToolbarContext& context)
{
    const auto active = context.tools->active_tool();
    toolbar_command_button(
        context, ToolbarIcon::select,
        {"select", "Selection tool", context.icon_size, active == ViewerTool::selection},
        ActivateViewerToolCommand{ViewerTool::selection});
    ImGui::SameLine();
    toolbar_command_button(context, ToolbarIcon::measure,
                           {"measure", "Distance measurement tool", context.icon_size,
                            active == ViewerTool::distance_measurement},
                           ActivateViewerToolCommand{ViewerTool::distance_measurement});
}

void draw_toolbar_visibility_group(const ToolbarContext& context)
{
    toolbar_command_button(
        context, ToolbarIcon::hide_selected,
        {"hide-selected", "Hide selected objects or instances", context.icon_size},
        SetSelectionVisibilityCommand{false});
    ImGui::SameLine();
    toolbar_command_button(
        context, ToolbarIcon::show_selected,
        {"show-selected", "Show selected objects or instances", context.icon_size},
        SetSelectionVisibilityCommand{true});
    ImGui::SameLine();
    toolbar_command_button(
        context, ToolbarIcon::isolate_selected,
        {"isolate-selected", "Isolate selected objects or instances", context.icon_size},
        IsolateSelectionCommand{});
    ImGui::SameLine();
    toolbar_command_button(context, ToolbarIcon::show_all,
                           {"show-all", "Show all objects", context.icon_size},
                           ShowAllEntitiesCommand{});
}

void draw_toolbar_content(const ToolbarContext& context, float button_size)
{
    ImGui::SetCursorPosY((context.state->shell.toolbar_height - button_size) * 0.5F +
                         toolbar_button_visual_center_offset);
    draw_toolbar_file_group(context);
    toolbar_group_gap();
    draw_toolbar_view_group(context);
    toolbar_group_gap();
    draw_toolbar_camera_group(context);
    toolbar_group_gap();
    draw_toolbar_tool_group(context);
    toolbar_group_gap();
    draw_toolbar_visibility_group(context);
    toolbar_group_gap();
    toolbar_command_button(context, ToolbarIcon::reset_layout,
                           {"reset-layout", "Reset dock layout", context.icon_size},
                           ResetViewerLayoutCommand{});
}

void build_toolbar(const ToolbarBuildContext& build)
{
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
        const ToolbarContext toolbar{&build.state, &build.icons,        &build.viewport,
                                     &build.tools, &build.capabilities, &build.commands,
                                     icon_size};
        draw_toolbar_content(toolbar, button_size);
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
                                                             ImVec2 dockspace_size)
{
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
    ImGui::DockBuilderDockWindow("Scene Browser", right);
    ImGui::DockBuilderDockWindow("Properties", right_bottom);
    ImGui::DockBuilderDockWindow("Rendering", right_bottom);
    ImGui::DockBuilderDockWindow("Measurement", right_bottom);
    ImGui::DockBuilderDockWindow("Navigation Settings", right_bottom);
    ImGui::DockBuilderFinish(dockspace_id);
    return DockLayoutNodes{center, right, right_bottom};
}

void set_default_dock(ImGuiID dock_id, bool force)
{
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

void draw_compact_tab_close_button(const CompactTabCloseContext& context, const ImGuiTabItem& tab)
{
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

void draw_compact_tab_close_buttons(ImGuiDockNode* node)
{
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

ImGuiID build_main_dockspace(ViewerFrameContext& state)
{
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
    adopt_saved_viewer_layout(state.shell, dockspace_id);
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
