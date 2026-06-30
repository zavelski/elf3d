module;

#include <elf3d/core/result.h>
#include <elf3d/scene.h>
#include <elf3d/scene_load.h>

#include <filesystem>
#include <vector>

export module elf.gltf;

export namespace elf3d::scene {
class ImportBuilder;
}

export namespace elf3d::gltf {

struct ImportReport {
    std::vector<SceneLoadDiagnostic> diagnostics;
};

[[nodiscard]] Result<ImportReport> import_scene(const std::filesystem::path &path,
                                                const SceneLoadOptions &options,
                                                scene::ImportBuilder &builder) noexcept;

} // namespace elf3d::gltf
