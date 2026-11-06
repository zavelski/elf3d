#include "device_mesh.h"
#include <utility>

namespace elf3d::backend::opengl::device_detail {
OpenGLStaticMesh::OpenGLStaticMesh(std::shared_ptr<OpenGLDeviceState> state,
                                   graphics::MeshStorageDescription description) noexcept
    : state_(std::move(state)), description_(description)
{
}
OpenGLStaticMesh::~OpenGLStaticMesh() noexcept
{
    if (!has_objects())
        return;
    if (!state_->can_destroy_objects())
        return;
    if (vertex_array != 0)
        glDeleteVertexArrays(1, &vertex_array);
    if (vertex_buffer != 0)
        glDeleteBuffers(1, &vertex_buffer);
    if (index_buffer != 0)
        glDeleteBuffers(1, &index_buffer);
}
std::uint32_t OpenGLStaticMesh::vertex_count() const noexcept
{
    return description_.vertex_count;
}
std::uint32_t OpenGLStaticMesh::index_count() const noexcept
{
    return description_.index_count;
}
graphics::VertexLayout OpenGLStaticMesh::vertex_layout() const noexcept
{
    return description_.vertex_layout;
}
std::uintptr_t OpenGLStaticMesh::backend_resource_token() const noexcept
{
    return opengl_resource_token();
}
Result<void> OpenGLStaticMesh::validate_operation() const noexcept
{
    return state_->validate_operation();
}
Result<void> OpenGLStaticMesh::validate_thread() const noexcept
{
    return state_->validate_thread();
}
bool OpenGLStaticMesh::has_objects() const noexcept
{
    return vertex_array != 0 || vertex_buffer != 0 || index_buffer != 0;
}
Result<graphics::ResourceRetirementStep>
OpenGLStaticMesh::advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (const auto thread = state_->validate_thread(); !thread)
        return thread.error();
    if (!has_objects())
        return graphics::ResourceRetirementStep{0, 0, true, false};
    if (budget.objects == 0 || budget.driver_calls < 3)
        return graphics::ResourceRetirementStep{};
    if (const auto context = state_->validate_operation(); !context)
        return context.error();
    ready = false;
    if (vertex_array != 0) {
        glDeleteVertexArrays(1, &vertex_array);
        vertex_array = 0;
    } else if (vertex_buffer != 0) {
        glDeleteBuffers(1, &vertex_buffer);
        vertex_buffer = 0;
    } else {
        glDeleteBuffers(1, &index_buffer);
        index_buffer = 0;
    }
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_buffer_creation_failed, "OpenGL mesh retirement failed"};
    return graphics::ResourceRetirementStep{1, 3, !has_objects(), true};
}
Result<MeshView> mesh_view(graphics::StaticMesh& mesh) noexcept
{
    if (mesh.backend_resource_token() != opengl_resource_token())
        return Error{ErrorCode::backend_mismatch, "The static mesh does not belong to OpenGL"};
    auto& native = static_cast<OpenGLStaticMesh&>(mesh);
    if (!native.ready)
        return Error{ErrorCode::gpu_buffer_creation_failed, "The static mesh upload is incomplete"};
    return MeshView{native.vertex_array, native.index_count(),
                    static_cast<std::uint8_t>(native.vertex_layout())};
}
} // namespace elf3d::backend::opengl::device_detail
