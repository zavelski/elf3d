#ifndef ELF3D_VIEWPORT_H
#define ELF3D_VIEWPORT_H

#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/clipping.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace elf3d {

class Engine;

struct BasicLighting {
    // Direction in which light travels in world space.
    Float3 direction{-0.5F, -1.0F, -0.3F};
    Color4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float ambient_intensity = 0.08F;
    float diffuse_intensity = 3.0F;
};

struct RenderStatistics {
    std::uint64_t draw_calls = 0;
    std::uint64_t triangles = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    // Activity caused by the latest render call.
    std::uint64_t texture_bindings = 0;
    std::uint64_t gpu_texture_uploads = 0;
    // Current renderer cache contents after the latest render call.
    std::uint64_t unique_gpu_textures = 0;
    std::uint64_t overlay_lines = 0;
    std::uint64_t overlay_markers = 0;
    std::uint64_t clipping_bounds_tested = 0;
    std::uint64_t clipping_bounds_rejected = 0;
    std::uint64_t clipping_bounds_intersecting = 0;

    bool operator==(const RenderStatistics &) const = default;
};

struct EntityHighlight {
    EntityId entity;
    Color4 color{1.0F, 0.55F, 0.05F, 1.0F};
    float strength = 0.0F;

    bool operator==(const EntityHighlight &) const = default;
};

struct ViewportRenderOptions {
    std::optional<EntityHighlight> highlight;
    std::span<const OverlayLineSegment> overlay_lines;
    std::span<const OverlayPointMarker> overlay_markers;

    [[nodiscard]] bool operator==(const ViewportRenderOptions &other) const noexcept {
        return highlight == other.highlight && overlay_lines.data() == other.overlay_lines.data() &&
               overlay_lines.size() == other.overlay_lines.size() &&
               overlay_markers.data() == other.overlay_markers.data() &&
               overlay_markers.size() == other.overlay_markers.size();
    }
};

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
    ~Viewport();

    Viewport(const Viewport &) = delete;
    Viewport &operator=(const Viewport &) = delete;

    Viewport(Viewport &&) = delete;
    Viewport &operator=(Viewport &&) = delete;

    [[nodiscard]] Extent2D extent() const noexcept;
    // Resize and render are graphics-thread operations. A zero component safely
    // releases the current render target and produces no color texture.
    [[nodiscard]] Result<void> resize(Extent2D extent);

    void set_clear_color(Color4 color) noexcept;
    [[nodiscard]] Color4 clear_color() const noexcept;

    void set_basic_lighting(const BasicLighting &lighting) noexcept;
    [[nodiscard]] BasicLighting basic_lighting() const noexcept;

    [[nodiscard]] Result<void> update_navigation(Scene &scene, EntityId camera,
                                                 const ViewportInput &input);
    [[nodiscard]] Result<void> set_examine_pivot(Scene &scene, EntityId camera,
                                                 Float3 world_position);
    [[nodiscard]] Result<void> fit_to_scene(Scene &scene, EntityId camera);
    [[nodiscard]] Result<void> reset_view(Scene &scene, EntityId camera);
    [[nodiscard]] Result<void> synchronize_navigation(const Scene &scene, EntityId camera);
    void cancel_interaction() noexcept;

    void set_navigation_enabled(bool enabled) noexcept;
    [[nodiscard]] bool navigation_enabled() const noexcept;
    [[nodiscard]] Result<void> set_navigation_settings(const OrbitNavigationSettings &settings);
    [[nodiscard]] OrbitNavigationSettings navigation_settings() const noexcept;
    [[nodiscard]] std::optional<NavigationSnapshot> navigation_snapshot() const noexcept;

    void set_active_tool(ViewportTool tool) noexcept;
    [[nodiscard]] ViewportTool active_tool() const noexcept;

    [[nodiscard]] Result<Ray3> make_picking_ray(const Scene &scene, EntityId camera,
                                                Float2 position_pixels) const;
    [[nodiscard]] Result<std::optional<PickHit>> pick(const Scene &scene, EntityId camera,
                                                      Float2 position_pixels,
                                                      const PickOptions &options = {}) const;
    [[nodiscard]] Result<std::optional<PickHit>> select_at(const Scene &scene, EntityId camera,
                                                           Float2 position_pixels);
    [[nodiscard]] Result<void> set_selected_entity(const Scene &scene, EntityId entity);
    void clear_selection() noexcept;
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::optional<EntityId> selected_entity() const noexcept;
    [[nodiscard]] std::optional<PickHit> selection_hit() const noexcept;
    [[nodiscard]] SelectionSnapshot selection_snapshot() const noexcept;
    void set_selection_enabled(bool enabled) noexcept;
    [[nodiscard]] bool selection_enabled() const noexcept;
    [[nodiscard]] Result<void> set_selection_settings(const SelectionSettings &settings);
    [[nodiscard]] SelectionSettings selection_settings() const noexcept;
    [[nodiscard]] Result<PickingStatistics> picking_statistics() const noexcept;

    [[nodiscard]] Result<void> begin_distance_measurement();
    void cancel_distance_measurement() noexcept;
    void clear_distance_measurement() noexcept;
    [[nodiscard]] DistanceMeasurementSnapshot
    distance_measurement_snapshot(const Scene &scene) const;
    [[nodiscard]] Result<void>
    set_measurement_settings(const DistanceMeasurementSettings &settings);
    [[nodiscard]] DistanceMeasurementSettings measurement_settings() const noexcept;
    [[nodiscard]] MeasurementStatistics measurement_statistics() const noexcept;
    [[nodiscard]] Result<ProjectedViewportPoint>
    project_world_to_viewport(const Scene &scene, EntityId camera, Float3 world_position) const;

    [[nodiscard]] Result<void> isolate_entity(const Scene &scene, EntityId entity);
    void clear_isolation() noexcept;
    [[nodiscard]] bool is_isolating() const noexcept;
    [[nodiscard]] std::optional<EntityId> isolated_entity() const noexcept;
    [[nodiscard]] Result<void> hide_selected(Scene &scene);
    [[nodiscard]] Result<void> show_selected(Scene &scene);
    [[nodiscard]] Result<void> isolate_selected(const Scene &scene);
    [[nodiscard]] Result<std::optional<Bounds3>> visible_bounds(const Scene &scene) const;

    [[nodiscard]] Result<void> set_section_plane(const SectionPlane &plane);
    void clear_section_plane() noexcept;
    [[nodiscard]] Result<std::uint32_t> add_clipping_box(const ClippingBox &box);
    [[nodiscard]] Result<void> set_clipping_box(std::uint32_t index, const ClippingBox &box);
    [[nodiscard]] Result<void> remove_clipping_box(std::uint32_t index);
    void clear_clipping_boxes() noexcept;
    void clear_clipping() noexcept;
    [[nodiscard]] Result<void> set_clipping_helpers_visible(bool visible) noexcept;
    [[nodiscard]] Result<void>
    set_clipping_helper_settings(const ClippingHelperSettings &settings) noexcept;
    [[nodiscard]] Result<void>
    reset_clipping_box_to_visible_bounds(const Scene &scene, std::uint32_t index);
    [[nodiscard]] Result<std::uint32_t>
    add_clipping_box_from_visible_bounds(const Scene &scene);
    [[nodiscard]] ClippingSnapshot clipping_snapshot() const noexcept;

    [[nodiscard]] Result<void> render(const Scene &scene, EntityId camera);
    [[nodiscard]] RenderStatistics statistics() const noexcept;
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
