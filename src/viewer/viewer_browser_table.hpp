#pragma once

#include "viewer_browser.hpp"

#include <optional>

namespace elf3d::viewer {
struct BrowserFileRequests {
    std::optional<std::filesystem::path> activated_file;
    std::optional<std::filesystem::path> open_file;
};

void build_open_browser_file_table(FileBrowserState& browser, BrowserFileRequests& requests,
                                   const FileBrowserFrameInput& input,
                                   ExternalEditorWorkflow& external_editor);
} // namespace elf3d::viewer
