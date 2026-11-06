#pragma once

#include "viewer_appearance.h"
#include <elf3d/core/result.h>

#include <cstdint>
#include <filesystem>

namespace elf3d::viewer {
struct PreferencesWriteRequest {
    std::filesystem::path storage_path;
    std::filesystem::path last_model_directory;
    std::uint64_t revision = 0;
    ViewerAppearance appearance;
};

// The caller exclusively owns this small snapshot for the whole synchronous
// filesystem operation. An application may invoke it on its own CPU lane.
[[nodiscard]] Result<void> store_viewer_preferences(const PreferencesWriteRequest& request);
} // namespace elf3d::viewer
