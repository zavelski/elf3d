module;

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/measurement.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

export module elf.tool.measurement;

import elf.clipping;
import elf.core;
import elf.picking;
import elf.scene;

export namespace elf3d::tools::measurement {

struct DisplayLength {
    double value = 0.0;
    LengthDisplayUnit unit = LengthDisplayUnit::meters;
};

[[nodiscard]] DisplayLength display_length(double meters, LengthDisplayUnit unit) noexcept;

struct MeasurementOverlay {
    std::array<OverlayLineSegment, 2> lines;
    std::array<OverlayPointMarker, 3> markers;
    std::size_t line_count = 0;
    std::size_t marker_count = 0;

    [[nodiscard]] std::span<const OverlayLineSegment> line_span() const noexcept {
        return std::span<const OverlayLineSegment>{lines.data(), line_count};
    }

    [[nodiscard]] std::span<const OverlayPointMarker> marker_span() const noexcept {
        return std::span<const OverlayPointMarker>{markers.data(), marker_count};
    }
};

class DistanceMeasurementController final {
  public:
    [[nodiscard]] Result<void> set_settings(const DistanceMeasurementSettings& settings) noexcept;
    [[nodiscard]] DistanceMeasurementSettings settings() const noexcept;

    void cancel_incomplete() noexcept;
    void clear() noexcept;
    void clear_scene(SceneId scene) noexcept;

    [[nodiscard]] Result<void> place_hit(const scene::Storage& scene, const PickHit& hit);
    [[nodiscard]] Result<void> update_preview(const scene::Storage& scene, const PickHit& hit);
    void clear_preview() noexcept;

    [[nodiscard]] bool has_incomplete_measurement() const noexcept;
    [[nodiscard]] bool wants_preview_pick(const scene::Storage& scene,
                                          const scene::VisibilityFilter& visibility,
                                          Float2 position_pixels,
                                          bool input_allows_preview) const noexcept;
    [[nodiscard]] bool wants_preview_pick(const scene::Storage& scene,
                                          const scene::VisibilityFilter& visibility,
                                          const clipping::ClippingFilter& clipping_filter,
                                          Float2 position_pixels,
                                          bool input_allows_preview) const noexcept;
    void record_preview_pick(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                             Float2 position_pixels) noexcept;
    void record_preview_pick(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                             const clipping::ClippingFilter& clipping_filter,
                             Float2 position_pixels) noexcept;

    [[nodiscard]] DistanceMeasurementSnapshot snapshot(const scene::Storage& scene,
                                                       const scene::VisibilityFilter& visibility,
                                                       ViewportTool active_tool) const noexcept;
    [[nodiscard]] DistanceMeasurementSnapshot
    snapshot(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
             const clipping::ClippingFilter& clipping_filter,
             ViewportTool active_tool) const noexcept;
    [[nodiscard]] Result<MeasurementOverlay> overlay(const scene::Storage& scene,
                                                     const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<MeasurementOverlay>
    overlay(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
            const clipping::ClippingFilter& clipping_filter);
    [[nodiscard]] MeasurementStatistics statistics() const noexcept;

  private:
    struct MeasurementAnchor {
        SceneId scene;
        EntityId entity;
        MeshHandle mesh;
        std::uint32_t primitive_index = 0;
        std::uint32_t triangle_index = 0;
        Float3 barycentric_coordinates;
    };

    struct ResolvedAnchor {
        MeasurementPoint point;
        bool visible = false;
    };

    struct LocalAnchorGeometry {
        Float3 position;
        Float3 normal;
    };

    [[nodiscard]] Result<MeasurementAnchor> anchor_from_hit(const scene::Storage& scene,
                                                            const PickHit& hit) const noexcept;
    [[nodiscard]] Result<ResolvedAnchor>
    resolve_anchor(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                   const MeasurementAnchor& anchor) const noexcept;
    [[nodiscard]] Result<ResolvedAnchor>
    resolve_anchor(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                   const clipping::ClippingFilter& clipping_filter,
                   const MeasurementAnchor& anchor) const noexcept;
    [[nodiscard]] Result<scene::RuntimePrimitiveView>
    anchor_primitive(const scene::Storage& scene, const MeasurementAnchor& anchor) const noexcept;
    [[nodiscard]] Result<LocalAnchorGeometry>
    local_anchor_geometry(const scene::RuntimePrimitiveView& primitive,
                          const MeasurementAnchor& anchor) const noexcept;
    [[nodiscard]] Result<std::optional<ResolvedAnchor>>
    resolve_required_anchor(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                            const clipping::ClippingFilter& clipping_filter,
                            const std::optional<MeasurementAnchor>& anchor) const noexcept;
    [[nodiscard]] std::optional<ResolvedAnchor>
    resolve_preview_anchor(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                           const clipping::ClippingFilter& clipping_filter,
                           const std::optional<MeasurementAnchor>& anchor) const noexcept;
    [[nodiscard]] static Result<void>
    apply_completed_measurement(DistanceMeasurementSnapshot& snapshot, const ResolvedAnchor& first,
                                const ResolvedAnchor& second) noexcept;
    [[nodiscard]] static Result<void>
    apply_preview_measurement(DistanceMeasurementSnapshot& snapshot, const ResolvedAnchor& first,
                              const ResolvedAnchor& preview) noexcept;
    static void set_resolved_points(DistanceMeasurementSnapshot& snapshot,
                                    const std::optional<ResolvedAnchor>& first,
                                    const std::optional<ResolvedAnchor>& second,
                                    const std::optional<ResolvedAnchor>& preview) noexcept;
    [[nodiscard]] static Result<void>
    apply_incomplete_measurement(DistanceMeasurementSnapshot& snapshot,
                                 const std::optional<ResolvedAnchor>& first,
                                 const std::optional<ResolvedAnchor>& preview) noexcept;
    [[nodiscard]] Result<DistanceMeasurementSnapshot>
    resolved_snapshot(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                      ViewportTool active_tool) const noexcept;
    [[nodiscard]] Result<DistanceMeasurementSnapshot>
    resolved_snapshot(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                      const clipping::ClippingFilter& clipping_filter,
                      ViewportTool active_tool) const noexcept;

    void store_diagnostic(Error error) noexcept;

    DistanceMeasurementSettings settings_;
    std::optional<MeasurementAnchor> first_;
    std::optional<MeasurementAnchor> second_;
    std::optional<MeasurementAnchor> preview_;
    std::optional<Error> diagnostic_;
    std::optional<Float2> last_preview_position_;
    std::uint64_t last_preview_scene_revision_ = 0;
    std::uint64_t last_preview_visibility_revision_ = 0;
    std::uint64_t last_preview_clipping_revision_ = 0;
    MeasurementStatistics statistics_;
};

} // namespace elf3d::tools::measurement
