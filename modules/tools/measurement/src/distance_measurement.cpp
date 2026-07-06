module;

#include <elf3d/math/value_types.h>

#include <algorithm>
#include <cmath>
#include <optional>

module elf.tool.measurement;

import elf.assets;
import elf.clipping;
import elf.math;
import elf.scene;

namespace elf3d::tools::measurement {
namespace {

constexpr float barycentric_tolerance = 0.001F;
constexpr float minimum_normal_length = 0.000001F;

[[nodiscard]] bool finite_float2(Float2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool finite_color(Color4 color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
}

[[nodiscard]] bool finite_matrix(const Float4x4& matrix) noexcept {
    for (float value : matrix.elements) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_barycentric(Float3 barycentric) noexcept {
    if (!math::is_finite(barycentric)) {
        return false;
    }
    const float sum = barycentric.x + barycentric.y + barycentric.z;
    return std::isfinite(sum) && std::abs(sum - 1.0F) <= barycentric_tolerance &&
           barycentric.x >= -barycentric_tolerance && barycentric.y >= -barycentric_tolerance &&
           barycentric.z >= -barycentric_tolerance &&
           barycentric.x <= 1.0F + barycentric_tolerance &&
           barycentric.y <= 1.0F + barycentric_tolerance &&
           barycentric.z <= 1.0F + barycentric_tolerance;
}

[[nodiscard]] bool valid_settings(const DistanceMeasurementSettings& settings) noexcept {
    const bool valid_depth_mode = settings.depth_mode == OverlayDepthMode::depth_tested ||
                                  settings.depth_mode == OverlayDepthMode::always_visible;
    const bool valid_unit = settings.display_unit == LengthDisplayUnit::automatic_metric ||
                            settings.display_unit == LengthDisplayUnit::meters ||
                            settings.display_unit == LengthDisplayUnit::centimeters ||
                            settings.display_unit == LengthDisplayUnit::millimeters ||
                            settings.display_unit == LengthDisplayUnit::feet ||
                            settings.display_unit == LengthDisplayUnit::inches;
    return finite_color(settings.line_color) && finite_color(settings.first_point_color) &&
           finite_color(settings.second_point_color) &&
           std::isfinite(settings.line_thickness_pixels) && settings.line_thickness_pixels > 0.0F &&
           std::isfinite(settings.marker_radius_pixels) && settings.marker_radius_pixels > 0.0F &&
           valid_depth_mode && valid_unit;
}

[[nodiscard]] Color4 sanitized_color(Color4 color) noexcept {
    const auto channel = [](float value, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 1.0F), channel(color.blue, 1.0F),
                  channel(color.alpha, 1.0F)};
}

[[nodiscard]] double distance_between(Float3 first, Float3 second) noexcept {
    const double x = static_cast<double>(second.x) - static_cast<double>(first.x);
    const double y = static_cast<double>(second.y) - static_cast<double>(first.y);
    const double z = static_cast<double>(second.z) - static_cast<double>(first.z);
    return std::sqrt(x * x + y * y + z * z);
}

[[nodiscard]] Float3 midpoint(Float3 first, Float3 second) noexcept {
    return Float3{(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F,
                  (first.z + second.z) * 0.5F};
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

[[nodiscard]] Float3 barycentric_point(Float3 first, Float3 second, Float3 third,
                                       Float3 barycentric) noexcept {
    return add(add(scale(first, barycentric.x), scale(second, barycentric.y)),
               scale(third, barycentric.z));
}

[[nodiscard]] Float3 multiply_normal_matrix(const math::Matrix3x3& matrix, Float3 normal) noexcept {
    return Float3{
        matrix[0] * normal.x + matrix[3] * normal.y + matrix[6] * normal.z,
        matrix[1] * normal.x + matrix[4] * normal.y + matrix[7] * normal.z,
        matrix[2] * normal.x + matrix[5] * normal.y + matrix[8] * normal.z,
    };
}

[[nodiscard]] DistanceMeasurementState
state_from_anchors(bool active_measurement_tool, bool has_first, bool has_second) noexcept {
    if (has_first && has_second) {
        return DistanceMeasurementState::complete;
    }
    if (has_first) {
        return DistanceMeasurementState::awaiting_second_point;
    }
    return active_measurement_tool ? DistanceMeasurementState::awaiting_first_point
                                   : DistanceMeasurementState::empty;
}

} // namespace

DisplayLength display_length(double meters, LengthDisplayUnit unit) noexcept {
    LengthDisplayUnit resolved_unit = unit;
    if (unit == LengthDisplayUnit::automatic_metric) {
        const double absolute = std::abs(meters);
        if (absolute >= 1.0) {
            resolved_unit = LengthDisplayUnit::meters;
        } else if (absolute >= 0.01) {
            resolved_unit = LengthDisplayUnit::centimeters;
        } else {
            resolved_unit = LengthDisplayUnit::millimeters;
        }
    }

    switch (resolved_unit) {
    case LengthDisplayUnit::meters:
        return DisplayLength{meters, resolved_unit};
    case LengthDisplayUnit::centimeters:
        return DisplayLength{meters * 100.0, resolved_unit};
    case LengthDisplayUnit::millimeters:
        return DisplayLength{meters * 1000.0, resolved_unit};
    case LengthDisplayUnit::feet:
        return DisplayLength{meters * 3.280839895, resolved_unit};
    case LengthDisplayUnit::inches:
        return DisplayLength{meters * 39.37007874, resolved_unit};
    case LengthDisplayUnit::automatic_metric:
        break;
    }
    return DisplayLength{meters, LengthDisplayUnit::meters};
}

Result<void>
DistanceMeasurementController::set_settings(const DistanceMeasurementSettings& settings) noexcept {
    if (!valid_settings(settings)) {
        return Error{ErrorCode::invalid_measurement_settings,
                     "Distance measurement settings require finite colors, positive pixel sizes, "
                     "and supported unit/depth modes"};
    }
    settings_ = settings;
    settings_.line_color = sanitized_color(settings.line_color);
    settings_.first_point_color = sanitized_color(settings.first_point_color);
    settings_.second_point_color = sanitized_color(settings.second_point_color);
    return {};
}

DistanceMeasurementSettings DistanceMeasurementController::settings() const noexcept {
    return settings_;
}

void DistanceMeasurementController::cancel_incomplete() noexcept {
    if (first_.has_value() && !second_.has_value()) {
        first_.reset();
        preview_.reset();
    }
    last_preview_position_.reset();
}

void DistanceMeasurementController::clear() noexcept {
    first_.reset();
    second_.reset();
    preview_.reset();
    last_preview_position_.reset();
}

void DistanceMeasurementController::clear_scene(SceneId scene) noexcept {
    if ((first_.has_value() && first_->scene == scene) ||
        (second_.has_value() && second_->scene == scene) ||
        (preview_.has_value() && preview_->scene == scene)) {
        clear();
    }
}

Result<void> DistanceMeasurementController::place_hit(const scene::Storage& scene,
                                                      const PickHit& hit) {
    Result<MeasurementAnchor> anchor = anchor_from_hit(scene, hit);
    if (!anchor) {
        return anchor.error();
    }

    if (!first_.has_value() || second_.has_value() || first_->scene != scene.id()) {
        first_ = anchor.value();
        second_.reset();
        preview_.reset();
        ++statistics_.committed_points;
        diagnostic_.reset();
        last_preview_position_.reset();
        return {};
    }

    second_ = anchor.value();
    preview_.reset();
    ++statistics_.committed_points;
    diagnostic_.reset();
    last_preview_position_.reset();
    return {};
}

Result<void> DistanceMeasurementController::update_preview(const scene::Storage& scene,
                                                           const PickHit& hit) {
    if (!first_.has_value() || second_.has_value()) {
        preview_.reset();
        return {};
    }
    Result<MeasurementAnchor> anchor = anchor_from_hit(scene, hit);
    if (!anchor) {
        return anchor.error();
    }
    if (anchor.value().scene != first_->scene) {
        preview_.reset();
        return {};
    }
    preview_ = anchor.value();
    return {};
}

void DistanceMeasurementController::clear_preview() noexcept {
    preview_.reset();
}

bool DistanceMeasurementController::has_incomplete_measurement() const noexcept {
    return first_.has_value() && !second_.has_value();
}

bool DistanceMeasurementController::wants_preview_pick(const scene::Storage& scene,
                                                       const scene::VisibilityFilter& visibility,
                                                       Float2 position_pixels,
                                                       bool input_allows_preview) const noexcept {
    return wants_preview_pick(scene, visibility, clipping::disabled_filter(), position_pixels,
                              input_allows_preview);
}

bool DistanceMeasurementController::wants_preview_pick(
    const scene::Storage& scene, const scene::VisibilityFilter& visibility,
    const clipping::ClippingFilter& clipping_filter, Float2 position_pixels,
    bool input_allows_preview) const noexcept {
    if (!input_allows_preview || !first_.has_value() || second_.has_value() ||
        !finite_float2(position_pixels)) {
        return false;
    }
    if (!last_preview_position_.has_value() || last_preview_position_.value() != position_pixels) {
        return true;
    }
    return last_preview_scene_revision_ != scene.revision() ||
           last_preview_visibility_revision_ != visibility.visibility_revision ||
           last_preview_clipping_revision_ != clipping_filter.revision;
}

void DistanceMeasurementController::record_preview_pick(const scene::Storage& scene,
                                                        const scene::VisibilityFilter& visibility,
                                                        Float2 position_pixels) noexcept {
    record_preview_pick(scene, visibility, clipping::disabled_filter(), position_pixels);
}

void DistanceMeasurementController::record_preview_pick(
    const scene::Storage& scene, const scene::VisibilityFilter& visibility,
    const clipping::ClippingFilter& clipping_filter, Float2 position_pixels) noexcept {
    last_preview_position_ = position_pixels;
    last_preview_scene_revision_ = scene.revision();
    last_preview_visibility_revision_ = visibility.visibility_revision;
    last_preview_clipping_revision_ = clipping_filter.revision;
    ++statistics_.preview_picks;
}

DistanceMeasurementSnapshot
DistanceMeasurementController::snapshot(const scene::Storage& scene,
                                        const scene::VisibilityFilter& visibility,
                                        ViewportTool active_tool) const noexcept {
    return snapshot(scene, visibility, clipping::disabled_filter(), active_tool);
}

DistanceMeasurementSnapshot DistanceMeasurementController::snapshot(
    const scene::Storage& scene, const scene::VisibilityFilter& visibility,
    const clipping::ClippingFilter& clipping_filter, ViewportTool active_tool) const noexcept {
    Result<DistanceMeasurementSnapshot> resolved =
        resolved_snapshot(scene, visibility, clipping_filter, active_tool);
    if (resolved) {
        DistanceMeasurementSnapshot result = resolved.value();
        if (!result.diagnostic.has_value()) {
            result.diagnostic = diagnostic_;
        }
        return result;
    }
    DistanceMeasurementSnapshot result;
    result.diagnostic = resolved.error();
    return result;
}

Result<MeasurementOverlay>
DistanceMeasurementController::overlay(const scene::Storage& scene,
                                       const scene::VisibilityFilter& visibility) {
    return overlay(scene, visibility, clipping::disabled_filter());
}

Result<MeasurementOverlay>
DistanceMeasurementController::overlay(const scene::Storage& scene,
                                       const scene::VisibilityFilter& visibility,
                                       const clipping::ClippingFilter& clipping_filter) {
    Result<DistanceMeasurementSnapshot> snapshot_result =
        resolved_snapshot(scene, visibility, clipping_filter, ViewportTool::distance_measurement);
    if (!snapshot_result) {
        store_diagnostic(snapshot_result.error());
        clear();
        return MeasurementOverlay{};
    }

    const DistanceMeasurementSnapshot snapshot_value = snapshot_result.value();
    MeasurementOverlay result;
    if (!snapshot_value.overlay_visible || !snapshot_value.first_point.has_value()) {
        return result;
    }

    const MeasurementPoint first = snapshot_value.first_point.value();
    if (snapshot_value.state == DistanceMeasurementState::awaiting_second_point) {
        result.markers[result.marker_count++] =
            OverlayPointMarker{first.world_position, settings_.first_point_color,
                               settings_.marker_radius_pixels, settings_.depth_mode};
        if (snapshot_value.preview_point.has_value()) {
            const MeasurementPoint preview = snapshot_value.preview_point.value();
            result.lines[result.line_count++] = OverlayLineSegment{
                first.world_position, preview.world_position, settings_.line_color,
                settings_.line_thickness_pixels, settings_.depth_mode};
            result.markers[result.marker_count++] =
                OverlayPointMarker{preview.world_position, settings_.second_point_color,
                                   settings_.marker_radius_pixels, settings_.depth_mode};
        }
    } else if (snapshot_value.state == DistanceMeasurementState::complete &&
               snapshot_value.second_point.has_value()) {
        const MeasurementPoint second = snapshot_value.second_point.value();
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

    statistics_.anchor_resolutions +=
        static_cast<std::uint64_t>(snapshot_value.first_point.has_value()) +
        static_cast<std::uint64_t>(snapshot_value.second_point.has_value()) +
        static_cast<std::uint64_t>(snapshot_value.preview_point.has_value());
    statistics_.overlay_lines = static_cast<std::uint64_t>(result.line_count);
    statistics_.overlay_markers = static_cast<std::uint64_t>(result.marker_count);
    return result;
}

MeasurementStatistics DistanceMeasurementController::statistics() const noexcept {
    return statistics_;
}

Result<DistanceMeasurementController::MeasurementAnchor>
DistanceMeasurementController::anchor_from_hit(const scene::Storage& scene,
                                               const PickHit& hit) const noexcept {
    if (!hit.entity.is_valid() || !hit.mesh.is_valid() ||
        detail::SceneHandleAccess::scene(hit.entity) != scene.id() ||
        detail::SceneHandleAccess::scene(hit.mesh) != scene.id() ||
        !math::is_finite(hit.world_position) || !math::is_finite(hit.world_normal) ||
        !valid_barycentric(hit.barycentric_coordinates) || !std::isfinite(hit.world_distance) ||
        hit.world_distance < 0.0F) {
        return Error{ErrorCode::invalid_measurement_hit,
                     "A measurement point requires a valid finite triangle PickHit"};
    }
    const Result<const scene::EntityRecord*> record = scene.entity(hit.entity);
    if (!record) {
        return record.error();
    }
    if (!record.value()->model.has_value()) {
        return Error{ErrorCode::invalid_measurement_hit,
                     "A measurement point must refer to a model entity"};
    }
    if (static_cast<std::size_t>(hit.primitive_index) >= record.value()->model->primitives.size()) {
        return Error{ErrorCode::invalid_measurement_hit,
                     "A measurement point refers to an invalid model primitive"};
    }
    const ModelPrimitiveBinding& primitive = record.value()->model->primitives[hit.primitive_index];
    if (primitive.mesh != hit.mesh) {
        return Error{ErrorCode::invalid_measurement_hit,
                     "A measurement point mesh does not match its model primitive"};
    }
    const Result<const assets::MeshAsset*> mesh_result = scene.assets().mesh(hit.mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }
    const std::size_t base = static_cast<std::size_t>(hit.triangle_index) * 3U;
    if (base + 2U >= mesh_result.value()->indices.size()) {
        return Error{ErrorCode::invalid_measurement_hit,
                     "A measurement point refers to an invalid mesh triangle"};
    }
    return MeasurementAnchor{scene.id(),          hit.entity,         hit.mesh,
                             hit.primitive_index, hit.triangle_index, hit.barycentric_coordinates};
}

Result<DistanceMeasurementController::ResolvedAnchor>
DistanceMeasurementController::resolve_anchor(const scene::Storage& scene,
                                              const scene::VisibilityFilter& visibility,
                                              const MeasurementAnchor& anchor) const noexcept {
    return resolve_anchor(scene, visibility, clipping::disabled_filter(), anchor);
}

Result<DistanceMeasurementController::ResolvedAnchor>
DistanceMeasurementController::resolve_anchor(const scene::Storage& scene,
                                              const scene::VisibilityFilter& visibility,
                                              const clipping::ClippingFilter& clipping_filter,
                                              const MeasurementAnchor& anchor) const noexcept {
    if (anchor.scene != scene.id()) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor belongs to a different scene"};
    }
    if (!valid_barycentric(anchor.barycentric_coordinates)) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor contains invalid barycentric coordinates"};
    }

    const Result<const scene::EntityRecord*> record = scene.entity(anchor.entity);
    if (!record) {
        return record.error();
    }
    if (!record.value()->model.has_value()) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor entity no longer has model geometry"};
    }
    if (static_cast<std::size_t>(anchor.primitive_index) >=
        record.value()->model->primitives.size()) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor primitive index is no longer valid"};
    }
    const ModelPrimitiveBinding& primitive =
        record.value()->model->primitives[anchor.primitive_index];
    if (primitive.mesh != anchor.mesh) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor mesh no longer matches its model primitive"};
    }

