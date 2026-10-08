#include <elf3d/internal/gltf.h>

#include "file_path.hpp"

#include <elf3d/core/detail/exception_boundary.h>

#include <string>
#include <system_error>
namespace elf3d::gltf::file_path {

Result<std::filesystem::path> from_utf8(std::string_view value) noexcept
{
    if (value.find('\0') != std::string_view::npos) {
        return Error{ErrorCode::invalid_argument, "A file path cannot contain a null character"};
    }
    return elf3d::detail::fatal_exception_boundary([&]() -> Result<std::filesystem::path> {
        try {
            std::u8string utf8;
            utf8.reserve(value.size());
            for (const char character : value) {
                utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
            }
            return std::filesystem::path{utf8};
        } catch (const std::system_error&) {
            return Error{ErrorCode::invalid_argument,
                         "The file path could not be converted from UTF-8"};
        }
    });
}

} // namespace elf3d::gltf::file_path
