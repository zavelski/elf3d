module;

#include "orbit_navigation_detail.h"
#include <elf3d/core/assert.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

module elf.navigation;

import elf.interaction;
import elf.math;
import elf.scene;

namespace elf3d::navigation {
namespace navigation_detail {

[[nodiscard]] bool finite_vector(const Float3& value) noexcept;
[[nodiscard]] bool nearly_equal_matrix(const Float4x4& left, const Float4x4& right) noexcept;
[[nodiscard]] bool valid_settings(const OrbitNavigationSettings& settings) noexcept;
[[nodiscard]] BoundsInfo bounds_info(std::optional<Bounds3> bounds_value) noexcept;
[[nodiscard]] DistanceLimits effective_distance_limits(const OrbitNavigationSettings& settings,
                                                       const BoundsInfo& bounds) noexcept;
[[nodiscard]] float sanitized_motion_reference(float distance) noexcept;
[[nodiscard]] float local_motion_reference_distance(float distance,
                                                    const BoundsInfo& bounds) noexcept;
[[nodiscard]] float dolly_step_from_multiplier(float multiplier, float signed_distance,
                                               const OrbitNavigationSettings& settings,
                                               float motion_reference_distance) noexcept;
[[nodiscard]] float clamp_signed_distance(float distance, DistanceLimits limits) noexcept;
[[nodiscard]] Float3 canonical_direction() noexcept;
[[nodiscard]] Float3 direction_from_angles(float yaw, float pitch) noexcept;
[[nodiscard]] CameraBasis basis_from_forward(const Float3& direction) noexcept;
[[nodiscard]] float fit_distance_to_bounds(const Bounds3& bounds, const Float3& center,
                                           const Float3& direction, float vertical_tangent,
                                           float horizontal_tangent) noexcept;
void angles_from_direction(const Float3& direction, float& yaw, float& pitch) noexcept;
void apply_orbit_delta(Float2 delta, const OrbitNavigationSettings& settings, float& yaw,
                       float& pitch) noexcept;
[[nodiscard]] Result<CameraBasis> camera_basis(const scene::Storage& scene, EntityId camera);
[[nodiscard]] Transform look_at_transform(const Float3& position, const Float3& direction) noexcept;
[[nodiscard]] Result<void> set_camera_world_transform(scene::Storage& scene, EntityId camera,
                                                      const Transform& world_transform);
[[nodiscard]] Result<void> set_camera_world_matrix(scene::Storage& scene, EntityId camera,
                                                   const Float4x4& camera_world);
[[nodiscard]] Result<void> validate_camera(const scene::Storage& scene, EntityId camera);
[[nodiscard]] Result<float> aspect_ratio(Extent2D extent) noexcept;
[[nodiscard]] Result<void> update_clip_planes(scene::Storage& scene, EntityId camera,
                                              float distance, float radius);
[[nodiscard]] NavigationInteractionMode
to_navigation_mode(interaction::InteractionMode mode) noexcept;

} // namespace navigation_detail

using namespace navigation_detail;

namespace {

void set_matrix_column(Float4x4& matrix, std::size_t column, Float3 value,
                       float homogeneous) noexcept {
    const std::size_t offset = column * 4;
    matrix.elements[offset] = value.x;
    matrix.elements[offset + 1] = value.y;
    matrix.elements[offset + 2] = value.z;
    matrix.elements[offset + 3] = homogeneous;
}

[[nodiscard]] Float3 screen_anchor_ray(const CameraBasis& basis, const Float3& camera_position,
                                       const Float3& anchor) noexcept {
    const Float3 ray = math::subtract(anchor, camera_position);
    const float distance = math::vector_length(ray);
    if (!finite_vector(ray) || !std::isfinite(distance) || distance <= minimum_axis_length ||
        math::dot(ray, basis.forward) <= 0.0F) {
        return basis.forward;
    }
    return math::normalized(ray);
}

[[nodiscard]] std::optional<Quaternion>
screen_anchor_rotation(const CameraBasis& basis, Float2 delta,
                       const OrbitNavigationSettings& settings) noexcept {
    float current_yaw = 0.0F;
    float current_pitch = 0.0F;
    angles_from_direction(basis.forward, current_yaw, current_pitch);
    const float yaw_delta = -delta.x * settings.orbit_sensitivity;
    const float vertical_sign = settings.invert_vertical_orbit ? 1.0F : -1.0F;
    const float requested_pitch_delta = delta.y * vertical_sign * settings.orbit_sensitivity;
    const float clamped_pitch =
        std::clamp(current_pitch + requested_pitch_delta, settings.minimum_pitch_radians,
                   settings.maximum_pitch_radians);
    const float pitch_delta = clamped_pitch - current_pitch;
    if (yaw_delta == 0.0F && pitch_delta == 0.0F) {
        return std::nullopt;
    }
    const Quaternion yaw_rotation =
        math::rotation_from_axis_angle(yaw_delta, Float3{0.0F, 1.0F, 0.0F});
    Float3 right_after_yaw = math::rotate_vector(yaw_rotation, basis.right);
    if (!finite_vector(right_after_yaw) ||
        math::vector_length(right_after_yaw) <= minimum_axis_length) {
        right_after_yaw = basis.right;
    }
    const Quaternion pitch_rotation =
        math::rotation_from_axis_angle(pitch_delta, math::normalized(right_after_yaw));
    return math::compose_rotations(pitch_rotation, yaw_rotation);
}

[[nodiscard]] Result<Float4x4> rotated_screen_anchor_camera(const Float4x4& current_world,
                                                            const CameraBasis& basis,
                                                            const Float3& anchor,
                                                            const Quaternion& rotation) noexcept {
    const Float3 new_position =
        math::add(anchor, math::rotate_vector(rotation, math::subtract(basis.position, anchor)));
    const Float3 new_right = math::normalized(math::rotate_vector(rotation, basis.right));
    const Float3 new_up = math::normalized(math::rotate_vector(rotation, basis.up));
    const Float3 new_backward =
        math::normalized(math::rotate_vector(rotation, math::negate(basis.forward)));
    if (!finite_vector(new_position) || !finite_vector(new_right) || !finite_vector(new_up) ||
        !finite_vector(new_backward)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Off-axis orbit produced an invalid camera transform"};
    }
    Float4x4 camera_world = current_world;
    set_matrix_column(camera_world, 0, new_right, 0.0F);
    set_matrix_column(camera_world, 1, new_up, 0.0F);
    set_matrix_column(camera_world, 2, new_backward, 0.0F);
    set_matrix_column(camera_world, 3, new_position, 1.0F);
    return camera_world;
}

struct SynchronizedView final {
    Float3 pivot;
    Float3 direction;
    float distance = 1.0F;
};

[[nodiscard]] float fallback_synchronized_distance(const BoundsInfo& bounds,
                                                   float minimum_distance) noexcept {
    return bounds.has_bounds ? std::max(bounds.radius * 2.0F, minimum_distance)
                             : std::max(1.0F, minimum_distance);
}

[[nodiscard]] SynchronizedView synchronized_view(const CameraBasis& basis, const BoundsInfo& bounds,
                                                 const OrbitNavigationSettings& settings,
                                                 Float3 existing_pivot,
                                                 bool preserve_existing_pivot) noexcept {
    Float3 pivot = preserve_existing_pivot && math::is_finite(existing_pivot)
                       ? existing_pivot
                       : math::add(basis.position, basis.forward);
    if (!preserve_existing_pivot && bounds.has_bounds) {
        pivot = bounds.center;
    }
    float distance = math::vector_length(math::subtract(pivot, basis.position));
    if (!std::isfinite(distance) || distance <= minimum_axis_length) {
        distance = fallback_synchronized_distance(bounds, settings.minimum_distance);
        pivot = math::add(basis.position, math::scale(basis.forward, distance));
    }
    const DistanceLimits limits = effective_distance_limits(settings, bounds);
    distance = std::clamp(distance, limits.minimum, limits.maximum);
    Float3 direction = math::subtract(pivot, basis.position);
    if (!finite_vector(direction) || math::vector_length(direction) <= minimum_axis_length) {
        direction = basis.forward;
    }
    return SynchronizedView{pivot, direction, distance};
}

struct FitProjection final {
    float vertical_tangent = 0.0F;
    float horizontal_tangent = 0.0F;
};

[[nodiscard]] Result<FitProjection> fit_projection(const PerspectiveCameraDescription& description,
                                                   float aspect) noexcept {
    const float vertical_half_angle = description.vertical_field_of_view_radians * 0.5F;
    const float vertical_tangent = std::tan(vertical_half_angle);
    const float horizontal_half_angle = std::atan(vertical_tangent * aspect);
    const float horizontal_tangent = std::tan(horizontal_half_angle);
    const float limiting_half_angle = std::min(vertical_half_angle, horizontal_half_angle);
    const float sine = std::sin(limiting_half_angle);
    if (!std::isfinite(sine) || sine <= 0.0F || !std::isfinite(vertical_tangent) ||
        vertical_tangent <= 0.0F || !std::isfinite(horizontal_tangent) ||
        horizontal_tangent <= 0.0F) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Camera fitting requires a valid field of view and aspect ratio"};
    }
    return FitProjection{vertical_tangent, horizontal_tangent};
}

} // namespace

