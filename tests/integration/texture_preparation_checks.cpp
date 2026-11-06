#include "texture_preparation_checks.h"
#include "cube_preparation_checks.h"
#include "pipeline_preparation_checks.h"
#include "texture_upload_state.h"
#include <backend_opengl/device_texture.h>

#include <algorithm>
#include <array>
#include <new>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::graphics;
using Phase = TexturePreparationPhase;
using namespace elf3d::test::native_texture;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::gpu_texture_upload_failed, message};
}
struct Source {
    TextureStorageDescription storage;
    std::vector<std::byte> bytes;
};
[[nodiscard]] Source source_image(TextureFormat format)
{
    Source source;
    source.storage.extent =
        format == TextureFormat::rgba16_float ? Extent2D{65, 37} : Extent2D{1025, 769};
    source.storage.format = format;
    source.storage.min_filter = TextureFilterMode::linear_mipmap_linear;
    source.bytes.resize(texture_storage_bytes(source.storage).value());
    if (format == TextureFormat::rgba16_float) {
        // Exact finite half floats, including signed values. Native byte order
        // must survive the hostile GL_UNPACK_SWAP_BYTES state.
        constexpr std::array<std::uint16_t, 4> pixel{0x3c00, 0x3800, 0xb800, 0x3c00};
        const auto bytes = std::as_bytes(std::span{pixel});
        for (std::size_t i = 0; i < source.bytes.size(); ++i)
            source.bytes[i] = bytes[i % bytes.size()];
    } else {
        for (std::size_t i = 0; i < source.bytes.size(); ++i)
            source.bytes[i] = static_cast<std::byte>((i * 37 + 13) % 251); // small fixture; < 251
    }
    return source;
}
[[nodiscard]] Result<void> reject_early(Texture2DPreparation& preparation)
{
    if (preparation.take_texture() || preparation.request(0))
        return failure("Texture preparation accepted early transfer or a zero allowance");
    const std::array<std::byte, 1> unexpected{};
    const auto before = preparation.driver_calls();
    if (preparation.advance(unexpected) || preparation.driver_calls() != before)
        return failure("Invalid texture setup payload executed native work");
    return {};
}
struct UploadEvidence {
    bool split_row = false;
    bool mipmaps = false;
    std::size_t total = 0;
};
[[nodiscard]] Result<void> reject_bad_pixels(Texture2DPreparation& preparation)
{
    const auto calls = preparation.driver_calls();
    const std::array<std::byte, 3> incomplete{};
    if (preparation.advance(incomplete) || preparation.driver_calls() != calls)
        return failure("Incomplete texture pixels advanced native state");
    return {};
}
[[nodiscard]] Result<void> check_step(const TextureUploadRequest& request,
                                      const TextureUploadStep& advanced, const ForeignState& before)
{
    if (advanced.driver_calls > request.driver_call_limit || request.driver_call_limit > 24 ||
        advanced.uploaded_bytes != request.bytes || foreign_state() != before)
        return failure("Texture step changed foreign state or exceeded its native bounds");
    return {};
}
[[nodiscard]] Result<void> advance_upload(Texture2DPreparation& preparation, const Source& source,
                                          const TextureUploadRequest& request,
                                          UploadEvidence& evidence)
{
    const auto before = foreign_state();
    if (request.phase == Phase::pixels && evidence.total == 0) {
        if (const auto rejected = reject_bad_pixels(preparation); !rejected)
            return rejected.error();
    }
    const auto payload = std::span{source.bytes}.subspan(request.offset_bytes, request.bytes);
    const auto advanced = preparation.advance(payload);
    if (!advanced)
        return advanced.error();
    if (const auto checked = check_step(request, advanced.value(), before); !checked)
        return checked.error();
    evidence.split_row = evidence.split_row || (request.phase == Phase::pixels &&
                                                request.extent.width < source.storage.extent.width);
    evidence.mipmaps = evidence.mipmaps || request.phase == Phase::mipmaps;
    evidence.total += advanced.value().uploaded_bytes;
    return {};
}
[[nodiscard]] Result<std::unique_ptr<Texture2D>> finish_upload(Texture2DPreparation& preparation,
                                                               const Source& source,
                                                               const UploadEvidence& evidence)
{
    if (!evidence.mipmaps || !evidence.split_row || evidence.total != source.bytes.size())
        return failure("Texture became ready before all pixels and mips completed");
    return preparation.take_texture();
}
[[nodiscard]] Result<void> grouped_sampler(Texture2DPreparation& preparation)
{
    const auto state = foreign_state();
    const auto before = preparation.driver_calls();
    const auto small = preparation.advance_setup(1);
    if (!small || small.value().driver_calls != 0 || preparation.driver_calls() != before)
        return failure("Insufficient sampler setup allowance changed state");
    const auto bulk = preparation.advance_setup(16);
    if (!bulk || bulk.value().driver_calls == 0 || bulk.value().driver_calls > 16 ||
        preparation.driver_calls() - before != bulk.value().driver_calls ||
        foreign_state() != state)
        return failure("Grouped sampler setup changed foreign state or call accounting");
    return {};
}
[[nodiscard]] Result<std::unique_ptr<Texture2D>> upload(Texture2DPreparation& preparation,
                                                        const Source& source)
{
    UploadEvidence evidence;
    for (std::size_t i = 0; i < 2048; ++i) {
        const auto next =
            preparation.request(evidence.split_row ? maximum_texture_upload_chunk : 128);
        if (!next)
            return next.error();
        if (next.value().phase == Phase::ready)
            return finish_upload(preparation, source, evidence);
        if (next.value().phase == Phase::sampler) {
            if (const auto setup = grouped_sampler(preparation); !setup)
                return setup.error();
            continue;
        }
        if (const auto step = advance_upload(preparation, source, next.value(), evidence); !step)
            return step.error();
    }
    return failure("Staged texture upload did not complete");
}
[[nodiscard]] Result<void> readback(Texture2D& texture, const Source& source)
{
    const auto native = backend::opengl::device_detail::texture_object(&texture);
    if (!native)
        return native.error();
    ReadbackTarget target{native.value()};
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return failure("Texture readback framebuffer is incomplete");
    const auto extent = source.storage.extent;
    const auto row_bytes = extent.width * texture_pixel_bytes(source.storage.format).value();
    std::vector<std::byte> row(row_bytes); // <= 8200 bytes in these generated fixtures.
    const GLenum type =
        source.storage.format == TextureFormat::rgba16_float ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        glReadPixels(0, static_cast<GLint>(y), static_cast<GLsizei>(extent.width), 1, GL_RGBA, type,
                     row.data());
        const auto expected = std::span{source.bytes}.subspan(y * row_bytes, row_bytes);
        if (glGetError() != GL_NO_ERROR || !std::equal(row.begin(), row.end(), expected.begin()))
            return failure("Texture readback differs from its original pixel bytes");
    }
    return {};
}
[[nodiscard]] Result<void> check_mips(Texture2D& texture, Extent2D extent)
{
    const auto native = backend::opengl::device_detail::texture_object(&texture);
    if (!native)
        return native.error();
    GLint previous = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    glBindTexture(GL_TEXTURE_2D, native.value());
    GLint width = 0;
    GLint height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &height);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous));
    if (width != static_cast<GLint>(extent.width / 2) ||
        height != static_cast<GLint>(extent.height / 2) || glGetError() != GL_NO_ERROR)
        return failure("Explicit mip generation did not produce the expected level");
    return {};
}
[[nodiscard]] Result<void> check_texture_deletion(const ResourceRetirementStep& step,
                                                  const ForeignState& before, GLuint object)
{
    if (!step.complete || step.deleted_objects != 1 || step.driver_calls > 3 ||
        foreign_state() != before || glIsTexture(object))
        return failure("Texture retirement changed foreign state or retained a native object");
    return {};
}
[[nodiscard]] Result<void> verify_texture_idempotence(Device& device, Texture2D& texture)
{
    const auto repeated = device.retire_texture_2d(texture, {0, 0});
    if (!repeated || !repeated.value().complete || repeated.value().driver_calls != 0)
        return failure("Retired texture repeated native work");
    return {};
}
[[nodiscard]] Result<void> verify_retirement(Device& device, Texture2D& texture)
{
    const auto native = backend::opengl::device_detail::texture_object(&texture);
    if (!native)
        return native.error();
    const auto before = foreign_state();
    const auto zero = device.retire_texture_2d(texture, {0, 0});
    if (!zero || zero.value().progressed ||
        !backend::opengl::device_detail::texture_object(&texture))
        return failure("Zero texture retirement allowance changed readiness");
    const auto retired = device.retire_texture_2d(texture, {1, 3});
    if (!retired)
        return retired.error();
    if (const auto checked = check_texture_deletion(retired.value(), before, native.value());
        !checked)
        return checked.error();
    return verify_texture_idempotence(device, texture);
}
[[nodiscard]] Result<void> verify_image(Device& device, TextureFormat format)
{
    const auto source = source_image(format);
    auto preparation = device.begin_texture_2d(source.storage);
    if (!preparation)
        return preparation.error();
    if (const auto early = reject_early(*preparation.value()); !early)
        return early.error();
    auto texture = upload(*preparation.value(), source);
    if (!texture)
        return texture.error();
    if (preparation.value()->request() || preparation.value()->take_texture())
        return failure("Consumed texture preparation remained reusable");
    if (const auto compared = readback(*texture.value(), source); !compared)
        return compared.error();
    if (const auto mips = check_mips(*texture.value(), source.storage.extent); !mips)
        return mips.error();
    return verify_retirement(device, *texture.value());
}
[[nodiscard]] Result<void> verify_abandon(Device& device, const TextureStorageDescription& storage)
{
    auto abandoned = device.begin_texture_2d(storage);
    if (!abandoned)
        return abandoned.error();
    const auto before = foreign_state();
    if (!abandoned.value()->advance())
        return failure("Could not allocate the abandoned texture probe");
    const auto retired = abandoned.value()->advance_retirement({1, 3});
    if (!retired || !retired.value().complete || retired.value().deleted_objects != 1 ||
        abandoned.value()->request())
        return failure("Incomplete texture retirement failed");
    abandoned.value().reset();
    if (foreign_state() != before || glGetError() != GL_NO_ERROR)
        return failure("Abandoning an incomplete texture changed foreign state");
    return {};
}
[[nodiscard]] Result<void> complete_empty(Texture2DPreparation& prepared)
{
    for (std::size_t i = 0; i < 32; ++i) {
        const auto next = prepared.request();
        if (!next)
            return next.error();
        if (next.value().phase == Phase::pixels || next.value().phase == Phase::mipmaps)
            return failure("Empty texture allocation attempted to upload or generate pixels");
        if (next.value().phase == Phase::ready)
            return prepared.take_texture() ? Result<void>{}
                                           : failure("Empty texture transfer failed");
        if (!prepared.advance())
            return failure("Empty texture allocation did not advance");
    }
    return failure("Empty texture allocation did not complete");
}
[[nodiscard]] Result<void> verify_empty_allocation(Device& device)
{
    Texture2DDescription source;
    source.extent = {65, 37};
    source.format = TextureFormat::rgba16_float;
    source.min_filter = TextureFilterMode::linear_mipmap_linear;
    const auto storage = describe_texture_storage(source);
    if (!storage)
        return storage.error();
    if (const auto abandoned = verify_abandon(device, storage.value()); !abandoned)
        return abandoned.error();
    auto prepared = device.begin_texture_2d(storage.value());
    if (!prepared)
        return prepared.error();
    return complete_empty(*prepared.value());
}
[[nodiscard]] Result<void> run(backend::opengl::GraphicsProcedureLoader loader)
{
    auto device = backend::opengl::create_device({loader});
    if (!device)
        return device.error();
    ForeignUnpack foreign;
    if (const auto image = verify_image(*device.value(), TextureFormat::rgba8_unorm); !image)
        return image.error();
    if (const auto image = verify_image(*device.value(), TextureFormat::rgba16_float); !image)
        return image.error();
    if (const auto empty = verify_empty_allocation(*device.value()); !empty)
        return empty.error();
    if (const auto cube = verify_staged_cube_upload(*device.value()); !cube)
        return cube.error();
    return verify_staged_pipeline(*device.value());
}
} // namespace
elf3d::Result<void>
verify_staged_texture_upload(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept
{
    try {
        return run(loader);
    } catch (const std::bad_alloc&) {
        elf3d::fatal_error("Texture preparation probe allocation failed");
    } catch (...) {
        elf3d::fatal_error("Unexpected texture preparation probe failure");
    }
}
