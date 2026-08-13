#ifndef ELF3D_TESTS_COPY_ADAPT_MEASUREMENT_TOOL_HPP
#define ELF3D_TESTS_COPY_ADAPT_MEASUREMENT_TOOL_HPP

#include <elf3d/elf3d.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace elf3d_external {

enum class MeasurementState : std::uint8_t {
    empty,
    awaiting_second_point,
    complete,
};

struct MeasurementSnapshot {
    MeasurementState state = MeasurementState::empty;
    std::optional<elf3d::ResolvedSurfaceAnchor> first_point;
    std::optional<elf3d::ResolvedSurfaceAnchor> second_point;
    std::optional<elf3d::ResolvedSurfaceAnchor> preview_point;
    double distance_meters = 0.0;
    double preview_distance_meters = 0.0;
    bool overlay_visible = false;
};

struct MeasurementOverlay {
    std::array<elf3d::OverlayLineSegment, 1> lines;
    std::array<elf3d::OverlayPointMarker, 2> markers;
    std::size_t line_count = 0;
    std::size_t marker_count = 0;

    [[nodiscard]] std::span<const elf3d::OverlayLineSegment> line_span() const noexcept {
        return {lines.data(), line_count};
    }

    [[nodiscard]] std::span<const elf3d::OverlayPointMarker> marker_span() const noexcept {
        return {markers.data(), marker_count};
    }
};

// This application-owned Tool is intentionally copied and adapted outside the
// viewer. Its complete dependency surface is the supported public Elf3D API.
class MeasurementTool final {
  public:
    [[nodiscard]] elf3d::Result<void> place_hit(const elf3d::Scene& scene,
                                                const elf3d::PickHit& hit) noexcept;
    [[nodiscard]] elf3d::Result<void> update_preview(const elf3d::Scene& scene,
                                                     const elf3d::PickHit& hit) noexcept;
    void clear() noexcept;

    [[nodiscard]] elf3d::Result<MeasurementSnapshot>
    snapshot(const elf3d::Scene& scene, const elf3d::Viewport& viewport) const noexcept;
    [[nodiscard]] elf3d::Result<MeasurementOverlay>
    overlay(const elf3d::Scene& scene, const elf3d::Viewport& viewport) const noexcept;

  private:
    struct ResolvedPoint {
        elf3d::ResolvedSurfaceAnchor surface;
        bool visible = false;
    };

    [[nodiscard]] elf3d::Result<ResolvedPoint>
    resolve(const elf3d::Scene& scene, const elf3d::Viewport& viewport,
            const elf3d::SurfaceAnchor& anchor) const noexcept;

    std::optional<elf3d::SurfaceAnchor> first_;
    std::optional<elf3d::SurfaceAnchor> second_;
    std::optional<elf3d::SurfaceAnchor> preview_;
};

} // namespace elf3d_external

#endif