Result<void> OrbitNavigationController::set_screen_anchor(scene::Storage& scene, EntityId camera,
                                                          Float3 world_position) {
    if (!math::is_finite(world_position)) {
        return Error{ErrorCode::invalid_argument,
                     "Navigation screen anchor requires a finite world-space position"};
    }

    const Result<void> sync = ensure_synchronized(scene, camera);
    if (!sync) {
        return sync.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_ = world_position;
    return {};
}

Result<void> OrbitNavigationController::fit_to_scene(scene::Storage& scene, EntityId camera,
                                                     Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return fit_to_scene(scene, camera, extent, visibility.value());
}

Result<void> OrbitNavigationController::fit_to_scene(scene::Storage& scene, EntityId camera,
                                                     Extent2D extent,
                                                     const scene::VisibilityFilter& visibility) {
    cancel_interaction();
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    const Float3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, direction,
                              scene.visible_world_bounds(visibility));
}

Result<void> OrbitNavigationController::reset_view(scene::Storage& scene, EntityId camera,
                                                   Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return reset_view(scene, camera, extent, visibility.value());
}

Result<void> OrbitNavigationController::reset_view(scene::Storage& scene, EntityId camera,
                                                   Extent2D extent,
                                                   const scene::VisibilityFilter& visibility) {
    cancel_interaction();
    return fit_with_direction(scene, camera, extent, canonical_direction(),
                              scene.visible_world_bounds(visibility));
}

