#include "device_cube.h"

#include <utility>

namespace elf3d::backend::opengl::device_detail {
OpenGLTextureCube::OpenGLTextureCube(std::shared_ptr<OpenGLDeviceState> state, std::uint32_t extent,
                                     std::uint32_t mip_count) noexcept
    : state_(std::move(state)), extent_(extent), mip_count_(mip_count)
{
}
OpenGLTextureCube::~OpenGLTextureCube() noexcept
{
    if (texture != 0 && state_->can_destroy_objects())
        glDeleteTextures(1, &texture);
}
std::uint32_t OpenGLTextureCube::extent() const noexcept
{
    return extent_;
}
std::uint32_t OpenGLTextureCube::mip_count() const noexcept
{
    return mip_count_;
}
std::uintptr_t OpenGLTextureCube::backend_resource_token() const noexcept
{
    return opengl_resource_token();
}
Result<void> OpenGLTextureCube::validate_operation() const noexcept
{
    return state_->validate_operation();
}
Result<void> OpenGLTextureCube::validate_thread() const noexcept
{
    return state_->validate_thread();
}
Result<graphics::ResourceRetirementStep>
OpenGLTextureCube::advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept
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
        return Error{ErrorCode::gpu_texture_upload_failed, "OpenGL cubemap retirement failed"};
    return graphics::ResourceRetirementStep{1, 3, true, true};
}
Result<std::unique_ptr<graphics::TextureCube>>
create_texture_cube(std::shared_ptr<OpenGLDeviceState> state,
                    const graphics::TextureCubeDescription& description) noexcept
{
    const auto storage = graphics::describe_cube_storage(description);
    if (!storage)
        return storage.error();
    auto preparation = begin_texture_cube(std::move(state), storage.value());
    if (!preparation)
        return preparation.error();
    return graphics::complete_cube_upload(*preparation.value(), description);
}
Result<GLuint> texture_cube_object(const graphics::TextureCube* texture) noexcept
{
    if (!texture)
        return GLuint{0};
    if (texture->backend_resource_token() != opengl_resource_token())
        return Error{ErrorCode::backend_mismatch,
                     "The environment cubemap does not belong to OpenGL"};
    const auto& native = static_cast<const OpenGLTextureCube&>(*texture);
    if (!native.ready)
        return Error{ErrorCode::gpu_texture_upload_failed,
                     "Cubemap storage is not ready for drawing"};
    return native.texture;
}
} // namespace elf3d::backend::opengl::device_detail
