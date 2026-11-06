#ifndef ELF3D_VIEWER_TOOLS_HPP
#define ELF3D_VIEWER_TOOLS_HPP

#include <elf3d/app/interaction.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace elf3d::viewer {

enum class ViewerTool : std::uint8_t {
    selection,
    distance_measurement,
};

enum class DistanceMeasurementState : std::uint8_t {
    empty,
    awaiting_first_point,
    awaiting_second_point,
    complete,
};

enum class LengthDisplayUnit : std::uint8_t {
    automatic_metric,
    meters,
    centimeters,
    millimeters,
    feet,
    inches,
};

struct MeasurementPoint {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
    Float3 barycentric_coordinates;
    Float3 world_position;
    Float3 world_normal;

    bool operator==(const MeasurementPoint&) const = default;
};

struct DistanceMeasurementSettings {
    Color4 line_color{1.0F, 0.75F, 0.1F, 1.0F};
    Color4 first_point_color{0.2F, 0.9F, 0.3F, 1.0F};
    Color4 second_point_color{1.0F, 0.35F, 0.2F, 1.0F};
    float line_thickness_pixels = 2.0F;
    float marker_radius_pixels = 6.0F;
    OverlayDepthMode depth_mode = OverlayDepthMode::always_visible;
    LengthDisplayUnit display_unit = LengthDisplayUnit::automatic_metric;

    bool operator==(const DistanceMeasurementSettings&) const = default;
};

struct DistanceMeasurementSnapshot {
    DistanceMeasurementState state = DistanceMeasurementState::empty;
    std::optional<MeasurementPoint> first_point;
    std::optional<MeasurementPoint> second_point;
    std::optional<MeasurementPoint> preview_point;
    std::optional<Float3> midpoint_world_position;
    double distance_meters = 0.0;
    double preview_distance_meters = 0.0;
    bool overlay_visible = false;
    bool anchors_currently_visible = false;
    std::optional<Error> diagnostic;
};

struct MeasurementStatistics {
    std::uint64_t committed_points = 0;
    std::uint64_t preview_picks = 0;
    std::uint64_t anchor_resolutions = 0;
    std::uint64_t overlay_lines = 0;
    std::uint64_t overlay_markers = 0;

    bool operator==(const MeasurementStatistics&) const = default;
};

struct SelectionToolSettings {
    float click_drag_threshold_pixels = 4.0F;
    Color4 highlight_color{1.0F, 0.55F, 0.05F, 1.0F};
    float highlight_strength = 0.45F;

    bool operator==(const SelectionToolSettings&) const = default;
};

enum class BrowserSelectionDomain : std::uint8_t { objects, resources };
using BrowserSelectionKey = std::variant<NodeId, EntityId, MeshId, PrimitiveId>;
enum class BrowserResourceAction { frame, hide, show, isolate };

class SelectionTool final {
  public:
    struct Entry {
        EntityId entity;
        bool camera_only = false;
        bool selected = false;
    };
    [[nodiscard]] Result<void> set_settings(const SelectionToolSettings& settings) noexcept;
    [[nodiscard]] SelectionToolSettings settings() const noexcept;

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

