module;

#include <elf3d/core/result.h>
#include <elf3d/viewport.h>

#include <memory>
#include <optional>

export module elf.viewport;

import elf.clipping;
import elf.graphics;
import elf.navigation;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.clipping.runtime;

export namespace elf3d::viewport {

struct ViewportPickRequest {
    EntityId camera;
    Float2 position_pixels;
    PickOptions options;
};

class OffscreenViewport final {
  private:
    struct ConstructionKey final {};
    class State final {
      public:
        class SelectionState;
        class VisibilityState;

        State();
        ~State() noexcept;

        [[nodiscard]] Result<std::optional<Bounds3>>
        visible_bounds(const scene::Storage& scene,
                       const scene::VisibilityFilter& visibility_filter,
                       const clipping::ClippingFilter& clipping_filter);
        void validate_visibility(const scene::Storage& scene) noexcept;
        [[nodiscard]] Result<scene::VisibilityFilter>
        visibility_filter(const scene::Storage& scene);
        [[nodiscard]] Result<void> isolate_entity(const scene::Storage& scene, EntityId entity);
        void clear_isolation() noexcept;
        void clear_scene_isolation(SceneId scene) noexcept;
        [[nodiscard]] bool is_isolating() const noexcept;
        [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;

        navigation::OrbitNavigationController navigation;
        std::unique_ptr<SelectionState> selection;
        std::unique_ptr<VisibilityState> visibility;
        clipping_runtime::ClippingController clipping;
        SceneId visible_bounds_scene;
        std::optional<EntityId> visible_bounds_isolated_root;
        std::optional<Bounds3> cached_visible_bounds;
        std::uint64_t visible_bounds_spatial_revision = 0;
        std::uint64_t visible_bounds_hierarchy_revision = 0;
        std::uint64_t visible_bounds_visibility_revision = 0;
        std::uint64_t visible_bounds_clipping_revision = 0;
        bool visible_bounds_valid = false;
    };
    struct Resources final {
        std::unique_ptr<graphics::RenderTarget> render_target;
        std::unique_ptr<graphics::PickingTarget> picking_target;
        std::unique_ptr<graphics::PickingTarget> focus_depth_target;
    };

  public:
    [[nodiscard]] static Result<std::unique_ptr<OffscreenViewport>> create(graphics::Device& device,
                                                                           Extent2D initial_extent);

    ~OffscreenViewport() noexcept;

    OffscreenViewport(const OffscreenViewport&) = delete;
    OffscreenViewport& operator=(const OffscreenViewport&) = delete;
    OffscreenViewport(OffscreenViewport&&) = delete;
    OffscreenViewport& operator=(OffscreenViewport&&) = delete;

    [[nodiscard]] Extent2D extent() const noexcept;
    [[nodiscard]] Result<void> resize(Extent2D extent);

    void set_clear_color(Color4 color) noexcept;
    [[nodiscard]] Color4 clear_color() const noexcept;

    void set_basic_lighting(const BasicLighting& lighting) noexcept;
    [[nodiscard]] BasicLighting basic_lighting() const noexcept;

    void set_environment_lighting(const EnvironmentLighting& lighting) noexcept;
    [[nodiscard]] EnvironmentLighting environment_lighting() const noexcept;

    void set_display_transform(const DisplayTransform& transform) noexcept;
    [[nodiscard]] DisplayTransform display_transform() const noexcept;

    void set_render_shading_mode(RenderShadingMode mode) noexcept;
    [[nodiscard]] RenderShadingMode render_shading_mode() const noexcept;
    [[nodiscard]] std::uint64_t render_revision() const noexcept;

