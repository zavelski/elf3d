#ifndef ELF3D_VIEWPORT_H
#define ELF3D_VIEWPORT_H

#include <elf3d/clipping.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace elf3d {

class Engine;

#if defined(_MSC_VER)
#pragma warning(push)
// Exported special members keep unique_ptr operations inside the DLL.
#pragma warning(disable : 4251)
#endif
class ELF3D_API Viewport final {
  private:
    class Impl;
    struct ConstructionKey final {};

  public:
    // Destruction must occur on the owning graphics thread while a compatible
    // host OpenGL context is current. The Engine must not outlive that context.
    ~Viewport() noexcept;

    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;

    Viewport(Viewport&&) = delete;
    Viewport& operator=(Viewport&&) = delete;

    // Frame configuration
    [[nodiscard]] Extent2D extent() const noexcept;
    // Resize and render are graphics-thread operations. A zero component safely
    // releases the current render target and produces no color texture.
    [[nodiscard]] Result<void> resize(Extent2D extent) noexcept;

    void set_clear_color(Color4 color) noexcept;
    [[nodiscard]] Color4 clear_color() const noexcept;

    void set_basic_lighting(const BasicLighting& lighting) noexcept;
    [[nodiscard]] BasicLighting basic_lighting() const noexcept;

    // Navigation mutates the supplied camera entity and retains only
    // viewport-local interaction state. The camera entity must belong to the
    // supplied Scene and contain a perspective-camera component.
    [[nodiscard]] Result<void> update_navigation(Scene& scene, EntityId camera_entity,
                                                 const ViewportInput& input) noexcept;
    [[nodiscard]] Result<void> set_examine_pivot(Scene& scene, EntityId camera_entity,
                                                 Float3 world_position) noexcept;
    [[nodiscard]] Result<void> fit_to_scene(Scene& scene, EntityId camera_entity) noexcept;
    [[nodiscard]] Result<void> reset_view(Scene& scene, EntityId camera_entity) noexcept;
    [[nodiscard]] Result<void> synchronize_navigation(const Scene& scene,
                                                      EntityId camera_entity) noexcept;

    void set_navigation_enabled(bool enabled) noexcept;
    [[nodiscard]] bool navigation_enabled() const noexcept;
    [[nodiscard]] Result<void>
    set_navigation_settings(const OrbitNavigationSettings& settings) noexcept;
    [[nodiscard]] OrbitNavigationSettings navigation_settings() const noexcept;
    [[nodiscard]] std::optional<NavigationSnapshot> navigation_snapshot() const noexcept;

    // Interaction arbitration
    void set_active_tool(ViewportTool tool) noexcept;
    [[nodiscard]] ViewportTool active_tool() const noexcept;
    void cancel_interaction() noexcept;

    // Picking performs stateless queries and does not change selection.
    [[nodiscard]] Result<Ray3> make_picking_ray(const Scene& scene, EntityId camera_entity,
                                                Float2 position_pixels) const noexcept;
    [[nodiscard]] Result<std::optional<PickHit>>
    pick(const Scene& scene, EntityId camera_entity, Float2 position_pixels,
         const PickOptions& options = {}) const noexcept;
    [[nodiscard]] Result<PickingStatistics> picking_statistics() const noexcept;

    // Selection state is local to this Viewport.
    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(const Scene& scene, EntityId camera_entity, Float2 position_pixels) noexcept;
    [[nodiscard]] Result<void> set_selected_entity(const Scene& scene,
                                                   EntityId selected_entity) noexcept;
    void clear_selection() noexcept;
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::optional<EntityId> selected_entity() const noexcept;
    [[nodiscard]] std::optional<PickHit> selection_hit() const noexcept;
    [[nodiscard]] SelectionSnapshot selection_snapshot() const noexcept;
    void set_selection_enabled(bool enabled) noexcept;
    [[nodiscard]] bool selection_enabled() const noexcept;
    [[nodiscard]] Result<void> set_selection_settings(const SelectionSettings& settings) noexcept;
    [[nodiscard]] SelectionSettings selection_settings() const noexcept;

    // Measurement state is local to this Viewport. Projection is a stateless
    // query and does not change measurement state.
    [[nodiscard]] Result<void> begin_distance_measurement() noexcept;
    void cancel_distance_measurement() noexcept;
    void clear_distance_measurement() noexcept;
    [[nodiscard]] DistanceMeasurementSnapshot
    distance_measurement_snapshot(const Scene& scene) const noexcept;
    [[nodiscard]] Result<void>
    set_measurement_settings(const DistanceMeasurementSettings& settings) noexcept;
    [[nodiscard]] DistanceMeasurementSettings measurement_settings() const noexcept;
    [[nodiscard]] MeasurementStatistics measurement_statistics() const noexcept;
    [[nodiscard]] Result<ProjectedViewportPoint>
    project_world_to_viewport(const Scene& scene, EntityId camera_entity,
                              Float3 world_position) const noexcept;

    // Isolation is local to this Viewport. The explicitly named hide/show
    // operations mutate persistent local visibility in the supplied Scene.
    [[nodiscard]] Result<void> isolate_entity(const Scene& scene,
                                              EntityId isolated_entity) noexcept;
    void clear_isolation() noexcept;
    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] Result<void> hide_selected_in_scene(Scene& scene) noexcept;
    [[nodiscard]] Result<void> show_selected_in_scene(Scene& scene) noexcept;
    [[nodiscard]] Result<void> isolate_selected(const Scene& scene) noexcept;
    [[nodiscard]] Result<std::optional<Bounds3>> visible_bounds(const Scene& scene) const noexcept;

    // Clipping state is local to this Viewport.
    [[nodiscard]] Result<void> set_section_plane(const SectionPlane& plane) noexcept;
    void clear_section_plane() noexcept;
    [[nodiscard]] Result<std::uint32_t> add_clipping_box(const ClippingBox& box) noexcept;
    [[nodiscard]] Result<void> set_clipping_box(std::uint32_t index,
                                                const ClippingBox& box) noexcept;
    [[nodiscard]] Result<void> remove_clipping_box(std::uint32_t index) noexcept;
    void clear_clipping_boxes() noexcept;
    void clear_clipping() noexcept;
    [[nodiscard]] Result<void> set_clipping_helpers_visible(bool visible) noexcept;
    [[nodiscard]] Result<void>
    set_clipping_helper_settings(const ClippingHelperSettings& settings) noexcept;
    [[nodiscard]] Result<void> reset_clipping_box_to_visible_bounds(const Scene& scene,
                                                                    std::uint32_t index) noexcept;
    [[nodiscard]] Result<std::uint32_t>
    add_clipping_box_from_visible_bounds(const Scene& scene) noexcept;
    [[nodiscard]] ClippingSnapshot clipping_snapshot() const noexcept;

    // Rendering observes but does not own the Scene or camera entity.
    [[nodiscard]] Result<void> render(const Scene& scene, EntityId camera_entity) noexcept;
    [[nodiscard]] RenderStatistics render_statistics() const noexcept;
    // The returned non-owning handle is invalidated by resize or destruction.
    [[nodiscard]] TextureHandle color_texture() const noexcept;
    [[nodiscard]] bool framebuffer_valid() const noexcept;

    explicit Viewport(ConstructionKey, std::unique_ptr<Impl> impl) noexcept;

  private:
    friend class Engine;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
