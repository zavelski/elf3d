#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <elf3d/core/diagnostics.h>
#include <elf3d/internal/graphics.h>
#include <memory>

namespace elf3d::backend::opengl {

using GraphicsProcedure = void (*)();
using GraphicsProcedureLoader = GraphicsProcedure (*)(const char* name) noexcept;

struct DeviceOptions {
    GraphicsProcedureLoader load_procedure = nullptr;
};

[[nodiscard]] Result<std::unique_ptr<graphics::Device>>
create_device(const DeviceOptions& options) noexcept;

} // namespace elf3d::backend::opengl
