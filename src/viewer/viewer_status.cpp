#include "viewer_components.hpp"

#include "viewer_browser.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>

namespace elf3d::viewer {

void build_error_modal(ViewerFrameContext& state)
{
    if (state.notifications.request_error_modal) {
        ImGui::OpenPopup("Model Load Error");
        state.notifications.request_error_modal = false;
    }

    push_professional_dialog_style();
    ImGui::SetNextWindowSize(ImVec2{600.0F, 0.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Model Load Error", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(ImVec4{1.00F, 0.470F, 0.390F, 1.00F}, "MODEL COULD NOT BE OPENED");
        ImGui::Separator();
        if (state.notifications.load_failure.has_value()) {
            ImGui::TextDisabled("SOURCE");
            ImGui::TextWrapped("%s", state.notifications.load_failure->source_path.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("CATEGORY");
            ImGui::TextUnformatted(error_category(state.notifications.load_failure->error.code()));
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.notifications.load_failure->error.message());
        }
        ImGui::Spacing();
        const float close_width = 118.0F;
        ImGui::SetCursorPosX(
            std::max(ImGui::GetCursorPosX(),
                     ImGui::GetWindowWidth() - close_width - ImGui::GetStyle().WindowPadding.x));
        if (ImGui::Button("Close", ImVec2{close_width, 0.0F}) || state.interaction.escape_pressed) {
            state.notifications.load_failure.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

void build_save_error_modal(ViewerFrameContext& state)
{
    if (state.notifications.request_save_error_modal) {
        ImGui::OpenPopup("Model Save Error");
        state.notifications.request_save_error_modal = false;
    }

    push_professional_dialog_style();
    ImGui::SetNextWindowSize(ImVec2{600.0F, 0.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Model Save Error", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(ImVec4{1.00F, 0.470F, 0.390F, 1.00F}, "MODEL COULD NOT BE SAVED");
        ImGui::Separator();
        if (state.notifications.save_failure.has_value()) {
            ImGui::TextDisabled("TARGET");
            ImGui::TextWrapped("%s", state.notifications.save_failure->source_path.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.notifications.save_failure->error.message());
        }
        if (ImGui::Button("Close", ImVec2{118.0F, 0.0F}) || state.interaction.escape_pressed) {
            state.notifications.save_failure.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

const char* graphics_backend_name(elf3d::GraphicsBackend backend) noexcept
{
    return backend == elf3d::GraphicsBackend::opengl ? "OpenGL 4.1 core" : "Unknown";
}

[[nodiscard]] float viewer_frame_rate(const ViewerFrameContext& state) noexcept
{
    return state.interaction.frame_delta_seconds > 0.0
               ? static_cast<float>(1.0 / state.interaction.frame_delta_seconds)
               : 0.0F;
}

void build_status_bar(const ViewerFrameContext& state, const elf3d::Engine& engine,
                      const SceneSession& scene, const elf3d::Viewport& engine_viewport,
                      const ToolCoordinator& tools)
{
    if (!state.shell.show_status_bar) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float status_height = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(
        ImVec2{viewport->Pos.x, viewport->Pos.y + viewport->Size.y - status_height});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, status_height});
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8.0F, 2.0F});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.86F, 0.86F, 0.86F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.00F, 0.00F, 0.00F, 1.00F});
    if (ImGui::Begin("##Elf3DStatusBar", nullptr, flags)) {
        const float fps = viewer_frame_rate(state);
        const float frame_time_ms = fps > 0.0F ? 1000.0F / fps : 0.0F;
        const elf3d::SelectionSnapshot selection = engine_viewport.selection_snapshot();
        const std::string selection_status = selected_entity_label(scene, selection);
        std::string isolation_status = "none";
        const std::optional<elf3d::EntityId> isolated = engine_viewport.isolated_entity();
        if (isolated.has_value()) {
            isolation_status = entity_label(scene, *isolated);
        }

        const DistanceMeasurementSnapshot measurement = tools.measurement().snapshot(
            *scene.scene, engine_viewport, tools.active_tool() == ViewerTool::distance_measurement);
        std::string tool_status = std::string{"Tool: "} + tool_name(tools.active_tool());
        if (tools.active_tool() == ViewerTool::distance_measurement) {
            tool_status += " | ";
            tool_status += measurement_state_name(measurement.state);
            if (measurement.preview_point.has_value()) {
                tool_status += " | ";
                tool_status += format_distance(measurement.preview_distance_meters,
                                               tools.measurement().settings().display_unit);
            }
        } else if (measurement.state == DistanceMeasurementState::complete) {
            tool_status += " | Measurement: ";
            tool_status += format_distance(measurement.distance_meters,
                                           tools.measurement().settings().display_unit);
        }
        ImGui::Text("Elf3D  |  %s  |  Viewport %u x %u  |  FBO %s  |  Draws %llu  |  "
                    "Triangles %llu  |  Selected: %s  |  Isolation: %s  |  %s  |  %.2f ms  "
                    "|  %.1f FPS",
                    graphics_backend_name(engine.graphics_backend()),
                    state.rendering.view_dimensions.width, state.rendering.view_dimensions.height,
                    state.rendering.framebuffer_valid ? "valid" : "inactive",
                    static_cast<unsigned long long>(state.rendering.statistics.draw_calls),
                    static_cast<unsigned long long>(state.rendering.statistics.triangles),
                    selection_status.c_str(), isolation_status.c_str(), tool_status.c_str(),
                    frame_time_ms, fps);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void build_about_property_row(const char* label, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
}

void build_about_window(ViewerFrameContext& state)
{
    if (state.shell.show_about) {
        ImGui::OpenPopup("About Elf3D");
        state.shell.show_about = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center{viewport->WorkPos.x + viewport->WorkSize.x * 0.5F,
                        viewport->WorkPos.y + viewport->WorkSize.y * 0.5F};
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5F, 0.5F});
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowSize(ImVec2{640.0F, 540.0F}, ImGuiCond_Appearing);
    push_professional_dialog_style();
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
    if (ImGui::BeginPopupModal("About Elf3D", nullptr, flags)) {
        if (ImGui::BeginChild("##AboutHero", ImVec2{0.0F, 118.0F}, true,
                              ImGuiWindowFlags_NoScrollbar)) {
            const ImVec2 origin = ImGui::GetWindowPos();
            const ImVec2 badge_min{origin.x + 18.0F, origin.y + 18.0F};
            const ImVec2 badge_max{badge_min.x + 72.0F, badge_min.y + 72.0F};
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(badge_min, badge_max, IM_COL32(27, 105, 164, 255), 8.0F);
            draw_list->AddRect(badge_min, badge_max, IM_COL32(91, 183, 238, 255), 8.0F, 0, 2.0F);
            const ImVec2 badge_text_size = ImGui::CalcTextSize("E3");
            draw_list->AddText(ImVec2{badge_min.x + (72.0F - badge_text_size.x) * 0.5F,
                                      badge_min.y + (72.0F - badge_text_size.y) * 0.5F},
                               IM_COL32(244, 249, 252, 255), "E3");
            ImGui::SetCursorPos(ImVec2{110.0F, 20.0F});
            ImGui::TextColored(ImVec4{0.455F, 0.725F, 0.900F, 1.00F}, "ELF3D");
            ImGui::SetCursorPosX(110.0F);
            ImGui::Text("Version %s", ELF3D_VERSION);
            ImGui::SetCursorPosX(110.0F);
            ImGui::TextDisabled("Professional glTF visualization and inspection");
        }
        ImGui::EndChild();
        ImGui::Spacing();
        ImGui::TextWrapped("Open glTF/GLB models, navigate, inspect and measure geometry, "
                           "and save models with the current supported writer.");
        ImGui::Separator();

        constexpr ImGuiTableFlags table_flags =
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV;
        if (ImGui::BeginTable("##AboutProperties", 2, table_flags)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 170.0F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            build_about_property_row("PRODUCT", "Elf3D Viewer");
            build_about_property_row("MODEL FORMAT", "glTF 2.0 / GLB");
            build_about_property_row("GRAPHICS", "OpenGL 4.1 core");
#if defined(_WIN32)
            build_about_property_row("PLATFORM", "Windows");
#elif defined(__APPLE__)
            build_about_property_row("PLATFORM", "macOS");
#else
            build_about_property_row("PLATFORM", "Linux / Unix");
#endif
            build_about_property_row("LICENSE", "MIT");
            ImGui::EndTable();
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Build details")) {
            ImGui::Text("Dear ImGui %s (%s)", ImGui::GetVersion(), ELF3D_IMGUI_BRANCH);
            ImGui::TextWrapped("Dear ImGui revision: %s", ELF3D_IMGUI_COMMIT_SHA);
        }
        ImGui::Separator();
        const float close_width = 118.0F;
        ImGui::SetCursorPosX(
            std::max(ImGui::GetCursorPosX(),
                     ImGui::GetWindowWidth() - close_width - ImGui::GetStyle().WindowPadding.x));
        if (ImGui::Button("Close", ImVec2{close_width, 0.0F}) || state.interaction.escape_pressed) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

namespace {
void help_shortcut(const char* input, const char* action)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", input);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", action);
}
void draw_help_shortcuts()
{
    ImGui::TextWrapped("Navigation uses the pointer over 3D View. Text input and modal dialogs "
                       "take precedence over application shortcuts.");
    if (!ImGui::BeginTable("Shortcuts", 2,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;
    ImGui::TableSetupColumn("Input", ImGuiTableColumnFlags_WidthStretch, 1);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 2);
    ImGui::TableHeadersRow();
    help_shortcut("Left drag", "Orbit");
    help_shortcut("Space + left drag", "Look around from the current position");
    help_shortcut("X + left drag / middle drag", "Pan (right drag pans more slowly)");
    help_shortcut("Z + left drag / mouse wheel", "Dolly");
    help_shortcut("Mouse button + W/S, A/D, Q/E", "Move forward/back, sideways, or along world Y");
    help_shortcut("F / Shift+F / Home", "Fit visible content / Frame Selected / Reset camera");
    help_shortcut("S / M", "Select tool / Measure Distance tool");
    help_shortcut("Ctrl+click in 3D View",
                  "Replace selection; Ctrl+Shift+click toggles membership");
    help_shortcut("Shift+click in 3D View", "Hide the visible geometry under the pointer");
    help_shortcut("Click / Ctrl+click / Shift+click in browser",
                  "Replace / toggle / range selection");
    help_shortcut("Arrow keys, Home/End in browser", "Navigate rows; Shift extends the range");
    help_shortcut("Escape / Delete",
                  "Cancel incomplete measurement or clear selection / Clear measurement");
    help_shortcut("Ctrl+S / Ctrl+Shift+S", "Save / Save As");
    ImGui::EndTable();
}
void draw_model_limitations()
{
    ImGui::TextWrapped(
        "Open supports glTF 2.0 (.gltf) and GLB 2.0 triangle geometry. Triangle "
        "strips and fans are converted. Points and lines are omitted with diagnostics.");
    ImGui::Spacing();
    ImGui::TextWrapped("Static/base geometry is supported. Animation playback, skin deformation "
                       "and morph deformation are unavailable. Perspective cameras are supported; "
                       "orthographic cameras remain transform-only with diagnostics.");
    ImGui::Spacing();
    ImGui::TextWrapped("PNG, JPEG and supported RGB TIFF images are decoded. Compressed geometry "
                       "and KTX2/BasisU/WebP decoding are unavailable. Unsupported required "
                       "extensions reject import; optional unsupported features may be omitted.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Objects and Meshes show supported data retained in the current Document. "
        "Inactive-scene objects and unused resources have no invented active instance. "
        "Inspect the load report in Model Information for omissions and warnings.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Save and Save As write the whole current Document, including hidden and "
        "unselected geometry. They do not export only the browser selection. "
        "Supported-structure preservation does not mean byte-identical source files.");
}
void draw_help_diagnostics(const ViewerFrameContext& state)
{
    ImGui::TextWrapped("Vendor: %s", state.diagnostics.gl_vendor.c_str());
    ImGui::TextWrapped("Renderer: %s", state.diagnostics.gl_renderer.c_str());
    ImGui::TextWrapped("OpenGL: %s", state.diagnostics.gl_version.c_str());
    ImGui::TextWrapped("GLSL: %s", state.diagnostics.glsl_version_report.c_str());
    ImGui::Text("Maximum texture size: %d", state.diagnostics.maximum_texture_size);
    ImGui::Text("View / target: %u x %u / %u x %u", state.rendering.view_dimensions.width,
                state.rendering.view_dimensions.height,
                state.rendering.render_target_dimensions.width,
                state.rendering.render_target_dimensions.height);
    ImGui::Separator();
    const auto& stats = state.rendering.statistics;
    ImGui::Text("Last 3D render: %llu draws, %llu triangles",
                static_cast<unsigned long long>(stats.draw_calls),
                static_cast<unsigned long long>(stats.triangles));
    ImGui::Text("Resident geometry / textures: %llu / %llu bytes",
                static_cast<unsigned long long>(stats.estimated_resident_geometry_bytes),
                static_cast<unsigned long long>(stats.estimated_resident_texture_bytes));
    ImGui::TextWrapped("Resource sizes are renderer-owned estimates from the last 3D render, "
                       "not physical VRAM usage. A retained view may reuse that frame.");
    if (state.rendering.graphics_error)
        ImGui::TextWrapped("Graphics preparation error: %s",
                           state.rendering.graphics_error->message());
    if (!state.notifications.viewport_error.empty())
        ImGui::TextWrapped("Viewport error: %s", state.notifications.viewport_error.c_str());
}
} // namespace

void build_help_window(ViewerFrameContext& state)
{
    if (!state.shell.help_topic)
        return;
    ImGui::SetNextWindowSize(ImVec2{620, 460}, ImGuiCond_FirstUseEver);
    bool open = true;
    if (ImGui::Begin("Help", &open)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        switch (*state.shell.help_topic) {
        case ViewerHelpTopic::shortcuts:
            ImGui::SeparatorText("Shortcuts");
            draw_help_shortcuts();
            break;
        case ViewerHelpTopic::model_limits:
            ImGui::SeparatorText("Supported Model Limitations");
            draw_model_limitations();
            break;
        case ViewerHelpTopic::diagnostics:
            ImGui::SeparatorText("Diagnostics");
            draw_help_diagnostics(state);
            break;
        }
    }
    ImGui::End();
    if (!open)
        state.shell.help_topic.reset();
}

} // namespace elf3d::viewer
