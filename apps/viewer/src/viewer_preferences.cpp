#include "viewer_internal.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace elf3d::viewer {

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] float window_content_scale(GLFWwindow* window) noexcept {
    if (window == nullptr) {
        return 1.0F;
    }

    float x_scale = 1.0F;
    float y_scale = 1.0F;
    glfwGetWindowContentScale(window, &x_scale, &y_scale);
    return std::max(1.0F, std::min(std::max(x_scale, y_scale), 3.0F));
}

[[nodiscard]] ImFont* load_viewer_font(GLFWwindow* window, const char* font_path_utf8,
                                       float font_size_pixels) {
    ImGuiIO& io = ImGui::GetIO();
    const float requested_font_size = font_size_pixels * window_content_scale(window);

    if (font_path_utf8 != nullptr && font_path_utf8[0] != '\0') {
        ImFont* font = io.Fonts->AddFontFromFileTTF(font_path_utf8, requested_font_size, nullptr,
                                                    io.Fonts->GetGlyphRangesCyrillic());
        if (font != nullptr) {
            return font;
        }
    }

    ImFontConfig font_config;
    font_config.SizePixels = requested_font_size;
    return io.Fonts->AddFontDefault(&font_config);
}

void copy_text_to_buffer(std::string_view value, std::span<char> buffer) {
    if (buffer.empty()) {
        return;
    }
    std::fill(buffer.begin(), buffer.end(), '\0');
    const std::size_t copy_count = std::min(value.size(), buffer.size() - 1U);
    std::copy_n(value.data(), copy_count, buffer.data());
}

[[nodiscard]] std::string lowercase_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] bool supported_model_path(const std::filesystem::path& path) {
    const std::string extension = lowercase_ascii(path.extension().string());
    return extension == ".gltf" || extension == ".glb";
}

[[nodiscard]] std::string file_name_label(const std::filesystem::path& path) {
    std::string label = path_to_utf8(path.filename());
    if (label.empty()) {
        label = path_to_utf8(path);
    }
    return label;
}

[[nodiscard]] std::filesystem::path fallback_open_directory() {
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? std::filesystem::path{"."} : current;
}

[[nodiscard]] std::filesystem::path absolute_path_no_throw(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : absolute.lexically_normal();
}

[[nodiscard]] std::string normalized_path_key(const std::filesystem::path& path) {
    std::string key = path_to_utf8(absolute_path_no_throw(path));
#if defined(_WIN32)
    key = lowercase_ascii(std::move(key));
#endif
    return key;
}

[[nodiscard]] bool same_path_key(const std::filesystem::path& left,
                                 const std::filesystem::path& right) {
    return normalized_path_key(left) == normalized_path_key(right);
}

