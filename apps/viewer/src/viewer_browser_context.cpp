#include "viewer_browser.hpp"

#include "viewer_ui.hpp"
#include "viewer_workflows.hpp"

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

void build_file_properties_popup(FileBrowserState& browser, const FileBrowserFrameInput& input) {
    if (browser.request_properties) {
        ImGui::OpenPopup("File Properties");
        browser.request_properties = false;
    }
    if (!browser.properties.has_value()) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2{620.0F, 0.0F}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("File Properties", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    const OpenBrowserEntry& entry = *browser.properties;
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
    const bool close = ImGui::Button("Close", ImVec2{118.0F, 0.0F}) || input.escape_pressed;
    if (close) {
        browser.properties.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void build_external_editor_menu_item(const char* label,
                                     const std::optional<std::filesystem::path>& editor,
                                     const OpenBrowserEntry& entry, FileBrowserState& browser,
                                     ExternalEditorWorkflow& workflow) {
    if (ImGui::MenuItem(label, nullptr, false, editor.has_value()) &&
        workflow.activate(ExternalEditorLaunchRequest{*editor, entry.path, label}) !=
            WorkflowActivation::accepted) {
        browser.error = "Another external-editor workflow is already active.";
    }
}

void build_file_context_menu(const OpenBrowserEntry& entry, FileBrowserState& browser,
                             std::optional<std::filesystem::path>& open_request,
                             ExternalEditorWorkflow& workflow) {
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
        browser.properties = entry;
        browser.request_properties = true;
    }
    ImGui::Separator();
    build_external_editor_menu_item("Edit in EmEditor", browser.external_editors.emeditor, entry,
                                    browser, workflow);
    build_external_editor_menu_item("Edit in Notepad", browser.external_editors.notepad, entry,
                                    browser, workflow);
    build_external_editor_menu_item("Edit in Notepad++", browser.external_editors.notepad_plus_plus,
                                    entry, browser, workflow);
    ImGui::EndPopup();
}

} // namespace elf3d::viewer
