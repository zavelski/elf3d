#pragma once

#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <cstddef>
#include <elf3d/core/diagnostics.h>
#include <optional>
#include <scene/scene.h>
#include <span>
#include <vector>

namespace elf3d::visibility {

class VisibilityController final {
  public:
    [[nodiscard]] Result<void> isolate_entity(const scene::Storage& scene, EntityId entity);
    [[nodiscard]] Result<void> isolate_entities(const scene::Storage& scene,
                                                std::span<const EntityId> entities);
    [[nodiscard]] Result<void> isolate_primitives(const scene::Storage& scene,
                                                  std::span<const PrimitiveInstance> primitives);
    [[nodiscard]] Result<bool>
    set_primitive_visibility(const scene::Storage& scene,
                             std::span<const PrimitiveInstance> primitives, bool visible);
    [[nodiscard]] Result<bool> primitive_local_visibility(const scene::Storage& scene,
                                                          PrimitiveInstance primitive) const;
    [[nodiscard]] Result<bool> primitive_effective_visibility(const scene::Storage& scene,
                                                              PrimitiveInstance primitive);
    [[nodiscard]] bool clear_primitive_visibility() noexcept;
    [[nodiscard]] std::size_t isolated_primitive_count() const noexcept;
    void clear_isolation() noexcept;
    void clear_scene(SceneId scene) noexcept;
    void validate_against(const scene::Storage& scene) noexcept;

    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] std::size_t isolated_entity_count() const noexcept;
    [[nodiscard]] Result<scene::VisibilityFilter> filter_for(const scene::Storage& scene);

  private:
    void validate_primitive_state(const scene::Storage& scene) noexcept;
    SceneId isolated_scene_;
    std::vector<EntityId> isolated_roots_;
    std::vector<PrimitiveInstance> isolated_primitives_;
    SceneId hidden_scene_;
    std::vector<PrimitiveInstance> hidden_primitives_;
    SceneId validated_scene_;
    std::uint64_t validated_hierarchy_revision_ = 0;
};

} // namespace elf3d::visibility
