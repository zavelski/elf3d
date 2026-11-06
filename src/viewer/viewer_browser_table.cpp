#include "viewer_browser_table.hpp"
#include "viewer_ui.hpp"
#include "viewer_workflows.hpp"

#include <imgui.h>

#include <algorithm>
#include <climits>
#include <string>

namespace elf3d::viewer {
struct BrowserEntryDrawContext {
    BrowserFileRequests& requests;
    std::optional<std::filesystem::path>& navigation_request;
    const FileBrowserFrameInput& input;
    ExternalEditorWorkflow& external_editor;
};

void handle_open_browser_selection(const OpenBrowserEntry& entry, FileBrowserState& browser,
                                   BrowserFileRequests& requests,
                                   std::optional<std::filesystem::path>& navigation_request,
                                   const FileBrowserFrameInput& input)
{
    if (entry.directory) {
        clear_file_browser_selection(browser);
        if (input.primary_double_clicked) {
            navigation_request = entry.path;
        }
        return;
    }
    select_file_browser_file(browser, entry.path);
    if (input.primary_double_clicked) {
        requests.activated_file = entry.path;
    }
}

void draw_open_browser_entry(const OpenBrowserEntry& entry, FileBrowserState& browser,
                             const BrowserEntryDrawContext& context)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const std::string display_label = entry.directory ? entry.label + "/" : entry.label;
    const std::string item_id = path_to_utf8(entry.path);
    ImGui::PushID(item_id.c_str());
    const bool selected = !entry.directory && !browser.selected_path.empty() &&
                          same_path_key(entry.path, browser.selected_path);
    if (ImGui::Selectable(display_label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
        handle_open_browser_selection(entry, browser, context.requests, context.navigation_request,
                                      context.input);
    }
    if (!entry.directory && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        select_file_browser_file(browser, entry.path);
    }
    if (!entry.directory) {
        build_file_context_menu(entry, browser, context.requests.open_file,
                                context.external_editor);
    }
    ImGui::PopID();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(entry.modified_time.c_str());
    ImGui::TableNextColumn();
    const std::string size_text = entry.has_size ? format_file_size(entry.size_bytes) : "";
    ImGui::TextUnformatted(size_text.c_str());
}

void draw_clipped_file_rows(FileBrowserState& browser, BrowserFileRequests& requests,
                            std::optional<std::filesystem::path>& navigation_request,
                            const FileBrowserFrameInput& input,
                            ExternalEditorWorkflow& external_editor)
{
    const bool filtered = !browser.filtered_search.empty();
    const auto count = filtered ? browser.filtered_entries.size() : browser.entries.size();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(std::min(count, static_cast<std::size_t>(INT_MAX))));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto index = filtered ? browser.filtered_entries[static_cast<std::size_t>(row)]
                                        : static_cast<std::size_t>(row);
            draw_open_browser_entry(
                browser.entries[index], browser,
                BrowserEntryDrawContext{requests, navigation_request, input, external_editor});
        }
    }
}

[[nodiscard]] bool draw_file_list_status(const FileBrowserState& browser)
{
    const bool reading = browser.directory_reading || browser.needs_refresh;
    const bool filtering = browser.filter_cursor < browser.entries.size();
    const auto count =
        browser.filtered_search.empty() ? browser.entries.size() : browser.filtered_entries.size();
    if (!reading && !filtering && count != 0) {
        return false;
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const char* message =
        reading ? "Reading folder..." : "No matching folders or .gltf/.glb files.";
    ImGui::TextDisabled("%s", filtering && !reading ? "Filtering files..." : message);
    return true;
}

void build_open_browser_file_table(FileBrowserState& browser, BrowserFileRequests& requests,
                                   const FileBrowserFrameInput& input,
                                   ExternalEditorWorkflow& external_editor)
{
    advance_file_browser_filter(browser);
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##OpenBrowserTable", 3, table_flags)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.58F);
    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthStretch, 0.28F);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch, 0.14F);
    ImGui::TableHeadersRow();

    std::optional<std::filesystem::path> navigation_request;
    if (!draw_file_list_status(browser)) {
        draw_clipped_file_rows(browser, requests, navigation_request, input, external_editor);
    }

    ImGui::EndTable();
    if (navigation_request.has_value()) {
        set_file_browser_directory(browser, *navigation_request);
    }
}

} // namespace elf3d::viewer
