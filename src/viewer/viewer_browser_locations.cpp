#include "viewer_browser.hpp"

#include <array>
#include <string>
#include <system_error>
#include <utility>

namespace elf3d::viewer {
namespace {
void add_location(std::vector<FileBrowserLocation>& locations, std::string label,
                  const std::filesystem::path& directory)
{
    std::error_code error;
    if (std::filesystem::is_directory(directory, error) && !error) {
        locations.push_back({std::move(label), directory});
    }
}
} // namespace

std::vector<FileBrowserLocation> discover_file_browser_locations()
{
    std::vector<FileBrowserLocation> locations;
    if (const auto home = environment_directory("USERPROFILE")) {
        add_location(locations, "Home", *home);
        constexpr std::array folders{"Desktop", "Documents", "Downloads",
                                     "Music",   "Pictures",  "Videos"};
        for (const auto* folder : folders) {
            add_location(locations, folder, *home / folder);
        }
    }
    if (const auto drive = environment_directory("OneDrive")) {
        add_location(locations, "OneDrive", *drive);
    }
#if defined(_WIN32)
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        std::string root{letter};
        root += ":\\";
        const auto converted = path_from_utf8(root);
        if (converted) {
            add_location(locations, root, converted.value());
        }
    }
#endif
    return locations;
}
} // namespace elf3d::viewer
