module;

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/picking.h>
#include <elf3d/selection.h>

#include <optional>

export module elf.tool.selection;

import elf.clipping;
import elf.core;
import elf.picking;
import elf.scene;

export namespace elf3d::tools::selection {

struct SelectionTarget {
    EntityId camera;
    Extent2D extent;
    Float2 position_pixels;
};

class SelectionController final {
  public:
    [[nodiscard]] Result<std::optional<PickHit>> select_at(picking::PickingService &picking,
                                                           const scene::Storage &scene,
                                                           SelectionTarget target);
    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(picking::PickingService &picking, const scene::Storage &scene, SelectionTarget target,
              const scene::VisibilityFilter &visibility);
    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(picking::PickingService &picking, const scene::Storage &scene, SelectionTarget target,
              const scene::VisibilityFilter &visibility,
              const clipping::ClippingFilter &clipping_filter);
    [[nodiscard]] Result<std::optional<PickHit>>
    select_hit(const scene::Storage &scene, const std::optional<PickHit> &hit);
    [[nodiscard]] Result<void> set_selected_entity(const scene::Storage &scene, EntityId entity);

    void clear() noexcept;
    void clear_scene(SceneId scene) noexcept;
    void validate_against(const scene::Storage &scene) noexcept;

    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::optional<EntityId> selected_entity() const noexcept;
    [[nodiscard]] std::optional<PickHit> selection_hit() const noexcept;
    [[nodiscard]] SelectionSnapshot snapshot() const noexcept;

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

    [[nodiscard]] Result<void> set_settings(const SelectionSettings &settings) noexcept;
    [[nodiscard]] SelectionSettings settings() const noexcept;

  private:
    bool enabled_ = true;
    SelectionSettings settings_;
    SceneId selected_scene_;
    std::optional<EntityId> entity_;
    std::optional<PickHit> hit_;
};

} // namespace elf3d::tools::selection
