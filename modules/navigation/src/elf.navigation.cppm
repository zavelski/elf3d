module;

#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/navigation.h>
#include <elf3d/scene.h>

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

struct NavigationUpdateRequest {
    EntityId camera;
    Extent2D extent;
    ViewportInput input;
    float click_drag_threshold_pixels = 0.0F;
};

class OrbitNavigationController final {
  public:
    [[nodiscard]] Result<NavigationUpdate> update(scene::Storage& scene, EntityId camera,
                                                  Extent2D extent, const ViewportInput& input,
                                                  float click_drag_threshold_pixels);
    [[nodiscard]] Result<NavigationUpdate> update(scene::Storage& scene,
                                                  const NavigationUpdateRequest& request,
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
    struct ScreenAnchorOrbit {
        Float4x4 camera_world;
        float anchor_distance = 0.0F;
        bool changed = false;
    };

    struct FitPreparation {
        Float4x4 old_matrix;
        PerspectiveCameraDescription old_description;
        Bounds3 bounds;
        Float3 direction;
        float aspect = 1.0F;
    };

    struct UpdateFrame {
        NavigationUpdate result;
        interaction::ViewportInteractionFrame interaction;
        std::optional<Bounds3> visible_bounds;
        Float2 pointer_delta;
        float keyboard_forward = 0.0F;
        float keyboard_view_pan = 0.0F;
        float keyboard_world_vertical_pan = 0.0F;
        bool has_hover_wheel = false;
        bool stop = false;
        bool changed = false;
    };

    [[nodiscard]] UpdateFrame make_update_frame(const NavigationUpdateRequest& request);
    void update_orbit_activation(const NavigationUpdateRequest& request, UpdateFrame& frame,
                                 bool keyboard_translation_active) noexcept;
    [[nodiscard]] Result<bool> apply_dolly(scene::Storage& scene, EntityId camera, float multiplier,
                                           std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> apply_wheel_navigation(scene::Storage& scene,
                                                      const NavigationUpdateRequest& request,
                                                      UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_pointer_navigation(scene::Storage& scene,
                                                        const NavigationUpdateRequest& request,
                                                        UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_pointer_orbit(scene::Storage& scene,
                                                   const NavigationUpdateRequest& request,
                                                   UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_pointer_pan(scene::Storage& scene,
                                                 const NavigationUpdateRequest& request,
                                                 UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_pointer_zoom(scene::Storage& scene,
                                                  const NavigationUpdateRequest& request,
                                                  UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_keyboard_forward(scene::Storage& scene,
                                                      const NavigationUpdateRequest& request,
                                                      UpdateFrame& frame);
    [[nodiscard]] Result<void> apply_keyboard_pan(scene::Storage& scene,
                                                  const NavigationUpdateRequest& request,
                                                  UpdateFrame& frame);
    [[nodiscard]] Result<void> commit_update(scene::Storage& scene, EntityId camera, bool changed);
    [[nodiscard]] Result<void> ensure_synchronized(const scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> apply_screen_anchor_dolly(scene::Storage& scene, EntityId camera,
                                                         float multiplier,
                                                         std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> apply_screen_anchor_orbit(scene::Storage& scene, EntityId camera,
                                                         Float2 delta,
                                                         std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> finish_screen_anchor_dolly(scene::Storage& scene, EntityId camera,
                                                          std::optional<Bounds3> bounds);
    [[nodiscard]] Result<ScreenAnchorOrbit> screen_anchor_orbit(const scene::Storage& scene,
                                                                EntityId camera, Float2 delta);
    [[nodiscard]] Result<void> commit_screen_anchor_orbit(scene::Storage& scene, EntityId camera,
                                                          const ScreenAnchorOrbit& orbit,
                                                          std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> apply_eye_orbit(scene::Storage& scene, EntityId camera, Float2 delta,
                                               std::optional<Bounds3> bounds);
    [[nodiscard]] Result<void> synchronize_from_camera(const scene::Storage& scene, EntityId camera,
                                                       bool preserve_existing_pivot);
    [[nodiscard]] Result<void> apply_camera(scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> fit_with_direction(scene::Storage& scene, EntityId camera,
                                                  Extent2D extent, Float3 direction,
                                                  std::optional<Bounds3> bounds);
    [[nodiscard]] Result<FitPreparation> prepare_fit(const scene::Storage& scene, EntityId camera,
                                                     Extent2D extent, Float3 direction,
                                                     std::optional<Bounds3> bounds) const;

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
