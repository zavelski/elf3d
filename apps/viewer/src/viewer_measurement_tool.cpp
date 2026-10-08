#include "viewer_tools.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "viewer_tool_style.hpp"

namespace elf3d::viewer {
using namespace tool_detail;
namespace {

[[nodiscard]] bool finite_float2(Float2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool valid_display_unit(LengthDisplayUnit unit) noexcept
{
    return unit == LengthDisplayUnit::automatic_metric || unit == LengthDisplayUnit::meters ||
           unit == LengthDisplayUnit::centimeters || unit == LengthDisplayUnit::millimeters ||
           unit == LengthDisplayUnit::feet || unit == LengthDisplayUnit::inches;
}

[[nodiscard]] bool valid_settings(const DistanceMeasurementSettings& settings) noexcept
{
    return finite_color(settings.line_color) && finite_color(settings.first_point_color) &&
           finite_color(settings.second_point_color) &&
           std::isfinite(settings.line_thickness_pixels) && settings.line_thickness_pixels > 0.0F &&
           std::isfinite(settings.marker_radius_pixels) && settings.marker_radius_pixels > 0.0F &&
           valid_depth_mode(settings.depth_mode) && valid_display_unit(settings.display_unit);
}

[[nodiscard]] DistanceMeasurementState measurement_state(bool active, bool has_first,
                                                         bool has_second) noexcept
{
    if (has_first && has_second) {
        return DistanceMeasurementState::complete;
    }
    if (has_first) {
        return DistanceMeasurementState::awaiting_second_point;
    }
    return active ? DistanceMeasurementState::awaiting_first_point
                  : DistanceMeasurementState::empty;
}

[[nodiscard]] double distance_between(Float3 first, Float3 second) noexcept
{
    const double x = static_cast<double>(second.x) - static_cast<double>(first.x);
    const double y = static_cast<double>(second.y) - static_cast<double>(first.y);
    const double z = static_cast<double>(second.z) - static_cast<double>(first.z);
    return std::sqrt(x * x + y * y + z * z);
}

[[nodiscard]] Float3 midpoint(Float3 first, Float3 second) noexcept
{
    return Float3{(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F,
                  (first.z + second.z) * 0.5F};
}

[[nodiscard]] bool valid_measurement_distances(const DistanceMeasurementSnapshot& snapshot) noexcept
{
    return std::isfinite(snapshot.distance_meters) && snapshot.distance_meters >= 0.0 &&
           std::isfinite(snapshot.preview_distance_meters) &&
           snapshot.preview_distance_meters >= 0.0;
}

} // namespace

Result<void> MeasurementTool::set_settings(const DistanceMeasurementSettings& settings) noexcept
{
    if (!valid_settings(settings)) {
        return Error{ErrorCode::invalid_argument,
                     "Distance measurement settings require finite colors, positive pixel sizes, "
                     "and supported unit/depth modes"};
    }
    settings_ = settings;
    settings_.line_color = sanitized_color(settings.line_color);
    settings_.first_point_color = sanitized_color(settings.first_point_color);
    settings_.second_point_color = sanitized_color(settings.second_point_color);
    return {};
}

DistanceMeasurementSettings MeasurementTool::settings() const noexcept
{
    return settings_;
}

void MeasurementTool::cancel_incomplete() noexcept
{
    if (first_.has_value() && !second_.has_value()) {
        first_.reset();
        preview_.reset();
    }

    last_preview_position_.reset();
}

void MeasurementTool::clear() noexcept
{
    first_.reset();
    second_.reset();
    preview_.reset();
    diagnostic_.reset();
    last_preview_position_.reset();
}

void MeasurementTool::clear_scene(SceneId scene) noexcept
{
    if ((first_.has_value() && first_->scene == scene) ||
        (second_.has_value() && second_->scene == scene) ||
        (preview_.has_value() && preview_->scene == scene)) {
        clear();
    }
}

Result<void> MeasurementTool::place_hit(const Scene& scene, const PickHit& hit) noexcept
{
    Result<SurfaceAnchor> anchor = scene.create_surface_anchor(hit);
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
    diagnostic_.reset();
    last_preview_position_.reset();
    ++statistics_.committed_points;
    return {};
}

Result<void> MeasurementTool::update_preview(const Scene& scene, const PickHit& hit) noexcept
{
    if (!first_.has_value() || second_.has_value()) {
        preview_.reset();
        return {};
    }

    Result<SurfaceAnchor> anchor = scene.create_surface_anchor(hit);
    if (!anchor) {
        return anchor.error();
    }
    preview_ = anchor.value().scene == first_->scene ? std::optional<SurfaceAnchor>{anchor.value()}
                                                     : std::nullopt;
    return {};
}

void MeasurementTool::clear_preview() noexcept
{
    preview_.reset();
}

bool MeasurementTool::has_incomplete_measurement() const noexcept
{
    return first_.has_value() && !second_.has_value();
}

bool MeasurementTool::wants_preview_pick(const Scene& scene, const Viewport& viewport,
                                         Float2 position_pixels,
                                         bool input_allows_preview) const noexcept
{
    if (!input_allows_preview || !first_.has_value() || second_.has_value() ||
        !finite_float2(position_pixels)) {
        return false;
    }
    return !last_preview_position_.has_value() || *last_preview_position_ != position_pixels ||
           last_preview_scene_revision_ != scene.revision() ||
           last_preview_viewport_revision_ != viewport.render_revision();
}

void MeasurementTool::record_preview_pick(const Scene& scene, const Viewport& viewport,
                                          Float2 position_pixels) noexcept
{
    last_preview_position_ = position_pixels;
    last_preview_scene_revision_ = scene.revision();
    last_preview_viewport_revision_ = viewport.render_revision();
    ++statistics_.preview_picks;
}

Result<MeasurementTool::ResolvedAnchor>
MeasurementTool::resolve_anchor(const Scene& scene, const Viewport& viewport,
                                const SurfaceAnchor& anchor) const noexcept
{
    const Result<ResolvedSurfaceAnchor> resolved = scene.resolve_surface_anchor(anchor);
    if (!resolved) {
        return resolved.error();
    }

    const Result<bool> visible = viewport.surface_anchor_visible(scene, resolved.value());
    if (!visible) {
        return visible.error();
    }
    MeasurementPoint point;
    point.entity = anchor.entity;
    point.mesh = anchor.mesh;
    point.primitive_index = anchor.primitive_index;
    point.triangle_index = anchor.triangle_index;
    point.barycentric_coordinates = anchor.barycentric_coordinates;
    point.world_position = resolved.value().world_position;
    point.world_normal = resolved.value().world_normal;
    return ResolvedAnchor{point, visible.value()};
}

Result<MeasurementTool::ResolvedAnchorSlot>
MeasurementTool::resolve_required_anchor(const Scene& scene, const Viewport& viewport,
                                         const std::optional<SurfaceAnchor>& anchor) const noexcept
{
    if (!anchor.has_value()) {
        return ResolvedAnchorSlot{};
    }

    Result<ResolvedAnchor> resolved = resolve_anchor(scene, viewport, *anchor);
    if (!resolved) {
        return resolved.error();
    }
    return ResolvedAnchorSlot{resolved.value()};
}

std::optional<MeasurementTool::ResolvedAnchor>
MeasurementTool::resolve_preview_anchor(const Scene& scene, const Viewport& viewport,
                                        const std::optional<SurfaceAnchor>& anchor) const noexcept
{
    if (!anchor.has_value()) {
        return std::nullopt;
    }

    Result<ResolvedAnchor> resolved = resolve_anchor(scene, viewport, *anchor);
    return resolved ? std::optional<ResolvedAnchor>{resolved.value()} : std::nullopt;
}

void MeasurementTool::assign_snapshot_points(DistanceMeasurementSnapshot& snapshot,
                                             const std::optional<ResolvedAnchor>& first,
                                             const std::optional<ResolvedAnchor>& second,
                                             const std::optional<ResolvedAnchor>& preview) noexcept
{
    snapshot.first_point =
        first.has_value() ? std::optional<MeasurementPoint>{first->point} : std::nullopt;
    snapshot.second_point =
        second.has_value() ? std::optional<MeasurementPoint>{second->point} : std::nullopt;
    snapshot.preview_point =
        preview.has_value() ? std::optional<MeasurementPoint>{preview->point} : std::nullopt;
}

void MeasurementTool::complete_snapshot(DistanceMeasurementSnapshot& snapshot,
                                        const ResolvedAnchor& first,
                                        const ResolvedAnchor& second) noexcept
{
    snapshot.distance_meters =
        distance_between(first.point.world_position, second.point.world_position);
    snapshot.midpoint_world_position =
        midpoint(first.point.world_position, second.point.world_position);
    snapshot.anchors_currently_visible = first.visible && second.visible;
    snapshot.overlay_visible = snapshot.anchors_currently_visible;
}

void MeasurementTool::preview_snapshot(DistanceMeasurementSnapshot& snapshot,
                                       const ResolvedAnchor& first,
                                       const std::optional<ResolvedAnchor>& preview) noexcept
{
    snapshot.anchors_currently_visible = first.visible;
    snapshot.overlay_visible = first.visible;
    if (!preview.has_value()) {
        return;
    }
    snapshot.preview_distance_meters =
        distance_between(first.point.world_position, preview->point.world_position);
    snapshot.midpoint_world_position =
        midpoint(first.point.world_position, preview->point.world_position);
    snapshot.anchors_currently_visible = first.visible && preview->visible;
    snapshot.overlay_visible = snapshot.anchors_currently_visible;
}

DistanceMeasurementSnapshot MeasurementTool::snapshot(const Scene& scene, const Viewport& viewport,
                                                      bool active) const noexcept
{
    DistanceMeasurementSnapshot result;
    result.state = measurement_state(active, first_.has_value(), second_.has_value());
    result.diagnostic = diagnostic_;

    const Result<ResolvedAnchorSlot> first_result =
        resolve_required_anchor(scene, viewport, first_);
    const Result<ResolvedAnchorSlot> second_result =
        resolve_required_anchor(scene, viewport, second_);
    if (!first_result || !second_result) {
        result.diagnostic = !first_result ? first_result.error() : second_result.error();
        return result;
    }

    const ResolvedAnchorSlot& first_slot = first_result.value();
    const ResolvedAnchorSlot& second_slot = second_result.value();
    const std::optional<ResolvedAnchor> preview =
        second_slot.anchor.has_value() ? std::nullopt
                                       : resolve_preview_anchor(scene, viewport, preview_);
    assign_snapshot_points(result, first_slot.anchor, second_slot.anchor, preview);
    if (first_slot.anchor.has_value() && second_slot.anchor.has_value()) {
        complete_snapshot(result, *first_slot.anchor, *second_slot.anchor);
    } else if (first_slot.anchor.has_value()) {
        preview_snapshot(result, *first_slot.anchor, preview);
    }
    if (!valid_measurement_distances(result)) {
        result = {};
        result.diagnostic = Error{ErrorCode::invalid_surface_anchor,
                                  "A measurement resolved to a non-finite distance"};
    }
    return result;
}

Result<MeasurementOverlay> MeasurementTool::overlay(const Scene& scene,
                                                    const Viewport& viewport) noexcept
{
    const DistanceMeasurementSnapshot value = snapshot(scene, viewport, true);
    if (value.diagnostic.has_value()) {
        store_diagnostic(*value.diagnostic);
        return value.diagnostic.value();
    }

    MeasurementOverlay result;
    if (!value.overlay_visible || !value.first_point.has_value()) {
        return result;
    }

    const MeasurementPoint first = *value.first_point;
    if (value.state == DistanceMeasurementState::awaiting_second_point) {
        result.markers[result.marker_count++] =
            OverlayPointMarker{first.world_position, settings_.first_point_color,
                               settings_.marker_radius_pixels, settings_.depth_mode};
        if (value.preview_point.has_value()) {
            const MeasurementPoint preview = *value.preview_point;
            result.lines[result.line_count++] = OverlayLineSegment{
                first.world_position, preview.world_position, settings_.line_color,
                settings_.line_thickness_pixels, settings_.depth_mode};
            result.markers[result.marker_count++] =
                OverlayPointMarker{preview.world_position, settings_.second_point_color,
                                   settings_.marker_radius_pixels, settings_.depth_mode};
        }
    } else if (value.state == DistanceMeasurementState::complete &&
               value.second_point.has_value()) {
        const MeasurementPoint second = *value.second_point;
        result.lines[result.line_count++] =
            OverlayLineSegment{first.world_position, second.world_position, settings_.line_color,
                               settings_.line_thickness_pixels, settings_.depth_mode};
        result.markers[result.marker_count++] =
            OverlayPointMarker{first.world_position, settings_.first_point_color,
                               settings_.marker_radius_pixels, settings_.depth_mode};
        result.markers[result.marker_count++] =
            OverlayPointMarker{second.world_position, settings_.second_point_color,
                               settings_.marker_radius_pixels, settings_.depth_mode};
    }
    statistics_.anchor_resolutions += static_cast<std::uint64_t>(value.first_point.has_value()) +
                                      static_cast<std::uint64_t>(value.second_point.has_value()) +
                                      static_cast<std::uint64_t>(value.preview_point.has_value());
    statistics_.overlay_lines = static_cast<std::uint64_t>(result.line_count);
    statistics_.overlay_markers = static_cast<std::uint64_t>(result.marker_count);
    return result;
}

MeasurementStatistics MeasurementTool::statistics() const noexcept
{
    return statistics_;
}

void MeasurementTool::store_diagnostic(Error error) noexcept
{
    diagnostic_ = error;
}

} // namespace elf3d::viewer
