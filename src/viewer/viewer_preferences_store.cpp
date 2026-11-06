#include "viewer_preferences_store.hpp"
#include "viewer_browser.hpp"

#include <fstream>
#include <system_error>

namespace elf3d::viewer {
Result<void> store_viewer_preferences(const PreferencesWriteRequest& request)
{
    if (!valid_viewer_appearance(request.appearance))
        return Error{ErrorCode::invalid_argument, "Invalid appearance preferences"};
    if (request.storage_path.empty()) {
        return {};
    }
    std::error_code error;
    if (request.storage_path.has_parent_path()) {
        std::filesystem::create_directories(request.storage_path.parent_path(), error);
    }
    if (error) {
        return Error{ErrorCode::source_file_write_failed,
                     "Cannot create the preferences directory"};
    }
    std::ofstream output{request.storage_path, std::ios::binary | std::ios::trunc};
    output << "last_model_directory=" << path_to_utf8(request.last_model_directory) << '\n';
    output << "theme=" << (request.appearance.theme == ViewerTheme::dark ? "dark" : "light") << '\n'
           << "ui_scale_percent=" << request.appearance.ui_scale_percent << '\n'
           << "text_scale_percent=" << request.appearance.text_scale_percent << '\n';
    output.close();
    if (!output) {
        return Error{ErrorCode::source_file_write_failed, "Cannot save application preferences"};
    }
    return {};
}
} // namespace elf3d::viewer
