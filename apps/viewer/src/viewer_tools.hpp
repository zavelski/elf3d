#ifndef ELF3D_VIEWER_TOOLS_HPP
#define ELF3D_VIEWER_TOOLS_HPP

#include <elf3d/app/interaction.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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

struct ClippingToolSettings {
    bool helpers_visible = true;
    Color4 section_plane_color{0.2F, 0.75F, 1.0F, 1.0F};
    Color4 box_color{1.0F, 0.8F, 0.15F, 1.0F};
    float line_thickness_pixels = 2.0F;
    OverlayDepthMode depth_mode = OverlayDepthMode::always_visible;

    bool operator==(const ClippingToolSettings&) const = default;
};

struct ClippingToolOverlay {
    std::array<OverlayLineSegment, 4 + maximum_clipping_boxes * 12> lines;
    std::size_t line_count = 0;

    [[nodiscard]] std::span<const OverlayLineSegment> line_span() const noexcept {
        return {lines.data(), line_count};
    }
};

class SelectionTool final {
  public:
    [[nodiscard]] Result<void> set_settings(const SelectionToolSettings& settings) noexcept;
    [[nodiscard]] SelectionToolSettings settings() const noexcept;

    void set_enabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;

    [[nodiscard]] Result<std::optional<PickHit>>
    select_at(Scene& scene, Viewport& viewport, EntityId camera, Float2 position_pixels) noexcept;
    [[nodiscard]] std::optional<EntityHighlight>
    render_feedback(const Viewport& viewport) const noexcept;

  private:
    SelectionToolSettings settings_;
    bool enabled_ = true;
};

class ClippingTool final {
  public:
    [[nodiscard]] Result<void> set_settings(const ClippingToolSettings& settings) noexcept;
    [[nodiscard]] ClippingToolSettings settings() const noexcept;
    void set_helpers_visible(bool visible) noexcept;
    [[nodiscard]] bool helpers_visible() const noexcept;

    [[nodiscard]] Result<std::uint32_t> add_box_from_visible_bounds(const Scene& scene,
                                                                    Viewport& viewport) noexcept;
    [[nodiscard]] Result<void> reset_box_to_visible_bounds(const Scene& scene, Viewport& viewport,
                                                           std::uint32_t index) noexcept;
    [[nodiscard]] Result<ClippingToolOverlay> overlay(const Scene& scene,
                                                      const Viewport& viewport) const noexcept;

  private:
    [[nodiscard]] Result<ClippingBox>
    box_from_visible_bounds(const Scene& scene, const Viewport& viewport) const noexcept;

    ClippingToolSettings settings_;
};

struct ToolUpdateContext {
    Scene& scene;
    Viewport& viewport;
    EntityId camera;
    const InteractionRegionInput& input;
    InputModifiers modifiers;
    bool navigation_captured = false;
};

struct MeasurementOverlay {
    std::array<OverlayLineSegment, 2> lines;
    std::array<OverlayPointMarker, 3> markers;
    std::size_t line_count = 0;
    std::size_t marker_count = 0;

    [[nodiscard]] std::span<const OverlayLineSegment> line_span() const noexcept {
        return {lines.data(), line_count};
    }

    [[nodiscard]] std::span<const OverlayPointMarker> marker_span() const noexcept {
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
    [[nodiscard]] ClippingTool& clipping() noexcept;
    [[nodiscard]] const ClippingTool& clipping() const noexcept;

  private:
    [[nodiscard]] Result<void> handle_primary_release(const ToolUpdateContext& context) noexcept;
    [[nodiscard]] Result<void> apply_primary_hit(const ToolUpdateContext& context,
                                                 const PickHit& hit) noexcept;
    [[nodiscard]] Result<void> update_measurement_preview(const ToolUpdateContext& context,
                                                          bool primary_down) noexcept;

    ViewerTool active_tool_ = ViewerTool::selection;
    SelectionTool selection_;
    ClippingTool clipping_;
    MeasurementTool measurement_;
    std::optional<Float2> primary_press_position_;
};

[[nodiscard]] const char* tool_name(ViewerTool tool) noexcept;
[[nodiscard]] const char* measurement_state_name(DistanceMeasurementState state) noexcept;

} // namespace elf3d::viewer

#endif