    [[nodiscard]] Result<std::optional<PickHit>> select_at(Scene& scene, Viewport& viewport,
                                                           EntityId camera, Float2 position_pixels,
                                                           bool additive = false) noexcept;
    [[nodiscard]] Result<void> select_entity(Scene& scene, Viewport& viewport, EntityId entity,
                                             bool additive = false) noexcept;
    [[nodiscard]] Result<void> select_all(Scene& scene, Viewport& viewport) noexcept;
    void clear(Viewport& viewport) noexcept;
    void clear_scene(SceneId scene) noexcept;
    [[nodiscard]] std::span<const Entry> entries() const noexcept;
    [[nodiscard]] std::size_t selected_count() const noexcept;
    [[nodiscard]] BrowserSelectionDomain browser_domain() const noexcept;
    [[nodiscard]] std::span<const BrowserSelectionKey> browser_selection() const noexcept;
    [[nodiscard]] std::optional<BrowserSelectionKey> browser_primary() const noexcept;
    [[nodiscard]] bool browser_selected(const BrowserSelectionKey& key) const noexcept;
    // One semantic owner. Runtime entry flags and Viewport's primary highlight
    // are derived projections; tab changes never invoke this operation.
    [[nodiscard]] Result<void>
    set_browser_selection(Scene& scene, Viewport& viewport,
                          std::span<const BrowserSelectionKey> values) noexcept;
    // Resolves resource keys through the shared index when selection or runtime
    // hierarchy changes. Merely switching browser tabs does not change scope.
    [[nodiscard]] Result<void>
    resolve_browser_resources(Scene& scene, Viewport& viewport,
                              const DocumentHierarchySnapshot& index) noexcept;
    [[nodiscard]] Result<void> set_selected_visibility(Scene& scene, Viewport& viewport,
                                                       bool visible) noexcept;
    [[nodiscard]] Result<void> isolate_selected(Scene& scene, Viewport& viewport) noexcept;
    [[nodiscard]] Result<void> frame_selected(Scene& scene, Viewport& viewport,
                                              EntityId camera) noexcept;
    [[nodiscard]] std::optional<EntityHighlight>
    render_feedback(const Viewport& viewport) const noexcept;

  private:
    [[nodiscard]] Result<void> synchronize(Scene& scene) noexcept;
    [[nodiscard]] Result<void> apply_membership(Scene& scene, Viewport& viewport,
                                                BrowserSelectionKey key, bool additive) noexcept;
    [[nodiscard]] Result<void> apply_pick(Scene& scene, Viewport& viewport,
                                          const std::optional<PickHit>& hit,
                                          bool additive) noexcept;
    void refresh_browser_projection(Scene& scene);
    SceneId scene_;
    std::uint64_t hierarchy_revision_ = 0;
    std::vector<Entry> entries_;
    BrowserSelectionDomain browser_domain_ = BrowserSelectionDomain::objects;
    std::vector<BrowserSelectionKey> browser_selection_;
    std::optional<BrowserSelectionKey> browser_primary_;
    std::size_t selected_count_ = 0;
    SelectionToolSettings settings_;
    bool enabled_ = true;
    bool resource_feedback_dirty_ = true;
    std::uint64_t resource_feedback_hierarchy_ = 0;
};

struct ToolUpdateContext {
    Scene& scene;
    Viewport& viewport;
    EntityId camera;
    const InteractionRegionInput& input;
    InputModifiers modifiers;
    bool navigation_captured = false;
    bool selection_changes_allowed = true;
};

struct MeasurementOverlay {
    std::array<OverlayLineSegment, 2> lines;
    std::array<OverlayPointMarker, 3> markers;
    std::size_t line_count = 0;
    std::size_t marker_count = 0;

    [[nodiscard]] std::span<const OverlayLineSegment> line_span() const noexcept
    {
        return {lines.data(), line_count};
    }

    [[nodiscard]] std::span<const OverlayPointMarker> marker_span() const noexcept
    {
        return {markers.data(), marker_count};
    }
};

class MeasurementTool final {
  public:
    [[nodiscard]] Result<void> set_settings(const DistanceMeasurementSettings& settings) noexcept;
    [[nodiscard]] DistanceMeasurementSettings settings() const noexcept;

    void cancel_incomplete() noexcept;
    void clear() noexcept;
    void clear_scene(SceneId scene) noexcept;

    [[nodiscard]] Result<void> place_hit(const Scene& scene, const PickHit& hit) noexcept;
    [[nodiscard]] Result<void> update_preview(const Scene& scene, const PickHit& hit) noexcept;
    void clear_preview() noexcept;

    [[nodiscard]] bool has_incomplete_measurement() const noexcept;
    [[nodiscard]] bool wants_preview_pick(const Scene& scene, const Viewport& viewport,
                                          Float2 position_pixels,
                                          bool input_allows_preview) const noexcept;
    void record_preview_pick(const Scene& scene, const Viewport& viewport,
                             Float2 position_pixels) noexcept;

