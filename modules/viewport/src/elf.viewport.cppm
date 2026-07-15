module;

#include <elf3d/core/result.h>
#include <elf3d/viewport.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

export module elf.viewport;

import elf.clipping;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;

export namespace elf3d::viewport {

struct ViewportPickRequest {
    EntityId camera;
    Float2 position_pixels;
    PickOptions options;
};

class OffscreenViewport final {
  private:
    struct ConstructionKey final {};
    class State;

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

    [[nodiscard]] Result<void> update_navigation(renderer::Renderer& renderer,
                                                 picking::PickingService& picking,
                                                 scene::Storage& scene, EntityId camera,
                                                 const ViewportInput& input);
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

    void set_active_tool(ViewportTool tool) noexcept;
    [[nodiscard]] ViewportTool active_tool() const noexcept;

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
    void set_selection_enabled(bool enabled) noexcept;
    [[nodiscard]] bool selection_enabled() const noexcept;
    [[nodiscard]] Result<void> set_selection_settings(const SelectionSettings& settings);
    [[nodiscard]] SelectionSettings selection_settings() const noexcept;
    [[nodiscard]] PickingStatistics
    picking_statistics(const picking::PickingService& picking) const noexcept;

    [[nodiscard]] Result<void> begin_distance_measurement();
    void cancel_distance_measurement() noexcept;
    void clear_distance_measurement() noexcept;
    void clear_scene_measurement(SceneId scene) noexcept;
    [[nodiscard]] DistanceMeasurementSnapshot
    distance_measurement_snapshot(const scene::Storage& scene) noexcept;
    [[nodiscard]] Result<void>
    set_measurement_settings(const DistanceMeasurementSettings& settings);
    [[nodiscard]] DistanceMeasurementSettings measurement_settings() const noexcept;
    [[nodiscard]] MeasurementStatistics measurement_statistics() const noexcept;
    [[nodiscard]] Result<ProjectedViewportPoint>
    project_world_to_viewport(const scene::Storage& scene, EntityId camera,
                              Float3 world_position) const;

    [[nodiscard]] Result<void> isolate_entity(const scene::Storage& scene, EntityId entity);
    void clear_isolation() noexcept;
    void clear_scene_isolation(SceneId scene) noexcept;
    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] Result<void> hide_selected(scene::Storage& scene);
    [[nodiscard]] Result<void> show_selected(scene::Storage& scene);
    [[nodiscard]] Result<void> isolate_selected(const scene::Storage& scene);
    [[nodiscard]] Result<std::optional<Bounds3>> visible_bounds(const scene::Storage& scene);

    [[nodiscard]] Result<void> set_section_plane(const SectionPlane& plane);
    void clear_section_plane() noexcept;
    [[nodiscard]] Result<std::uint32_t> add_clipping_box(const ClippingBox& box);
    [[nodiscard]] Result<void> set_clipping_box(std::uint32_t index, const ClippingBox& box);
    [[nodiscard]] Result<void> remove_clipping_box(std::uint32_t index);
    void clear_clipping_boxes() noexcept;
    void clear_clipping() noexcept;
    [[nodiscard]] Result<void> set_clipping_helpers_visible(bool visible) noexcept;
    [[nodiscard]] Result<void>
    set_clipping_helper_settings(const ClippingHelperSettings& settings) noexcept;
    [[nodiscard]] Result<void> reset_clipping_box_to_visible_bounds(const scene::Storage& scene,
                                                                    std::uint32_t index);
    [[nodiscard]] Result<std::uint32_t>
    add_clipping_box_from_visible_bounds(const scene::Storage& scene);
    [[nodiscard]] ClippingSnapshot clipping_snapshot() const noexcept;

    [[nodiscard]] Result<void> render(renderer::Renderer& renderer, const scene::Storage& scene,
                                      EntityId camera);
    [[nodiscard]] RenderStatistics statistics() const noexcept;
    [[nodiscard]] TextureHandle color_texture() const noexcept;
    [[nodiscard]] bool framebuffer_valid() const noexcept;

    OffscreenViewport(ConstructionKey, std::unique_ptr<graphics::RenderTarget> render_target,
                      std::unique_ptr<graphics::PickingTarget> picking_target) noexcept;

  private:
    struct InteractionFrame {
        EntityId camera;
        ViewportInput input;
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
    [[nodiscard]] Result<InteractionFrame> interaction_frame(scene::Storage& scene, EntityId camera,
                                                             const ViewportInput& input);
    [[nodiscard]] ViewportInput normalized_pointer_hover(const ViewportInput& input) const noexcept;
    [[nodiscard]] Result<void>
    update_orbit_screen_anchor(renderer::Renderer& renderer, scene::Storage& scene,
                               const InteractionFrame& frame,
                               std::optional<Float2> orbit_start_position);
    [[nodiscard]] Result<void> handle_navigation_click(renderer::Renderer& renderer,
                                                       picking::PickingService& picking,
                                                       scene::Storage& scene,
                                                       const InteractionFrame& frame,
                                                       std::optional<Float2> click_position);
    [[nodiscard]] Result<void> handle_measurement_click(renderer::Renderer& renderer,
                                                        picking::PickingService& picking,
                                                        scene::Storage& scene,
                                                        const InteractionFrame& frame,
                                                        Float2 click_position);
    [[nodiscard]] Result<void> handle_selection_click(renderer::Renderer& renderer,
                                                      picking::PickingService& picking,
                                                      scene::Storage& scene,
                                                      const InteractionFrame& frame,
                                                      Float2 click_position);
    [[nodiscard]] Result<bool> hide_clicked_entity(scene::Storage& scene,
                                                   const InteractionFrame& frame,
                                                   const std::optional<PickHit>& hit);
    [[nodiscard]] Result<void> select_control_click(const scene::Storage& scene,
                                                    const InteractionFrame& frame,
                                                    const std::optional<PickHit>& hit);
    [[nodiscard]] Result<void> anchor_plain_click(scene::Storage& scene,
                                                  const InteractionFrame& frame,
                                                  const std::optional<PickHit>& hit);
    [[nodiscard]] Result<void> update_measurement_preview(renderer::Renderer& renderer,
                                                          picking::PickingService& picking,
                                                          scene::Storage& scene,
                                                          const InteractionFrame& frame);

    std::unique_ptr<graphics::RenderTarget> render_target_;
    std::unique_ptr<graphics::PickingTarget> picking_target_;
    std::unique_ptr<State> state_;
    ViewportTool active_tool_ = ViewportTool::selection;
    std::array<OverlayLineSegment, 2 + 4 + maximum_clipping_boxes * 12> overlay_lines_;
    std::array<OverlayPointMarker, 3> overlay_markers_;
    std::size_t overlay_line_count_ = 0;
    std::size_t overlay_marker_count_ = 0;
    Color4 clear_color_{0.08F, 0.16F, 0.28F, 1.0F};
    BasicLighting lighting_;
    RenderStatistics statistics_;
    PickingStatistics gpu_picking_statistics_;
};

} // namespace elf3d::viewport
