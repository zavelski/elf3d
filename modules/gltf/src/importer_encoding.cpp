module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

module elf.gltf;

import elf.image;

namespace elf3d::gltf::importer_encoding {
namespace {

[[nodiscard]] int base64_value(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    return character == '/' ? 63 : -1;
}

struct Base64Group {
    std::uint32_t value = 0;
    bool pad2 = false;
    bool pad3 = false;
};

[[nodiscard]] bool valid_base64_characters(int first, int second, int third, int fourth) noexcept {
    return first >= 0 && second >= 0 && third >= 0 && fourth >= 0;
}

[[nodiscard]] bool valid_base64_padding(bool pad2, bool pad3, bool final_group) noexcept {
    return (!pad2 || pad3) && (!(pad2 || pad3) || final_group);
}

[[nodiscard]] Result<Base64Group> base64_group(std::string_view payload, std::size_t index) {
    const bool final_group = index + 4 == payload.size();
    const bool pad2 = payload[index + 2] == '=';
    const bool pad3 = payload[index + 3] == '=';
    const int first = base64_value(payload[index]);
    const int second = base64_value(payload[index + 1]);
    const int third = pad2 ? 0 : base64_value(payload[index + 2]);
    const int fourth = pad3 ? 0 : base64_value(payload[index + 3]);
    if (!valid_base64_characters(first, second, third, fourth) ||
        !valid_base64_padding(pad2, pad3, final_group)) {
        return Error{ErrorCode::invalid_base64_payload,
                     "Image data URI contains malformed base64 padding or characters"};
    }
    const std::uint32_t value =
        (static_cast<std::uint32_t>(first) << 18U) | (static_cast<std::uint32_t>(second) << 12U) |
        (static_cast<std::uint32_t>(third) << 6U) | static_cast<std::uint32_t>(fourth);
    return Base64Group{value, pad2, pad3};
}

void append_base64_group(std::vector<std::byte>& result, Base64Group group) {
    result.push_back(static_cast<std::byte>((group.value >> 16U) & 0xffU));
    if (!group.pad2) {
        result.push_back(static_cast<std::byte>((group.value >> 8U) & 0xffU));
    }
    if (!group.pad3) {
        result.push_back(static_cast<std::byte>(group.value & 0xffU));
    }
}

[[nodiscard]] int hex_value(char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
}

[[nodiscard]] Result<char> percent_escape(std::string_view uri, std::size_t index) {
    if (index + 2 >= uri.size()) {
        return Error{ErrorCode::external_image_read_failed,
                     "External image URI contains a truncated percent escape"};
    }
    const int high = hex_value(uri[index + 1]);
    const int low = hex_value(uri[index + 2]);
    if (high < 0 || low < 0) {
        return Error{ErrorCode::external_image_read_failed,
                     "External image URI contains an invalid percent escape"};
    }
    return static_cast<char>((high << 4) | low);
}

} // namespace

Result<std::vector<std::byte>> decode_base64(std::string_view payload) {
    if (payload.empty() || payload.size() % 4 != 0 ||
        payload.size() / 4 > image::maximum_encoded_bytes / 3 + 1) {
        return Error{ErrorCode::invalid_base64_payload,
                     "Image data URI contains an invalid base64 length"};
    }
    std::vector<std::byte> result;
    result.reserve(payload.size() / 4 * 3);
    for (std::size_t index = 0; index < payload.size(); index += 4) {
        const Result<Base64Group> group = base64_group(payload, index);
        if (!group) {
            return group.error();
        }
        append_base64_group(result, group.value());
    }
    if (result.size() > image::maximum_encoded_bytes) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Decoded image data URI exceeds the 64 MiB encoded-byte limit"};
    }
    return result;
}

Result<std::string> percent_decode(std::string_view uri) {
    std::string result;
    result.reserve(uri.size());
    for (std::size_t index = 0; index < uri.size(); ++index) {
        if (uri[index] != '%') {
            result.push_back(uri[index]);
            continue;
        }
        const Result<char> decoded = percent_escape(uri, index);
        if (!decoded) {
            return decoded.error();
        }
        result.push_back(decoded.value());
        index += 2;
    }
    return result;
}

} // namespace elf3d::gltf::importer_encoding
