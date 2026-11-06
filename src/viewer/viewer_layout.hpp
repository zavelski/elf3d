#pragma once

#include <elf3d/core/result.h>
#include <imgui.h>

#include <string>
#include <string_view>

namespace elf3d::viewer {
struct ViewerShellState;
struct ViewerPreferencesState;
// Pure text migration preserves all unrecognized ImGui settings verbatim.
[[nodiscard]] Result<std::string> migrate_viewer_layout(std::string_view text);
[[nodiscard]] Result<void> load_viewer_layout(ViewerShellState& shell,
                                              const ViewerPreferencesState& preferences) noexcept;
[[nodiscard]] Result<void> save_viewer_layout(const ViewerShellState& shell,
                                              const ViewerPreferencesState& preferences) noexcept;
void adopt_saved_viewer_layout(ViewerShellState& shell, ImGuiID dockspace) noexcept;
} // namespace elf3d::viewer