    [[nodiscard]] DistanceMeasurementSnapshot snapshot(const Scene& scene, const Viewport& viewport,
                                                       bool active) const noexcept;
    [[nodiscard]] Result<MeasurementOverlay> overlay(const Scene& scene,
                                                     const Viewport& viewport) noexcept;
    [[nodiscard]] MeasurementStatistics statistics() const noexcept;

  private:
    struct ResolvedAnchor {
        MeasurementPoint point;
        bool visible = false;
    };

    struct ResolvedAnchorSlot {
        std::optional<ResolvedAnchor> anchor;
    };

    [[nodiscard]] Result<ResolvedAnchor> resolve_anchor(const Scene& scene,
                                                        const Viewport& viewport,
                                                        const SurfaceAnchor& anchor) const noexcept;
    [[nodiscard]] Result<ResolvedAnchorSlot>
    resolve_required_anchor(const Scene& scene, const Viewport& viewport,
                            const std::optional<SurfaceAnchor>& anchor) const noexcept;
    [[nodiscard]] std::optional<ResolvedAnchor>
    resolve_preview_anchor(const Scene& scene, const Viewport& viewport,
                           const std::optional<SurfaceAnchor>& anchor) const noexcept;

    static void assign_snapshot_points(DistanceMeasurementSnapshot& snapshot,
                                       const std::optional<ResolvedAnchor>& first,
                                       const std::optional<ResolvedAnchor>& second,
                                       const std::optional<ResolvedAnchor>& preview) noexcept;
    static void complete_snapshot(DistanceMeasurementSnapshot& snapshot,
                                  const ResolvedAnchor& first,
                                  const ResolvedAnchor& second) noexcept;
    static void preview_snapshot(DistanceMeasurementSnapshot& snapshot, const ResolvedAnchor& first,
                                 const std::optional<ResolvedAnchor>& preview) noexcept;

    void store_diagnostic(Error error) noexcept;

    DistanceMeasurementSettings settings_;
    std::optional<SurfaceAnchor> first_;
    std::optional<SurfaceAnchor> second_;
    std::optional<SurfaceAnchor> preview_;
    std::optional<Error> diagnostic_;
    std::optional<Float2> last_preview_position_;
    std::uint64_t last_preview_scene_revision_ = 0;
    std::uint64_t last_preview_viewport_revision_ = 0;
    MeasurementStatistics statistics_;
};

class ToolCoordinator final {
  public:
    [[nodiscard]] ViewerTool active_tool() const noexcept;
    void activate(ViewerTool tool) noexcept;
    void cancel() noexcept;
    void clear_scene(SceneId scene) noexcept;
    [[nodiscard]] Result<void> update(const ToolUpdateContext& context) noexcept;

    [[nodiscard]] MeasurementTool& measurement() noexcept;
    [[nodiscard]] const MeasurementTool& measurement() const noexcept;
    [[nodiscard]] SelectionTool& selection() noexcept;
    [[nodiscard]] const SelectionTool& selection() const noexcept;

  private:
    [[nodiscard]] Result<void> handle_primary_release(const ToolUpdateContext& context) noexcept;
    [[nodiscard]] Result<void> apply_primary_hit(const ToolUpdateContext& context,
                                                 const PickHit& hit) noexcept;
    [[nodiscard]] Result<void> update_measurement_preview(const ToolUpdateContext& context,
                                                          bool primary_down) noexcept;

    ViewerTool active_tool_ = ViewerTool::selection;
    SelectionTool selection_;
    MeasurementTool measurement_;
    std::optional<Float2> primary_press_position_;
};

[[nodiscard]] const char* tool_name(ViewerTool tool) noexcept;
[[nodiscard]] const char* measurement_state_name(DistanceMeasurementState state) noexcept;

} // namespace elf3d::viewer

#endif
