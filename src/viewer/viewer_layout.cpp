#include "viewer_layout.hpp"
#include "viewer_browser.hpp"
#include "viewer_component_state.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>
#include <fstream>
#include <new>
#include <system_error>
#include <utility>

namespace elf3d::viewer {
namespace {
constexpr std::string_view header = "; Elf3D layout version=1\n";
constexpr std::string_view old_header = "; Elf3D layout version=0\n";
constexpr std::size_t maximum_layout_bytes = 1024 * 1024;

[[nodiscard]] Error layout_error(const char* message)
{
    return {ErrorCode::invalid_argument, message};
}

void rename_window(std::string& text, std::string_view before, std::string_view after)
{
    std::size_t position = 0;
    while ((position = text.find(before, position)) != std::string::npos) {
        const auto end = position + before.size(); // find proved the complete token is in range
        if ((position != 0 && text[position - 1] != '\n') ||
            (end != text.size() && text[end] != '\n' && text[end] != '\r')) {
            position = end;
            continue;
        }
        text.replace(position, before.size(), after);
        position += after.size();
    }
}

[[nodiscard]] std::filesystem::path layout_path(const ViewerPreferencesState& preferences)
{
    auto path = preferences.storage_path;
    path += ".layout";
    return path;
}

[[nodiscard]] Result<void> read_layout(ViewerShellState& shell,
                                       const ViewerPreferencesState& preferences)
{
    if (preferences.storage_path.empty()) {
        return {};
    }
    const auto path = layout_path(preferences);
    shell.layout_read_only = true;
    std::error_code error;
    const auto exists = std::filesystem::exists(path, error);
    if (error) {
        return layout_error("Cannot inspect the saved workspace layout");
    }
    if (!exists) {
        shell.layout_read_only = false;
        return {};
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maximum_layout_bytes) {
        return layout_error("Saved workspace layout exceeds its size limit or cannot be read");
    }
    std::ifstream input{path, std::ios::binary};
    std::string text(static_cast<std::size_t>(size), '\0'); // checked <= 1 MiB
    if (!input.read(text.data(), static_cast<std::streamsize>(text.size()))) {
        return layout_error("Cannot read the saved workspace layout");
    }
    auto migrated = migrate_viewer_layout(text);
    if (!migrated) {
        return migrated.error();
    }
    ImGui::LoadIniSettingsFromMemory(migrated.value().data(), migrated.value().size());
    shell.layout_read_only = false;
    return {};
}

[[nodiscard]] Result<void> write_layout(const ViewerShellState& shell,
                                        const ViewerPreferencesState& preferences)
{
    if (preferences.storage_path.empty() || shell.layout_read_only) {
        return {};
    }
    std::size_t size = 0;
    const auto* text = ImGui::SaveIniSettingsToMemory(&size);
    if (size > maximum_layout_bytes - header.size()) {
        return layout_error("Workspace layout exceeds its storage limit");
    }
    const auto path = layout_path(preferences);
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    if (error) {
        return layout_error("Cannot create the workspace settings directory");
    }
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    output.write(text, static_cast<std::streamsize>(size)); // checked <= 1 MiB
    output.flush();
    if (!output) {
        return layout_error("Cannot save the workspace layout");
    }
    return {};
}
} // namespace

Result<std::string> migrate_viewer_layout(std::string_view text)
{
    if (text.size() > maximum_layout_bytes || text.find('\0') != std::string_view::npos) {
        return layout_error("Saved workspace layout is invalid");
    }
    if (text.starts_with(header)) {
        text.remove_prefix(header.size());
    } else if (text.starts_with(old_header)) {
        text.remove_prefix(old_header.size());
    } else if (!text.empty() && !text.starts_with("[Window][") && !text.starts_with("[Docking][")) {
        return layout_error("Saved workspace layout uses an unsupported format version");
    }
    std::string migrated{text};
    rename_window(migrated, "[Window][Scene Hierarchy]", "[Window][Scene Browser]");
    rename_window(migrated, "[Window][Selection]", "[Window][Properties]");
    return migrated;
}

Result<void> load_viewer_layout(ViewerShellState& shell,
                                const ViewerPreferencesState& preferences) noexcept
{
    try {
        return read_layout(shell, preferences);
    } catch (const std::bad_alloc&) {
        fatal_error("Workspace layout allocation failed");
    } catch (const std::filesystem::filesystem_error&) {
        return layout_error("Cannot load the workspace layout");
    } catch (...) {
        fatal_error("Unexpected workspace layout read failure");
    }
}

Result<void> save_viewer_layout(const ViewerShellState& shell,
                                const ViewerPreferencesState& preferences) noexcept
{
    try {
        return write_layout(shell, preferences);
    } catch (const std::bad_alloc&) {
        fatal_error("Workspace layout allocation failed");
    } catch (const std::filesystem::filesystem_error&) {
        return layout_error("Cannot save the workspace layout");
    } catch (...) {
        fatal_error("Unexpected workspace layout write failure");
    }
}

void adopt_saved_viewer_layout(ViewerShellState& shell, ImGuiID dockspace) noexcept
{
    if (shell.dock_layout_initialized || shell.reset_dock_layout) {
        return;
    }
    const auto* root = ImGui::DockBuilderGetNode(dockspace);
    if (!root) {
        return;
    }
    const auto saved_dock = [dockspace](const char* name) {
        const auto* settings = ImGui::FindWindowSettingsByID(ImHashStr(name));
        return settings && settings->DockId != 0 ? settings->DockId : dockspace;
    };
    shell.dock_center_id = saved_dock("3D View");
    shell.dock_right_id = saved_dock("Scene Browser");
    shell.dock_right_bottom_id = saved_dock("Properties");
    shell.dock_layout_initialized = true;
    shell.apply_dock_layout = false;
}
} // namespace elf3d::viewer
