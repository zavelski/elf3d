module;

#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <optional>

export module elf.tool.visibility;

import elf.core;
import elf.scene;

export namespace elf3d::tools::visibility {

class VisibilityController final {
  public:
    [[nodiscard]] Result<void> isolate_entity(const scene::Storage &scene, EntityId entity);
    void clear_isolation() noexcept;
    void clear_scene(SceneId scene) noexcept;
    void validate_against(const scene::Storage &scene) noexcept;

    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] Result<scene::VisibilityFilter> filter_for(const scene::Storage &scene);

  private:
    SceneId isolated_scene_;
    std::optional<EntityId> isolated_entity_;
};

} // namespace elf3d::tools::visibility
