#ifndef ELF3D_BACKEND_OPENGL_DEVICE_FACTORY_H
#define ELF3D_BACKEND_OPENGL_DEVICE_FACTORY_H

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <memory>

namespace elf3d::graphics {
class Device;
}

namespace elf3d::backend::opengl {

[[nodiscard]] Result<std::shared_ptr<graphics::Device>>
create_device(const OpenGLConfiguration &configuration) noexcept;

} // namespace elf3d::backend::opengl

#endif
