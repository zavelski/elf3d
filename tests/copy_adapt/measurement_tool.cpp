#include "measurement_tool.hpp"

#include <cmath>

namespace elf3d_external {
namespace {

[[nodiscard]] double distance_between(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    const double x = static_cast<double>(right.x) - static_cast<double>(left.x);
    const double y = static_cast<double>(right.y) - static_cast<double>(left.y);
    const double z = static_cast<double>(right.z) - static_cast<double>(left.z);
    return std::sqrt(x * x + y * y + z * z);
}

constexpr elf3d::Color4 line_color{1.0F, 0.75F, 0.1F, 1.0F};
constexpr elf3d::Color4 first_color{0.2F, 0.9F, 0.3F, 1.0F};
constexpr elf3d::Color4 second_color{1.0F, 0.35F, 0.2F, 1.0F};

} // namespace

elf3d::Result<void> MeasurementTool::place_hit(const elf3d::Scene& scene,
                                               const elf3d::PickHit& hit) noexcept {
    const elf3d::Result<elf3d::SurfaceAnchor> anchor = scene.create_surface_anchor(hit);
    if (!anchor) {
        return anchor.error();
    }
    if (!first_.has_value() || second_.has_value() || first_->scene != scene.id()) {
        first_ = anchor.value();
        second_.reset();
    } else {
        second_ = anchor.value();
    }
    preview_.reset();
    return {};
}

elf3d::Result<void> MeasurementTool::update_preview(const elf3d::Scene& scene,
                                                    const elf3d::PickHit& hit) noexcept {
    if (!first_.has_value() || second_.has_value()) {
        preview_.reset();
        return {};
    }
    const elf3d::Result<elf3d::SurfaceAnchor> anchor = scene.create_surface_anchor(hit);
    if (!anchor) {
        return anchor.error();
    }
    preview_ = anchor.value().scene == first_->scene
                   ? std::optional<elf3d::SurfaceAnchor>{anchor.value()}
                   : std::nullopt;
    return {};
}

void MeasurementTool::clear() noexcept {
    first_.reset();
    second_.reset();
    preview_.reset();
}

elf3d::Result<MeasurementTool::ResolvedPoint>
MeasurementTool::resolve(const elf3d::Scene& scene, const elf3d::Viewport& viewport,
                         const elf3d::SurfaceAnchor& anchor) const noexcept {
    const elf3d::Result<elf3d::ResolvedSurfaceAnchor> resolved =
        scene.resolve_surface_anchor(anchor);
    if (!resolved) {
        return resolved.error();
    }
    const elf3d::Result<bool> visible = viewport.surface_anchor_visible(scene, resolved.value());
    if (!visible) {
        return visible.error();
    }
    return ResolvedPoint{resolved.value(), visible.value()};
}

elf3d::Result<MeasurementSnapshot>
MeasurementTool::snapshot(const elf3d::Scene& scene,
                          const elf3d::Viewport& viewport) const noexcept {
    MeasurementSnapshot result;
    if (!first_.has_value()) {
        return result;
    }

    const elf3d::Result<ResolvedPoint> first = resolve(scene, viewport, *first_);
    if (!first) {
        return first.error();
    }
    result.state =
        second_.has_value() ? MeasurementState::complete : MeasurementState::awaiting_second_point;
    result.first_point = first.value().surface;
    result.overlay_visible = first.value().visible;

    if (second_.has_value()) {
        const elf3d::Result<ResolvedPoint> second = resolve(scene, viewport, *second_);
        if (!second) {
            return second.error();
        }
        result.second_point = second.value().surface;
        result.distance_meters = distance_between(first.value().surface.world_position,
                                                  second.value().surface.world_position);
        result.overlay_visible = first.value().visible && second.value().visible;
        return result;
    }

    if (preview_.has_value()) {
        const elf3d::Result<ResolvedPoint> preview = resolve(scene, viewport, *preview_);
        if (!preview) {
            return preview.error();
        }
        result.preview_point = preview.value().surface;
        result.preview_distance_meters = distance_between(first.value().surface.world_position,
                                                          preview.value().surface.world_position);
        result.overlay_visible = first.value().visible && preview.value().visible;
    }
    return result;
}

elf3d::Result<MeasurementOverlay>
MeasurementTool::overlay(const elf3d::Scene& scene,
                         const elf3d::Viewport& viewport) const noexcept {
    const elf3d::Result<MeasurementSnapshot> value = snapshot(scene, viewport);
    if (!value) {
        return value.error();
    }

    MeasurementOverlay result;
    if (!value.value().overlay_visible || !value.value().first_point.has_value()) {
        return result;
    }

    const elf3d::Float3 first = value.value().first_point->world_position;
    result.markers[result.marker_count++] = elf3d::OverlayPointMarker{
        first, first_color, 6.0F, elf3d::OverlayDepthMode::always_visible};

    const std::optional<elf3d::ResolvedSurfaceAnchor>& endpoint =
        value.value().second_point.has_value() ? value.value().second_point
                                               : value.value().preview_point;
    if (endpoint.has_value()) {
        result.lines[result.line_count++] =
            elf3d::OverlayLineSegment{first, endpoint->world_position, line_color, 2.0F,
                                      elf3d::OverlayDepthMode::always_visible};
        result.markers[result.marker_count++] = elf3d::OverlayPointMarker{
            endpoint->world_position, second_color, 6.0F, elf3d::OverlayDepthMode::always_visible};
    }
    return result;
}

} // namespace elf3d_external
