#pragma once

#include <backend_opengl/backend_opengl.h>

// Native probe borrows the framework's current context; it owns no window,
// event loop or alternate context. The loader is invoked synchronously.
[[nodiscard]] elf3d::Result<void>
verify_staged_mesh_upload(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept;
[[nodiscard]] elf3d::Result<void>
verify_staged_resource_uploads(elf3d::backend::opengl::GraphicsProcedureLoader loader) noexcept;
