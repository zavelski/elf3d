#include "device_texture.h"
#include "device_texture_state.h"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <utility>

namespace elf3d::backend::opengl::device_detail {
namespace {
using Phase = graphics::TexturePreparationPhase;
[[nodiscard]] GLint wrap_value(graphics::TextureAddressMode mode) noexcept
{
    switch (mode) {
    case graphics::TextureAddressMode::repeat:
        return GL_REPEAT;
    case graphics::TextureAddressMode::mirrored_repeat:
        return GL_MIRRORED_REPEAT;
    case graphics::TextureAddressMode::clamp_to_edge:
        return GL_CLAMP_TO_EDGE;
    }
    return GL_REPEAT;
}
[[nodiscard]] GLint filter_value(graphics::TextureFilterMode mode) noexcept
{
    switch (mode) {
    case graphics::TextureFilterMode::nearest:
        return GL_NEAREST;
    case graphics::TextureFilterMode::linear:
        return GL_LINEAR;
    case graphics::TextureFilterMode::nearest_mipmap_nearest:
        return GL_NEAREST_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::linear_mipmap_nearest:
        return GL_LINEAR_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::nearest_mipmap_linear:
        return GL_NEAREST_MIPMAP_LINEAR;
    case graphics::TextureFilterMode::linear_mipmap_linear:
        return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}
[[nodiscard]] GLint internal_format(graphics::TextureFormat format) noexcept
{
    switch (format) {
    case graphics::TextureFormat::rgba8_unorm:
        return GL_RGBA8;
    case graphics::TextureFormat::rgba8_srgb:
        return GL_SRGB8_ALPHA8;
    case graphics::TextureFormat::rgba16_float:
        return GL_RGBA16F;
    }
    return GL_RGBA8;
}
[[nodiscard]] std::uint32_t call_limit(Phase phase) noexcept
{
    if (phase == Phase::storage || phase == Phase::pixels)
        return 24; // context/error 2 + texture binding 3 + unpack state 18 + operation 1
    if (phase == Phase::sampler || phase == Phase::mipmaps)
        return 6;
    if (phase == Phase::object)
        return 3;
    return phase == Phase::validation ? 2 : 0;
}
class OpenGLTexturePreparation final : public graphics::Texture2DPreparation {
  public:
    OpenGLTexturePreparation(std::unique_ptr<OpenGLTexture2D> texture,
                             graphics::TextureStorageDescription storage,
                             std::size_t bytes) noexcept
        : texture_(std::move(texture)), storage_(storage), bytes_(bytes)
    {
    }
    [[nodiscard]] Result<graphics::TextureUploadRequest>
    request(std::size_t maximum_bytes) const noexcept override;
    [[nodiscard]] Result<graphics::TextureUploadStep>
    advance(std::span<const std::byte> payload) noexcept override;
    [[nodiscard]] Result<graphics::TextureUploadStep>
    advance_setup(std::uint32_t maximum_calls) noexcept override;
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return calls_;
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept override
    {
        if (!texture_)
            return graphics::ResourceRetirementStep{0, 0, true, false};
        auto step = texture_->advance_retirement(budget);
        if (step && (step.value().progressed || step.value().complete))
            phase_ = Phase::failed;
        return step;
    }
    [[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>> take_texture() noexcept override;

  private:
    [[nodiscard]] Phase after_pixels() const noexcept;
    [[nodiscard]] Result<void> execute(const graphics::TextureUploadRequest& request,
                                       std::span<const std::byte> payload) noexcept;
    [[nodiscard]] Result<graphics::TextureUploadRequest>
    payload_request(std::span<const std::byte> payload) const noexcept;
    void sampler_parameter() noexcept;
    void sampler_value() noexcept;
    void image_storage() noexcept;
    void upload(const graphics::TextureUploadRequest& request,
                std::span<const std::byte> payload) noexcept;
    void advance_pixels(const graphics::TextureUploadRequest& request) noexcept;
    std::unique_ptr<OpenGLTexture2D> texture_;
    graphics::TextureStorageDescription storage_;
    std::size_t bytes_ = 0;
    Phase phase_ = Phase::object;
    std::uint32_t parameter_ = 0;
    std::uint32_t x_ = 0;
    std::uint32_t y_ = 0;
    std::uint64_t calls_ = 1; // Factory's successful context validation.
};
Result<graphics::TextureUploadRequest>
OpenGLTexturePreparation::request(std::size_t maximum_bytes) const noexcept
{
    if (phase_ == Phase::failed || phase_ == Phase::consumed)
        return Error{ErrorCode::invalid_argument, "Texture preparation has failed or was consumed"};
    if (const auto valid = texture_->validate_thread(); !valid)
        return valid.error();
    if (maximum_bytes == 0)
        return Error{ErrorCode::invalid_argument, "Texture upload allowance must be positive"};
    graphics::TextureUploadRequest next;
    next.phase = phase_;
    next.driver_call_limit = call_limit(phase_);
    if (phase_ != Phase::pixels)
        return next;
    auto region = graphics::texture_upload_region(storage_, x_, y_, maximum_bytes);
    if (region)
        region.value().driver_call_limit = next.driver_call_limit;
    return region;
}
Phase OpenGLTexturePreparation::after_pixels() const noexcept
{
    const bool filtered = storage_.min_filter != graphics::TextureFilterMode::nearest &&
                          storage_.min_filter != graphics::TextureFilterMode::linear;
    return filtered && storage_.initialization == graphics::TextureInitialization::pixels
               ? Phase::mipmaps
               : Phase::validation;
}
void OpenGLTexturePreparation::sampler_value() noexcept
{
    constexpr std::array<GLenum, 6> names{GL_TEXTURE_WRAP_S,     GL_TEXTURE_WRAP_T,
                                          GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER,
                                          GL_TEXTURE_BASE_LEVEL, GL_TEXTURE_MAX_LEVEL};
    constexpr std::uint32_t maximum = 31;
    const std::array<GLint, 6> values{wrap_value(storage_.wrap_u),
                                      wrap_value(storage_.wrap_v),
                                      filter_value(storage_.min_filter),
                                      filter_value(storage_.mag_filter),
                                      0,
                                      static_cast<GLint>(maximum)}; // <= 31
    glTexParameteri(GL_TEXTURE_2D, names[parameter_], values[parameter_]);
    if (++parameter_ == names.size())
        phase_ = Phase::storage;
}
void OpenGLTexturePreparation::sampler_parameter() noexcept
{
    TextureBinding binding{GL_TEXTURE_2D, texture_->texture};
    sampler_value();
}
Result<graphics::TextureUploadStep>
OpenGLTexturePreparation::advance_setup(std::uint32_t maximum_calls) noexcept
{
    if (phase_ != Phase::sampler)
        return graphics::TextureUploadStep{};
    const auto calls = static_cast<std::uint32_t>(6 - parameter_ + 5); // <= 11
    if (maximum_calls < calls)
        return graphics::TextureUploadStep{};
    if (const auto context = texture_->validate_operation(); !context)
        return context.error();
    TextureBinding binding{GL_TEXTURE_2D, texture_->texture};
    while (phase_ == Phase::sampler)
        sampler_value();
    if (calls > std::numeric_limits<std::uint64_t>::max() - calls_)
        fatal_error("Texture preparation driver-call counter exhausted");
    calls_ += calls;
    if (glGetError() != GL_NO_ERROR) {
        phase_ = Phase::failed;
        return Error{ErrorCode::gpu_texture_upload_failed, "OpenGL sampler group failed"};
    }
    return graphics::TextureUploadStep{0, calls};
}
void OpenGLTexturePreparation::image_storage() noexcept
{
    TextureBinding binding{GL_TEXTURE_2D, texture_->texture};
    PixelUnpack unpack;
    const GLenum type =
        storage_.format == graphics::TextureFormat::rgba16_float ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
    // Extents were checked against GLsizei and the cached device size limit.
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format(storage_.format),
                 static_cast<GLsizei>(storage_.extent.width),
                 static_cast<GLsizei>(storage_.extent.height), 0, GL_RGBA, type, nullptr);
    phase_ = storage_.initialization == graphics::TextureInitialization::pixels ? Phase::pixels
                                                                                : after_pixels();
}
void OpenGLTexturePreparation::advance_pixels(
    const graphics::TextureUploadRequest& request) noexcept
{
    x_ += request.extent.width;
    if (x_ == storage_.extent.width) {
        x_ = 0;
        y_ += request.extent.height;
    }
    if (y_ == storage_.extent.height)
        phase_ = after_pixels();
}
void OpenGLTexturePreparation::upload(const graphics::TextureUploadRequest& request,
                                      std::span<const std::byte> payload) noexcept
{
    TextureBinding binding{GL_TEXTURE_2D, texture_->texture};
    PixelUnpack unpack;
    const GLenum type =
        storage_.format == graphics::TextureFormat::rgba16_float ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
    glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(request.x), static_cast<GLint>(request.y),
                    static_cast<GLsizei>(request.extent.width),
                    static_cast<GLsizei>(request.extent.height), GL_RGBA, type,
                    payload.data()); // Every range is within the checked GLsizei extent.
    advance_pixels(request);
}
Result<graphics::TextureUploadRequest>
OpenGLTexturePreparation::payload_request(std::span<const std::byte> payload) const noexcept
{
    auto next =
        request(phase_ == Phase::pixels ? payload.size() : graphics::maximum_texture_upload_chunk);
    if (!next)
        return next.error();
    if (payload.size() != next.value().bytes ||
        payload.size() > graphics::maximum_texture_upload_chunk)
        return Error{ErrorCode::invalid_argument,
                     "Texture payload must match one bounded pixel rectangle"};
    return next;
}
Result<void> OpenGLTexturePreparation::execute(const graphics::TextureUploadRequest& request,
                                               std::span<const std::byte> payload) noexcept
{
    switch (phase_) {
    case Phase::object:
        glGenTextures(1, &texture_->texture);
        if (texture_->texture == 0)
            return Error{ErrorCode::gpu_texture_creation_failed,
                         "OpenGL could not allocate a texture object"};
        phase_ = Phase::sampler;
        break;
    case Phase::sampler:
        sampler_parameter();
        break;
    case Phase::storage:
        image_storage();
        break;
    case Phase::pixels:
        upload(request, payload);
        break;
    case Phase::mipmaps: {
        TextureBinding binding{GL_TEXTURE_2D, texture_->texture};
        glGenerateMipmap(GL_TEXTURE_2D);
        phase_ = Phase::validation;
        break;
    }
    case Phase::validation:
        phase_ = Phase::ready;
        break;
    default:
        return Error{ErrorCode::invalid_argument, "Texture preparation cannot execute this phase"};
    }
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "OpenGL texture preparation step failed"};
    return {};
}
Result<graphics::TextureUploadStep>
OpenGLTexturePreparation::advance(std::span<const std::byte> payload) noexcept
{
    const auto next = payload_request(payload);
    if (!next)
        return next.error();
    if (phase_ == Phase::ready)
        return graphics::TextureUploadStep{0, 0, true};
    if (const auto context = texture_->validate_operation(); !context)
        return context.error();
    const auto limit = next.value().driver_call_limit;
    if (limit > std::numeric_limits<std::uint64_t>::max() - calls_)
        fatal_error("Texture preparation driver-call counter exhausted");
    calls_ += limit;
    if (const auto executed = execute(next.value(), payload); !executed) {
        phase_ = Phase::failed;
        return executed.error();
    }
    texture_->ready = phase_ == Phase::ready;
    const auto allocated = next.value().phase == Phase::storage ? bytes_ : 0;
    return graphics::TextureUploadStep{payload.size(), limit, texture_->ready, allocated};
}
Result<std::unique_ptr<graphics::Texture2D>> OpenGLTexturePreparation::take_texture() noexcept
{
    const auto next = request(graphics::maximum_texture_upload_chunk);
    if (!next)
        return next.error();
    if (phase_ != Phase::ready)
        return Error{ErrorCode::invalid_argument, "Texture storage is incomplete"};
    phase_ = Phase::consumed;
    return std::unique_ptr<graphics::Texture2D>{std::move(texture_)};
}
[[nodiscard]] Result<std::size_t>
validate_storage(const graphics::TextureStorageDescription& description,
                 const OpenGLDeviceState& state) noexcept
{
    const auto bytes = graphics::texture_storage_bytes(description);
    if (!bytes)
        return bytes.error();
    if (!state.supports(description.extent) ||
        description.extent.width >
            static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) ||
        description.extent.height > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()))
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "Texture dimensions exceed OpenGL limits"};
    if (description.mag_filter != graphics::TextureFilterMode::nearest &&
        description.mag_filter != graphics::TextureFilterMode::linear)
        return Error{ErrorCode::invalid_sampler_filter,
                     "Texture magnification supports nearest or linear only"};
    if (description.initialization == graphics::TextureInitialization::empty &&
        description.format != graphics::TextureFormat::rgba16_float)
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "Empty texture allocation requires RGBA16F"};
    return bytes.value();
}
} // namespace
Result<std::unique_ptr<graphics::Texture2DPreparation>>
begin_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                 const graphics::TextureStorageDescription& description) noexcept
{
    if (const auto context = state->validate_operation(); !context)
        return context.error();
    const auto bytes = validate_storage(description, *state);
    if (!bytes)
        return bytes.error();
    try {
        return std::unique_ptr<graphics::Texture2DPreparation>{
            std::make_unique<OpenGLTexturePreparation>(
                std::make_unique<OpenGLTexture2D>(std::move(state), description.extent),
                description, bytes.value())};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}
} // namespace elf3d::backend::opengl::device_detail
