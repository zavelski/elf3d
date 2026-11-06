#pragma once

#include <elf3d/model.h>
#include <elf3d/scene_load.h>

#include <vector>

namespace elf3d {
class SceneLoadReport::Impl final {
  public:
    std::vector<ModelLoadDiagnostic> diagnostics;
};
} // namespace elf3d
