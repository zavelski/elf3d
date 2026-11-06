#pragma once

#include <elf3d/scene.h>
#include <scene/scene.h>

#include <memory>
#include <optional>

namespace elf3d {
class PreparedSceneDocument::Impl final {
  public:
    explicit Impl(SceneId id) noexcept : id{id}
    {
    }
    SceneId id;
    std::unique_ptr<scene::Storage> storage;
    SceneStatistics statistics;
    std::optional<Bounds3> bounds;
};
} // namespace elf3d
