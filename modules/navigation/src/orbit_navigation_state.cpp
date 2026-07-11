module;

#include <elf3d/core/assert.h>
#include <elf3d/math/detail/glm_helpers.h>
#include "orbit_navigation_detail.h"

#include <algorithm>
#include <cmath>
#include <optional>

module elf.navigation;

import elf.interaction;
import elf.math;
import elf.scene;

namespace elf3d::navigation {
namespace navigation_detail {

[[nodiscard]] bool finite_vector(const math::Vector3& value) noexcept;
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
[[nodiscard]] math::Vector3 canonical_direction() noexcept;
[[nodiscard]] math::Vector3 direction_from_angles(float yaw, float pitch) noexcept;
[[nodiscard]] CameraBasis basis_from_forward(const math::Vector3& direction) noexcept;
[[nodiscard]] float fit_distance_to_bounds(const Bounds3& bounds, const math::Vector3& center,
                                           const math::Vector3& direction, float vertical_tangent,
                                           float horizontal_tangent) noexcept;
void angles_from_direction(const math::Vector3& direction, float& yaw, float& pitch) noexcept;
void apply_orbit_delta(Float2 delta, const OrbitNavigationSettings& settings, float& yaw,
                       float& pitch) noexcept;
[[nodiscard]] Result<CameraBasis> camera_basis(const scene::Storage& scene, EntityId camera);
[[nodiscard]] Transform look_at_transform(const math::Vector3& position,
                                          const math::Vector3& direction) noexcept;
[[nodiscard]] Result<void> set_camera_world_transform(scene::Storage& scene, EntityId camera,
                                                      const Transform& world_transform);
[[nodiscard]] Result<void> set_camera_world_matrix(scene::Storage& scene, EntityId camera,
                                                   const math::Matrix4& camera_world);
[[nodiscard]] Result<void> validate_camera(const scene::Storage& scene, EntityId camera);
[[nodiscard]] Result<float> aspect_ratio(Extent2D extent) noexcept;
[[nodiscard]] Result<void> update_clip_planes(scene::Storage& scene, EntityId camera,
                                              float distance, float radius);
[[nodiscard]] NavigationInteractionMode
to_navigation_mode(interaction::InteractionMode mode) noexcept;

} // namespace navigation_detail

using namespace navigation_detail;

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
    const math::Vector3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, math::to_float3(direction),
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
    return fit_with_direction(scene, camera, extent, math::to_float3(canonical_direction()),
                              scene.visible_world_bounds(visibility));
}

Result<void> OrbitNavigationController::fit_to_bounds(scene::Storage& scene, EntityId camera,
                                                      Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    const math::Vector3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, math::to_float3(direction), bounds);
}

