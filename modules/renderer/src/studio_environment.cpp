module;

#include <elf3d/core/result.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

module elf.renderer;

import elf.graphics;

namespace elf3d::renderer {
namespace {

constexpr std::size_t resource_header_bytes = 64;
constexpr std::uint32_t resource_version = 3;
constexpr std::uint32_t rgba16_float_format = 1;
constexpr std::uint32_t cubemap_face_count = 6;
constexpr std::uint32_t diffuse_extent = 32;
constexpr std::uint32_t diffuse_mip_count = 1;
constexpr std::uint32_t specular_extent = 128;
constexpr std::uint32_t specular_mip_count = 8;
constexpr std::uint32_t brdf_extent = 256;
constexpr std::uint64_t expected_payload_bytes = 1'622'000;
constexpr std::uint64_t maximum_environment_bytes = 2U * 1024U * 1024U;
constexpr std::size_t rgba16_float_texel_bytes = 8;
constexpr std::array<std::byte, 8> resource_magic{
    std::byte{'E'}, std::byte{'L'}, std::byte{'F'}, std::byte{'3'},
    std::byte{'D'}, std::byte{'I'}, std::byte{'B'}, std::byte{'L'},
};

constexpr std::size_t version_offset = 8;
constexpr std::size_t header_size_offset = 12;
constexpr std::size_t format_offset = 16;
constexpr std::size_t face_count_offset = 20;
constexpr std::size_t diffuse_extent_offset = 24;
constexpr std::size_t diffuse_mip_count_offset = 28;
constexpr std::size_t specular_extent_offset = 32;
constexpr std::size_t specular_mip_count_offset = 36;
constexpr std::size_t brdf_width_offset = 40;
constexpr std::size_t brdf_height_offset = 44;
constexpr std::size_t payload_size_offset = 48;
constexpr std::size_t payload_checksum_offset = 56;
constexpr std::array<std::pair<std::size_t, std::uint32_t>, 10> expected_header_fields{{
    {version_offset, resource_version},
    {header_size_offset, static_cast<std::uint32_t>(resource_header_bytes)},
    {format_offset, rgba16_float_format},
    {face_count_offset, cubemap_face_count},
    {diffuse_extent_offset, diffuse_extent},
    {diffuse_mip_count_offset, diffuse_mip_count},
    {specular_extent_offset, specular_extent},
    {specular_mip_count_offset, specular_mip_count},
    {brdf_width_offset, brdf_extent},
    {brdf_height_offset, brdf_extent},
}};

struct StudioEnvironmentView final {
    std::array<graphics::TextureCubeMipDescription, diffuse_mip_count> diffuse_mips;
    std::array<graphics::TextureCubeMipDescription, specular_mip_count> specular_mips;
    std::span<const std::byte> brdf_pixels;
};

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> bytes,
                                     std::size_t offset) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << static_cast<std::uint32_t>(index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(std::span<const std::byte> bytes,
                                     std::size_t offset) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << static_cast<std::uint32_t>(index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept {
    std::uint64_t value = 14'695'981'039'346'656'037ULL;
    for (const std::byte byte : bytes) {
        value ^= std::to_integer<std::uint8_t>(byte);
        value *= 1'099'511'628'211ULL;
    }
    return value;
}

[[nodiscard]] bool has_expected_header(std::span<const std::byte> resource) noexcept {
    if (resource.size() < resource_header_bytes) {
        return false;
    }
    for (std::size_t index = 0; index < resource_magic.size(); ++index) {
        if (resource[index] != resource_magic[index]) {
            return false;
        }
    }
    for (const auto& [offset, expected] : expected_header_fields) {
        if (read_u32(resource, offset) != expected) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Result<std::span<const std::byte>>
validated_payload(std::span<const std::byte> resource) noexcept {
    if (!has_expected_header(resource)) {
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in studio environment header is invalid"};
    }
    const std::uint64_t payload_bytes = read_u64(resource, payload_size_offset);
    if (payload_bytes != expected_payload_bytes || payload_bytes > maximum_environment_bytes ||
        resource.size() != resource_header_bytes + expected_payload_bytes) {
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in studio environment byte count is invalid"};
    }
    const std::span<const std::byte> payload = resource.subspan(resource_header_bytes);
    if (fnv1a64(payload) != read_u64(resource, payload_checksum_offset)) {
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in studio environment checksum is invalid"};
    }
    return payload;
}

[[nodiscard]] std::size_t face_bytes(std::uint32_t extent) noexcept {
    return static_cast<std::size_t>(extent) * extent * rgba16_float_texel_bytes;
}

void describe_mip(graphics::TextureCubeMipDescription& description,
                  std::span<const std::byte> payload, std::size_t& cursor,
                  std::uint32_t extent) noexcept {
    description.extent = extent;
    const std::size_t bytes = face_bytes(extent);
    for (std::span<const std::byte>& face : description.faces) {
        face = payload.subspan(cursor, bytes);
        cursor += bytes;
    }
}

[[nodiscard]] StudioEnvironmentView describe_environment(std::span<const std::byte> payload) {
    StudioEnvironmentView result;
    std::size_t cursor = 0;
    describe_mip(result.diffuse_mips.front(), payload, cursor, diffuse_extent);
    for (std::uint32_t level = 0; level < specular_mip_count; ++level) {
        describe_mip(result.specular_mips[level], payload, cursor,
                     std::max(specular_extent >> level, 1U));
    }
    result.brdf_pixels = payload.subspan(cursor, face_bytes(brdf_extent));
    return result;
}

} // namespace

Result<bool> Renderer::ensure_environment_resources() {
    if (environment_ != nullptr) {
        return false;
    }
    if (environment_source_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Renderer studio environment source is unavailable"};
    }

    Result<std::span<const std::byte>> resource_result = environment_source_->bytes();
    if (!resource_result) {
        return resource_result.error();
    }
    Result<std::span<const std::byte>> payload_result = validated_payload(resource_result.value());
    if (!payload_result) {
        return payload_result.error();
    }
    const StudioEnvironmentView environment = describe_environment(payload_result.value());

    const graphics::TextureCubeDescription diffuse_description{
        graphics::TextureFormat::rgba16_float, environment.diffuse_mips,
        graphics::TextureFilterMode::linear, graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::TextureCube>> diffuse_result =
        device_->create_texture_cube(diffuse_description);
    if (!diffuse_result) {
        return diffuse_result.error();
    }

    const graphics::TextureCubeDescription specular_description{
        graphics::TextureFormat::rgba16_float, environment.specular_mips,
        graphics::TextureFilterMode::linear_mipmap_linear, graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::TextureCube>> specular_result =
        device_->create_texture_cube(specular_description);
    if (!specular_result) {
        return specular_result.error();
    }

    const graphics::Texture2DDescription brdf_description{
        {brdf_extent, brdf_extent},
        graphics::TextureFormat::rgba16_float,
        environment.brdf_pixels,
        graphics::TextureAddressMode::clamp_to_edge,
        graphics::TextureAddressMode::clamp_to_edge,
        graphics::TextureFilterMode::linear,
        graphics::TextureFilterMode::linear};
    Result<std::unique_ptr<graphics::Texture2D>> brdf_result =
        device_->create_texture_2d(brdf_description);
    if (!brdf_result) {
        return brdf_result.error();
    }

    auto resources = std::make_unique<EnvironmentResources>();
    resources->diffuse = std::move(diffuse_result).value();
    resources->specular = std::move(specular_result).value();
    resources->brdf_lut = std::move(brdf_result).value();
    resources->resident_bytes = expected_payload_bytes;
    environment_ = std::move(resources);
    return true;
}

} // namespace elf3d::renderer
