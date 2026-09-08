#ifndef ELF3D_FACADE_STUDIO_ENVIRONMENT_RESOURCE_H
#define ELF3D_FACADE_STUDIO_ENVIRONMENT_RESOURCE_H

#include <memory>

// Include after importing elf.renderer; its types belong to that named module.

namespace elf3d::detail {

[[nodiscard]] std::unique_ptr<renderer::StudioEnvironmentSource> create_studio_environment_source();

} // namespace elf3d::detail

#endif
