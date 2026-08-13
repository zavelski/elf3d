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

using GraphicsProcedure = void (*)();
using GraphicsProcedureLoader = GraphicsProcedure (*)(const char* name) noexcept;

struct DeviceOptions {
    GraphicsProcedureLoader load_procedure = nullptr;
};

[[nodiscard]] Result<std::unique_ptr<graphics::Device>>
create_device(const DeviceOptions& options) noexcept;

} // namespace elf3d::backend::opengl