Result<void> OrbitNavigationController::fit_to_bounds(scene::Storage& scene, EntityId camera,
                                                      Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    const Float3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, direction, bounds);
}

Result<void> OrbitNavigationController::reset_to_bounds(scene::Storage& scene, EntityId camera,
                                                        Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    return fit_with_direction(scene, camera, extent, canonical_direction(), bounds);
}

Result<void> OrbitNavigationController::synchronize(const scene::Storage& scene, EntityId camera) {
    return synchronize_from_camera(scene, camera, true);
}

void OrbitNavigationController::cancel_interaction() noexcept {
    interaction_.cancel();
    screen_anchor_.reset();
    keyboard_navigation_used_ = false;
    eye_orbit_active_ = false;
}

void OrbitNavigationController::set_enabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        cancel_interaction();
    }
}

bool OrbitNavigationController::enabled() const noexcept {
    return enabled_;
}

bool OrbitNavigationController::has_screen_anchor() const noexcept {
    return screen_anchor_.has_value();
}

Result<void>
OrbitNavigationController::set_settings(const OrbitNavigationSettings& settings) noexcept {
    if (!valid_settings(settings)) {
        return Error{ErrorCode::invalid_navigation_settings,
                     "Orbit navigation settings require finite, ordered distances, motion scale, "
                     "and pitch limits"};
    }
    settings_ = settings;
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    if (std::isfinite(distance_) && std::abs(distance_) > settings_.maximum_distance) {
        distance_ = std::copysign(settings_.maximum_distance, distance_);
    }
    return {};
}

OrbitNavigationSettings OrbitNavigationController::settings() const noexcept {
    return settings_;
}

