module;

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <memory>

export module elf.backend.opengl;

import elf.core;
import elf.graphics;

export namespace elf3d::graphics {
class Device;
}

export namespace elf3d::backend::opengl {

[[nodiscard]] Result<std::unique_ptr<graphics::Device>>
create_device(const OpenGLConfiguration& configuration) noexcept;

} // namespace elf3d::backend::opengl
