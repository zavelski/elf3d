#include "device_cube.h"
#include "device_texture_state.h"

#include <array>
#include <limits>
#include <new>
#include <utility>

namespace elf3d::backend::opengl::device_detail {
namespace {
using Phase = graphics::TexturePreparationPhase;
[[nodiscard]] GLint filter_value(graphics::TextureFilterMode filter) noexcept
{
    constexpr std::array<GLint, 6> values{GL_NEAREST,
                                          GL_LINEAR,
                                          GL_NEAREST_MIPMAP_NEAREST,
                                          GL_LINEAR_MIPMAP_NEAREST,
                                          GL_NEAREST_MIPMAP_LINEAR,
                                          GL_LINEAR_MIPMAP_LINEAR};
    const auto index = static_cast<std::size_t>(filter);
    return index < values.size() ? values[index] : GL_LINEAR;
}
[[nodiscard]] std::uint32_t call_limit(Phase phase) noexcept
{
    if (phase == Phase::storage || phase == Phase::pixels)
        return 24;
    if (phase == Phase::sampler)
        return 6;
    if (phase == Phase::object)
        return 3;
    return phase == Phase::validation ? 2 : 0;
}
class OpenGLCubePreparation final : public graphics::TextureCubePreparation {
  public:
    OpenGLCubePreparation(std::unique_ptr<OpenGLTextureCube> texture,
                          graphics::TextureCubeStorageDescription storage) noexcept
        : texture_(std::move(texture)), storage_(storage)
    {
    }
    [[nodiscard]] Result<graphics::CubeUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override;
    [[nodiscard]] Result<graphics::TextureUploadStep>
    advance(std::span<const std::byte> pixels) noexcept override;
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return calls_;
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept override;
    [[nodiscard]] Result<std::unique_ptr<graphics::TextureCube>> take_texture() noexcept override;