bool OrbitNavigationController::has_state() const noexcept {
    return has_valid_state_;
}

NavigationSnapshot OrbitNavigationController::snapshot() const noexcept {
    ELF3D_ASSERT(has_valid_state_);
    const interaction::InteractionMode mode = interaction_.mode();
    const NavigationInteractionMode public_mode = to_navigation_mode(mode);
    return NavigationSnapshot{pivot_,
                              distance_,
                              yaw_radians_,
                              pitch_radians_,
                              mode == interaction::InteractionMode::orbit,
                              mode == interaction::InteractionMode::pan,
                              interaction_.pointer_captured(),
                              public_mode};
}

Result<void> OrbitNavigationController::ensure_synchronized(const scene::Storage& scene,
                                                            EntityId camera) {
    if (!has_valid_state_ || scene.id() != scene_ || camera != camera_) {
        const bool preserve_existing_pivot =
            has_valid_state_ && scene.id() == scene_ && camera == camera_;
        return synchronize_from_camera(scene, camera, preserve_existing_pivot);
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    if (!nearly_equal_matrix(camera_world_result.value(), camera_world_)) {
        return synchronize_from_camera(scene, camera, true);
    }
    return {};
}

Result<void> OrbitNavigationController::apply_screen_anchor_dolly(
    scene::Storage& scene, EntityId camera, float multiplier, std::optional<Bounds3> bounds_value) {
    if (!screen_anchor_.has_value()) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Screen-stable dolly requires a navigation screen anchor"};
    }
    const Result<Float4x4> current_world_result = scene.world_matrix(camera);
    if (!current_world_result) {
        return current_world_result.error();
    }

    Float4x4 camera_world = current_world_result.value();
    const Result<CameraBasis> current_basis = camera_basis(scene, camera);
    if (!current_basis) {
        return current_basis.error();
    }
    const Float3 camera_position = math::matrix_column(camera_world, 3);
    const Float3 anchor = screen_anchor_.value();
    const Float3 ray = screen_anchor_ray(current_basis.value(), camera_position, anchor);

    const BoundsInfo bounds = bounds_info(bounds_value);
    const float signed_anchor_distance =
        math::dot(math::subtract(anchor, camera_position), current_basis.value().forward);
    const float motion_reference = local_motion_reference_distance(signed_anchor_distance, bounds);
    const float step =
        dolly_step_from_multiplier(multiplier, signed_anchor_distance, settings_, motion_reference);
    const Float3 new_position = math::add(camera_position, math::scale(ray, step));

    set_matrix_column(camera_world, 3, new_position, 1.0F);
    const Result<void> transform_result = set_camera_world_matrix(scene, camera, camera_world);
    if (!transform_result) {
        return transform_result.error();
    }

    return finish_screen_anchor_dolly(scene, camera, bounds_value);
}

Result<void>
OrbitNavigationController::finish_screen_anchor_dolly(scene::Storage& scene, EntityId camera,
                                                      std::optional<Bounds3> bounds_value) {
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    const BoundsInfo bounds = bounds_info(bounds_value);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    angles_from_direction(basis.value().forward, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    Float3 centerline_pivot = pivot_;
    if (!finite_vector(centerline_pivot)) {
        centerline_pivot =
            math::add(basis.value().position,
                      math::scale(basis.value().forward, sanitized_motion_reference(distance_)));
    }
    distance_ = clamp_signed_distance(
        math::dot(math::subtract(centerline_pivot, basis.value().position), basis.value().forward),
        limits);
    pivot_ = math::add(basis.value().position, math::scale(basis.value().forward, distance_));
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    const Result<void> clip_result =
        update_clip_planes(scene, camera, std::abs(distance_), bounds.radius);
    if (!clip_result) {
        return clip_result.error();
    }
    return {};
}

Result<OrbitNavigationController::ScreenAnchorOrbit>
OrbitNavigationController::screen_anchor_orbit(const scene::Storage& scene, EntityId camera,
                                               Float2 delta) {
    if (!screen_anchor_.has_value()) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Off-axis orbit requires a navigation screen anchor"};
    }
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }
    const Result<Float4x4> current_world_result = scene.world_matrix(camera);
    if (!current_world_result) {
        return current_world_result.error();
    }

    const Float3 anchor = screen_anchor_.value();
    const Float3 camera_to_anchor = math::subtract(anchor, basis.value().position);
    const float anchor_distance = math::vector_length(camera_to_anchor);
    if (!finite_vector(camera_to_anchor) || !std::isfinite(anchor_distance) ||
        anchor_distance <= minimum_axis_length) {
        screen_anchor_.reset();
        return ScreenAnchorOrbit{current_world_result.value(), 0.0F, false};
    }
    const std::optional<Quaternion> rotation =
        screen_anchor_rotation(basis.value(), delta, settings_);
    if (!rotation.has_value()) {
        return ScreenAnchorOrbit{current_world_result.value(), anchor_distance, false};
    }
    const Result<Float4x4> rotated = rotated_screen_anchor_camera(current_world_result.value(),
                                                                  basis.value(), anchor, *rotation);
    if (!rotated) {
        return rotated.error();
    }
    return ScreenAnchorOrbit{rotated.value(), anchor_distance, true};
}

