#include "viewer_browser_directory.hpp"

#include <elf3d/core/assert.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace elf3d::viewer {
namespace {
[[nodiscard]] std::string modified_time(const std::filesystem::directory_entry& entry)
{
    std::error_code error;
    const auto file_time = entry.last_write_time(error);
    if (error) {
        return "-";
    }
    const auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() +
        std::chrono::system_clock::now());
    const auto time = std::chrono::system_clock::to_time_t(system_time);
    std::tm local{};
#if defined(_WIN32)
    if (localtime_s(&local, &time) != 0) {
        return "-";
    }
#else
    if (localtime_r(&time, &local) == nullptr) {
        return "-";
    }
#endif
    std::ostringstream text;
    text << std::put_time(&local, "%Y-%m-%d %H:%M");
    return text.str();
}

void append_entry(FileBrowserListing& listing, const std::filesystem::directory_entry& entry)
{
    std::error_code error;
    const bool directory = entry.is_directory(error);
    if (error || (!directory && !supported_model_path(entry.path()))) {
        return;
    }
    OpenBrowserEntry value;
    value.path = entry.path();
    value.label = file_name_label(entry.path());
    value.search_key = lowercase_ascii(value.label);
    value.directory = directory;
    value.modified_time = modified_time(entry);
    if (!directory) {
        value.size_bytes = entry.file_size(error);
        value.has_size = !error;
    }
    listing.entries.push_back(std::move(value));
}
} // namespace

Result<FileBrowserDirectoryRead>
FileBrowserDirectoryRead::begin(const std::filesystem::path& directory)
{
    FileBrowserDirectoryRead reader;
    std::error_code error;
    reader.iterator_ = std::filesystem::directory_iterator{directory, error};
    if (error) {
        return Error{ErrorCode::source_file_read_failed, "Could not read folder (system code " +
                                                             std::to_string(error.value()) +
                                                             "): " + path_to_utf8(directory)};
    }
    return reader;
}

Result<void> FileBrowserDirectoryRead::advance(std::size_t maximum_entries)
{
    if (ready_) {
        return {};
    }
    const std::filesystem::directory_iterator end;
    for (std::size_t count = 0; count < maximum_entries && iterator_ != end; ++count) {
        append_entry(listing_, *iterator_);
        std::error_code error;
        iterator_.increment(error);
        if (error) {
            return Error{ErrorCode::source_file_read_failed,
                         "Could not finish reading folder (system code " +
                             std::to_string(error.value()) + ")"};
        }
    }
    if (iterator_ == end) {
        std::sort(listing_.entries.begin(), listing_.entries.end(),
                  [](const OpenBrowserEntry& left, const OpenBrowserEntry& right) {
                      if (left.directory != right.directory) {
                          return left.directory;
                      }
                      if (left.search_key != right.search_key) {
                          return left.search_key < right.search_key;
                      }
                      return left.label < right.label;
                  });
        ready_ = true;
    }
    return {};
}

FileBrowserListing FileBrowserDirectoryRead::take() noexcept
{
    ELF3D_ASSERT(ready_);
    return std::move(listing_);
}

void advance_file_browser_filter(FileBrowserState& browser, std::size_t maximum_entries)
{
    const auto search = lowercase_ascii(std::string{browser.search.data()});
    if (browser.filtered_search != search || browser.filter_revision != browser.entries_revision) {
        browser.filtered_entries.clear();
        browser.filter_cursor = 0;
        browser.filter_revision = browser.entries_revision;
        browser.filtered_search = search;
    }
    if (search.empty()) {
        browser.filter_cursor = browser.entries.size();
        return;
    }
    const auto stop = browser.filter_cursor +
                      std::min(maximum_entries, browser.entries.size() - browser.filter_cursor);
    while (browser.filter_cursor < stop) {
        const auto index = browser.filter_cursor++;
        if (browser.entries[index].search_key.find(search) != std::string::npos) {
            browser.filtered_entries.push_back(index);
        }
    }
}
} // namespace elf3d::viewer