void push_unique_directory(std::vector<std::filesystem::path>& directories,
                           const std::filesystem::path& directory, std::size_t maximum_count) {
    std::error_code error;
    const std::filesystem::path resolved = absolute_path_no_throw(directory);
    if (!std::filesystem::is_directory(resolved, error) || error) {
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

[[nodiscard]] std::optional<std::string> environment_value(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t value_size = 0;
    const int status = _dupenv_s(&value, &value_size, name);
    const std::unique_ptr<char, decltype(&std::free)> owned_value{value, &std::free};
    if (status != 0 || owned_value == nullptr || owned_value.get()[0] == '\0') {
        return std::nullopt;
    }
    return std::string{owned_value.get()};
#else
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string{value};
#endif
}

[[nodiscard]] std::optional<std::filesystem::path> environment_directory(const char* name) {
    const std::optional<std::string> value = environment_value(name);
    if (!value.has_value()) {
        return std::nullopt;
    }

    std::error_code error;
    std::filesystem::path path = absolute_path_no_throw(path_from_utf8(*value));
    if (!std::filesystem::is_directory(path, error) || error) {
        return std::nullopt;
    }
    return path;
}

[[nodiscard]] std::optional<std::filesystem::path> viewer_preferences_path() {
#if defined(_WIN32)
    const std::optional<std::string> base = environment_value("LOCALAPPDATA");
    if (base.has_value()) {
        return path_from_utf8(*base) / "Elf3D" / "viewer-state.ini";
    }
#elif defined(__APPLE__)
    const std::optional<std::string> home = environment_value("HOME");
    if (home.has_value()) {
        return path_from_utf8(*home) / "Library" / "Application Support" / "Elf3D" /
               "viewer-state.ini";
    }
#else
    const std::optional<std::string> config_home = environment_value("XDG_CONFIG_HOME");
    if (config_home.has_value()) {
        return path_from_utf8(*config_home) / "Elf3D" / "viewer-state.ini";
    }
    const std::optional<std::string> home = environment_value("HOME");
    if (home.has_value()) {
        return path_from_utf8(*home) / ".config" / "Elf3D" / "viewer-state.ini";
    }
#endif
    return std::nullopt;
}

void load_viewer_preferences(ViewerState& state) {
    const std::optional<std::filesystem::path> path = viewer_preferences_path();
    if (!path.has_value()) {
        return;
    }
    state.preferences_path = *path;
    std::ifstream input{*path, std::ios::binary};
    std::string line;
    constexpr std::string_view prefix = "last_model_directory=";
    if (!std::getline(input, line) || !line.starts_with(prefix)) {
        return;
    }
    const std::filesystem::path directory = path_from_utf8(line.substr(prefix.size()));
    std::error_code error;
    if (std::filesystem::is_directory(directory, error) && !error) {
        state.last_model_directory = absolute_path_no_throw(directory);
    }
}

void remember_model_directory(ViewerState& state, const std::filesystem::path& model_path) {
    if (!model_path.has_parent_path()) {
        return;
    }
    state.last_model_directory = absolute_path_no_throw(model_path.parent_path());
    if (state.preferences_path.empty()) {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(state.preferences_path.parent_path(), error);
    if (error) {
        return;
    }
    std::ofstream output{state.preferences_path, std::ios::binary | std::ios::trunc};
    output << "last_model_directory=" << path_to_utf8(state.last_model_directory) << '\n';
}

void initialize_open_browser_bookmarks(ViewerState& state) {
    if (!state.open_browser_bookmarks.empty()) {
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
        push_unique_directory(state.open_browser_bookmarks, path, 12);
    }
}

void set_open_browser_selected_file(ViewerState& state, const std::filesystem::path& path) {
    state.open_browser_selected_path = path;
    copy_text_to_buffer(path_to_utf8(path), state.open_file_path);
}

void clear_open_browser_selected_file(ViewerState& state) {
    state.open_browser_selected_path.clear();
    copy_text_to_buffer("", state.open_file_path);
}

[[nodiscard]] std::string format_file_size(std::uintmax_t bytes) {
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

[[nodiscard]] std::string format_file_time(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::file_time_type file_time = std::filesystem::last_write_time(path, error);
    if (error) {
        return "-";
    }

    const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    const std::time_t time = std::chrono::system_clock::to_time_t(system_time);

    std::tm local_time{};
#if defined(_WIN32)
    if (localtime_s(&local_time, &time) != 0) {
        return "-";
    }
#else
    if (localtime_r(&time, &local_time) == nullptr) {
        return "-";
    }
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%d %b %Y %H:%M");
    return stream.str();
}

void set_open_browser_directory(ViewerState& state, const std::filesystem::path& directory,
                                bool record_history) {
    std::filesystem::path resolved = absolute_path_no_throw(directory);
    std::error_code error;
    if (!std::filesystem::is_directory(resolved, error) || error) {
        state.open_browser_error = "Folder is not available: " + path_to_utf8(directory);
        return;
    }

    state.open_browser_directory = resolved;
    clear_open_browser_selected_file(state);
    state.open_browser_error.clear();
    state.open_browser_needs_refresh = true;
    copy_text_to_buffer(path_to_utf8(resolved), state.open_folder_path);
    push_unique_directory(state.open_browser_recents, resolved, 10);

    if (record_history) {
        if (state.open_browser_history.empty() ||
            !same_path_key(state.open_browser_history[state.open_browser_history_index],
                           resolved)) {
            if (state.open_browser_history_index + 1U < state.open_browser_history.size()) {
                state.open_browser_history.erase(
                    state.open_browser_history.begin() +
                        static_cast<std::ptrdiff_t>(state.open_browser_history_index + 1U),
                    state.open_browser_history.end());
            }
            state.open_browser_history.push_back(resolved);
            state.open_browser_history_index = state.open_browser_history.size() - 1U;
        }
    }
}

void navigate_open_browser_history(ViewerState& state, int offset) {
    if (state.open_browser_history.empty()) {
        return;
    }

    const int current = static_cast<int>(state.open_browser_history_index);
    const int maximum = static_cast<int>(state.open_browser_history.size() - 1U);
    const int next = std::clamp(current + offset, 0, maximum);
    if (next == current) {
        return;
    }

    state.open_browser_history_index = static_cast<std::size_t>(next);
    set_open_browser_directory(state, state.open_browser_history[state.open_browser_history_index],
                               false);
}

void refresh_open_browser_entries(ViewerState& state) {
    if (!state.open_browser_needs_refresh) {
        return;
    }

    state.open_browser_needs_refresh = false;
    state.open_browser_entries.clear();

    std::error_code iteration_error;
    constexpr std::filesystem::directory_options options =
        std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::directory_iterator iterator{state.open_browser_directory, options,
                                                 iteration_error};
    const std::filesystem::directory_iterator end;
    if (iteration_error) {
        state.open_browser_error = "Could not read folder: " + iteration_error.message();
        return;
    }

    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        std::error_code entry_error;
        const bool is_directory = entry.is_directory(entry_error);
        if (!entry_error && (is_directory || supported_model_path(entry.path()))) {
            OpenBrowserEntry browser_entry;
            browser_entry.path = entry.path();
            browser_entry.label = file_name_label(entry.path());
            browser_entry.modified_time = format_file_time(entry.path());
            browser_entry.directory = is_directory;
            if (!is_directory) {
                std::error_code size_error;
                browser_entry.size_bytes = entry.file_size(size_error);
                browser_entry.has_size = !size_error;
            }
            state.open_browser_entries.push_back(std::move(browser_entry));
        }
        iterator.increment(iteration_error);
        if (iteration_error) {
            state.open_browser_error =
                "Could not finish reading folder: " + iteration_error.message();
            break;
        }
    }

    std::sort(state.open_browser_entries.begin(), state.open_browser_entries.end(),
              [](const OpenBrowserEntry& left, const OpenBrowserEntry& right) {
                  if (left.directory != right.directory) {
                      return left.directory;
                  }
                  return lowercase_ascii(left.label) < lowercase_ascii(right.label);
              });
}

void initialize_open_browser(ViewerState& state, const ViewerScene& scene) {
    initialize_open_browser_bookmarks(state);
    state.external_editors = find_external_editors();
    state.open_browser_properties.reset();
    state.request_open_browser_properties = false;
    state.open_browser_history.clear();
    state.open_browser_history_index = 0;
    copy_text_to_buffer("", state.open_search);

    std::filesystem::path directory = fallback_open_directory();
    if (scene.is_imported() && scene.source_path.has_parent_path()) {
        directory = scene.source_path.parent_path();
    } else if (!state.last_model_directory.empty()) {
        directory = state.last_model_directory;
    }

    state.open_browser_initialized = true;
    set_open_browser_directory(state, directory);
    if (scene.is_imported()) {
        set_open_browser_selected_file(state, scene.source_path);
    }
}

void initialize_save_browser(ViewerState& state, const ViewerScene& scene) {
    initialize_open_browser(state, scene);
    state.pending_save_path.reset();
    if (scene.is_imported()) {
        copy_text_to_buffer(path_to_utf8(scene.source_path.filename()), state.open_file_path);
    }
}

} // namespace elf3d::viewer
