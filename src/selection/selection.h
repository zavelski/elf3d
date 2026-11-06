#pragma once

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/picking.h>
#include <elf3d/selection.h>

#include <clipping/clipping.h>
#include <cstdint>
#include <elf3d/core/diagnostics.h>
#include <optional>
#include <picking/picking.h>
#include <scene/scene.h>
#include <span>
#include <vector>

namespace elf3d::selection {

struct SelectionTarget {
    EntityId camera;
    Extent2D extent;
    Float2 position_pixels;
};

class SelectionController final {
  public:
    [[nodiscard]] Result<std::optional<PickHit>> select_at(picking::PickingService& picking,
                                                           const scene::Storage& scene,
                                                           SelectionTarget target);
    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(picking::PickingService& picking, const scene::Storage& scene, SelectionTarget target,
              const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(picking::PickingService& picking, const scene::Storage& scene, SelectionTarget target,
              const scene::VisibilityFilter& visibility,
              const clipping::ClippingFilter& clipping_filter);
    [[nodiscard]] Result<std::optional<PickHit>> select_hit(const scene::Storage& scene,
                                                            const std::optional<PickHit>& hit);
    [[nodiscard]] Result<void> set_selected_entity(const scene::Storage& scene, EntityId entity);
    [[nodiscard]] Result<void>
    set_selected_primitives(const scene::Storage& scene,
                            std::span<const PrimitiveInstance> primitives);
    [[nodiscard]] std::span<const PrimitiveInstance> selected_primitives() const noexcept;

    void clear() noexcept;
    void clear_scene(SceneId scene) noexcept;
    void validate_against(const scene::Storage& scene) noexcept;

    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::optional<EntityId> selected_entity() const noexcept;
    [[nodiscard]] std::optional<PickHit> selection_hit() const noexcept;
    [[nodiscard]] SelectionSnapshot snapshot() const noexcept;

  private:
    void validate_primitives(const scene::Storage& scene) noexcept;
    SceneId selected_scene_;
    std::optional<EntityId> entity_;
    std::optional<PickHit> hit_;
    std::vector<PrimitiveInstance> primitives_;
    std::uint64_t primitive_revision_ = 0;
    std::uint64_t validated_hierarchy_revision_ = 0;
};

} // namespace elf3d::selection