    [[nodiscard]] Result<void> update_navigation(renderer::Renderer& renderer,
                                                 scene::Storage& scene, EntityId camera,
                                                 const NavigationInput& input);
    [[nodiscard]] Result<void> set_examine_pivot(scene::Storage& scene, EntityId camera,
                                                 Float3 world_position);
    [[nodiscard]] Result<void> fit_to_scene(scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> reset_view(scene::Storage& scene, EntityId camera);
    [[nodiscard]] Result<void> synchronize_navigation(const scene::Storage& scene, EntityId camera);
    void cancel_interaction() noexcept;

    void set_navigation_enabled(bool enabled) noexcept;
    [[nodiscard]] bool navigation_enabled() const noexcept;
    [[nodiscard]] Result<void> set_navigation_settings(const OrbitNavigationSettings& settings);
    [[nodiscard]] OrbitNavigationSettings navigation_settings() const noexcept;
    [[nodiscard]] std::optional<NavigationSnapshot> navigation_snapshot() const noexcept;

    [[nodiscard]] Result<Ray3> make_picking_ray(picking::PickingService& picking,
                                                const scene::Storage& scene, EntityId camera,
                                                Float2 position_pixels) const;
    [[nodiscard]] Result<std::optional<PickHit>> pick(renderer::Renderer& renderer,
                                                      picking::PickingService& picking,
                                                      const scene::Storage& scene,
                                                      const ViewportPickRequest& request);
    [[nodiscard]] Result<std::optional<PickHit>> select_at(renderer::Renderer& renderer,
                                                           picking::PickingService& picking,
                                                           const scene::Storage& scene,
                                                           EntityId camera, Float2 position_pixels);
    [[nodiscard]] Result<void> set_selected_entity(const scene::Storage& scene, EntityId entity);
    void clear_selection() noexcept;
    void clear_scene_selection(SceneId scene) noexcept;
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::optional<EntityId> selected_entity() const noexcept;
    [[nodiscard]] std::optional<PickHit> selection_hit() const noexcept;
    [[nodiscard]] SelectionSnapshot selection_snapshot() const noexcept;
    [[nodiscard]] PickingStatistics
    picking_statistics(const picking::PickingService& picking) const noexcept;

    [[nodiscard]] Result<ProjectedViewportPoint>
    project_world_to_viewport(const scene::Storage& scene, EntityId camera,
                              Float3 world_position) const;
    [[nodiscard]] Result<bool> surface_anchor_visible(const scene::Storage& scene,
                                                      const ResolvedSurfaceAnchor& anchor);

    [[nodiscard]] Result<void> isolate_entity(const scene::Storage& scene, EntityId entity);
    void clear_isolation() noexcept;
    void clear_scene_isolation(SceneId scene) noexcept;
    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] Result<std::optional<Bounds3>> visible_bounds(const scene::Storage& scene);
    [[nodiscard]] Result<std::optional<Bounds3>>
    unclipped_visible_bounds(const scene::Storage& scene);

    [[nodiscard]] Result<void> set_section_plane(const SectionPlane& plane);
    void clear_section_plane() noexcept;
    [[nodiscard]] Result<std::uint32_t> add_clipping_box(const ClippingBox& box);
    [[nodiscard]] Result<void> set_clipping_box(std::uint32_t index, const ClippingBox& box);
    [[nodiscard]] Result<void> remove_clipping_box(std::uint32_t index);
    void clear_clipping_boxes() noexcept;
    void clear_clipping() noexcept;
    [[nodiscard]] ClippingSnapshot clipping_snapshot() const noexcept;

    [[nodiscard]] Result<void> render(renderer::Renderer& renderer, const scene::Storage& scene,
                                      EntityId camera);
    [[nodiscard]] Result<void> render(renderer::Renderer& renderer, const scene::Storage& scene,
                                      EntityId camera, const ViewportRenderOptions& options);
    [[nodiscard]] RenderStatistics statistics() const noexcept;
    [[nodiscard]] TextureHandle color_texture() const noexcept;
    [[nodiscard]] bool framebuffer_valid() const noexcept;

    OffscreenViewport(ConstructionKey, Resources resources) noexcept;

  private:
    struct InteractionFrame {
        EntityId camera;
        scene::VisibilityFilter visibility;
        elf3d::clipping::ClippingFilter clipping_filter;
    };

    struct PickOperation {
        EntityId camera;
        Float2 position_pixels;
        PickOptions options;
        elf3d::clipping::ClippingFilter clipping_filter;
    };

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_gpu_first(renderer::Renderer& renderer, picking::PickingService& picking,
                   const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                   const PickOperation& operation);
    [[nodiscard]] Result<std::optional<Float3>>
    focus_depth_anchor(renderer::Renderer& renderer, const scene::Storage& scene, EntityId camera,
                       const scene::VisibilityFilter& visibility,
                       const elf3d::clipping::ClippingFilter& clipping_filter);
    [[nodiscard]] Result<void>
    update_orbit_screen_anchor(renderer::Renderer& renderer, scene::Storage& scene,
                               const InteractionFrame& frame,
                               std::optional<Float2> orbit_start_position);

    std::unique_ptr<graphics::RenderTarget> render_target_;
    std::unique_ptr<graphics::PickingTarget> picking_target_;
    std::unique_ptr<graphics::PickingTarget> focus_depth_target_;
    std::unique_ptr<State> state_;
    Color4 clear_color_{0.08F, 0.16F, 0.28F, 1.0F};
    BasicLighting lighting_;
    EnvironmentLighting environment_lighting_;
    DisplayTransform display_transform_;
    RenderShadingMode shading_mode_ = RenderShadingMode::standard;
    std::uint64_t render_revision_ = 1;
    RenderStatistics statistics_;
    PickingStatistics gpu_picking_statistics_;
};

} // namespace elf3d::viewport
