#ifndef ELF3D_GLTF_IMPORTER_H
#define ELF3D_GLTF_IMPORTER_H

#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <filesystem>
#include <string>
#include <vector>

namespace elf3d::scene {
class ImportBuilder;
}

namespace elf3d::gltf {

struct ImportReport {
    std::vector<std::string> warnings;
};

[[nodiscard]] Result<ImportReport> import_scene(const std::filesystem::path &path,
                                                const SceneLoadOptions &options,
                                                scene::ImportBuilder &builder) noexcept;

} // namespace elf3d::gltf

#endif
