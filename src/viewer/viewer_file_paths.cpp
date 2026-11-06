#include "viewer_browser.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <utility>

namespace elf3d::viewer {
[[nodiscard]] Result<std::filesystem::path> path_from_utf8(std::string_view value) noexcept
{
    if (value.find('\0') != std::string_view::npos) {
        return Error{ErrorCode::invalid_argument, "A file path cannot contain a null character"};
    }
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
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D viewer path allocation failed");
    } catch (...) {
        fatal_error("Elf3D viewer path conversion encountered an unexpected exception");
    }
}

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] std::string lowercase_ascii(std::string value)
{
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character + ('a' - 'A'));
        }
    }
    return value;
}

[[nodiscard]] bool supported_model_path(const std::filesystem::path& path)
{
    const std::string extension = lowercase_ascii(path.extension().string());
    return extension == ".gltf" || extension == ".glb";
}

[[nodiscard]] std::string file_name_label(const std::filesystem::path& path)
{
    std::string label = path_to_utf8(path.filename());
    if (label.empty()) {
        label = path_to_utf8(path);
    }
    return label;
}

[[nodiscard]] std::filesystem::path fallback_open_directory()
{
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? std::filesystem::path{"."} : current;
}

[[nodiscard]] std::filesystem::path absolute_path_no_throw(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : absolute.lexically_normal();
}

[[nodiscard]] std::string normalized_path_key(const std::filesystem::path& path)
{
    std::string key = path_to_utf8(absolute_path_no_throw(path));
#if defined(_WIN32)
    key = lowercase_ascii(std::move(key));
#endif
    return key;
}

[[nodiscard]] bool same_path_key(const std::filesystem::path& left,
                                 const std::filesystem::path& right)
{
    return normalized_path_key(left) == normalized_path_key(right);
}

[[nodiscard]] std::optional<std::string> environment_value(const char* name)
{
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

[[nodiscard]] std::optional<std::filesystem::path> environment_directory(const char* name)
{
    const std::optional<std::string> value = environment_value(name);
    if (!value.has_value()) {
        return std::nullopt;
    }

    std::error_code error;
    Result<std::filesystem::path> converted = path_from_utf8(*value);
    if (!converted) {
        return std::nullopt;
    }
    std::filesystem::path path = absolute_path_no_throw(converted.value());
    if (!std::filesystem::is_directory(path, error) || error) {
        return std::nullopt;
    }
    return path;
}

} // namespace elf3d::viewer
