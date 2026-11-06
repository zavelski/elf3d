#include "renderer_environment_preparation.h"

#include <algorithm>
#include <elf3d/core/assert.h>
#include <utility>

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

[[nodiscard]] std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << static_cast<std::uint32_t>(index * 8U);
    }
    return value;
}

[[nodiscard]] std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + index]))
                 << static_cast<std::uint32_t>(index * 8U);
    }
    return value;
}

[[nodiscard]] bool has_expected_header(std::span<const std::byte> resource) noexcept
{
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

[[nodiscard]] Result<std::span<const std::byte>> payload(std::span<const std::byte> resource)
{
    if (!has_expected_header(resource))
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in studio environment header is invalid"};
    const auto bytes = read_u64(resource, payload_size_offset);
    if (bytes != expected_payload_bytes || bytes > maximum_environment_bytes ||
        resource.size() != resource_header_bytes + expected_payload_bytes)
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in studio environment byte count is invalid"};
    return resource.subspan(resource_header_bytes);
}
[[nodiscard]] SceneResourceWork shared_work(GraphicsPreparationPhase phase, bool progressed,
                                            std::optional<EnvironmentTextureKind> texture = {})
{
    SceneResourceWork work;
    work.phase = phase;
    work.progressed = progressed;
    work.shared_resource = true;
    work.environment_texture = texture;
    return work;
}
[[nodiscard]] std::size_t face_bytes(std::uint32_t extent) noexcept
{
    return static_cast<std::size_t>(extent) * extent * rgba16_float_texel_bytes;
}
[[nodiscard]] graphics::TextureCubeStorageDescription cube_storage(std::size_t index)
{
    return {graphics::TextureFormat::rgba16_float, index == 0 ? diffuse_extent : specular_extent,
            index == 0 ? diffuse_mip_count : specular_mip_count,
            index == 0 ? graphics::TextureFilterMode::linear
                       : graphics::TextureFilterMode::linear_mipmap_linear,
            graphics::TextureFilterMode::linear};
}
[[nodiscard]] std::size_t cube_offset(std::size_t index, const graphics::CubeUploadRequest& request)
{
    const auto storage = cube_storage(index);
    std::size_t offset = index == 0 ? 0 : face_bytes(diffuse_extent) * 6;
    for (std::uint32_t mip = 0; mip < request.mip; ++mip)
        offset += face_bytes(graphics::cube_face_storage(storage, mip).extent.width) * 6;
    offset +=
        face_bytes(graphics::cube_face_storage(storage, request.mip).extent.width) * request.face;
    return offset + request.region.offset_bytes;
}
[[nodiscard]] SceneResourceWork texture_work(const graphics::TextureUploadStep& native,
                                             graphics::TexturePreparationPhase phase,
                                             EnvironmentTextureKind texture)
{
    const auto activity = phase == graphics::TexturePreparationPhase::storage
                              ? GraphicsPreparationPhase::environment_allocation
                          : phase == graphics::TexturePreparationPhase::pixels
                              ? GraphicsPreparationPhase::environment_upload
                              : GraphicsPreparationPhase::environment_setup;
    auto work = shared_work(activity, true, texture);
    work.uploaded_bytes = native.uploaded_bytes;
    work.allocated_bytes = native.allocated_bytes;
    work.driver_calls = native.driver_calls;
    return work;
}
[[nodiscard]] SceneResourceWork cube_work(const graphics::TextureUploadStep& native,
                                          const graphics::CubeUploadRequest& request,
                                          std::size_t index)
{
    auto work = texture_work(native, request.region.phase,
                             index == 0 ? EnvironmentTextureKind::diffuse
                                        : EnvironmentTextureKind::specular);
    if (request.region.phase == graphics::TexturePreparationPhase::storage ||
        request.region.phase == graphics::TexturePreparationPhase::pixels) {
        work.mip_level = request.mip;
        work.cube_face = request.face;
    }
    return work;
}
[[nodiscard]] GraphicsPreparationFailure attempted_environment(const SceneResourceWork& work,
                                                               std::uint32_t call_limit)
{
    GraphicsPreparationFailure attempt;
    attempt.phase = work.phase;
    attempt.environment_texture = work.environment_texture;
    attempt.mip_level = work.mip_level;
    attempt.cube_face = work.cube_face;
    attempt.driver_call_limit = call_limit;
    return attempt;
}
[[nodiscard]] Result<std::span<const std::byte>> pixel_slice(std::span<const std::byte> bytes,
                                                             std::size_t offset, std::size_t size)
{
    if (size == 0)
        return std::span<const std::byte>{};
    if (offset > bytes.size() || size > bytes.size() - offset)
        return Error{ErrorCode::graphics_initialization_failed,
                     "Environment upload request exceeds its immutable source"};
    return bytes.subspan(offset, size);
}
} // namespace