Result<void> OrbitNavigationController::reset_to_bounds(scene::Storage& scene, EntityId camera,
                                                        Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    return fit_with_direction(scene, camera, extent, math::to_float3(canonical_direction()),
                              bounds);
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

    math::Matrix4 camera_world = math::to_matrix(current_world_result.value());
    const Result<CameraBasis> current_basis = camera_basis(scene, camera);
    if (!current_basis) {
        return current_basis.error();
    }
    const math::Vector3 camera_position{camera_world[3]};
    const math::Vector3 anchor = math::to_vector(screen_anchor_.value());
    math::Vector3 ray = anchor - camera_position;
    const float anchor_distance = glm::length(ray);
    if (!finite_vector(ray) || !std::isfinite(anchor_distance) ||
        anchor_distance <= minimum_axis_length ||
        glm::dot(ray, current_basis.value().forward) <= 0.0F) {
        ray = current_basis.value().forward;
    } else {
        ray = glm::normalize(ray);
    }

    const BoundsInfo bounds = bounds_info(bounds_value);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    const float signed_anchor_distance =
        glm::dot(anchor - camera_position, current_basis.value().forward);
    const float motion_reference = local_motion_reference_distance(signed_anchor_distance, bounds);
    const float step =
        dolly_step_from_multiplier(multiplier, signed_anchor_distance, settings_, motion_reference);
    const math::Vector3 new_position = camera_position + ray * step;

    camera_world[3] = math::Vector4{new_position, 1.0F};
    const Result<void> transform_result = set_camera_world_matrix(scene, camera, camera_world);
    if (!transform_result) {
        return transform_result.error();
    }

    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    angles_from_direction(basis.value().forward, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    math::Vector3 centerline_pivot = math::to_vector(pivot_);
    if (!finite_vector(centerline_pivot)) {
        centerline_pivot =
            basis.value().position + basis.value().forward * sanitized_motion_reference(distance_);
    }
    distance_ = clamp_signed_distance(
        glm::dot(centerline_pivot - basis.value().position, basis.value().forward), limits);
    pivot_ = math::to_float3(basis.value().position + basis.value().forward * distance_);
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

Result<void> OrbitNavigationController::apply_screen_anchor_orbit(
    scene::Storage& scene, EntityId camera, Float2 delta, std::optional<Bounds3> bounds_value) {
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

    const math::Vector3 anchor = math::to_vector(screen_anchor_.value());
    const math::Vector3 camera_to_anchor = anchor - basis.value().position;
    const float anchor_distance = glm::length(camera_to_anchor);
    if (!finite_vector(camera_to_anchor) || !std::isfinite(anchor_distance) ||
        anchor_distance <= minimum_axis_length) {
        screen_anchor_.reset();
        return {};
    }

    float current_yaw = yaw_radians_;
    float current_pitch = pitch_radians_;
    angles_from_direction(basis.value().forward, current_yaw, current_pitch);
    const float yaw_delta = -delta.x * settings_.orbit_sensitivity;
    const float vertical_sign = settings_.invert_vertical_orbit ? 1.0F : -1.0F;
    const float requested_pitch_delta = delta.y * vertical_sign * settings_.orbit_sensitivity;
    const float clamped_pitch =
        std::clamp(current_pitch + requested_pitch_delta, settings_.minimum_pitch_radians,
                   settings_.maximum_pitch_radians);
    const float pitch_delta = clamped_pitch - current_pitch;
    if (yaw_delta == 0.0F && pitch_delta == 0.0F) {
        return {};
    }

    const math::Rotation yaw_rotation = glm::angleAxis(yaw_delta, math::Vector3{0.0F, 1.0F, 0.0F});
    math::Vector3 right_after_yaw = yaw_rotation * basis.value().right;
    if (!finite_vector(right_after_yaw) || glm::length(right_after_yaw) <= minimum_axis_length) {
        right_after_yaw = basis.value().right;
    }
    right_after_yaw = glm::normalize(right_after_yaw);
    const math::Rotation pitch_rotation = glm::angleAxis(pitch_delta, right_after_yaw);
    const math::Rotation rotation = glm::normalize(pitch_rotation * yaw_rotation);

    const math::Vector3 camera_offset = basis.value().position - anchor;
    const math::Vector3 new_position = anchor + rotation * camera_offset;
    const math::Vector3 new_right = glm::normalize(rotation * basis.value().right);
    const math::Vector3 new_up = glm::normalize(rotation * basis.value().up);
    const math::Vector3 new_backward = glm::normalize(rotation * -basis.value().forward);
    if (!finite_vector(new_position) || !finite_vector(new_right) || !finite_vector(new_up) ||
        !finite_vector(new_backward)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Off-axis orbit produced an invalid camera transform"};
    }

    math::Matrix4 camera_world = math::to_matrix(current_world_result.value());
    camera_world[0] = math::Vector4{new_right, 0.0F};
    camera_world[1] = math::Vector4{new_up, 0.0F};
    camera_world[2] = math::Vector4{new_backward, 0.0F};
    camera_world[3] = math::Vector4{new_position, 1.0F};
    const Result<void> transform_result = set_camera_world_matrix(scene, camera, camera_world);
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
    distance_ = std::clamp(anchor_distance, limits.minimum, limits.maximum);
    const math::Vector3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
    pivot_ = math::to_float3(new_basis.value().position + direction * distance_);
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    return {};
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

    const math::Vector3 direction = direction_from_angles(next_yaw, next_pitch);
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
    pivot_ = math::to_float3(new_basis.value().position + new_basis.value().forward * distance_);
    scene_ = scene.id();
    camera_ = camera;
    camera_world_ = camera_world_result.value();
    has_valid_state_ = true;
    screen_anchor_.reset();

    const Result<void> clip_result =
        update_clip_planes(scene, camera, distance_, bounds.radius);
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
    math::Vector3 pivot = preserve_existing_pivot && math::is_finite(pivot_)
                              ? math::to_vector(pivot_)
                              : basis.value().position + basis.value().forward;
    if (!preserve_existing_pivot && bounds.has_bounds) {
        pivot = bounds.center;
    }

    float distance = glm::length(pivot - basis.value().position);
    if (!std::isfinite(distance) || distance <= minimum_axis_length) {
        distance = bounds.has_bounds ? std::max(bounds.radius * 2.0F, settings_.minimum_distance)
                                     : std::max(1.0F, settings_.minimum_distance);
        pivot = basis.value().position + basis.value().forward * distance;
    }
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance = std::clamp(distance, limits.minimum, limits.maximum);

    math::Vector3 direction = pivot - basis.value().position;
    if (!finite_vector(direction) || glm::length(direction) <= minimum_axis_length) {
        direction = basis.value().forward;
    }
    angles_from_direction(direction, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    pivot_ = math::to_float3(pivot);
    distance_ = distance;
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
    const math::Vector3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const math::Vector3 position = math::to_vector(pivot_) - direction * distance_;
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

Result<void> OrbitNavigationController::fit_with_direction(scene::Storage& scene, EntityId camera,
                                                           Extent2D extent, Float3 direction,
                                                           std::optional<Bounds3> bounds_value) {
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
    if (!math::is_finite(direction) ||
        glm::length(math::to_vector(direction)) <= minimum_axis_length) {
        direction = math::to_float3(canonical_direction());
    }

    const Result<Float4x4> old_matrix = scene.local_matrix(camera);
    if (!old_matrix) {
        return old_matrix.error();
    }
    const Result<PerspectiveCameraDescription> old_description = scene.perspective_camera(camera);
    if (!old_description) {
        return old_description.error();
    }

    const float vertical_half_angle = old_description.value().vertical_field_of_view_radians * 0.5F;
    const float vertical_tangent = std::tan(vertical_half_angle);
    const float horizontal_half_angle = std::atan(vertical_tangent * aspect.value());
    const float horizontal_tangent = std::tan(horizontal_half_angle);
    const float limiting_half_angle = std::min(vertical_half_angle, horizontal_half_angle);
    const float sine = std::sin(limiting_half_angle);
    if (!std::isfinite(sine) || sine <= 0.0F || !std::isfinite(vertical_tangent) ||
        vertical_tangent <= 0.0F || !std::isfinite(horizontal_tangent) ||
        horizontal_tangent <= 0.0F) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Camera fitting requires a valid field of view and aspect ratio"};
    }

    angles_from_direction(math::to_vector(direction), yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);

    pivot_ = math::to_float3(bounds.center);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    const float fit_distance = fit_distance_to_bounds(
        *bounds_value, bounds.center, direction_from_angles(yaw_radians_, pitch_radians_),
        vertical_tangent, horizontal_tangent);
    distance_ = std::clamp(fit_distance * fit_margin, limits.minimum, limits.maximum);
    const math::Vector3 fit_direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const math::Vector3 position = math::to_vector(pivot_) - fit_direction * distance_;
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
        const Result<void> restore_matrix = scene.set_local_matrix(camera, old_matrix.value());
        const Result<void> restore_camera =
            scene.set_perspective_camera(camera, old_description.value());
        (void)restore_matrix;
        (void)restore_camera;
        return clip_result.error();
    }
    return {};
}


} // namespace elf3d::navigation
