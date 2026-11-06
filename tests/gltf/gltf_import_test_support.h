#pragma once

#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::gltf::import_tests {

inline constexpr std::array<std::uint8_t, 77> asymmetric_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
     0x73, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xc0,
     0x00, 0xc2, 0x0c, 0xff, 0xff, 0xff, 0x67, 0x00, 0x00, 0x1e, 0xef, 0x04, 0xfc, 0xa3, 0xc8, 0xb4,
     0xf7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

inline constexpr std::string_view jpeg_base64 =
    "/9j/4AAQSkZJRgABAQAAAQABAAD/"
    "2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKS"
    "j/"
    "2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKC"
    "j/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAP/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/"
    "8QAFAEBAAAAAAAAAAAAAAAAAAAABv/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AJAB58//2Q==";

class TemporaryDirectory final {
  public:
    TemporaryDirectory() : path_(std::filesystem::path{ELF3D_TEST_BINARY_DIR} / "gltf_import")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

inline void append_byte(std::vector<std::byte>& output, std::uint8_t value)
{
    output.push_back(static_cast<std::byte>(value));
}

inline void append_u16(std::vector<std::byte>& output, std::uint16_t value)
{
    append_byte(output, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

inline void append_u32(std::vector<std::byte>& output, std::uint32_t value)
{
    append_byte(output, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

inline void append_float(std::vector<std::byte>& output, float value)
{
    append_u32(output, std::bit_cast<std::uint32_t>(value));
}

[[nodiscard]] inline std::vector<std::byte> triangle_positions()
{
    std::vector<std::byte> output;
    for (const float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}) {
        append_float(output, value);
    }
    return output;
}

[[nodiscard]] inline std::vector<std::byte> textured_geometry()
{
    std::vector<std::byte> output = triangle_positions();
    for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}) {
        append_float(output, value);
    }
    for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}) {
        append_float(output, value);
    }
    return output;
}

[[nodiscard]] inline bool write_bytes(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes)
{
    std::ofstream stream{path, std::ios::binary};
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] inline bool write_text(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream{path, std::ios::binary};
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] inline std::string base64(std::span<const std::byte> bytes)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((bytes.size() + 2U) / 3U * 4U);
    for (std::size_t index = 0; index < bytes.size(); index += 3U) {
        const std::uint32_t first = std::to_integer<std::uint8_t>(bytes[index]);
        const std::uint32_t second =
            index + 1U < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 1U]) : 0U;
        const std::uint32_t third =
            index + 2U < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 2U]) : 0U;
        const std::uint32_t value = (first << 16U) | (second << 8U) | third;
        output.push_back(alphabet[(value >> 18U) & 0x3fU]);
        output.push_back(alphabet[(value >> 12U) & 0x3fU]);
        output.push_back(index + 1U < bytes.size() ? alphabet[(value >> 6U) & 0x3fU] : '=');
        output.push_back(index + 2U < bytes.size() ? alphabet[value & 0x3fU] : '=');
    }
    return output;
}

[[nodiscard]] inline std::uint32_t decode_base64_value(char character) noexcept
{
    if (character >= 'A' && character <= 'Z') {
        return static_cast<std::uint32_t>(character - 'A');
    }
    if (character >= 'a' && character <= 'z') {
        return static_cast<std::uint32_t>(character - 'a' + 26);
    }
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint32_t>(character - '0' + 52);
    }
    return character == '+' ? 62U : 63U;
}

[[nodiscard]] inline std::vector<std::byte> decode_base64(std::string_view source)
{
    std::vector<std::byte> output;
    for (std::size_t index = 0; index < source.size(); index += 4U) {
        const std::uint32_t combined =
            (decode_base64_value(source[index]) << 18U) |
            (decode_base64_value(source[index + 1U]) << 12U) |
            ((source[index + 2U] == '=' ? 0U : decode_base64_value(source[index + 2U])) << 6U) |
            (source[index + 3U] == '=' ? 0U : decode_base64_value(source[index + 3U]));
        append_byte(output, static_cast<std::uint8_t>((combined >> 16U) & 0xffU));
        if (source[index + 2U] != '=') {
            append_byte(output, static_cast<std::uint8_t>((combined >> 8U) & 0xffU));
        }
        if (source[index + 3U] != '=') {
            append_byte(output, static_cast<std::uint8_t>(combined & 0xffU));
        }
    }
    return output;
}

[[nodiscard]] inline std::vector<std::byte> make_glb(std::string json,
                                                     std::vector<std::byte> binary)
{
    while (json.size() % 4U != 0U) {
        json.push_back(' ');
    }
    while (binary.size() % 4U != 0U) {
        binary.push_back(std::byte{0});
    }

    std::vector<std::byte> output;
    append_u32(output, 0x46546c67U);
    append_u32(output, 2U);
    append_u32(output, static_cast<std::uint32_t>(20U + json.size() + 8U + binary.size()));
    append_u32(output, static_cast<std::uint32_t>(json.size()));
    append_u32(output, 0x4e4f534aU);
    for (const char character : json) {
        append_byte(output, static_cast<std::uint8_t>(character));
    }
    append_u32(output, static_cast<std::uint32_t>(binary.size()));
    append_u32(output, 0x004e4942U);
    output.insert(output.end(), binary.begin(), binary.end());
    return output;
}

[[nodiscard]] inline bool nearly_equal(float left, float right) noexcept
{
    return std::abs(left - right) <= 0.00001F;
}

[[nodiscard]] inline bool has_diagnostic(const elf3d::ModelLoadReport& report,
                                         elf3d::ModelLoadDiagnosticCode code)
{
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [code](const elf3d::ModelLoadDiagnostic& diagnostic) noexcept {
                           return diagnostic.code == code;
                       });
}

[[nodiscard]] inline bool same_bytes(std::span<const std::byte> left,
                                     std::span<const std::byte> right) noexcept
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

[[nodiscard]] inline std::string simple_triangle_json(std::string_view buffer_member)
{
    return std::string{R"json({"asset":{"version":"2.0"},"buffers":[{)json"} +
           std::string{buffer_member} +
           R"json(}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
}

[[nodiscard]] int test_containers_and_images(const TemporaryDirectory& temporary);

} // namespace elf3d::gltf::import_tests
