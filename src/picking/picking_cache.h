#pragma once

#include "acceleration_detail.h"
#include <map>
#include <utility>

namespace elf3d::picking {
// Owned BVH arrays only: no Scene, Document, geometry view or callback survives
// acquire. release detaches a complete group by moving its map node.
class MeshCache final {
  public:
    [[nodiscard]] std::uint64_t size() const noexcept;
    [[nodiscard]] Result<const acceleration_detail::MeshAcceleration*>
    acquire(SceneId scene, const scene::RuntimePrimitiveView& primitive,
            PickingStatistics& statistics);
    void release(SceneId scene) noexcept;
    [[nodiscard]] PickingRetirementStatus retirement_status() const noexcept;
    [[nodiscard]] PickingRetirementStep advance_retirement();

  private:
    struct SceneKey {
        SceneId scene;
        std::uint64_t generation = 0;
    };
    struct SceneLess {
        bool operator()(const SceneKey& a, const SceneKey& b) const noexcept;
    };
    struct Group {
        std::map<std::pair<bool, std::uint64_t>, acceleration_detail::MeshAcceleration> meshes;
        std::uint64_t bytes = 0;
    };
    std::map<SceneKey, Group, SceneLess> active_;
    std::map<SceneKey, Group, SceneLess> retired_;
    std::uint64_t mesh_count_ = 0;
    std::uint64_t retirement_sequence_ = 0;
    PickingRetirementStatus retired_status_;
};
} // namespace elf3d::picking
