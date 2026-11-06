#include "viewer_browser.hpp"

#include "viewer_browser_directory.hpp"
#include "viewer_preferences_store.hpp"
#include "viewer_scene_session.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace elf3d::viewer {
[[nodiscard]] std::optional<std::string> environment_value(const char* name);

void set_file_browser_directory_utf8(FileBrowserState& browser, std::string_view value)
{
    Result<std::filesystem::path> path = path_from_utf8(value);
    if (path) {
        set_file_browser_directory(browser, path.value());
    } else {
        browser.error = path.error().message();
    }
}

void copy_text_to_buffer(std::string_view value, std::span<char> buffer)
{
    if (buffer.empty()) {
        return;
    }

    std::fill(buffer.begin(), buffer.end(), '\0');
    const std::size_t copy_count = std::min(value.size(), buffer.size() - 1U);
    std::copy_n(value.data(), copy_count, buffer.data());
}

void push_unique_directory(std::vector<std::filesystem::path>& directories,
                           const std::filesystem::path& directory, std::size_t maximum_count,
                           bool verify_available)
{
    std::error_code error;
    const std::filesystem::path resolved = absolute_path_no_throw(directory);
    if (verify_available && (!std::filesystem::is_directory(resolved, error) || error)) {
        return;
    }

    directories.erase(std::remove_if(directories.begin(), directories.end(),
                                     [&resolved](const std::filesystem::path& existing) {
                                         return same_path_key(existing, resolved);
                                     }),
                      directories.end());
    directories.insert(directories.begin(), resolved);
    if (directories.size() > maximum_count) {
        directories.resize(maximum_count);
    }
}

[[nodiscard]] std::optional<std::filesystem::path> viewer_preferences_path()
{
#if defined(_WIN32)
    const std::optional<std::string> base = environment_value("LOCALAPPDATA");
    if (base.has_value()) {
        Result<std::filesystem::path> converted = path_from_utf8(*base);
        if (converted) {
            return converted.value() / "Elf3D" / "viewer-state.ini";
        }
    }
#elif defined(__APPLE__)
    const std::optional<std::string> home = environment_value("HOME");
    if (home.has_value()) {
        Result<std::filesystem::path> converted = path_from_utf8(*home);
        if (converted) {
            return converted.value() / "Library" / "Application Support" / "Elf3D" /
                   "viewer-state.ini";
        }
    }
#else
    const std::optional<std::string> config_home = environment_value("XDG_CONFIG_HOME");
    if (config_home.has_value()) {
        Result<std::filesystem::path> converted = path_from_utf8(*config_home);
        if (converted) {
            return converted.value() / "Elf3D" / "viewer-state.ini";
        }
    }

    const std::optional<std::string> home = environment_value("HOME");
    if (home.has_value()) {
        Result<std::filesystem::path> converted = path_from_utf8(*home);
        if (converted) {
            return converted.value() / ".config" / "Elf3D" / "viewer-state.ini";
        }
    }
#endif
    return std::nullopt;
}

namespace {
void read_model_directory_preference(ViewerPreferencesState& preferences, std::string_view value)
{
    const auto converted = path_from_utf8(value);
    if (!converted)
        return;
    std::error_code error;
    if (std::filesystem::is_directory(converted.value(), error) && !error)
        preferences.last_model_directory = absolute_path_no_throw(converted.value());
}
[[nodiscard]] std::uint16_t preference_percent(std::string_view text, std::uint16_t previous)
{
    std::uint16_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? value : previous;
}
void read_preference_line(ViewerPreferencesState& preferences, std::string_view line)
{
    if (line.ends_with('\r'))
        line.remove_suffix(1);
    constexpr std::string_view directory = "last_model_directory=";
    constexpr std::string_view ui_scale = "ui_scale_percent=";
    constexpr std::string_view text_scale = "text_scale_percent=";
    if (line.starts_with(directory)) {
        read_model_directory_preference(preferences, line.substr(directory.size()));
        return;
    }
    auto appearance = preferences.appearance;
    if (line == "theme=light")
        appearance.theme = ViewerTheme::light;
    else if (line == "theme=dark")
        appearance.theme = ViewerTheme::dark;
    else if (line.starts_with(ui_scale))
        appearance.ui_scale_percent =
            preference_percent(line.substr(ui_scale.size()), appearance.ui_scale_percent);
    else if (line.starts_with(text_scale))
        appearance.text_scale_percent =
            preference_percent(line.substr(text_scale.size()), appearance.text_scale_percent);
    if (valid_viewer_appearance(appearance))
        preferences.appearance = appearance;
}
} // namespace
void load_viewer_preferences(ViewerPreferencesState& preferences)
{
    std::optional<std::filesystem::path> path = viewer_preferences_path();
    if (!preferences.storage_path.empty()) {
        path = preferences.storage_path;
    }
    if (!path.has_value()) {
        return;
    }
    preferences.storage_path = *path;
    std::ifstream input{*path, std::ios::binary};
    std::string line;
    while (std::getline(input, line))
        read_preference_line(preferences, line);
}

void remember_model_directory(ViewerPreferencesState& preferences,
                              const std::filesystem::path& model_path)
{
    if (!model_path.has_parent_path()) {
        return;
    }
    preferences.last_model_directory = absolute_path_no_throw(model_path.parent_path());
    mark_viewer_preferences_changed(preferences);
}

