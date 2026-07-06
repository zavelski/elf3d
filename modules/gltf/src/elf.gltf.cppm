module;

#include <elf3d/core/result.h>
#include <elf3d/scene.h>
#include <elf3d/scene_load.h>

#include <filesystem>
#include <vector>

export module elf.gltf;

import elf.core;

export namespace elf3d::scene {
class Storage;
}

export namespace elf3d::gltf {

struct ImportReport {
    std::vector<SceneLoadDiagnostic> diagnostics;
};

[[nodiscard]] Result<ImportReport> import_scene(const std::filesystem::path &path,
                                                const SceneLoadOptions &options,
                                                scene::Storage &builder) noexcept;

} // namespace elf3d::gltf
