#include "viewer_tools.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace elf3d::viewer {
namespace {

[[nodiscard]] bool finite_float2(Float2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool finite_color(Color4 color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
}

[[nodiscard]] bool valid_depth_mode(OverlayDepthMode mode) noexcept {
    return mode == OverlayDepthMode::depth_tested || mode == OverlayDepthMode::always_visible;
}

[[nodiscard]] bool valid_display_unit(LengthDisplayUnit unit) noexcept {
    return unit == LengthDisplayUnit::automatic_metric || unit == LengthDisplayUnit::meters ||
           unit == LengthDisplayUnit::centimeters || unit == LengthDisplayUnit::millimeters ||
           unit == LengthDisplayUnit::feet || unit == LengthDisplayUnit::inches;
}

[[nodiscard]] bool valid_settings(const DistanceMeasurementSettings& settings) noexcept {
    return finite_color(settings.line_color) && finite_color(settings.first_point_color) &&
           finite_color(settings.second_point_color) &&
           std::isfinite(settings.line_thickness_pixels) && settings.line_thickness_pixels > 0.0F &&
           std::isfinite(settings.marker_radius_pixels) && settings.marker_radius_pixels > 0.0F &&
           valid_depth_mode(settings.depth_mode) && valid_display_unit(settings.display_unit);
}

[[nodiscard]] bool valid_selection_settings(const SelectionToolSettings& settings) noexcept {
    return std::isfinite(settings.click_drag_threshold_pixels) &&
           settings.click_drag_threshold_pixels >= 0.0F && finite_color(settings.highlight_color) &&
           std::isfinite(settings.highlight_strength) && settings.highlight_strength >= 0.0F &&
           settings.highlight_strength <= 1.0F;
}

[[nodiscard]] bool valid_clipping_settings(const ClippingToolSettings& settings) noexcept {
    return finite_color(settings.section_plane_color) && finite_color(settings.box_color) &&
           std::isfinite(settings.line_thickness_pixels) && settings.line_thickness_pixels > 0.0F &&
           settings.line_thickness_pixels <= 32.0F && valid_depth_mode(settings.depth_mode);
}

[[nodiscard]] Color4 sanitized_color(Color4 color) noexcept {
    const auto channel = [](float value, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 1.0F), channel(color.blue, 1.0F),
                  channel(color.alpha, 1.0F)};
}

[[nodiscard]] DistanceMeasurementState measurement_state(bool active, bool has_first,
                                                         bool has_second) noexcept {
    if (has_first && has_second) {
        return DistanceMeasurementState::complete;
    }
    if (has_first) {
        return DistanceMeasurementState::awaiting_second_point;
    }
    return active ? DistanceMeasurementState::awaiting_first_point
                  : DistanceMeasurementState::empty;
}

[[nodiscard]] double distance_between(Float3 first, Float3 second) noexcept {
    const double x = static_cast<double>(second.x) - static_cast<double>(first.x);
    const double y = static_cast<double>(second.y) - static_cast<double>(first.y);
    const double z = static_cast<double>(second.z) - static_cast<double>(first.z);
    return std::sqrt(x * x + y * y + z * z);
}

[[nodiscard]] float pointer_distance_squared(Float2 first, Float2 second) noexcept {
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return x * x + y * y;
}

[[nodiscard]] Float3 midpoint(Float3 first, Float3 second) noexcept {
    return Float3{(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F,
                  (first.z + second.z) * 0.5F};
}

[[nodiscard]] bool
valid_measurement_distances(const DistanceMeasurementSnapshot& snapshot) noexcept {
    return std::isfinite(snapshot.distance_meters) && snapshot.distance_meters >= 0.0 &&
           std::isfinite(snapshot.preview_distance_meters) &&
           snapshot.preview_distance_meters >= 0.0;
}

[[nodiscard]] bool requests_direct_selection(const ToolUpdateContext& context,
                                             ViewerTool active_tool) noexcept {
    return active_tool == ViewerTool::selection && context.modifiers.control &&
           !context.modifiers.shift;
}

[[nodiscard]] Float3 subtract(Float3 left, Float3 right) noexcept {
    return Float3{left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Float3 add(Float3 left, Float3 right) noexcept {
    return Float3{left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Float3 scale(Float3 value, float multiplier) noexcept {
    return Float3{value.x * multiplier, value.y * multiplier, value.z * multiplier};
}

[[nodiscard]] float dot(Float3 left, Float3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length(Float3 value) noexcept {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] Float3 cross(Float3 left, Float3 right) noexcept {
    return Float3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                  left.x * right.y - left.y * right.x};
}

[[nodiscard]] Float3 normalized_or(Float3 value, Float3 fallback) noexcept {
    const float value_length = length(value);
    if (!std::isfinite(value_length) || value_length <= 0.000001F) {
        return fallback;
    }
    return scale(value, 1.0F / value_length);
}

[[nodiscard]] Float3 center(Bounds3 bounds) noexcept {
    return Float3{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                  (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                  (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

[[nodiscard]] float radius(Bounds3 bounds) noexcept {
    return std::max(0.5F * length(subtract(bounds.maximum, bounds.minimum)), 0.5F);
}

void expand_degenerate_bounds(Bounds3& bounds) noexcept {
    const Float3 extent = subtract(bounds.maximum, bounds.minimum);
    const float largest_extent = std::max({extent.x, extent.y, extent.z, 0.0F});
    const float minimum_half_extent = std::max(largest_extent * 0.005F, 0.0005F);
    const auto expand_axis = [minimum_half_extent](float& minimum, float& maximum) noexcept {
        if (maximum > minimum) {
            return;
        }
        const float axis_center = (minimum + maximum) * 0.5F;
        minimum = axis_center - minimum_half_extent;
        maximum = axis_center + minimum_half_extent;
    };
    expand_axis(bounds.minimum.x, bounds.maximum.x);
    expand_axis(bounds.minimum.y, bounds.maximum.y);
    expand_axis(bounds.minimum.z, bounds.maximum.z);
}

void append_clipping_line(ClippingToolOverlay& overlay, Float3 start, Float3 end,
                          const ClippingToolSettings& settings, Color4 color) noexcept {
    if (overlay.line_count >= overlay.lines.size()) {
        return;
    }
    overlay.lines[overlay.line_count++] =
        OverlayLineSegment{start, end, color, settings.line_thickness_pixels, settings.depth_mode};
}

void append_clipping_box(ClippingToolOverlay& overlay, const ClippingBox& box,
                         const ClippingToolSettings& settings) noexcept {
    const Float3 min = box.minimum;
    const Float3 max = box.maximum;
    const std::array<Float3, 8> corners{{
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z},
    }};
    constexpr std::array<std::array<std::size_t, 2>, 12> edges{{
        {{0, 1}},
        {{0, 2}},
        {{1, 3}},
        {{2, 3}},
        {{4, 5}},
        {{4, 6}},
        {{5, 7}},
        {{6, 7}},
        {{0, 4}},
        {{1, 5}},
        {{2, 6}},
        {{3, 7}},
    }};
    for (const std::array<std::size_t, 2>& edge : edges) {
        append_clipping_line(overlay, corners[edge[0]], corners[edge[1]], settings,
                             settings.box_color);
    }
}

void append_section_plane(ClippingToolOverlay& overlay, const SectionPlane& plane, Bounds3 bounds,
                          const ClippingToolSettings& settings) noexcept {
    if (!plane.enabled) {
        return;
    }
    const Float3 normal = normalized_or(plane.normal, {0.0F, 1.0F, 0.0F});
    Float3 plane_center = center(bounds);
    const float signed_distance = dot(normal, subtract(plane_center, plane.point));
    plane_center = subtract(plane_center, scale(normal, signed_distance));
    const float plane_radius = radius(bounds);
    const Float3 reference =
        std::abs(normal.y) < 0.9F ? Float3{0.0F, 1.0F, 0.0F} : Float3{1.0F, 0.0F, 0.0F};
    const Float3 first_axis =
        scale(normalized_or(cross(normal, reference), {1.0F, 0.0F, 0.0F}), plane_radius);
    const Float3 second_axis =
        scale(normalized_or(cross(normal, first_axis), {0.0F, 0.0F, 1.0F}), plane_radius);
    const std::array<Float3, 4> corners{{
        subtract(subtract(plane_center, first_axis), second_axis),
        subtract(add(plane_center, first_axis), second_axis),
        add(add(plane_center, first_axis), second_axis),
        add(subtract(plane_center, first_axis), second_axis),
    }};
    append_clipping_line(overlay, corners[0], corners[1], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[1], corners[2], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[2], corners[3], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[3], corners[0], settings, settings.section_plane_color);
}

} // namespace

Result<void> SelectionTool::set_settings(const SelectionToolSettings& settings) noexcept {
    if (!valid_selection_settings(settings)) {
        return Error{ErrorCode::invalid_argument,
                     "Selection Tool settings require finite color, non-negative click threshold, "
                     "and highlight strength in [0, 1]"};
    }
    settings_ = settings;
    settings_.highlight_color = sanitized_color(settings.highlight_color);
    return {};
}

SelectionToolSettings SelectionTool::settings() const noexcept {
    return settings_;
}

void SelectionTool::set_enabled(bool enabled) noexcept {
    enabled_ = enabled;
}

bool SelectionTool::enabled() const noexcept {
    return enabled_;
}

Result<std::optional<PickHit>> SelectionTool::select_at(Scene& scene, Viewport& viewport,
                                                        EntityId camera,
                                                        Float2 position_pixels) noexcept {
    if (!enabled_) {
        return std::optional<PickHit>{};
    }
    return viewport.select_at(scene, camera, position_pixels);
}

std::optional<EntityHighlight>
SelectionTool::render_feedback(const Viewport& viewport) const noexcept {
    const std::optional<EntityId> selected = enabled_ ? viewport.selected_entity() : std::nullopt;
    if (!selected.has_value()) {
        return std::nullopt;
    }
    return EntityHighlight{*selected, settings_.highlight_color, settings_.highlight_strength};
}

Result<void> ClippingTool::set_settings(const ClippingToolSettings& settings) noexcept {
    if (!valid_clipping_settings(settings)) {
        return Error{ErrorCode::invalid_argument,
                     "Clipping Tool settings require finite colors, line thickness in (0, 32], "
                     "and a supported depth mode"};
    }
    settings_ = settings;
    settings_.section_plane_color = sanitized_color(settings.section_plane_color);
    settings_.box_color = sanitized_color(settings.box_color);
    return {};
}

ClippingToolSettings ClippingTool::settings() const noexcept {
    return settings_;
}

void ClippingTool::set_helpers_visible(bool visible) noexcept {
    settings_.helpers_visible = visible;
}

bool ClippingTool::helpers_visible() const noexcept {
    return settings_.helpers_visible;
}

Result<ClippingBox> ClippingTool::box_from_visible_bounds(const Scene& scene,
                                                          const Viewport& viewport) const noexcept {
    const Result<std::optional<Bounds3>> bounds = viewport.unclipped_visible_bounds(scene);
    if (!bounds) {
        return bounds.error();
    }
    if (!bounds.value().has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Clipping box creation requires visible renderable scene bounds"};
    }
    Bounds3 expanded = *bounds.value();
    expand_degenerate_bounds(expanded);
    return ClippingBox{expanded.minimum, expanded.maximum, true};
}

Result<std::uint32_t> ClippingTool::add_box_from_visible_bounds(const Scene& scene,
                                                                Viewport& viewport) noexcept {
    const Result<ClippingBox> box = box_from_visible_bounds(scene, viewport);
    if (!box) {
        return box.error();
    }
    return viewport.add_clipping_box(box.value());
}

Result<void> ClippingTool::reset_box_to_visible_bounds(const Scene& scene, Viewport& viewport,
                                                       std::uint32_t index) noexcept {
    const Result<ClippingBox> box = box_from_visible_bounds(scene, viewport);
    if (!box) {
        return box.error();
    }
    return viewport.set_clipping_box(index, box.value());
}

Result<ClippingToolOverlay> ClippingTool::overlay(const Scene& scene,
                                                  const Viewport& viewport) const noexcept {
    ClippingToolOverlay result;
    if (!settings_.helpers_visible) {
        return result;
    }
    const ClippingSnapshot clipping = viewport.clipping_snapshot();
    if (clipping.section_plane.enabled) {
        const Result<std::optional<Bounds3>> bounds = viewport.unclipped_visible_bounds(scene);
        if (!bounds) {
            return bounds.error();
        }
        if (bounds.value().has_value()) {
            append_section_plane(result, clipping.section_plane, *bounds.value(), settings_);
        }
    }
    for (std::uint32_t index = 0; index < clipping.box_count; ++index) {
        if (clipping.boxes[index].enabled) {
            append_clipping_box(result, clipping.boxes[index], settings_);
        }
    }
    return result;
}

Result<void> MeasurementTool::set_settings(const DistanceMeasurementSettings& settings) noexcept {
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

DistanceMeasurementSettings MeasurementTool::settings() const noexcept {
    return settings_;
}

void MeasurementTool::cancel_incomplete() noexcept {
    if (first_.has_value() && !second_.has_value()) {
        first_.reset();
        preview_.reset();
    }
    last_preview_position_.reset();
}

void MeasurementTool::clear() noexcept {
    first_.reset();
    second_.reset();
    preview_.reset();
    diagnostic_.reset();
    last_preview_position_.reset();
}

void MeasurementTool::clear_scene(SceneId scene) noexcept {
    if ((first_.has_value() && first_->scene == scene) ||
        (second_.has_value() && second_->scene == scene) ||
        (preview_.has_value() && preview_->scene == scene)) {
        clear();
    }
}

Result<void> MeasurementTool::place_hit(const Scene& scene, const PickHit& hit) noexcept {
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

Result<void> MeasurementTool::update_preview(const Scene& scene, const PickHit& hit) noexcept {
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

void MeasurementTool::clear_preview() noexcept {
    preview_.reset();
}

bool MeasurementTool::has_incomplete_measurement() const noexcept {
    return first_.has_value() && !second_.has_value();
}

bool MeasurementTool::wants_preview_pick(const Scene& scene, const Viewport& viewport,
                                         Float2 position_pixels,
                                         bool input_allows_preview) const noexcept {
    if (!input_allows_preview || !first_.has_value() || second_.has_value() ||
        !finite_float2(position_pixels)) {
        return false;
    }
    return !last_preview_position_.has_value() || *last_preview_position_ != position_pixels ||
           last_preview_scene_revision_ != scene.revision() ||
           last_preview_viewport_revision_ != viewport.render_revision();
}

void MeasurementTool::record_preview_pick(const Scene& scene, const Viewport& viewport,
                                          Float2 position_pixels) noexcept {
    last_preview_position_ = position_pixels;
    last_preview_scene_revision_ = scene.revision();
    last_preview_viewport_revision_ = viewport.render_revision();
    ++statistics_.preview_picks;
}

Result<MeasurementTool::ResolvedAnchor>
MeasurementTool::resolve_anchor(const Scene& scene, const Viewport& viewport,
                                const SurfaceAnchor& anchor) const noexcept {
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

Result<MeasurementTool::ResolvedAnchorSlot> MeasurementTool::resolve_required_anchor(
    const Scene& scene, const Viewport& viewport,
    const std::optional<SurfaceAnchor>& anchor) const noexcept {
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
                                        const std::optional<SurfaceAnchor>& anchor) const noexcept {
    if (!anchor.has_value()) {
        return std::nullopt;
    }
    Result<ResolvedAnchor> resolved = resolve_anchor(scene, viewport, *anchor);
    return resolved ? std::optional<ResolvedAnchor>{resolved.value()} : std::nullopt;
}

void MeasurementTool::assign_snapshot_points(
    DistanceMeasurementSnapshot& snapshot, const std::optional<ResolvedAnchor>& first,
    const std::optional<ResolvedAnchor>& second,
    const std::optional<ResolvedAnchor>& preview) noexcept {
    snapshot.first_point =
        first.has_value() ? std::optional<MeasurementPoint>{first->point} : std::nullopt;
    snapshot.second_point =
        second.has_value() ? std::optional<MeasurementPoint>{second->point} : std::nullopt;
    snapshot.preview_point =
        preview.has_value() ? std::optional<MeasurementPoint>{preview->point} : std::nullopt;
}

void MeasurementTool::complete_snapshot(DistanceMeasurementSnapshot& snapshot,
                                        const ResolvedAnchor& first,
                                        const ResolvedAnchor& second) noexcept {
    snapshot.distance_meters =
        distance_between(first.point.world_position, second.point.world_position);
    snapshot.midpoint_world_position =
        midpoint(first.point.world_position, second.point.world_position);
    snapshot.anchors_currently_visible = first.visible && second.visible;
    snapshot.overlay_visible = snapshot.anchors_currently_visible;
}

void MeasurementTool::preview_snapshot(DistanceMeasurementSnapshot& snapshot,
                                       const ResolvedAnchor& first,
                                       const std::optional<ResolvedAnchor>& preview) noexcept {
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
                                                      bool active) const noexcept {
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
                                                    const Viewport& viewport) noexcept {
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

MeasurementStatistics MeasurementTool::statistics() const noexcept {
    return statistics_;
}

void MeasurementTool::store_diagnostic(Error error) noexcept {
    diagnostic_ = error;
}

ViewerTool ToolCoordinator::active_tool() const noexcept {
    return active_tool_;
}

void ToolCoordinator::activate(ViewerTool tool) noexcept {
    active_tool_ = tool;
}

void ToolCoordinator::cancel() noexcept {
    measurement_.cancel_incomplete();
    primary_press_position_.reset();
}

void ToolCoordinator::clear_scene(SceneId scene) noexcept {
    measurement_.clear_scene(scene);
    primary_press_position_.reset();
}

Result<void> ToolCoordinator::update(const ToolUpdateContext& context) noexcept {
    const InputTransition& primary =
        context.input.buttons[static_cast<std::size_t>(InputButton::left)];
    if (primary.pressed && context.input.hovered) {
        primary_press_position_ = context.input.pointer_position_pixels;
    }
    const float click_threshold = selection_.settings().click_drag_threshold_pixels;
    if (primary.down && primary_press_position_.has_value() &&
        pointer_distance_squared(*primary_press_position_, context.input.pointer_position_pixels) >
            click_threshold * click_threshold) {
        primary_press_position_.reset();
    }
    if (primary.released) {
        const Result<void> release = handle_primary_release(context);
        if (!release) {
            return release.error();
        }
    }
    return update_measurement_preview(context, primary.down);
}

Result<void> ToolCoordinator::handle_primary_release(const ToolUpdateContext& context) noexcept {
    const bool is_click = primary_press_position_.has_value() && context.input.hovered &&
                          !context.navigation_captured;
    primary_press_position_.reset();
    if (!is_click) {
        return {};
    }
    if (requests_direct_selection(context, active_tool_)) {
        const Result<std::optional<PickHit>> selected = selection_.select_at(
            context.scene, context.viewport, context.camera, context.input.pointer_position_pixels);
        return selected ? Result<void>{} : Result<void>{selected.error()};
    }
    Result<std::optional<PickHit>> hit =
        context.viewport.pick(context.scene, context.camera, context.input.pointer_position_pixels);
    if (!hit || !hit.value().has_value()) {
        return hit ? Result<void>{} : Result<void>{hit.error()};
    }
    return apply_primary_hit(context, *hit.value());
}

Result<void> ToolCoordinator::apply_primary_hit(const ToolUpdateContext& context,
                                                const PickHit& hit) noexcept {
    if (active_tool_ == ViewerTool::distance_measurement) {
        return measurement_.place_hit(context.scene, hit);
    }
    if (context.modifiers.shift) {
        return context.scene.set_entity_local_visibility(hit.entity, false);
    }
    return context.viewport.set_examine_pivot(context.scene, context.camera, hit.world_position);
}

Result<void> ToolCoordinator::update_measurement_preview(const ToolUpdateContext& context,
                                                         bool primary_down) noexcept {
    if (active_tool_ != ViewerTool::distance_measurement) {
        measurement_.clear_preview();
        return {};
    }

    const bool preview_allowed =
        context.input.hovered && !primary_down && !context.navigation_captured;
    if (!measurement_.wants_preview_pick(context.scene, context.viewport,
                                         context.input.pointer_position_pixels, preview_allowed)) {
        if (!preview_allowed) {
            measurement_.clear_preview();
        }
        return {};
    }
    measurement_.record_preview_pick(context.scene, context.viewport,
                                     context.input.pointer_position_pixels);
    Result<std::optional<PickHit>> preview =
        context.viewport.pick(context.scene, context.camera, context.input.pointer_position_pixels);
    if (!preview) {
        return preview.error();
    }
    if (!preview.value().has_value()) {
        measurement_.clear_preview();
        return {};
    }
    return measurement_.update_preview(context.scene, *preview.value());
}

MeasurementTool& ToolCoordinator::measurement() noexcept {
    return measurement_;
}

const MeasurementTool& ToolCoordinator::measurement() const noexcept {
    return measurement_;
}

SelectionTool& ToolCoordinator::selection() noexcept {
    return selection_;
}

const SelectionTool& ToolCoordinator::selection() const noexcept {
    return selection_;
}

ClippingTool& ToolCoordinator::clipping() noexcept {
    return clipping_;
}

const ClippingTool& ToolCoordinator::clipping() const noexcept {
    return clipping_;
}

const char* tool_name(ViewerTool tool) noexcept {
    switch (tool) {
    case ViewerTool::selection:
        return "Select";
    case ViewerTool::distance_measurement:
        return "Measure Distance";
    }
    return "Select";
}

const char* measurement_state_name(DistanceMeasurementState state) noexcept {
    switch (state) {
    case DistanceMeasurementState::empty:
        return "Empty";
    case DistanceMeasurementState::awaiting_first_point:
        return "Select first point";
    case DistanceMeasurementState::awaiting_second_point:
        return "Select second point";
    case DistanceMeasurementState::complete:
        return "Complete";
    }
    return "Empty";
}

} // namespace elf3d::viewer