  private:
    [[nodiscard]] Result<void> execute(const graphics::TextureUploadRequest& region,
                                       std::span<const std::byte> pixels) noexcept;
    void configure_sampler() noexcept;
    void allocate_face() noexcept;
    void upload_face(const graphics::TextureUploadRequest& region,
                     std::span<const std::byte> pixels) noexcept;
    void advance_pixels(const graphics::TextureUploadRequest& region) noexcept;
    std::unique_ptr<OpenGLTextureCube> texture_;
    graphics::TextureCubeStorageDescription storage_;
    Phase phase_ = Phase::object;
    std::uint32_t parameter_ = 0;
    std::uint32_t mip_ = 0;
    std::uint32_t face_ = 0;
    std::uint32_t x_ = 0;
    std::uint32_t y_ = 0;
    std::uint64_t calls_ = 1; // Factory context validation.
};
Result<graphics::CubeUploadRequest>
OpenGLCubePreparation::request(std::size_t maximum_bytes) const noexcept
{
    if (phase_ == Phase::failed || phase_ == Phase::consumed)
        return Error{ErrorCode::invalid_argument, "Cubemap preparation has failed or was consumed"};
    if (const auto valid = texture_->validate_thread(); !valid)
        return valid.error();
    if (maximum_bytes == 0)
        return Error{ErrorCode::invalid_argument, "Cubemap upload allowance must be positive"};
    graphics::CubeUploadRequest next;
    next.mip = mip_;
    next.face = face_;
    next.region.phase = phase_;
    next.region.driver_call_limit = call_limit(phase_);
    if (phase_ == Phase::pixels) {
        const auto region = graphics::texture_upload_region(
            graphics::cube_face_storage(storage_, mip_), x_, y_, maximum_bytes);
        if (!region)
            return region.error();
        next.region = region.value();
        next.region.driver_call_limit = call_limit(phase_);
    }
    return next;
}
void OpenGLCubePreparation::configure_sampler() noexcept
{
    constexpr std::array<GLenum, 7> names{
        GL_TEXTURE_WRAP_S,     GL_TEXTURE_WRAP_T,     GL_TEXTURE_WRAP_R,   GL_TEXTURE_MIN_FILTER,
        GL_TEXTURE_MAG_FILTER, GL_TEXTURE_BASE_LEVEL, GL_TEXTURE_MAX_LEVEL};
    const std::array<GLint, 7> values{GL_CLAMP_TO_EDGE,
                                      GL_CLAMP_TO_EDGE,
                                      GL_CLAMP_TO_EDGE,
                                      filter_value(storage_.min_filter),
                                      filter_value(storage_.mag_filter),
                                      0,
                                      static_cast<GLint>(storage_.mip_count - 1)};
    TextureBinding binding{GL_TEXTURE_CUBE_MAP, texture_->texture};
    glTexParameteri(GL_TEXTURE_CUBE_MAP, names[parameter_], values[parameter_]);
    if (++parameter_ == names.size())
        phase_ = Phase::storage;
}
void OpenGLCubePreparation::allocate_face() noexcept
{
    const auto face = graphics::cube_face_storage(storage_, mip_);
    TextureBinding binding{GL_TEXTURE_CUBE_MAP, texture_->texture};
    PixelUnpack unpack;
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_, static_cast<GLint>(mip_), GL_RGBA16F,
                 static_cast<GLsizei>(face.extent.width), static_cast<GLsizei>(face.extent.height),
                 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    phase_ = Phase::pixels;
}
void OpenGLCubePreparation::advance_pixels(const graphics::TextureUploadRequest& region) noexcept
{
    const auto extent = graphics::cube_face_storage(storage_, mip_).extent;
    x_ += region.extent.width;
    if (x_ == extent.width) {
        x_ = 0;
        y_ += region.extent.height;
    }
    if (y_ != extent.height)
        return;
    y_ = 0;
    if (++face_ == 6) {
        face_ = 0;
        ++mip_;
    }
    phase_ = mip_ == storage_.mip_count ? Phase::validation : Phase::storage;
}
void OpenGLCubePreparation::upload_face(const graphics::TextureUploadRequest& region,
                                        std::span<const std::byte> pixels) noexcept
{
    TextureBinding binding{GL_TEXTURE_CUBE_MAP, texture_->texture};
    PixelUnpack unpack;
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_, static_cast<GLint>(mip_),
                    static_cast<GLint>(region.x), static_cast<GLint>(region.y),
                    static_cast<GLsizei>(region.extent.width),
                    static_cast<GLsizei>(region.extent.height), GL_RGBA, GL_HALF_FLOAT,
                    pixels.data());
    advance_pixels(region);
}
Result<void> OpenGLCubePreparation::execute(const graphics::TextureUploadRequest& region,
                                            std::span<const std::byte> pixels) noexcept
{
    switch (phase_) {
    case Phase::object:
        glGenTextures(1, &texture_->texture);
        if (texture_->texture == 0)
            return Error{ErrorCode::gpu_texture_creation_failed,
                         "OpenGL could not allocate a cubemap object"};
        phase_ = Phase::sampler;
        break;
    case Phase::sampler:
        configure_sampler();
        break;
    case Phase::storage:
        allocate_face();
        break;
    case Phase::pixels:
        upload_face(region, pixels);
        break;
    case Phase::validation:
        phase_ = Phase::ready;
        break;
    default:
        return Error{ErrorCode::invalid_argument, "Cubemap cannot execute this preparation phase"};
    }
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "OpenGL cubemap preparation step failed"};
    return {};
}
Result<graphics::TextureUploadStep>
OpenGLCubePreparation::advance(std::span<const std::byte> pixels) noexcept
{
    const auto next =
        request(phase_ == Phase::pixels ? pixels.size() : graphics::maximum_texture_upload_chunk);
    if (!next)
        return next.error();
    const auto& region = next.value().region;
    if (pixels.size() != region.bytes || pixels.size() > graphics::maximum_texture_upload_chunk)
        return Error{ErrorCode::invalid_argument,
                     "Cubemap pixels must match one bounded rectangle"};
    if (phase_ == Phase::ready)
        return graphics::TextureUploadStep{0, 0, true};
    if (const auto context = texture_->validate_operation(); !context)
        return context.error();
    if (region.driver_call_limit > std::numeric_limits<std::uint64_t>::max() - calls_)
        fatal_error("Cubemap preparation driver-call counter exhausted");
    calls_ += region.driver_call_limit;
    const auto allocated =
        region.phase == Phase::storage
            ? graphics::texture_storage_bytes(graphics::cube_face_storage(storage_, mip_)).value()
            : 0;
    if (const auto executed = execute(region, pixels); !executed) {
        phase_ = Phase::failed;
        return executed.error();
    }
    texture_->ready = phase_ == Phase::ready;
    return graphics::TextureUploadStep{pixels.size(), region.driver_call_limit, texture_->ready,
                                       allocated};
}
Result<graphics::ResourceRetirementStep>
OpenGLCubePreparation::advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (!texture_)
        return graphics::ResourceRetirementStep{0, 0, true, false};
    auto step = texture_->advance_retirement(budget);
    if (step && (step.value().progressed || step.value().complete))
        phase_ = Phase::failed;
    return step;
}
Result<std::unique_ptr<graphics::TextureCube>> OpenGLCubePreparation::take_texture() noexcept
{
    const auto next = request(graphics::maximum_texture_upload_chunk);
    if (!next)
        return next.error();
    if (phase_ != Phase::ready)
        return Error{ErrorCode::invalid_argument, "Cubemap storage is incomplete"};
    phase_ = Phase::consumed;
    return std::unique_ptr<graphics::TextureCube>{std::move(texture_)};
}
} // namespace
Result<std::unique_ptr<graphics::TextureCubePreparation>>
begin_texture_cube(std::shared_ptr<OpenGLDeviceState> state,
                   const graphics::TextureCubeStorageDescription& storage) noexcept
{
    if (const auto context = state->validate_operation(); !context)
        return context.error();
    if (const auto bytes = graphics::cube_storage_bytes(storage); !bytes)
        return bytes.error();
    if (!state->supports({storage.extent, storage.extent}))
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Cubemap dimensions exceed device limits"};
    if (storage.mag_filter != graphics::TextureFilterMode::nearest &&
        storage.mag_filter != graphics::TextureFilterMode::linear)
        return Error{ErrorCode::invalid_sampler_filter,
                     "Cubemap magnification requires nearest or linear"};
    try {
        return std::unique_ptr<graphics::TextureCubePreparation>{
            std::make_unique<OpenGLCubePreparation>(
                std::make_unique<OpenGLTextureCube>(std::move(state), storage.extent,
                                                    storage.mip_count),
                storage)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}
} // namespace elf3d::backend::opengl::device_detail
