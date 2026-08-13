#pragma once

#include <elf3d/elf3d.h>

#include <filesystem>
#include <memory>
#include <optional>

namespace elf3d::viewer {

struct SceneSession {
    std::unique_ptr<Scene> scene;
    EntityId camera;
    std::optional<EntityId> cube;
    std::optional<MaterialHandle> cube_material;
    std::filesystem::path source_path;
    SceneStatistics source_statistics;
    std::optional<Bounds3> source_bounds;
    bool camera_needs_reset = true;
    bool hierarchy_snapshot_valid = false;
    SceneHierarchySnapshot hierarchy_snapshot;
    SceneLoadReport load_report;

    [[nodiscard]] bool is_imported() const noexcept {
        return !source_path.empty();
    }
};

} // namespace elf3d::viewer
