#pragma once

#include "device_internal.hpp"
#include <graphics/cube_preparation.h>

namespace elf3d::backend::opengl::device_detail {
[[nodiscard]] Result<GLuint> texture_cube_object(const graphics::TextureCube* texture) noexcept;
class OpenGLTextureCube final : public graphics::TextureCube {
  public:
    OpenGLTextureCube(std::shared_ptr<OpenGLDeviceState> state, std::uint32_t extent,
                      std::uint32_t mip_count) noexcept;
    ~OpenGLTextureCube() noexcept override;
    [[nodiscard]] std::uint32_t extent() const noexcept override;
    [[nodiscard]] std::uint32_t mip_count() const noexcept override;
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override;
    [[nodiscard]] Result<void> validate_operation() const noexcept;
    [[nodiscard]] Result<void> validate_thread() const noexcept;
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept;
    GLuint texture = 0;
    bool ready = false;

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    std::uint32_t extent_ = 0;
    std::uint32_t mip_count_ = 0;
};
[[nodiscard]] Result<std::unique_ptr<graphics::TextureCubePreparation>>
begin_texture_cube(std::shared_ptr<OpenGLDeviceState> state,
                   const graphics::TextureCubeStorageDescription& storage) noexcept;
} // namespace elf3d::backend::opengl::device_detail