void mark_viewer_preferences_changed(ViewerPreferencesState& preferences)
{
    if (preferences.storage_path.empty()) {
        return;
    }

    if (preferences.write_revision == std::numeric_limits<std::uint64_t>::max()) {
        fatal_error("Viewer preference revisions exhausted");
    }
    ++preferences.write_revision;
    preferences.write_error.reset();
    if (preferences.asynchronous_writes) {
        return;
    }
    const auto stored =
        store_viewer_preferences({preferences.storage_path, preferences.last_model_directory,
                                  preferences.write_revision, preferences.appearance});
    preferences.attempted_revision = preferences.write_revision;
    if (!stored) {
        preferences.write_error = stored.error();
    }
}

void initialize_bookmarks(FileBrowserState& browser)
{
    if (!browser.bookmarks.empty()) {
        return;
    }
    if (browser.asynchronous_directory) {
        push_unique_directory(browser.bookmarks, browser.directory, 12, false);
        return;
    }

    const std::optional<std::filesystem::path> user_profile = environment_directory("USERPROFILE");
    std::vector<std::filesystem::path> defaults;
    if (user_profile.has_value()) {
        const std::filesystem::path& profile_path = *user_profile;
        defaults.push_back(profile_path);
    }

    defaults.push_back(fallback_open_directory());

    for (const std::filesystem::path& path : defaults) {
        push_unique_directory(browser.bookmarks, path, 12);
    }
}

void select_file_browser_file(FileBrowserState& browser, const std::filesystem::path& path)
{
    browser.selected_path = path;
    copy_text_to_buffer(path_to_utf8(path), browser.file_path);
}

void clear_file_browser_selection(FileBrowserState& browser)
{
    browser.selected_path.clear();
    copy_text_to_buffer("", browser.file_path);
}

[[nodiscard]] std::string format_file_size(std::uintmax_t bytes)
{
    constexpr std::array<const char*, 5> units{"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1U < units.size()) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream stream;
    if (unit_index == 0U) {
        stream << bytes << ' ' << units[unit_index];
    } else {
        stream << std::fixed << std::setprecision(value < 10.0 ? 1 : 0) << value << ' '
               << units[unit_index];
    }
    return stream.str();
}

void set_file_browser_directory(FileBrowserState& browser, const std::filesystem::path& directory,
                                bool record_history)
{
    std::filesystem::path resolved = absolute_path_no_throw(directory);
    std::error_code error;
    if (!browser.asynchronous_directory &&
        (!std::filesystem::is_directory(resolved, error) || error)) {
        browser.error = "Folder is not available: " + path_to_utf8(directory);
        return;
    }

    browser.directory = resolved;
    clear_file_browser_selection(browser);
    browser.error.clear();
    browser.needs_refresh = true;
    ++browser.directory_sequence;
    copy_text_to_buffer(path_to_utf8(resolved), browser.folder_path);
    push_unique_directory(browser.recents, resolved, 10, !browser.asynchronous_directory);

    if (record_history) {
        if (browser.history.empty() ||
            !same_path_key(browser.history[browser.history_index], resolved)) {
            if (browser.history_index + 1U < browser.history.size()) {
                browser.history.erase(browser.history.begin() +
                                          static_cast<std::ptrdiff_t>(browser.history_index + 1U),
                                      browser.history.end());
            }

            browser.history.push_back(resolved);
            browser.history_index = browser.history.size() - 1U;
        }
    }
}

void navigate_file_browser_history(FileBrowserState& browser, int offset)
{
    if (browser.history.empty()) {
        return;
    }

    const int current = static_cast<int>(browser.history_index);
    const int maximum = static_cast<int>(browser.history.size() - 1U);
    const int next = std::clamp(current + offset, 0, maximum);
    if (next == current) {
        return;
    }

    browser.history_index = static_cast<std::size_t>(next);
    set_file_browser_directory(browser, browser.history[browser.history_index], false);
}

void refresh_file_browser_entries(FileBrowserState& browser)
{
    if (!browser.needs_refresh || browser.asynchronous_directory) {
        return;
    }
    browser.needs_refresh = false;
    browser.entries.clear();
    ++browser.entries_revision;
    auto reader = FileBrowserDirectoryRead::begin(browser.directory);
    if (!reader) {
        browser.error = reader.error().message();
        return;
    }
    while (!reader.value().ready()) {
        if (const auto advanced = reader.value().advance(256); !advanced) {
            browser.error = advanced.error().message();
            return;
        }
    }
    auto listing = reader.value().take();
    browser.entries = std::move(listing.entries);
    browser.error.clear();
}

void initialize_open_browser(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                             const SceneSession& scene)
{
    if (!browser.asynchronous_directory) {
        browser.external_editors = find_external_editors();
    }
    browser.properties.reset();
    browser.request_properties = false;
    browser.history.clear();
    browser.history_index = 0;
    copy_text_to_buffer("", browser.search);

    std::filesystem::path directory = fallback_open_directory();
    if (scene.is_imported() && scene.source_path.has_parent_path()) {
        directory = scene.source_path.parent_path();
    } else if (!preferences.last_model_directory.empty()) {
        directory = preferences.last_model_directory;
    }

    browser.initialized = true;
    set_file_browser_directory(browser, directory);
    initialize_bookmarks(browser);
    if (scene.is_imported()) {
        select_file_browser_file(browser, scene.source_path);
    }
}

void initialize_save_browser(FileBrowserState& browser, const ViewerPreferencesState& preferences,
                             const SceneSession& scene)
{
    initialize_open_browser(browser, preferences, scene);
    browser.pending_save_path.reset();
    if (scene.is_imported()) {
        copy_text_to_buffer(path_to_utf8(scene.source_path.filename()), browser.file_path);
    }
}

} // namespace elf3d::viewer