    const Result<const assets::MeshAsset*> mesh_result = scene.assets().mesh(anchor.mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }
    const assets::MeshAsset& mesh = *mesh_result.value();
    const std::size_t base = static_cast<std::size_t>(anchor.triangle_index) * 3U;
    if (base + 2U >= mesh.indices.size()) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor triangle index is no longer valid"};
    }
    const std::uint32_t i0 = mesh.indices[base];
    const std::uint32_t i1 = mesh.indices[base + 1U];
    const std::uint32_t i2 = mesh.indices[base + 2U];
    if (static_cast<std::size_t>(i0) >= mesh.vertices.size() ||
        static_cast<std::size_t>(i1) >= mesh.vertices.size() ||
        static_cast<std::size_t>(i2) >= mesh.vertices.size()) {
        return Error{ErrorCode::mesh_index_out_of_range,
                     "A measurement anchor triangle references a stale mesh index"};
    }

    const Float3 a = mesh.vertices[i0].position;
    const Float3 b = mesh.vertices[i1].position;
    const Float3 c = mesh.vertices[i2].position;
    const Float3 local_position = barycentric_point(a, b, c, anchor.barycentric_coordinates);
    const Float3 local_normal = cross(subtract(b, a), subtract(c, a));
    const float local_normal_length = length(local_normal);
    if (!std::isfinite(local_normal_length) || local_normal_length <= minimum_normal_length) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor triangle has degenerate geometry"};
    }

    const Result<Float4x4> world = scene.world_matrix(anchor.entity);
    if (!world) {
        return world.error();
    }
    if (!finite_matrix(world.value())) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "A measurement anchor entity has a non-finite world transform"};
    }
    const Result<math::Matrix3x3> normal_transform = math::normal_matrix(world.value());
    if (!normal_transform) {
        return normal_transform.error();
    }

    const Float3 world_position = math::transform_point(world.value(), local_position);
    Float3 world_normal = multiply_normal_matrix(normal_transform.value(),
                                                 scale(local_normal, 1.0F / local_normal_length));
    const float world_normal_length = length(world_normal);
    if (!math::is_finite(world_position) || !std::isfinite(world_normal_length) ||
        world_normal_length <= minimum_normal_length) {
        return Error{ErrorCode::invalid_measurement_anchor,
                     "A measurement anchor resolved to non-finite world values"};
    }
    world_normal = scale(world_normal, 1.0F / world_normal_length);

    MeasurementPoint point;
    point.entity = anchor.entity;
    point.mesh = anchor.mesh;
    point.primitive_index = anchor.primitive_index;
    point.triangle_index = anchor.triangle_index;
    point.barycentric_coordinates = anchor.barycentric_coordinates;
    point.world_position = world_position;
    point.world_normal = world_normal;
    const bool visible = scene::entity_visible_in_filter(scene, visibility, anchor.entity) &&
                         clipping::contains_point(clipping_filter, point.world_position);
    return ResolvedAnchor{point, visible};
}

