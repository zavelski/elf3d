#pragma once

#include <backend_opengl/backend_opengl.h>

[[nodiscard]] elf3d::Result<void>
verify_staged_texture_upload(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept;