Result<void>
OrbitNavigationController::commit_screen_anchor_orbit(scene::Storage& scene, EntityId camera,
                                                      const ScreenAnchorOrbit& orbit,
                                                      std::optional<Bounds3> bounds_value) {
    if (!orbit.changed) {
        return {};
    }
    const Result<void> transform_result =
        set_camera_world_matrix(scene, camera, orbit.camera_world);
    if (!transform_result) {
        return transform_result.error();
    }

    const Result<CameraBasis> new_basis = camera_basis(scene, camera);
    if (!new_basis) {
        return new_basis.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    angles_from_direction(new_basis.value().forward, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    yaw_radians_ = std::remainder(yaw_radians_, pi * 2.0F);

    const BoundsInfo bounds = bounds_info(bounds_value);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = std::clamp(orbit.anchor_distance, limits.minimum, limits.maximum);
    const Float3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
    pivot_ = math::add(new_basis.value().position, math::scale(direction, distance_));
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    return {};
}

Result<void> OrbitNavigationController::apply_screen_anchor_orbit(
    scene::Storage& scene, EntityId camera, Float2 delta, std::optional<Bounds3> bounds_value) {
    const Result<ScreenAnchorOrbit> orbit = screen_anchor_orbit(scene, camera, delta);
    if (!orbit) {
        return orbit.error();
    }
    return commit_screen_anchor_orbit(scene, camera, orbit.value(), bounds_value);
}

Result<void> OrbitNavigationController::apply_eye_orbit(scene::Storage& scene, EntityId camera,
                                                        Float2 delta,
                                                        std::optional<Bounds3> bounds_value) {
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }

    float next_yaw = yaw_radians_;
    float next_pitch = pitch_radians_;
    angles_from_direction(basis.value().forward, next_yaw, next_pitch);
    apply_orbit_delta(delta, settings_, next_yaw, next_pitch);

    const Float3 direction = direction_from_angles(next_yaw, next_pitch);
    const Transform transform = look_at_transform(basis.value().position, direction);
    const Result<void> transform_result = set_camera_world_transform(scene, camera, transform);
    if (!transform_result) {
        return transform_result.error();
    }

    const Result<CameraBasis> new_basis = camera_basis(scene, camera);
    if (!new_basis) {
        return new_basis.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }

    angles_from_direction(new_basis.value().forward, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    yaw_radians_ = std::remainder(yaw_radians_, pi * 2.0F);

    const BoundsInfo bounds = bounds_info(bounds_value);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = std::clamp(sanitized_motion_reference(distance_), limits.minimum, limits.maximum);
    pivot_ =
        math::add(new_basis.value().position, math::scale(new_basis.value().forward, distance_));
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_.reset();

    const Result<void> clip_result = update_clip_planes(scene, camera, distance_, bounds.radius);
    if (!clip_result) {
        return clip_result.error();
    }
    return {};
}

Result<void> OrbitNavigationController::synchronize_from_camera(const scene::Storage& scene,
                                                                EntityId camera,
                                                                bool preserve_existing_pivot) {
    const Result<void> valid = validate_camera(scene, camera);
    if (!valid) {
        return valid.error();
    }
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }

    const BoundsInfo bounds = bounds_info(scene.world_bounds());
    const SynchronizedView synchronized =
        synchronized_view(basis.value(), bounds, settings_, pivot_, preserve_existing_pivot);
    angles_from_direction(synchronized.direction, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    pivot_ = synchronized.pivot;
    distance_ = synchronized.distance;
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_.reset();
    return {};
}

Result<void> OrbitNavigationController::apply_camera(scene::Storage& scene, EntityId camera) {
    const BoundsInfo bounds = bounds_info(scene.world_bounds());
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = clamp_signed_distance(distance_, limits);
    const Float3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const Float3 position = math::subtract(pivot_, math::scale(direction, distance_));
    const Transform transform = look_at_transform(position, direction);
    const Result<void> transform_result = set_camera_world_transform(scene, camera, transform);
    if (!transform_result) {
        return transform_result.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_.reset();
    const Result<void> clip_result =
        update_clip_planes(scene, camera, std::abs(distance_), bounds.radius);
    if (!clip_result) {
        return clip_result.error();
    }
    return {};
}

Result<OrbitNavigationController::FitPreparation>
OrbitNavigationController::prepare_fit(const scene::Storage& scene, EntityId camera,
                                       Extent2D extent, Float3 direction,
                                       std::optional<Bounds3> bounds_value) const {
    const Result<void> valid = validate_camera(scene, camera);
    if (!valid) {
        return valid.error();
    }
    const Result<float> aspect = aspect_ratio(extent);
    if (!aspect) {
        return aspect.error();
    }
    const BoundsInfo bounds = bounds_info(bounds_value);
    if (!bounds.has_bounds) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera fitting requires a scene with renderable bounds"};
    }
    if (!math::is_finite(direction) || math::vector_length(direction) <= minimum_axis_length) {
        direction = canonical_direction();
    }
    const Result<Float4x4> old_matrix = scene.local_matrix(camera);
    if (!old_matrix) {
        return old_matrix.error();
    }
    const Result<PerspectiveCameraDescription> old_description = scene.perspective_camera(camera);
    if (!old_description) {
        return old_description.error();
    }
    return FitPreparation{old_matrix.value(), old_description.value(), *bounds_value, direction,
                          aspect.value()};
}

Result<void> OrbitNavigationController::fit_with_direction(scene::Storage& scene, EntityId camera,
                                                           Extent2D extent, Float3 direction,
                                                           std::optional<Bounds3> bounds_value) {
    const Result<FitPreparation> preparation =
        prepare_fit(scene, camera, extent, direction, bounds_value);
    if (!preparation) {
        return preparation.error();
    }
    const Result<FitProjection> projection =
        fit_projection(preparation.value().old_description, preparation.value().aspect);
    if (!projection) {
        return projection.error();
    }

    angles_from_direction(preparation.value().direction, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);

    const BoundsInfo bounds = bounds_info(preparation.value().bounds);
    pivot_ = bounds.center;
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    const float fit_distance = fit_distance_to_bounds(
        preparation.value().bounds, bounds.center,
        direction_from_angles(yaw_radians_, pitch_radians_), projection.value().vertical_tangent,
        projection.value().horizontal_tangent);
    distance_ = std::clamp(fit_distance * fit_margin, limits.minimum, limits.maximum);
    const Float3 fit_direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const Float3 position = math::subtract(pivot_, math::scale(fit_direction, distance_));
    const Transform transform = look_at_transform(position, fit_direction);
    const Result<void> transform_result = set_camera_world_transform(scene, camera, transform);
    if (!transform_result) {
        return transform_result.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_.reset();
    const Result<void> clip_result = update_clip_planes(scene, camera, distance_, bounds.radius);
    if (!clip_result) {
        const Result<void> restore_matrix =
            scene.set_local_matrix(camera, preparation.value().old_matrix);
        const Result<void> restore_camera =
            scene.set_perspective_camera(camera, preparation.value().old_description);
        (void)restore_matrix;
        (void)restore_camera;
        return clip_result.error();
    }
    return {};
}

} // namespace elf3d::navigation