Result<DistanceMeasurementSnapshot>
DistanceMeasurementController::resolved_snapshot(const scene::Storage& scene,
                                                 const scene::VisibilityFilter& visibility,
                                                 ViewportTool active_tool) const noexcept {
    return resolved_snapshot(scene, visibility, clipping::disabled_filter(), active_tool);
}

Result<DistanceMeasurementSnapshot> DistanceMeasurementController::resolved_snapshot(
    const scene::Storage& scene, const scene::VisibilityFilter& visibility,
    const clipping::ClippingFilter& clipping_filter, ViewportTool active_tool) const noexcept {
    DistanceMeasurementSnapshot result;
    result.state = state_from_anchors(active_tool == ViewportTool::distance_measurement,
                                      first_.has_value(), second_.has_value());
    result.diagnostic = diagnostic_;

    std::optional<ResolvedAnchor> first;
    std::optional<ResolvedAnchor> second;
    std::optional<ResolvedAnchor> preview;
    if (first_.has_value()) {
        Result<ResolvedAnchor> resolved =
            resolve_anchor(scene, visibility, clipping_filter, first_.value());
        if (!resolved) {
            return resolved.error();
        }
        first = resolved.value();
        result.first_point = first->point;
    }
    if (second_.has_value()) {
        Result<ResolvedAnchor> resolved =
            resolve_anchor(scene, visibility, clipping_filter, second_.value());
        if (!resolved) {
            return resolved.error();
        }
        second = resolved.value();
        result.second_point = second->point;
    }
    if (preview_.has_value() && !second_.has_value()) {
        Result<ResolvedAnchor> resolved =
            resolve_anchor(scene, visibility, clipping_filter, preview_.value());
        if (resolved) {
            preview = resolved.value();
            result.preview_point = preview->point;
        }
    }

    if (first.has_value() && second.has_value()) {
        result.distance_meters =
            distance_between(first->point.world_position, second->point.world_position);
        if (!std::isfinite(result.distance_meters) || result.distance_meters < 0.0) {
            return Error{ErrorCode::invalid_measurement_anchor,
                         "A measurement resolved to a non-finite distance"};
        }
        result.midpoint_world_position =
            midpoint(first->point.world_position, second->point.world_position);
        result.anchors_currently_visible = first->visible && second->visible;
        result.overlay_visible = result.anchors_currently_visible;
    } else if (first.has_value()) {
        result.anchors_currently_visible = first->visible;
        result.overlay_visible = first->visible;
        if (preview.has_value()) {
            result.preview_distance_meters =
                distance_between(first->point.world_position, preview->point.world_position);
            if (!std::isfinite(result.preview_distance_meters) ||
                result.preview_distance_meters < 0.0) {
                return Error{ErrorCode::invalid_measurement_anchor,
                             "A measurement preview resolved to a non-finite distance"};
            }
            result.midpoint_world_position =
                midpoint(first->point.world_position, preview->point.world_position);
            result.overlay_visible = first->visible && preview->visible;
            result.anchors_currently_visible = result.overlay_visible;
        }
    }
    return result;
}

void DistanceMeasurementController::store_diagnostic(Error error) noexcept {
    diagnostic_ = error;
}

} // namespace elf3d::tools::measurement
