#include "device_texture.h"

#include <utility>

namespace elf3d::backend::opengl::device_detail {
OpenGLTexture2D::OpenGLTexture2D(std::shared_ptr<OpenGLDeviceState> state, Extent2D extent) noexcept
    : state_(std::move(state)), extent_(extent)
{
}
OpenGLTexture2D::~OpenGLTexture2D() noexcept
{
    if (texture != 0 && state_->can_destroy_objects())
        glDeleteTextures(1, &texture);
}
Extent2D OpenGLTexture2D::extent() const noexcept
{
    return extent_;
}
std::uintptr_t OpenGLTexture2D::backend_resource_token() const noexcept
{
    return opengl_resource_token();
}
Result<void> OpenGLTexture2D::validate_operation() const noexcept
{
    return state_->validate_operation();
}
Result<void> OpenGLTexture2D::validate_thread() const noexcept
{
    return state_->validate_thread();
}
Result<graphics::ResourceRetirementStep>
OpenGLTexture2D::advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (const auto thread = state_->validate_thread(); !thread)
        return thread.error();
    if (texture == 0)
        return graphics::ResourceRetirementStep{0, 0, true, false};
    if (budget.objects == 0 || budget.driver_calls < 3)
        return graphics::ResourceRetirementStep{};
    if (const auto context = state_->validate_operation(); !context)
        return context.error();
    ready = false;
    glDeleteTextures(1, &texture);
    texture = 0;
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_texture_upload_failed, "OpenGL texture retirement failed"};
    return graphics::ResourceRetirementStep{1, 3, true, true};
}
Result<std::unique_ptr<graphics::Texture2D>>
create_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::Texture2DDescription& description) noexcept
{
    const auto storage = graphics::describe_texture_storage(description);
    if (!storage)
        return storage.error();
    auto prepared = begin_texture_2d(std::move(state), storage.value());
    if (!prepared)
        return prepared.error();
    return graphics::complete_texture_upload(*prepared.value(), description.pixels);
}
Result<GLuint> texture_object(const graphics::Texture2D* texture) noexcept
{
    if (!texture)
        return GLuint{0};
    if (texture->backend_resource_token() != opengl_resource_token())
        return Error{ErrorCode::backend_mismatch, "The material texture does not belong to OpenGL"};
    const auto& native = static_cast<const OpenGLTexture2D&>(*texture);
    if (!native.ready)
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "Texture storage is not ready for drawing"};
    return native.texture;
}
} // namespace elf3d::backend::opengl::device_detail
