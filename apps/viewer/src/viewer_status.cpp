#include "viewer_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>

namespace elf3d::viewer {

void build_error_modal(ViewerState& state) {
    if (state.request_error_modal) {
        ImGui::OpenPopup("Model Load Error");
        state.request_error_modal = false;
    }
    push_professional_dialog_style();
    ImGui::SetNextWindowSize(ImVec2{600.0F, 0.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Model Load Error", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(ImVec4{1.00F, 0.470F, 0.390F, 1.00F}, "MODEL COULD NOT BE OPENED");
        ImGui::Separator();
        if (state.load_failure.has_value()) {
            ImGui::TextDisabled("SOURCE");
            ImGui::TextWrapped("%s", state.load_failure->source_path.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("CATEGORY");
            ImGui::TextUnformatted(error_category(state.load_failure->error.code()));
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.load_failure->error.message());
        }
        ImGui::Spacing();
        const float close_width = 118.0F;
        ImGui::SetCursorPosX(
            std::max(ImGui::GetCursorPosX(),
                     ImGui::GetWindowWidth() - close_width - ImGui::GetStyle().WindowPadding.x));
        if (ImGui::Button("Close", ImVec2{close_width, 0.0F}) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            state.load_failure.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

void build_save_error_modal(ViewerState& state) {
    if (state.request_save_error_modal) {
        ImGui::OpenPopup("Model Save Error");
        state.request_save_error_modal = false;
    }
    push_professional_dialog_style();
    ImGui::SetNextWindowSize(ImVec2{600.0F, 0.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Model Save Error", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextColored(ImVec4{1.00F, 0.470F, 0.390F, 1.00F}, "MODEL COULD NOT BE SAVED");
        ImGui::Separator();
        if (state.save_failure.has_value()) {
            ImGui::TextDisabled("TARGET");
            ImGui::TextWrapped("%s", state.save_failure->source_path.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.save_failure->error.message());
        }
        if (ImGui::Button("Close", ImVec2{118.0F, 0.0F}) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            state.save_failure.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

const char* graphics_backend_name(elf3d::GraphicsBackend backend) noexcept {
    return backend == elf3d::GraphicsBackend::opengl ? "OpenGL 4.1 core" : "Unknown";
}

void build_status_bar(const ViewerState& state, const elf3d::Engine& engine,
                      const ViewerScene& scene, const elf3d::Viewport& engine_viewport) {
    if (!state.show_status_bar) {
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
        const float fps = ImGui::GetIO().Framerate;
        const float frame_time_ms = fps > 0.0F ? 1000.0F / fps : 0.0F;
        const elf3d::SelectionSnapshot selection = engine_viewport.selection_snapshot();
        const std::string selection_status = selected_entity_label(scene, selection);
        std::string isolation_status = "none";
        const std::optional<elf3d::EntityId> isolated = engine_viewport.isolated_entity();
        if (isolated.has_value()) {
            isolation_status = entity_label(scene, *isolated);
        }
        const elf3d::DistanceMeasurementSnapshot measurement =
            engine_viewport.distance_measurement_snapshot(*scene.scene);
        const elf3d::Result<std::optional<elf3d::Bounds3>> visible_bounds =
            engine_viewport.visible_bounds(*scene.scene);
        const std::string clipping =
            clipping_status(engine_viewport.clipping_snapshot(),
                            visible_bounds && visible_bounds.value().has_value());
        std::string tool_status = std::string{"Tool: "} + tool_name(engine_viewport.active_tool());
        if (engine_viewport.active_tool() == elf3d::ViewportTool::distance_measurement) {
            tool_status += " | ";
            tool_status += measurement_state_name(measurement.state);
            if (measurement.preview_point.has_value()) {
                tool_status += " | ";
                tool_status += format_distance(measurement.preview_distance_meters,
                                               engine_viewport.measurement_settings().display_unit);
            }
        } else if (measurement.state == elf3d::DistanceMeasurementState::complete) {
            tool_status += " | Measurement: ";
            tool_status += format_distance(measurement.distance_meters,
                                           engine_viewport.measurement_settings().display_unit);
        }
        ImGui::Text("Elf3D  |  %s  |  Viewport %u x %u  |  FBO %s  |  Draws %llu  |  "
                    "Triangles %llu  |  Selected: %s  |  Isolation: %s  |  %s  |  %s  |  %.2f ms  "
                    "|  %.1f FPS",
                    graphics_backend_name(engine.graphics_backend()), state.view_dimensions.width,
                    state.view_dimensions.height, state.framebuffer_valid ? "valid" : "inactive",
                    static_cast<unsigned long long>(state.statistics.draw_calls),
                    static_cast<unsigned long long>(state.statistics.triangles),
                    selection_status.c_str(), isolation_status.c_str(), clipping.c_str(),
                    tool_status.c_str(), frame_time_ms, fps);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void build_about_property_row(const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
}

void build_about_window(ViewerState& state) {
    if (state.show_about) {
        ImGui::OpenPopup("About Elf3D");
        state.show_about = false;
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
        ImGui::TextWrapped(
            "Elf3D Viewer is the reference desktop host and graphical testbed for the Elf3D "
            "embeddable C++20 visualization engine.");
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
        if (ImGui::Button("Close", ImVec2{close_width, 0.0F}) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
}

} // namespace elf3d::viewer
