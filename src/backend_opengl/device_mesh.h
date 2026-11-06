#pragma once

#include "device_internal.hpp"
#include <graphics/mesh_preparation.h>

namespace elf3d::backend::opengl::device_detail {
class OpenGLStaticMesh final : public graphics::StaticMesh {
  public:
    OpenGLStaticMesh(std::shared_ptr<OpenGLDeviceState> state,
                     graphics::MeshStorageDescription description) noexcept;
    ~OpenGLStaticMesh() noexcept override;
    [[nodiscard]] std::uint32_t vertex_count() const noexcept override;
    [[nodiscard]] std::uint32_t index_count() const noexcept override;
    [[nodiscard]] graphics::VertexLayout vertex_layout() const noexcept override;
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override;
    [[nodiscard]] Result<void> validate_operation() const noexcept;
    [[nodiscard]] Result<void> validate_thread() const noexcept;
    [[nodiscard]] bool has_objects() const noexcept;
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept;
    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    GLuint index_buffer = 0;
    bool ready = false;

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    graphics::MeshStorageDescription description_;
};
[[nodiscard]] Result<std::unique_ptr<graphics::StaticMeshPreparation>>
begin_static_mesh(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::MeshStorageDescription& description) noexcept;
[[nodiscard]] Result<MeshView> mesh_view(graphics::StaticMesh& mesh) noexcept;
} // namespace elf3d::backend::opengl::device_detail
