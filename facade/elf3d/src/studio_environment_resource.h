#ifndef ELF3D_FACADE_STUDIO_ENVIRONMENT_RESOURCE_H
#define ELF3D_FACADE_STUDIO_ENVIRONMENT_RESOURCE_H

#include <memory>

namespace elf3d::renderer {
class StudioEnvironmentSource;
}

namespace elf3d::detail {

[[nodiscard]] std::unique_ptr<renderer::StudioEnvironmentSource> create_studio_environment_source();

} // namespace elf3d::detail

#endif