std::uint64_t EnvironmentPreparation::allocated_bytes() const noexcept
{
    return allocated[0] + allocated[1] + allocated[2];
}
std::unique_ptr<EnvironmentResources> EnvironmentPreparation::take_resources()
{
    ELF3D_ASSERT(!failed && phase == Phase::ready);
    auto resources = std::make_unique<EnvironmentResources>();
    resources->resident_bytes = allocated_bytes();
    auto diffuse = cubes[0]->take_texture();
    auto specular = cubes[1]->take_texture();
    auto brdf = lut->take_texture();
    ELF3D_ASSERT(diffuse.has_value() && specular.has_value() && brdf.has_value());
    resources->diffuse = std::move(diffuse).value();
    resources->specular = std::move(specular).value();
    resources->brdf_lut = std::move(brdf).value();
    return resources;
}
bool EnvironmentPreparation::needs_retirement() const noexcept
{
    return failed && (cubes[0] || cubes[1] || lut);
}
Result<GraphicsRetirementStep>
EnvironmentPreparation::retire_unit(const graphics::ResourceRetirementBudget& budget)
{
    GraphicsRetirementStep step;
    step.phase = GraphicsRetirementPhase::environment_object;
    if (!needs_retirement())
        return step;
    const std::size_t index = cubes[0] ? 0 : cubes[1] ? 1 : 2;
    auto native =
        index < 2 ? cubes[index]->advance_retirement(budget) : lut->advance_retirement(budget);
    if (!native)
        return native.error();
    step.driver_calls = native.value().driver_calls;
    step.deleted_objects = native.value().deleted_objects;
    step.progressed = native.value().progressed || native.value().complete;
    if (native.value().complete) {
        allocated[index] = 0;
        if (index < 2)
            cubes[index].reset();
        else
            lut.reset();
    }
    return step;
}
Result<SceneResourceWork> EnvironmentPreparation::checksum(std::span<const std::byte> bytes)
{
    const auto size = bytes.size() - checksum_cursor_;
    for (const auto byte : bytes.subspan(checksum_cursor_, size)) {
        checksum_ ^= std::to_integer<std::uint8_t>(byte);
        checksum_ *= 1'099'511'628'211ULL;
    }
    checksum_cursor_ += size;
    if (checksum_cursor_ == bytes.size()) {
        if (checksum_ != expected_checksum_)
            return Error{ErrorCode::graphics_initialization_failed,
                         "The built-in studio environment checksum is invalid"};
        phase = Phase::diffuse;
    }
    return shared_work(GraphicsPreparationPhase::environment_checksum, true);
}
Result<SceneResourceWork>
EnvironmentPreparation::begin_cube(graphics::Device& device, std::size_t index,
                                   const GraphicsPreparationBudget& budget)
{
    if (budget.driver_calls == 0)
        return shared_work(GraphicsPreparationPhase::environment_setup, false);
    attempt = attempted_environment(shared_work(GraphicsPreparationPhase::environment_setup, false,
                                                index == 0 ? EnvironmentTextureKind::diffuse
                                                           : EnvironmentTextureKind::specular),
                                    1);
    auto made = device.begin_texture_cube(cube_storage(index));
    if (!made)
        return made.error();
    cubes[index] = std::move(made).value();
    auto work = shared_work(GraphicsPreparationPhase::environment_setup, true,
                            index == 0 ? EnvironmentTextureKind::diffuse
                                       : EnvironmentTextureKind::specular);
    work.driver_calls = static_cast<std::uint32_t>(cubes[index]->driver_calls());
    return work;
}
Result<SceneResourceWork>
EnvironmentPreparation::advance_cube(graphics::Device& device, std::span<const std::byte> bytes,
                                     const GraphicsPreparationBudget& budget)
{
    const std::size_t index = phase == Phase::diffuse ? 0 : 1;
    auto& preparation = cubes[index];
    if (!preparation)
        return begin_cube(device, index, budget);
    const auto next = preparation->request(
        std::max<std::size_t>(std::min(budget.upload_bytes, budget.transfer_bytes), 8));
    if (!next)
        return next.error();
    const auto& region = next.value().region;
    attempt = attempted_environment(cube_work({}, next.value(), index), region.driver_call_limit);
    if (region.phase == graphics::TexturePreparationPhase::ready) {
        phase = index == 0 ? Phase::specular : Phase::lut;
        return shared_work(GraphicsPreparationPhase::environment_setup, true);
    }
    if (region.driver_call_limit > budget.driver_calls || region.bytes > budget.upload_bytes)
        return shared_work(GraphicsPreparationPhase::environment_upload, false);
    const auto input = pixel_slice(bytes, cube_offset(index, next.value()), region.bytes);
    if (!input)
        return input.error();
    const auto step = preparation->advance(input.value());
    if (!step)
        return step.error();
    allocated[index] += step.value().allocated_bytes;
    return cube_work(step.value(), next.value(), index);
}
Result<SceneResourceWork>
EnvironmentPreparation::advance_lut(graphics::Device& device, std::span<const std::byte> bytes,
                                    const GraphicsPreparationBudget& budget)
{
    if (!lut) {
        if (budget.driver_calls == 0)
            return shared_work(GraphicsPreparationPhase::environment_setup, false);
        graphics::TextureStorageDescription storage;
        storage.extent = {brdf_extent, brdf_extent};
        storage.format = graphics::TextureFormat::rgba16_float;
        storage.wrap_u = graphics::TextureAddressMode::clamp_to_edge;
        storage.wrap_v = graphics::TextureAddressMode::clamp_to_edge;
        attempt = attempted_environment(shared_work(GraphicsPreparationPhase::environment_setup,
                                                    false, EnvironmentTextureKind::brdf_lut),
                                        1);
        auto made = device.begin_texture_2d(storage);
        if (!made)
            return made.error();
        lut = std::move(made).value();
        auto work = shared_work(GraphicsPreparationPhase::environment_setup, true);
        work.driver_calls = static_cast<std::uint32_t>(lut->driver_calls());
        return work;
    }
    const auto next = lut->request(
        std::max<std::size_t>(std::min(budget.upload_bytes, budget.transfer_bytes), 8));
    if (!next)
        return next.error();
    attempt = attempted_environment(
        texture_work({}, next.value().phase, EnvironmentTextureKind::brdf_lut),
        next.value().driver_call_limit);
    if (next.value().phase == graphics::TexturePreparationPhase::ready) {
        phase = Phase::ready;
        return shared_work(GraphicsPreparationPhase::environment_ready, true);
    }
    if (next.value().driver_call_limit > budget.driver_calls ||
        next.value().bytes > budget.upload_bytes)
        return shared_work(GraphicsPreparationPhase::environment_upload, false);
    const auto offset = bytes.size() - face_bytes(brdf_extent) + next.value().offset_bytes;
    const auto input = pixel_slice(bytes, offset, next.value().bytes);
    if (!input)
        return input.error();
    const auto step = lut->advance(input.value());
    if (!step)
        return step.error();
    allocated[2] += step.value().allocated_bytes;
    return texture_work(step.value(), next.value().phase, EnvironmentTextureKind::brdf_lut);
}
Result<SceneResourceWork>
EnvironmentPreparation::advance_payload(graphics::Device& device,
                                        std::span<const std::byte> resource,
                                        const GraphicsPreparationBudget& budget)
{
    const auto bytes = payload(resource);
    if (!bytes)
        return bytes.error();
    const auto expected = read_u64(resource, payload_checksum_offset);
    if (phase == Phase::header) {
        expected_checksum_ = expected;
        phase = Phase::checksum;
        return shared_work(GraphicsPreparationPhase::environment_checksum, true);
    }
    if (expected != expected_checksum_)
        return Error{ErrorCode::graphics_initialization_failed,
                     "The built-in environment changed during preparation"};
    if (phase == Phase::checksum)
        return checksum(bytes.value());
    if (phase == Phase::diffuse || phase == Phase::specular)
        return advance_cube(device, bytes.value(), budget);
    if (phase == Phase::lut)
        return advance_lut(device, bytes.value(), budget);
    return shared_work(GraphicsPreparationPhase::environment_ready, true);
}
Result<SceneResourceWork> EnvironmentPreparation::advance(graphics::Device& device,
                                                          std::span<const std::byte> resource,
                                                          const GraphicsPreparationBudget& budget)
{
    attempt = {};
    attempt.phase = phase == Phase::checksum ? GraphicsPreparationPhase::environment_checksum
                                             : GraphicsPreparationPhase::environment_setup;
    if (failed)
        return Error{ErrorCode::invalid_argument,
                     "Failed environment preparation requires retirement"};
    auto step = advance_payload(device, resource, budget);
    if (!step)
        failed = true;
    return step;
}
} // namespace elf3d::renderer
