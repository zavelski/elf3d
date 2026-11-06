#pragma once

#include "viewer_browser.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace elf3d::viewer {
struct FileBrowserListing {
    std::vector<OpenBrowserEntry> entries;
};

// Explicit filesystem reader, driven by the reference synchronous wrapper or
// an application's CPU lane. Owns only directory metadata and its OS iterator.
class FileBrowserDirectoryRead {
  public:
    FileBrowserDirectoryRead() noexcept = default;
    FileBrowserDirectoryRead(FileBrowserDirectoryRead&&) noexcept = default;
    FileBrowserDirectoryRead& operator=(FileBrowserDirectoryRead&&) noexcept = default;
    FileBrowserDirectoryRead(const FileBrowserDirectoryRead&) = delete;
    FileBrowserDirectoryRead& operator=(const FileBrowserDirectoryRead&) = delete;
    [[nodiscard]] static Result<FileBrowserDirectoryRead>
    begin(const std::filesystem::path& directory);
    // Dispose the reader after an ordinary failure; no partial list is published.
    [[nodiscard]] Result<void> advance(std::size_t maximum_entries);
    [[nodiscard]] bool ready() const noexcept
    {
        return ready_;
    }
    [[nodiscard]] FileBrowserListing take() noexcept;

  private:
    std::filesystem::directory_iterator iterator_;
    FileBrowserListing listing_;
    bool ready_ = false;
};
} // namespace elf3d::viewer
