#include "viewer_internal.hpp"

#include <imgui.h>

#include <filesystem>
#include <optional>
#include <string>

namespace elf3d::viewer {

void draw_file_property(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", label);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", value.c_str());
}

[[nodiscard]] std::string model_file_type(const std::filesystem::path& path) {
    const std::string extension = lowercase_ascii(path_to_utf8(path.extension()));
    return extension == ".glb" ? "glTF Binary Model (.glb)" : "glTF Model (.gltf)";
}

void build_file_properties_popup(ViewerState& state) {
    if (state.request_open_browser_properties) {
        ImGui::OpenPopup("File Properties");
        state.request_open_browser_properties = false;
    }
    if (!state.open_browser_properties.has_value()) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2{620.0F, 0.0F}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("File Properties", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    const OpenBrowserEntry& entry = *state.open_browser_properties;
    ImGui::TextUnformatted(entry.label.c_str());
    ImGui::Separator();
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##FileProperties", 2, table_flags)) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 112.0F);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        draw_file_property("Type", model_file_type(entry.path));
        draw_file_property("Location", path_to_utf8(entry.path.parent_path()));
        draw_file_property("Size", entry.has_size ? format_file_size(entry.size_bytes) : "-");
        draw_file_property("Modified", entry.modified_time);
        draw_file_property("Full path", path_to_utf8(entry.path));
        ImGui::EndTable();
    }
    ImGui::Separator();
    const bool close =
        ImGui::Button("Close", ImVec2{118.0F, 0.0F}) || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (close) {
        state.open_browser_properties.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void build_external_editor_menu_item(const char* label,
                                     const std::optional<std::filesystem::path>& editor,
                                     const OpenBrowserEntry& entry, ViewerState& state) {
    if (ImGui::MenuItem(label, nullptr, false, editor.has_value()) &&
        !launch_external_editor(*editor, entry.path)) {
        state.open_browser_error = "Could not launch " + std::string{label} + ".";
    }
}

void build_file_context_menu(const OpenBrowserEntry& entry, ViewerState& state,
                             std::optional<std::filesystem::path>& open_request) {
    if (!ImGui::BeginPopupContextItem("##FileContextMenu")) {
        return;
    }
    if (ImGui::MenuItem("Open")) {
        open_request = entry.path;
    }
    if (ImGui::MenuItem("Copy as path")) {
        const std::string clipboard_text = '"' + path_to_utf8(entry.path) + '"';
        ImGui::SetClipboardText(clipboard_text.c_str());
    }
    if (ImGui::MenuItem("Properties")) {
        state.open_browser_properties = entry;
        state.request_open_browser_properties = true;
    }
    ImGui::Separator();
    build_external_editor_menu_item("Edit in EmEditor", state.external_editors.emeditor, entry,
                                    state);
    build_external_editor_menu_item("Edit in Notepad", state.external_editors.notepad, entry,
                                    state);
    build_external_editor_menu_item("Edit in Notepad++", state.external_editors.notepad_plus_plus,
                                    entry, state);
    ImGui::EndPopup();
}

} // namespace elf3d::viewer
