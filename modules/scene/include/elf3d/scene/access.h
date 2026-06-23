#ifndef ELF3D_SCENE_ACCESS_H
#define ELF3D_SCENE_ACCESS_H

#include <elf3d/scene.h>

namespace elf3d::scene {

class Storage;

class Access final {
  public:
    [[nodiscard]] static Storage *storage(Scene &scene) noexcept;
    [[nodiscard]] static const Storage *storage(const Scene &scene) noexcept;
};

} // namespace elf3d::scene

#endif
