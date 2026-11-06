#pragma once

#include "device_internal.hpp"
#include <graphics/texture_preparation.h>

namespace elf3d::backend::opengl::device_detail {
[[nodiscard]] Result<GLuint> texture_object(const graphics::Texture2D* texture) noexcept;
class OpenGLTexture2D final : public graphics::Texture2D {
  public:
    OpenGLTexture2D(std::shared_ptr<OpenGLDeviceState> state, Extent2D extent) noexcept;
    ~OpenGLTexture2D() noexcept override;
    [[nodiscard]] Extent2D extent() const noexcept override;
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override;
    [[nodiscard]] Result<void> validate_operation() const noexcept;
    [[nodiscard]] Result<void> validate_thread() const noexcept;
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept;
    GLuint texture = 0;
    bool ready = false;

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    Extent2D extent_;
};
[[nodiscard]] Result<std::unique_ptr<graphics::Texture2DPreparation>>
begin_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                 const graphics::TextureStorageDescription& description) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>>
create_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::Texture2DDescription& description) noexcept;
} // namespace elf3d::backend::opengl::device_detail
