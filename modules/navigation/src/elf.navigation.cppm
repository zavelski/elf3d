module;

#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/navigation.h>

#include <cstdint>
#include <optional>

export module elf.navigation;

import elf.core;
import elf.interaction;
import elf.scene;

export namespace elf3d::navigation {

struct NavigationUpdate {
    std::optional<Float2> click_position_pixels;
    std::optional<Float2> orbit_start_position_pixels;
};

class OrbitNavigationController final {
  public:
    [[nodiscard]] Result<NavigationUpdate> update(scene::Storage& scene, EntityId camera,
                                                  Extent2D extent, const ViewportInput& input,
                                                  float click_drag_threshold_pixels);
    [[nodiscard]] Result<NavigationUpdate> update(scene::Storage& scene, EntityId camera,
                                                  Extent2D extent, const ViewportInput& input,
                                                  float click_drag_threshold_pixels,
                                                  const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<void> set_screen_anchor(scene::Storage& scene, EntityId camera,
                                                 Float3 world_position);
    [[nodiscard]] Result<void> fit_to_scene(scene::Storage& scene, EntityId camera,
                                            Extent2D extent);
    [[nodiscard]] Result<void> fit_to_scene(scene::Storage& scene, EntityId camera, Extent2D extent,
                                            const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<void> reset_view(scene::Storage& scene, EntityId camera, Extent2D extent);
    [[nodiscard]] Result<void> reset_view(scene::Storage& scene, EntityId camera, Extent2D extent,
                                          const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<void> fit_to_bounds(scene::Storage& scene, EntityId camera,
                                             Extent2D extent, Bounds3 bounds);
    [[nodiscard]] Result<void> reset_to_bounds(scene::Storage& scene, EntityId camera,
                                               Extent2D extent, Bounds3 bounds);
    [[nodiscard]] Result<void> synchronize(const scene::Storage& scene, EntityId camera);

    void cancel_interaction() noexcept;
    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] bool has_screen_anchor() const noexcept;

    [[nodiscard]] Result<void> set_settings(const OrbitNavigationSettings& settings) noexcept;
    [[nodiscard]] OrbitNavigationSettings settings() const noexcept;
    [[nodiscard]] bool has_state() const noexcept;
    [[nodiscard]] NavigationSnapshot snapshot() const noexcept;

  private:
    [[nodiscard]] Result<void> ensure_synchronized(const scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> apply_screen_anchor_dolly(scene::Storage& scene, EntityId camera,
                                                         float multiplier,
                                                         std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> apply_screen_anchor_orbit(scene::Storage& scene, EntityId camera,
                                                         Float2 delta,
                                                         std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> apply_eye_orbit(scene::Storage& scene, EntityId camera,
                                               Float2 delta, std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> synchronize_from_camera(const scene::Storage& scene, EntityId camera,
                                                       bool preserve_existing_pivot);
    [[nodiscard]] Result<void> apply_camera(scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> fit_with_direction(scene::Storage& scene, EntityId camera,
                                                  Extent2D extent, Float3 direction,
                                                  std::optional<Bounds3> bounds);

    interaction::ViewportInteractionState interaction_;
    OrbitNavigationSettings settings_;
    bool enabled_ = true;
    bool has_valid_state_ = false;
    bool keyboard_navigation_used_ = false;
    bool eye_orbit_active_ = false;
    SceneId scene_;
    EntityId camera_;
    Float4x4 camera_world_;
    Float3 pivot_;
    float distance_ = 1.0F;
    float yaw_radians_ = 3.14159265F;
    float pitch_radians_ = 0.0F;
    std::optional<Float3> screen_anchor_;
};

} // namespace elf3d::navigation
