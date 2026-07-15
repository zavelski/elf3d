#include "viewer_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace elf3d::viewer {

[[nodiscard]] bool is_file_io_error(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::source_file_not_found:
    case elf3d::ErrorCode::source_file_read_failed:
    case elf3d::ErrorCode::missing_external_buffer:
    case elf3d::ErrorCode::missing_external_image:
    case elf3d::ErrorCode::external_image_read_failed:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_unsupported_gltf_error(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::unsupported_scene_format:
    case elf3d::ErrorCode::unsupported_required_extension:
    case elf3d::ErrorCode::unsupported_remote_uri:
    case elf3d::ErrorCode::unsupported_primitive_mode:
    case elf3d::ErrorCode::unsupported_index_type:
    case elf3d::ErrorCode::unsupported_image_mime_type:
    case elf3d::ErrorCode::unsupported_image_extension:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_invalid_gltf_structure_error(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::malformed_gltf:
    case elf3d::ErrorCode::malformed_glb:
    case elf3d::ErrorCode::gltf_validation_failed:
    case elf3d::ErrorCode::invalid_buffer_range:
    case elf3d::ErrorCode::invalid_buffer_view:
    case elf3d::ErrorCode::invalid_accessor:
    case elf3d::ErrorCode::imported_index_out_of_range:
    case elf3d::ErrorCode::invalid_texcoord:
    case elf3d::ErrorCode::mismatched_texcoord_count:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_invalid_gltf_payload_error(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::malformed_data_uri:
    case elf3d::ErrorCode::invalid_base64_payload:
    case elf3d::ErrorCode::invalid_image_buffer_view:
    case elf3d::ErrorCode::image_range_out_of_bounds:
    case elf3d::ErrorCode::image_decode_failed:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool is_resource_limit_error(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::resource_limit_exceeded:
    case elf3d::ErrorCode::size_overflow:
    case elf3d::ErrorCode::zero_image_dimensions:
    case elf3d::ErrorCode::excessive_image_dimensions:
    case elf3d::ErrorCode::decoded_image_size_overflow:
    case elf3d::ErrorCode::image_resource_limit_exceeded:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] const char* error_category(elf3d::ErrorCode code) noexcept {
    if (is_file_io_error(code)) {
        return "File I/O";
    }
    if (is_unsupported_gltf_error(code)) {
        return "Unsupported glTF feature";
    }
    if (is_invalid_gltf_structure_error(code) || is_invalid_gltf_payload_error(code)) {
        return "Invalid glTF data";
    }
    return is_resource_limit_error(code) ? "Resource limit" : "Scene import";
}

constexpr int professional_dialog_style_var_count = 7;
constexpr int professional_dialog_style_color_count = 27;

void push_professional_dialog_style() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{16.0F, 14.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{9.0F, 6.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0F, 8.0F});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 14.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4{0.105F, 0.118F, 0.128F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.105F, 0.118F, 0.128F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4{0.085F, 0.095F, 0.103F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4{0.105F, 0.118F, 0.128F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4{0.085F, 0.095F, 0.103F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.130F, 0.145F, 0.155F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4{0.255F, 0.285F, 0.300F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.900F, 0.915F, 0.920F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_InputTextCursor, ImVec4{0.900F, 0.915F, 0.920F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4{0.545F, 0.580F, 0.595F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{0.070F, 0.080F, 0.088F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.120F, 0.145F, 0.160F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{0.145F, 0.175F, 0.190F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.190F, 0.215F, 0.225F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.250F, 0.300F, 0.330F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.220F, 0.360F, 0.470F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0.165F, 0.255F, 0.320F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{0.205F, 0.340F, 0.430F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{0.225F, 0.405F, 0.535F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4{0.145F, 0.165F, 0.175F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4{0.115F, 0.128F, 0.138F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4{0.130F, 0.145F, 0.155F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4{0.250F, 0.280F, 0.295F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4{0.080F, 0.090F, 0.098F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4{0.270F, 0.315F, 0.335F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4{0.370F, 0.450F, 0.490F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4{0.400F, 0.560F, 0.650F, 1.00F});
}

void pop_professional_dialog_style() {
    ImGui::PopStyleColor(professional_dialog_style_color_count);
    ImGui::PopStyleVar(professional_dialog_style_var_count);
}

void push_primary_action_style() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.105F, 0.385F, 0.610F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.135F, 0.480F, 0.740F, 1.00F});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.090F, 0.325F, 0.520F, 1.00F});
}

void pop_primary_action_style() {
    ImGui::PopStyleColor(3);
}

[[nodiscard]] bool open_browser_toolbar_button(const char* label, const char* tooltip_text,
                                               bool enabled = true) {
    if (!enabled) {
        ImGui::BeginDisabled();
    }
    const float size = ImGui::GetFrameHeight();
    const float width =
        std::max(size, ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F);
    const bool pressed = ImGui::Button(label, ImVec2{width, size});
    tooltip(tooltip_text);
    if (!enabled) {
        ImGui::EndDisabled();
    }
    return enabled && pressed;
}

[[nodiscard]] std::optional<std::filesystem::path>
open_browser_file_candidate(const ViewerState& state) {
    const std::string text{state.open_file_path.data()};
    if (text.empty()) {
        return std::nullopt;
    }

    std::filesystem::path candidate = path_from_utf8(text);
    if (candidate.is_relative()) {
        candidate = state.open_browser_directory / candidate;
    }
    candidate = absolute_path_no_throw(candidate);

    std::error_code error;
    if (!supported_model_path(candidate) || !std::filesystem::is_regular_file(candidate, error) ||
        error) {
        return std::nullopt;
    }
    return candidate;
}

[[nodiscard]] std::optional<std::filesystem::path>
save_browser_file_candidate(const ViewerState& state) {
    const std::string text{state.open_file_path.data()};
    if (text.empty()) {
        return std::nullopt;
    }
    std::filesystem::path candidate = path_from_utf8(text);
    if (candidate.extension().empty()) {
        candidate += ".glb";
    }
    if (candidate.is_relative()) {
        candidate = state.open_browser_directory / candidate;
    }
    candidate = absolute_path_no_throw(candidate);
    std::error_code error;
    if (!supported_model_path(candidate) || !candidate.has_parent_path() ||
        !std::filesystem::is_directory(candidate.parent_path(), error) || error) {
        return std::nullopt;
    }
    return candidate;
}

[[nodiscard]] bool open_browser_entry_matches_search(const OpenBrowserEntry& entry,
                                                     std::string_view search_text) {
    if (search_text.empty()) {
        return true;
    }
    return lowercase_ascii(entry.label).find(search_text) != std::string::npos;
}

void build_open_browser_sidebar_item(ViewerState& state, const char* section_id,
                                     std::string_view label,
                                     const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error) || error) {
        return;
    }

    const std::string display{label};
    const std::string item_id = path_to_utf8(directory);
    ImGui::PushID(section_id);
    ImGui::PushID(item_id.c_str());

    const bool selected = same_path_key(directory, state.open_browser_directory);
    if (ImGui::Selectable(display.c_str(), selected)) {
        set_open_browser_directory(state, directory);
    }
    tooltip(item_id.c_str());
    ImGui::PopID();
    ImGui::PopID();
}

void build_open_browser_sidebar_section(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4{0.470F, 0.680F, 0.810F, 1.00F}, "%s", title);
    ImGui::Spacing();
}

void build_open_browser_bookmarks(ViewerState& state) {
    build_open_browser_sidebar_section("FAVORITES");
    for (const std::filesystem::path& bookmark : state.open_browser_bookmarks) {
        build_open_browser_sidebar_item(state, "favorites", file_name_label(bookmark), bookmark);
    }
}

void build_open_browser_system_locations(ViewerState& state) {
    build_open_browser_sidebar_section("LOCATIONS");
    const std::optional<std::filesystem::path> user_profile = environment_directory("USERPROFILE");
    if (user_profile.has_value()) {
        const std::filesystem::path& profile_path = *user_profile;
        build_open_browser_sidebar_item(state, "locations", "Home", profile_path);
        build_open_browser_sidebar_item(state, "locations", "Desktop", profile_path / "Desktop");
        build_open_browser_sidebar_item(state, "locations", "Documents",
                                        profile_path / "Documents");
        build_open_browser_sidebar_item(state, "locations", "Downloads",
                                        profile_path / "Downloads");
        build_open_browser_sidebar_item(state, "locations", "Music", profile_path / "Music");
        build_open_browser_sidebar_item(state, "locations", "Pictures", profile_path / "Pictures");
        build_open_browser_sidebar_item(state, "locations", "Videos", profile_path / "Videos");
    }
    const std::optional<std::filesystem::path> one_drive = environment_directory("OneDrive");
    if (one_drive.has_value()) {
        build_open_browser_sidebar_item(state, "locations", "OneDrive", *one_drive);
    }
}

void build_open_browser_volumes(ViewerState& state) {
    build_open_browser_sidebar_section("STORAGE");
#if defined(_WIN32)
    bool has_drive = false;
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        std::string root;
        root.push_back(letter);
        root += ":\\";

        std::error_code error;
        const std::filesystem::path root_path = path_from_utf8(root);
        if (!std::filesystem::is_directory(root_path, error) || error) {
            continue;
        }

        has_drive = true;
        build_open_browser_sidebar_item(state, "storage", root, root_path);
    }
    if (!has_drive) {
        ImGui::TextDisabled("none");
    }
#else
    static_cast<void>(state);
#endif
}

void build_open_browser_recents(ViewerState& state) {
    build_open_browser_sidebar_section("RECENT");
    if (state.open_browser_recents.empty()) {
        ImGui::TextDisabled("none");
        return;
    }
    for (const std::filesystem::path& recent : state.open_browser_recents) {
        build_open_browser_sidebar_item(state, "recent", file_name_label(recent), recent);
    }
}

void build_open_browser_sidebar(ViewerState& state) {
    build_open_browser_bookmarks(state);
    build_open_browser_system_locations(state);
    build_open_browser_volumes(state);
    build_open_browser_recents(state);
}

void build_open_browser_top_bar(ViewerState& state) {
    if (open_browser_toolbar_button("<", "Back", state.open_browser_history_index > 0U)) {
        navigate_open_browser_history(state, -1);
    }
    ImGui::SameLine();
    if (open_browser_toolbar_button(">", "Forward",
                                    !state.open_browser_history.empty() &&
                                        state.open_browser_history_index + 1U <
                                            state.open_browser_history.size())) {
        navigate_open_browser_history(state, 1);
    }
    ImGui::SameLine();
    if (open_browser_toolbar_button("Up", "Parent folder")) {
        const std::filesystem::path parent = state.open_browser_directory.parent_path();
        if (!parent.empty() && parent != state.open_browser_directory) {
            set_open_browser_directory(state, parent);
        }
    }
    ImGui::SameLine();
    if (open_browser_toolbar_button("R", "Refresh folder")) {
        state.open_browser_needs_refresh = true;
    }
    ImGui::SameLine();
    if (open_browser_toolbar_button("+", "Pin current folder to Favorites")) {
        push_unique_directory(state.open_browser_bookmarks, state.open_browser_directory, 12);
    }

    ImGui::SameLine();
    const float search_width = 230.0F;
    ImGui::SetNextItemWidth(-(search_width + ImGui::GetStyle().ItemSpacing.x));
    if (ImGui::InputTextWithHint("##OpenFolderPath", "Folder path", state.open_folder_path.data(),
                                 state.open_folder_path.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        set_open_browser_directory(state,
                                   path_from_utf8(std::string{state.open_folder_path.data()}));
    }
    tooltip("Current folder");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(search_width);
    ImGui::InputTextWithHint("##OpenSearch", "Search models", state.open_search.data(),
                             state.open_search.size());
    tooltip("Search visible folders and glTF files");
}

struct BrowserFileRequests {
    std::optional<std::filesystem::path> activated_file;
    std::optional<std::filesystem::path> open_file;
};

void handle_open_browser_selection(const OpenBrowserEntry& entry, ViewerState& state,
                                   BrowserFileRequests& requests,
                                   std::optional<std::filesystem::path>& navigation_request) {
    if (entry.directory) {
        clear_open_browser_selected_file(state);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            navigation_request = entry.path;
        }
        return;
    }
    set_open_browser_selected_file(state, entry.path);
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        requests.activated_file = entry.path;
    }
}

void draw_open_browser_entry(const OpenBrowserEntry& entry, ViewerState& state,
                             BrowserFileRequests& requests,
                             std::optional<std::filesystem::path>& navigation_request) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const std::string display_label = entry.directory ? entry.label + "/" : entry.label;
    const std::string item_id = path_to_utf8(entry.path);
    ImGui::PushID(item_id.c_str());
    const bool selected = !entry.directory && !state.open_browser_selected_path.empty() &&
                          same_path_key(entry.path, state.open_browser_selected_path);
    if (ImGui::Selectable(display_label.c_str(), selected,
                          ImGuiSelectableFlags_SpanAllColumns |
                              ImGuiSelectableFlags_AllowDoubleClick)) {
        handle_open_browser_selection(entry, state, requests, navigation_request);
    }
    if (!entry.directory && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        set_open_browser_selected_file(state, entry.path);
    }
    if (!entry.directory) {
        build_file_context_menu(entry, state, requests.open_file);
    }
    ImGui::PopID();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(entry.modified_time.c_str());
    ImGui::TableNextColumn();
    const std::string size_text = entry.has_size ? format_file_size(entry.size_bytes) : "";
    ImGui::TextUnformatted(size_text.c_str());
}

void build_open_browser_file_table(ViewerState& state, BrowserFileRequests& requests) {
    const std::string search_text = lowercase_ascii(std::string{state.open_search.data()});
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

    bool has_visible_entry = false;
    std::optional<std::filesystem::path> navigation_request;
    for (const OpenBrowserEntry& entry : state.open_browser_entries) {
        if (!open_browser_entry_matches_search(entry, search_text)) {
            continue;
        }
        has_visible_entry = true;
        draw_open_browser_entry(entry, state, requests, navigation_request);
    }

    if (!has_visible_entry) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("No matching folders or .gltf/.glb files.");
    }

    ImGui::EndTable();
    if (navigation_request.has_value()) {
        set_open_browser_directory(state, *navigation_request);
    }
}

void build_open_browser_error(const ViewerState& state) {
    if (state.open_browser_error.empty()) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.265F, 0.100F, 0.095F, 1.00F});
    if (ImGui::BeginChild("##OpenBrowserError", ImVec2{0.0F, 38.0F}, true,
                          ImGuiWindowFlags_NoScrollbar)) {
        ImGui::TextColored(ImVec4{1.00F, 0.630F, 0.560F, 1.0F}, "%s",
                           state.open_browser_error.c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

[[nodiscard]] std::optional<std::string> build_open_browser_footer(ViewerState& state) {
    std::optional<std::string> requested_path;
    const std::optional<std::filesystem::path> candidate = open_browser_file_candidate(state);
    const bool can_open = candidate.has_value();
    const float button_width = 118.0F;
    const float button_area_width = button_width * 2.0F + ImGui::GetStyle().ItemSpacing.x;
    constexpr float field_label_width = 130.0F;
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("File name");
    ImGui::SameLine(field_label_width);
    ImGui::SetNextItemWidth(-button_area_width - ImGui::GetStyle().ItemSpacing.x);
    const bool open_from_enter = ImGui::InputTextWithHint(
        "##OpenFilePath", "Select a .gltf or .glb model", state.open_file_path.data(),
        state.open_file_path.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    tooltip("Selected model file");
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_open);
    push_primary_action_style();
    if (ImGui::Button("Open", ImVec2{button_width, 0.0F}) || (open_from_enter && can_open)) {
        requested_path = path_to_utf8(*candidate);
        ImGui::CloseCurrentPopup();
    }
    pop_primary_action_style();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2{button_width, 0.0F})) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("File type");
    ImGui::SameLine(field_label_width);
    ImGui::TextUnformatted("glTF 2.0 Model (*.gltf, *.glb)");
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !state.open_browser_properties.has_value()) {
        ImGui::CloseCurrentPopup();
    }
    return requested_path;
}

[[nodiscard]] std::optional<FileDialogResult> build_open_modal(ViewerState& state,
                                                               const ViewerScene& scene) {
    if (state.request_open_modal) {
        initialize_open_browser(state, scene);
        ImGui::OpenPopup("Open Model");
        state.request_open_modal = false;
    }
    std::optional<FileDialogResult> result;

    push_professional_dialog_style();

    ImGui::SetNextWindowSize(ImVec2{1120.0F, 720.0F}, ImGuiCond_Appearing);
    constexpr float maximum_dialog_size = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints(ImVec2{820.0F, 540.0F},
                                        ImVec2{maximum_dialog_size, maximum_dialog_size});
    if (ImGui::BeginPopupModal("Open Model", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        refresh_open_browser_entries(state);

        ImGui::TextColored(ImVec4{0.455F, 0.725F, 0.900F, 1.00F}, "GLTF 2.0 ASSET BROWSER");
        ImGui::SameLine();
        ImGui::TextDisabled("Folders and .gltf/.glb files");
        ImGui::Separator();
        build_open_browser_top_bar(state);
        build_open_browser_error(state);
        ImGui::Separator();

        BrowserFileRequests requests;
        const float footer_height = ImGui::GetFrameHeightWithSpacing() * 2.0F + 18.0F;
        const float sidebar_width = 250.0F;
        if (ImGui::BeginChild("##OpenBrowserSidebar", ImVec2{sidebar_width, -footer_height},
                              true)) {
            build_open_browser_sidebar(state);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        if (ImGui::BeginChild("##OpenBrowserFiles", ImVec2{0.0F, -footer_height}, true)) {
            build_open_browser_file_table(state, requests);
        }
        ImGui::EndChild();

        const std::optional<std::filesystem::path> open_request =
            requests.open_file.has_value() ? requests.open_file : requests.activated_file;
        if (open_request.has_value()) {
            result = FileDialogResult{FileDialogAction::open, path_to_utf8(*open_request)};
            ImGui::CloseCurrentPopup();
        }
        if (!result.has_value()) {
            std::optional<std::string> footer_request = build_open_browser_footer(state);
            if (footer_request.has_value()) {
                result = FileDialogResult{FileDialogAction::open, std::move(*footer_request)};
            }
            build_file_properties_popup(state);
        }
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
    return result;
}

[[nodiscard]] std::optional<std::filesystem::path>
begin_save_browser_request(ViewerState& state, const std::filesystem::path& candidate) {
    std::error_code error;
    if (std::filesystem::exists(candidate, error) && !error) {
        state.pending_save_path = candidate;
        ImGui::OpenPopup("Confirm Replace");
        return std::nullopt;
    }
    return candidate;
}

[[nodiscard]] std::optional<std::filesystem::path> build_save_browser_footer(ViewerState& state) {
    std::optional<std::filesystem::path> requested_path;
    const std::optional<std::filesystem::path> candidate = save_browser_file_candidate(state);
    const bool can_save = candidate.has_value();
    const float button_width = 118.0F;
    const float button_area_width = button_width * 2.0F + ImGui::GetStyle().ItemSpacing.x;
    constexpr float field_label_width = 130.0F;
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("File name");
    ImGui::SameLine(field_label_width);
    ImGui::SetNextItemWidth(-button_area_width - ImGui::GetStyle().ItemSpacing.x);
    const bool save_from_enter = ImGui::InputTextWithHint(
        "##SaveFilePath", "Model name (.glb is the default)", state.open_file_path.data(),
        state.open_file_path.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(!can_save);
    push_primary_action_style();
    if (ImGui::Button("Save", ImVec2{button_width, 0.0F}) || (save_from_enter && can_save)) {
        requested_path = begin_save_browser_request(state, *candidate);
        if (requested_path.has_value()) {
            ImGui::CloseCurrentPopup();
        }
    }
    pop_primary_action_style();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2{button_width, 0.0F})) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("File type");
    ImGui::SameLine(field_label_width);
    ImGui::TextUnformatted("glTF 2.0 Model (*.glb, *.gltf)");
    return requested_path;
}

[[nodiscard]] std::optional<std::filesystem::path> build_replace_confirmation(ViewerState& state) {
    std::optional<std::filesystem::path> confirmed;
    ImGui::SetNextWindowSize(ImVec2{520.0F, 0.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Confirm Replace", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted("A model with this name already exists.");
        if (state.pending_save_path.has_value()) {
            ImGui::TextWrapped("%s", path_to_utf8(*state.pending_save_path).c_str());
        }
        ImGui::TextUnformatted("Replace it?");
        ImGui::Separator();
        const float button_width = 118.0F;
        push_primary_action_style();
        if (ImGui::Button("Replace", ImVec2{button_width, 0.0F})) {
            confirmed = state.pending_save_path;
            ImGui::CloseCurrentPopup();
        }
        pop_primary_action_style();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2{button_width, 0.0F})) {
            state.pending_save_path.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return confirmed;
}

void build_save_browser_file_area(ViewerState& state, BrowserFileRequests& requests) {
    const float footer_height = ImGui::GetFrameHeightWithSpacing() * 2.0F + 18.0F;
    if (ImGui::BeginChild("##SaveBrowserSidebar", ImVec2{250.0F, -footer_height}, true)) {
        build_open_browser_sidebar(state);
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##SaveBrowserFiles", ImVec2{0.0F, -footer_height}, true)) {
        build_open_browser_file_table(state, requests);
    }
    ImGui::EndChild();
}

[[nodiscard]] std::optional<FileDialogResult>
build_save_browser_actions(ViewerState& state, const BrowserFileRequests& requests) {
    std::optional<FileDialogResult> result;
    std::optional<std::filesystem::path> activated_save;
    if (requests.open_file.has_value()) {
        result = FileDialogResult{FileDialogAction::open, path_to_utf8(*requests.open_file)};
        ImGui::CloseCurrentPopup();
    } else if (requests.activated_file.has_value()) {
        activated_save = begin_save_browser_request(state, *requests.activated_file);
    }

    std::optional<std::filesystem::path> footer_request;
    const bool properties_active = state.open_browser_properties.has_value();
    if (!result.has_value()) {
        footer_request = build_save_browser_footer(state);
        build_file_properties_popup(state);
    }
    const std::optional<std::filesystem::path> replacement = build_replace_confirmation(state);
    std::optional<std::filesystem::path> selected = replacement;
    if (!selected.has_value()) {
        selected = activated_save;
    }
    if (!selected.has_value()) {
        selected = footer_request;
    }
    if (selected.has_value()) {
        result = FileDialogResult{FileDialogAction::save, path_to_utf8(*selected)};
        state.pending_save_path.reset();
        ImGui::CloseCurrentPopup();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !state.pending_save_path.has_value() &&
               !properties_active) {
        ImGui::CloseCurrentPopup();
    }
    return result;
}

[[nodiscard]] std::optional<FileDialogResult> build_save_modal(ViewerState& state,
                                                               const ViewerScene& scene) {
    if (state.request_save_modal) {
        initialize_save_browser(state, scene);
        ImGui::OpenPopup("Save Model As");
        state.request_save_modal = false;
    }
    std::optional<FileDialogResult> result;
    push_professional_dialog_style();
    ImGui::SetNextWindowSize(ImVec2{1120.0F, 720.0F}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Save Model As", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        refresh_open_browser_entries(state);
        ImGui::TextColored(ImVec4{0.455F, 0.725F, 0.900F, 1.00F}, "GLTF 2.0 EXPORT");
        ImGui::SameLine();
        ImGui::TextDisabled("Save the retained model document as GLB or glTF");
        ImGui::Separator();
        build_open_browser_top_bar(state);
        build_open_browser_error(state);
        ImGui::Separator();
        BrowserFileRequests requests;
        build_save_browser_file_area(state, requests);
        result = build_save_browser_actions(state, requests);
        ImGui::EndPopup();
    }
    pop_professional_dialog_style();
    return result;
}

} // namespace elf3d::viewer
