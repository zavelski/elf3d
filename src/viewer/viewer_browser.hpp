#pragma once

#include "viewer_appearance.h"
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::viewer {

struct SceneSession;
class ExternalEditorWorkflow;

struct OpenBrowserEntry {
    std::filesystem::path path;
    std::string label;
    std::string search_key;
    std::string modified_time;
    std::uintmax_t size_bytes = 0;
    bool directory = false;
    bool has_size = false;
};

struct ExternalEditorPaths {
    std::optional<std::filesystem::path> emeditor;
    std::optional<std::filesystem::path> notepad;
    std::optional<std::filesystem::path> notepad_plus_plus;
};

struct ViewerPreferencesState {
    std::filesystem::path storage_path;
    std::filesystem::path last_model_directory;
    std::uint64_t write_revision = 0;
    std::uint64_t attempted_revision = 0;
    std::uint64_t writer_thread = 0;
    std::optional<Error> write_error;
    bool asynchronous_writes = false;
    ViewerAppearance appearance;
};

struct FileBrowserLocation {
    std::string label;
    std::filesystem::path directory;
};

struct FileBrowserState {
    bool request_open_modal = false;
    bool request_save_modal = false;
    std::array<char, 2048> folder_path{};
    std::array<char, 2048> file_path{};
    std::array<char, 256> search{};
    std::filesystem::path directory;
    std::filesystem::path selected_path;
    std::vector<OpenBrowserEntry> entries;
    std::vector<std::size_t> filtered_entries;
    std::string filtered_search;
    std::size_t filter_cursor = 0;
    std::uint64_t entries_revision = 1;
    std::uint64_t filter_revision = 0;
    std::uint64_t directory_sequence = 0;
    std::uint64_t directory_worker_thread = 0;
    std::vector<std::filesystem::path> bookmarks;
    std::vector<std::filesystem::path> recents;
    std::vector<std::filesystem::path> history;
    std::vector<FileBrowserLocation> system_locations;
    std::size_t history_index = 0;
    std::string error;
    ExternalEditorPaths external_editors;
    std::optional<OpenBrowserEntry> properties;
    std::optional<std::filesystem::path> pending_save_path;
    bool initialized = false;
    bool needs_refresh = false;
    bool request_properties = false;
    // The reference Viewer keeps synchronous directory reading. An application
    // may drive this through its own CPU lane without widget directory I/O.
    bool asynchronous_directory = false;
    bool open_modal_active = false;
    bool save_modal_active = false;
    bool directory_reading = false;
    bool locations_ready = false;
};

struct FileBrowserFrameInput {
    bool escape_pressed = false;
    bool primary_double_clicked = false;
};

enum class FileDialogAction {
    open,
    save,
};

struct FileDialogResult {
    FileDialogAction action;
    std::string path;
    ModelWriteOptions write_options;
};

[[nodiscard]] Result<std::filesystem::path> path_from_utf8(std::string_view value) noexcept;
void set_file_browser_directory_utf8(FileBrowserState& browser, std::string_view value);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
[[nodiscard]] std::string lowercase_ascii(std::string value);
[[nodiscard]] std::string file_name_label(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path absolute_path_no_throw(const std::filesystem::path& path);
[[nodiscard]] bool same_path_key(const std::filesystem::path& left,
                                 const std::filesystem::path& right);
void push_unique_directory(std::vector<std::filesystem::path>& directories,
                           const std::filesystem::path& candidate, std::size_t maximum_count,
                           bool verify_available = true);
[[nodiscard]] std::optional<std::filesystem::path> environment_directory(const char* name);

void load_viewer_preferences(ViewerPreferencesState& preferences);
void mark_viewer_preferences_changed(ViewerPreferencesState& preferences);
void remember_model_directory(ViewerPreferencesState& preferences,
                              const std::filesystem::path& model_path);
void initialize_open_browser(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                             const SceneSession& scene);
void initialize_save_browser(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                             const SceneSession& scene);
void set_file_browser_directory(FileBrowserState& browser, const std::filesystem::path& directory,
                                bool add_to_history = true);
void navigate_file_browser_history(FileBrowserState& browser, int offset);
void refresh_file_browser_entries(FileBrowserState& browser);
void advance_file_browser_filter(FileBrowserState& browser, std::size_t maximum_entries = 512);
void select_file_browser_file(FileBrowserState& browser, const std::filesystem::path& path);
void clear_file_browser_selection(FileBrowserState& browser);
void cancel_model_save(FileBrowserState& browser);
[[nodiscard]] bool is_file_io_error(ErrorCode code) noexcept;
[[nodiscard]] bool supported_model_path(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path fallback_open_directory();
[[nodiscard]] std::string format_file_size(std::uintmax_t bytes);
[[nodiscard]] std::vector<FileBrowserLocation> discover_file_browser_locations();
[[nodiscard]] ExternalEditorPaths find_external_editors();
[[nodiscard]] bool launch_external_editor(const std::filesystem::path& editor,
                                          const std::filesystem::path& file);
void build_file_context_menu(const OpenBrowserEntry& entry, FileBrowserState& browser,
                             std::optional<std::filesystem::path>& open_request,
                             ExternalEditorWorkflow& workflow);
void build_file_properties_popup(FileBrowserState& browser, const FileBrowserFrameInput& input);

[[nodiscard]] std::optional<FileDialogResult>
build_open_modal(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                 const SceneSession& scene, const FileBrowserFrameInput& input,
                 ExternalEditorWorkflow& external_editor);
[[nodiscard]] std::optional<FileDialogResult>
build_save_modal(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                 const SceneSession& scene, const FileBrowserFrameInput& input,
                 ExternalEditorWorkflow& external_editor);

} // namespace elf3d::viewer
