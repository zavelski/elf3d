#include "cube_preparation_checks.h"
#include "texture_upload_state.h"
#include <backend_opengl/device_cube.h>

#include <array>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::graphics;
using namespace elf3d::test::native_texture;
using Phase = TexturePreparationPhase;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::gpu_texture_upload_failed, message};
}
[[nodiscard]] std::uint16_t pattern(std::uint32_t x, std::uint32_t y, std::uint32_t plane,
                                    std::uint32_t channel) noexcept
{
    constexpr std::array<std::uint16_t, 6> values{0x0000, 0x3800, 0x3c00, 0x4000, 0xb800, 0xbc00};
    return values[(3 * x + 5 * y + 7 * plane + channel) % values.size()];
}
[[nodiscard]] std::vector<std::uint16_t> pixels_for(const CubeUploadRequest& request)
{
    const auto& region = request.region;
    std::vector<std::uint16_t> pixels(region.bytes / sizeof(std::uint16_t));
    std::size_t cursor = 0;
    for (std::uint32_t y = 0; y < region.extent.height; ++y)
        for (std::uint32_t x = 0; x < region.extent.width; ++x)
            for (std::uint32_t channel = 0; channel < 4; ++channel)
                pixels[cursor++] =
                    pattern(region.x + x, region.y + y, request.mip * 6 + request.face, channel);
    return pixels;
}
class CubeBinding final {
  public:
    CubeBinding() noexcept
    {
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previous_);
        glGenTextures(1, &texture_);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture_);
    }
    ~CubeBinding() noexcept
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(previous_));
        glDeleteTextures(1, &texture_);
    }
    [[nodiscard]] bool unchanged() const noexcept
    {
        GLint current = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &current);
        return current == static_cast<GLint>(texture_);
    }

  private:
    GLint previous_ = 0;
    GLuint texture_ = 0;
};
[[nodiscard]] Result<void> read_face(GLuint texture, std::uint32_t mip, std::uint32_t face,
                                     std::uint32_t extent)
{
    ReadbackTarget target{texture, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, static_cast<GLint>(mip)};
    if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return failure("Cubemap face framebuffer is incomplete");
    std::vector<std::uint16_t> row(extent * 4);
    for (std::uint32_t y = 0; y < extent; ++y) {
        glReadPixels(0, static_cast<GLint>(y), static_cast<GLsizei>(extent), 1, GL_RGBA,
                     GL_HALF_FLOAT, row.data());
        for (std::uint32_t x = 0; x < extent; ++x)
            for (std::uint32_t channel = 0; channel < 4; ++channel)
                if (row[x * 4 + channel] != pattern(x, y, mip * 6 + face, channel))
                    return failure("Cubemap face/mip pixels changed during bounded upload");
    }
    return glGetError() == GL_NO_ERROR ? Result<void>{} : failure("Cubemap readback failed");
}
[[nodiscard]] Result<void> verify_pixels(TextureCube& texture,
                                         const TextureCubeStorageDescription& storage)
{
    const auto native = backend::opengl::device_detail::texture_cube_object(&texture);
    if (!native || texture.extent() != storage.extent || texture.mip_count() != storage.mip_count)
        return failure("Adopted cubemap lost its metadata or native identity");
    for (std::uint32_t mip = 0; mip < storage.mip_count; ++mip)
        for (std::uint32_t face = 0; face < 6; ++face) {
            const auto checked =
                read_face(native.value(), mip, face, cube_face_storage(storage, mip).extent.width);
            if (!checked)
                return checked.error();
        }
    return {};
}
struct Coverage {
    bool tiled = false;
    bool large = false;
    std::uint64_t bytes = 0;
    void record(const TextureUploadRequest& region, std::uint32_t base_extent) noexcept
    {
        tiled = tiled || (region.phase == Phase::pixels && region.extent.width < base_extent);
        large = large || region.bytes > 1024 * 1024;
        bytes += region.bytes;
    }
    [[nodiscard]] Result<void> validate(const TextureCubeStorageDescription& storage) const
    {
        if (!tiled || !large || bytes != cube_storage_bytes(storage).value())
            return failure("Cubemap did not exercise bounded tiles and complete byte coverage");
        return {};
    }
};
[[nodiscard]] Result<void> advance_packet(TextureCubePreparation& preparation,
                                          const CubeUploadRequest& request)
{
    const auto before = foreign_state();
    const auto calls = preparation.driver_calls();
    const auto pixels = pixels_for(request); // This packet expires after advance.
    const auto step = preparation.advance(std::as_bytes(std::span{pixels}));
    if (!step || step.value().uploaded_bytes != request.region.bytes ||
        step.value().driver_calls > request.region.driver_call_limit)
        return failure("Cubemap exceeded a bounded upload request");
    if (calls + step.value().driver_calls != preparation.driver_calls() ||
        foreign_state() != before)
        return failure("Cubemap call accounting or foreign unpack state changed");
    return {};
}
[[nodiscard]] Result<std::unique_ptr<TextureCube>>
upload(TextureCubePreparation& preparation, const TextureCubeStorageDescription& storage)
{
    Coverage coverage;
    for (unsigned units = 0; units < 4096; ++units) {
        const auto next = preparation.request(coverage.tiled ? maximum_texture_upload_chunk : 128);
        if (!next)
            return next.error();
        if (next.value().region.phase == Phase::ready) {
            if (const auto checked = coverage.validate(storage); !checked)
                return checked.error();
            return preparation.take_texture();
        }
        if (const auto advanced = advance_packet(preparation, next.value()); !advanced)
            return advanced.error();
        coverage.record(next.value().region, storage.extent);
    }
    return failure("Cubemap preparation did not finish its finite units");
}
[[nodiscard]] Result<void> retire_incomplete(TextureCubePreparation& preparation)
{
    const auto idle = preparation.advance_retirement({0, 0});
    if (!idle || idle.value().progressed || !preparation.request())
        return failure("Zero retirement allowance changed the incomplete cubemap");
    const auto retired = preparation.advance_retirement({1, 3});
    if (!retired || !retired.value().complete || retired.value().deleted_objects != 1 ||
        preparation.request())
        return failure("Incomplete cubemap retirement failed");
    return {};
}
[[nodiscard]] Result<void> verify_incomplete(Device& device, TextureCubeStorageDescription storage)
{
    auto invalid = storage;
    invalid.format = TextureFormat::rgba8_unorm;
    if (device.begin_texture_cube(invalid))
        return failure("Cubemap accepted an unsupported pixel format");
    auto prepared = device.begin_texture_cube(storage);
    if (!prepared || prepared.value()->take_texture() || prepared.value()->request(0))
        return failure("Cubemap accepted early adoption or a zero allowance");
    if (!prepared.value()->advance())
        return failure("Cubemap did not allocate its incomplete object");
    return retire_incomplete(*prepared.value());
}
} // namespace
elf3d::Result<void> verify_staged_cube_upload(elf3d::graphics::Device& device)
{
    const TextureCubeStorageDescription storage{TextureFormat::rgba16_float, 513, 3,
                                                TextureFilterMode::linear_mipmap_linear,
                                                TextureFilterMode::linear};
    CubeBinding foreign;
    if (const auto incomplete = verify_incomplete(device, storage); !incomplete)
        return incomplete.error();
    auto prepared = device.begin_texture_cube(storage);
    if (!prepared)
        return prepared.error();
    auto texture = upload(*prepared.value(), storage);
    if (!texture)
        return texture.error();
    if (prepared.value()->request() || !foreign.unchanged())
        return failure("Cubemap adoption retained preparation or changed its foreign binding");
    return verify_pixels(*texture.value(), storage);
}
