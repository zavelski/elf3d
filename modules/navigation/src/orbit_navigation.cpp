module;

#include <elf3d/core/assert.h>
#include <elf3d/math/detail/glm_helpers.h>
#include "orbit_navigation_detail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

module elf.navigation;

import elf.math;
import elf.scene;

namespace elf3d::navigation {
namespace navigation_detail {

[[nodiscard]] bool finite_vector(const math::Vector3& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool nearly_equal_matrix(const Float4x4& left, const Float4x4& right) noexcept {
    for (std::size_t index = 0; index < left.elements.size(); ++index) {
        if (!std::isfinite(left.elements[index]) || !std::isfinite(right.elements[index]) ||
            std::abs(left.elements[index] - right.elements[index]) > matrix_comparison_epsilon) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool finite_input(const ViewportInput& input) noexcept {
    return std::isfinite(input.pointer_position_pixels.x) &&
           std::isfinite(input.pointer_position_pixels.y) &&
           std::isfinite(input.pointer_delta_pixels.x) &&
           std::isfinite(input.pointer_delta_pixels.y) &&
           std::isfinite(input.frame_delta_seconds) && input.frame_delta_seconds >= 0.0F &&
           std::isfinite(input.wheel_delta);
}

[[nodiscard]] interaction::PointerInputSnapshot
interaction_input(const ViewportInput& input) noexcept {
    return interaction::PointerInputSnapshot{input.pointer_position_pixels,
                                             input.pointer_delta_pixels,
                                             input.is_hovered,
                                             input.is_focused,
                                             input.left_button_down,
                                             input.middle_button_down,
                                             input.right_button_down,
                                             input.x_down,
                                             input.z_down};
}

[[nodiscard]] NavigationInteractionMode
to_navigation_mode(interaction::InteractionMode mode) noexcept {
    switch (mode) {
    case interaction::InteractionMode::orbit:
        return NavigationInteractionMode::orbit;
    case interaction::InteractionMode::pan:
        return NavigationInteractionMode::pan;
    case interaction::InteractionMode::zoom:
        return NavigationInteractionMode::zoom;
    case interaction::InteractionMode::none:
        return NavigationInteractionMode::none;
    }
    return NavigationInteractionMode::none;
}

[[nodiscard]] Float2 sanitized_delta(Float2 delta) noexcept {
    return Float2{std::clamp(delta.x, -maximum_pointer_delta_pixels, maximum_pointer_delta_pixels),
                  std::clamp(delta.y, -maximum_pointer_delta_pixels, maximum_pointer_delta_pixels)};
}

[[nodiscard]] float scaled_dolly_multiplier(float multiplier, float speed_scale) noexcept {
    return 1.0F + (multiplier - 1.0F) * speed_scale;
}

[[nodiscard]] float keyboard_time_scale(const ViewportInput& input) noexcept {
    const float frame_delta =
        std::clamp(input.frame_delta_seconds, 0.0F, maximum_keyboard_frame_delta_seconds);
    return frame_delta * keyboard_reference_updates_per_second;
}

[[nodiscard]] std::array<math::Vector3, 8> bounds_corners(const Bounds3& bounds) noexcept {
    return {{
        {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
    }};
}

[[nodiscard]] float keyboard_forward_delta(const ViewportInput& input) noexcept {
    float delta = 0.0F;
    if (input.w_pressed) {
        delta += 1.0F;
    }
    if (input.s_pressed) {
        delta -= 1.0F;
    }
    return delta;
}

[[nodiscard]] KeyboardPanDelta keyboard_pan_delta_pixels(const ViewportInput& input,
                                                         Extent2D extent) noexcept {
    if (extent.width == 0) {
        return {};
    }
    const float keyboard_pan_step_pixels = static_cast<float>(extent.width) /
                                           keyboard_pan_step_width_divisor *
                                           keyboard_time_scale(input);
    KeyboardPanDelta delta{};
    if (input.a_pressed) {
        delta.view_horizontal_pixels += keyboard_pan_step_pixels;
    }
    if (input.d_pressed) {
        delta.view_horizontal_pixels -= keyboard_pan_step_pixels;
    }
    if (input.q_pressed) {
        delta.world_vertical_pixels -= keyboard_pan_step_pixels;
    }
    if (input.e_pressed) {
        delta.world_vertical_pixels += keyboard_pan_step_pixels;
    }
    return delta;
}

[[nodiscard]] BoundsInfo bounds_info(std::optional<Bounds3> bounds_value) noexcept {
    if (!bounds_value.has_value()) {
        return {};
    }
    const Bounds3 bounds = *bounds_value;
    if (!math::is_finite(bounds.minimum) || !math::is_finite(bounds.maximum) ||
        bounds.minimum.x > bounds.maximum.x || bounds.minimum.y > bounds.maximum.y ||
        bounds.minimum.z > bounds.maximum.z) {
        return {};
    }

    const math::Vector3 minimum = math::to_vector(bounds.minimum);
    const math::Vector3 maximum = math::to_vector(bounds.maximum);
    const math::Vector3 center = (minimum + maximum) * 0.5F;
    const math::Vector3 half_extent = (maximum - minimum) * 0.5F;
    float radius = glm::length(half_extent);
    if (!finite_vector(center) || !std::isfinite(radius)) {
        return {};
    }
    radius = std::max(radius, 0.000001F);
    return BoundsInfo{true, center, radius};
}

[[nodiscard]] bool valid_settings(const OrbitNavigationSettings& settings) noexcept {
    return std::isfinite(settings.orbit_sensitivity) && settings.orbit_sensitivity >= 0.0F &&
           std::isfinite(settings.pan_sensitivity) && settings.pan_sensitivity >= 0.0F &&
           std::isfinite(settings.zoom_sensitivity) && settings.zoom_sensitivity >= 0.0F &&
           std::isfinite(settings.minimum_distance) && settings.minimum_distance > 0.0F &&
           std::isfinite(settings.maximum_distance) &&
           settings.maximum_distance > settings.minimum_distance &&
           std::isfinite(settings.minimum_motion_scale) && settings.minimum_motion_scale > 0.0F &&
           std::isfinite(settings.minimum_pitch_radians) &&
           std::isfinite(settings.maximum_pitch_radians) &&
           settings.minimum_pitch_radians < settings.maximum_pitch_radians &&
           settings.minimum_pitch_radians > -half_pi && settings.maximum_pitch_radians < half_pi;
}

[[nodiscard]] DistanceLimits effective_distance_limits(const OrbitNavigationSettings& settings,
                                                       const BoundsInfo& bounds) noexcept {
    DistanceLimits limits{settings.minimum_distance, settings.maximum_distance};
    if (bounds.has_bounds) {
        limits.minimum = std::max(limits.minimum, bounds.radius * 1.0e-5F);
        limits.maximum = std::min(limits.maximum, bounds.radius * 1.0e5F);
    }
    if (!std::isfinite(limits.minimum) || limits.minimum <= 0.0F) {
        limits.minimum = 0.001F;
    }
    if (!std::isfinite(limits.maximum) || limits.maximum <= limits.minimum) {
        limits.maximum = limits.minimum * 1000.0F;
    }
    if (!std::isfinite(limits.maximum) || limits.maximum <= limits.minimum) {
        limits.maximum = std::numeric_limits<float>::max();
    }
    return limits;
}

[[nodiscard]] float sanitized_motion_reference(float distance) noexcept {
    const float absolute_distance = std::abs(distance);
    return std::isfinite(absolute_distance) && absolute_distance > minimum_axis_length
               ? absolute_distance
               : 1.0F;
}

[[nodiscard]] float minimum_motion_distance(const OrbitNavigationSettings& settings,
                                            float motion_reference_distance) noexcept {
    const float scene_scale = sanitized_motion_reference(motion_reference_distance);
    const float minimum_distance =
        std::max(settings.minimum_distance, scene_scale * settings.minimum_motion_scale);
    return std::isfinite(minimum_distance) && minimum_distance > 0.0F ? minimum_distance : 0.001F;
}

[[nodiscard]] float local_motion_reference_distance(float distance,
                                                    const BoundsInfo& bounds) noexcept {
    const float local_distance = sanitized_motion_reference(distance);
    if (!bounds.has_bounds) {
        return local_distance;
    }
    return std::max(local_distance, sanitized_motion_reference(bounds.radius));
}

[[nodiscard]] float dolly_step_from_multiplier(float multiplier, float signed_distance,
                                               const OrbitNavigationSettings& settings,
                                               float motion_reference_distance) noexcept {
    if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
        return 0.0F;
    }
    const float factor = std::abs(1.0F - multiplier);
    if (!std::isfinite(factor) || factor == 0.0F) {
        return 0.0F;
    }
    const float distance_scale = std::max(
        std::abs(signed_distance), minimum_motion_distance(settings, motion_reference_distance));
    const float direction = multiplier < 1.0F ? 1.0F : -1.0F;
    return direction * distance_scale * factor;
}

[[nodiscard]] float clamp_signed_distance(float distance, DistanceLimits limits) noexcept {
    if (!std::isfinite(distance)) {
        return limits.minimum;
    }
    if (std::abs(distance) > limits.maximum) {
        return std::copysign(limits.maximum, distance);
    }
    return distance;
}

[[nodiscard]] math::Vector3 canonical_direction() noexcept {
    return glm::normalize(math::Vector3{-1.0F, -0.75F, -1.0F});
}

[[nodiscard]] math::Vector3 direction_from_angles(float yaw, float pitch) noexcept {
    const float cosine_pitch = std::cos(pitch);
    return glm::normalize(
        math::Vector3{std::sin(yaw) * cosine_pitch, std::sin(pitch), std::cos(yaw) * cosine_pitch});
}

[[nodiscard]] CameraBasis basis_from_forward(const math::Vector3& direction) noexcept {
    const math::Vector3 forward =
        finite_vector(direction) && glm::length(direction) > minimum_axis_length
            ? glm::normalize(direction)
            : canonical_direction();
    math::Vector3 right = glm::cross(forward, math::Vector3{0.0F, 1.0F, 0.0F});
    if (glm::length(right) <= minimum_axis_length) {
        right = glm::cross(forward, math::Vector3{0.0F, 0.0F, 1.0F});
    }
    right = glm::normalize(right);
    return CameraBasis{{}, right, glm::normalize(glm::cross(right, forward)), forward};
}

[[nodiscard]] float fit_distance_to_bounds(const Bounds3& bounds, const math::Vector3& center,
                                           const math::Vector3& direction, float vertical_tangent,
                                           float horizontal_tangent) noexcept {
    if (!std::isfinite(vertical_tangent) || !std::isfinite(horizontal_tangent) ||
        vertical_tangent <= 0.0F || horizontal_tangent <= 0.0F) {
        return 0.0F;
    }

    const CameraBasis basis = basis_from_forward(direction);
    float distance = minimum_axis_length;
    for (const math::Vector3 corner : bounds_corners(bounds)) {
        const math::Vector3 offset = corner - center;
        const float forward_offset = glm::dot(offset, basis.forward);
        const float horizontal_distance =
            std::abs(glm::dot(offset, basis.right)) / horizontal_tangent - forward_offset;
        const float vertical_distance =
            std::abs(glm::dot(offset, basis.up)) / vertical_tangent - forward_offset;
        distance = std::max({distance, horizontal_distance, vertical_distance});
    }
    return std::isfinite(distance) ? distance : 0.0F;
}

void angles_from_direction(const math::Vector3& direction, float& yaw, float& pitch) noexcept {
    const math::Vector3 normalized = glm::normalize(direction);
    pitch = std::asin(std::clamp(normalized.y, -1.0F, 1.0F));
    yaw = std::atan2(normalized.x, normalized.z);
}

void apply_orbit_delta(Float2 delta, const OrbitNavigationSettings& settings, float& yaw,
                       float& pitch) noexcept {
    yaw -= delta.x * settings.orbit_sensitivity;
    const float vertical_sign = settings.invert_vertical_orbit ? 1.0F : -1.0F;
    pitch += delta.y * vertical_sign * settings.orbit_sensitivity;
    pitch = std::clamp(pitch, settings.minimum_pitch_radians, settings.maximum_pitch_radians);
    yaw = std::remainder(yaw, pi * 2.0F);
}

[[nodiscard]] Result<PanOffset> pan_offset(const scene::Storage& scene, EntityId camera,
                                           Extent2D extent, Float2 delta,
                                           const OrbitNavigationSettings& settings, float distance,
                                           float motion_reference_distance, float yaw_radians,
                                           float pitch_radians, float speed_scale) {
    if (extent.height == 0 || (delta.x == 0.0F && delta.y == 0.0F)) {
        return PanOffset{};
    }

    const auto camera_description = scene.perspective_camera(camera);
    if (!camera_description) {
        return camera_description.error();
    }

    const math::Vector3 direction = direction_from_angles(yaw_radians, pitch_radians);
    math::Vector3 right = glm::normalize(glm::cross(direction, math::Vector3{0.0F, 1.0F, 0.0F}));
    if (!finite_vector(right) || glm::length(right) <= minimum_axis_length) {
        right = math::Vector3{1.0F, 0.0F, 0.0F};
    }
    const math::Vector3 up = glm::normalize(glm::cross(right, direction));
    const float visible_height =
        2.0F *
        std::max(std::abs(distance), minimum_motion_distance(settings, motion_reference_distance)) *
        std::tan(camera_description.value().vertical_field_of_view_radians * 0.5F);
    const float world_units_per_pixel = visible_height / static_cast<float>(extent.height);
    if (std::isfinite(world_units_per_pixel) && world_units_per_pixel > 0.0F &&
        std::isfinite(speed_scale) && speed_scale > 0.0F) {
        const math::Vector3 offset = (-right * delta.x + up * delta.y) * world_units_per_pixel *
                                     settings.pan_sensitivity * speed_scale;
        return PanOffset{true, offset};
    }
    return PanOffset{};
}

[[nodiscard]] Result<PanOffset>
world_vertical_pan_offset(const scene::Storage& scene, EntityId camera, Extent2D extent,
                          float delta_pixels, const OrbitNavigationSettings& settings,
                          float distance, float motion_reference_distance) {
    if (extent.height == 0 || delta_pixels == 0.0F) {
        return PanOffset{};
    }

    const auto camera_description = scene.perspective_camera(camera);
    if (!camera_description) {
        return camera_description.error();
    }

    const float visible_height =
        2.0F *
        std::max(std::abs(distance), minimum_motion_distance(settings, motion_reference_distance)) *
        std::tan(camera_description.value().vertical_field_of_view_radians * 0.5F);
    const float world_units_per_pixel = visible_height / static_cast<float>(extent.height);
    if (std::isfinite(world_units_per_pixel) && world_units_per_pixel > 0.0F) {
        const math::Vector3 offset{
            0.0F, delta_pixels * world_units_per_pixel * settings.pan_sensitivity, 0.0F};
        return PanOffset{true, offset};
    }
    return PanOffset{};
}

[[nodiscard]] Result<CameraBasis> camera_basis(const scene::Storage& scene, EntityId camera) {
    const Result<Float4x4> camera_world_result = scene.world_matrix(camera);
    if (!camera_world_result) {
        return camera_world_result.error();
    }
    const Result<Float4x4> view = math::camera_view_matrix(camera_world_result.value());
    if (!view) {
        return view.error();
    }

    const math::Matrix4 camera_world = math::to_matrix(camera_world_result.value());
    const math::Vector3 position{camera_world[3]};
    math::Vector3 right{camera_world[0]};
    math::Vector3 up{camera_world[1]};
    if (!finite_vector(position) || !finite_vector(right) || !finite_vector(up) ||
        glm::length(right) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Navigation requires a finite camera world transform"};
    }
    right = glm::normalize(right);
    up -= right * glm::dot(right, up);
    if (glm::length(up) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Navigation requires a non-degenerate camera orientation"};
    }
    up = glm::normalize(up);
    const math::Vector3 backward = glm::normalize(glm::cross(right, up));
    return CameraBasis{position, right, up, -backward};
}

[[nodiscard]] Transform look_at_transform(const math::Vector3& position,
                                          const math::Vector3& direction) noexcept {
    const CameraBasis basis = basis_from_forward(direction);

    math::Matrix4 rotation_matrix{1.0F};
    rotation_matrix[0] = math::Vector4{basis.right, 0.0F};
    rotation_matrix[1] = math::Vector4{basis.up, 0.0F};
    rotation_matrix[2] = math::Vector4{-basis.forward, 0.0F};
    const math::Rotation rotation = glm::normalize(glm::quat_cast(math::Matrix3{rotation_matrix}));
    return Transform{math::to_float3(position), math::to_quaternion(rotation),
                     Float3{1.0F, 1.0F, 1.0F}};
}

[[nodiscard]] Result<void> set_camera_world_transform(scene::Storage& scene, EntityId camera,
                                                      const Transform& world_transform) {
    const Result<const scene::EntityRecord*> record = scene.entity(camera);
    if (!record) {
        return record.error();
    }
    const math::Matrix4 camera_world = math::to_matrix(math::transform_matrix(world_transform));
    if (!record.value()->parent.has_value()) {
        return scene.set_local_transform(camera, world_transform);
    }

    const Result<Float4x4> parent_world_result = scene.world_matrix(*record.value()->parent);
    if (!parent_world_result) {
        return parent_world_result.error();
    }
    const math::Matrix4 parent_world = math::to_matrix(parent_world_result.value());
    const float determinant = glm::determinant(math::Matrix3{parent_world});
    if (!std::isfinite(determinant) || std::abs(determinant) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "The camera parent transform is not invertible"};
    }
    return scene.set_local_matrix(camera,
                                  math::to_float4x4(glm::inverse(parent_world) * camera_world));
}

[[nodiscard]] Result<void> set_camera_world_matrix(scene::Storage& scene, EntityId camera,
                                                   const math::Matrix4& camera_world) {
    const Result<const scene::EntityRecord*> record = scene.entity(camera);
    if (!record) {
        return record.error();
    }
    if (!record.value()->parent.has_value()) {
        return scene.set_local_matrix(camera, math::to_float4x4(camera_world));
    }

    const Result<Float4x4> parent_world_result = scene.world_matrix(*record.value()->parent);
    if (!parent_world_result) {
        return parent_world_result.error();
    }
    const math::Matrix4 parent_world = math::to_matrix(parent_world_result.value());
    const float determinant = glm::determinant(math::Matrix3{parent_world});
    if (!std::isfinite(determinant) || std::abs(determinant) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "The camera parent transform is not invertible"};
    }
    return scene.set_local_matrix(camera,
                                  math::to_float4x4(glm::inverse(parent_world) * camera_world));
}

[[nodiscard]] Result<void> validate_camera(const scene::Storage& scene, EntityId camera) {
    const Result<PerspectiveCameraDescription> description = scene.perspective_camera(camera);
    if (!description) {
        return description.error();
    }
    if (!scene::valid_camera_description(description.value())) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Navigation requires a valid perspective camera configuration"};
    }
    return {};
}

[[nodiscard]] Result<float> aspect_ratio(Extent2D extent) noexcept {
    if (extent.width == 0 || extent.height == 0) {
        return Error{ErrorCode::invalid_viewport_dimensions,
                     "Camera fitting requires a nonzero viewport extent"};
    }
    return static_cast<float>(extent.width) / static_cast<float>(extent.height);
}

[[nodiscard]] Result<void> update_clip_planes(scene::Storage& scene, EntityId camera,
                                              float distance, float radius) {
    Result<PerspectiveCameraDescription> description = scene.perspective_camera(camera);
    if (!description) {
        return description.error();
    }
    const float useful_radius = std::max(radius, 0.000001F);
    const float useful_distance = std::max(std::abs(distance), 0.0F);
    const float minimum_range = std::max(0.0001F, useful_radius * 0.001F);
    float far_plane =
        std::max({minimum_range, useful_radius * 2.0F, useful_distance + useful_radius * 3.0F});
    float near_plane = std::max(0.00001F, far_plane / 10000.0F);
    far_plane = std::max(far_plane, near_plane + minimum_range);
    near_plane = std::max(near_plane, far_plane / 10000.0F);
    if (!std::isfinite(near_plane) || !std::isfinite(far_plane) || near_plane <= 0.0F ||
        far_plane <= near_plane) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Camera fitting produced invalid clipping planes"};
    }
    description.value().near_plane = near_plane;
    description.value().far_plane = far_plane;
    return scene.set_perspective_camera(camera, description.value());
}

} // namespace navigation_detail

using namespace navigation_detail;

Result<NavigationUpdate> OrbitNavigationController::update(scene::Storage& scene, EntityId camera,
                                                           Extent2D extent,
                                                           const ViewportInput& input,
                                                           float click_drag_threshold_pixels) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return update(scene, camera, extent, input, click_drag_threshold_pixels, visibility.value());
}

Result<NavigationUpdate>
OrbitNavigationController::update(scene::Storage& scene, EntityId camera, Extent2D extent,
                                  const ViewportInput& input, float click_drag_threshold_pixels,
                                  const scene::VisibilityFilter& visibility) {
    if (!finite_input(input)) {
        cancel_interaction();
        return Error{
            ErrorCode::invalid_viewport_input,
            "Viewport navigation input must contain finite pointer, frame, and wheel values"};
    }
    if (!enabled_) {
        cancel_interaction();
        return NavigationUpdate{};
    }

    const interaction::ViewportInteractionFrame interaction =
        interaction_.update(interaction_input(input), click_drag_threshold_pixels);
    NavigationUpdate update_result;
    if (interaction.click_released) {
        if (!keyboard_navigation_used_) {
            update_result.click_position_pixels = interaction.click_position_pixels;
        }
        keyboard_navigation_used_ = false;
    }
    const bool has_hover_wheel = input.is_hovered && input.wheel_delta != 0.0F;
    const bool keyboard_navigation_active =
        input.is_focused && (input.left_button_down || input.right_button_down);
    const float keyboard_forward =
        keyboard_navigation_active ? keyboard_forward_delta(input) : 0.0F;
    const KeyboardPanDelta keyboard_pan =
        keyboard_navigation_active ? keyboard_pan_delta_pixels(input, extent) : KeyboardPanDelta{};
    const bool keyboard_pan_active =
        keyboard_pan.view_horizontal_pixels != 0.0F || keyboard_pan.world_vertical_pixels != 0.0F;
    const bool keyboard_translation_active = keyboard_forward != 0.0F || keyboard_pan_active;
    const bool left_keyboard_navigation_started =
        interaction.left_pressed && !interaction.drag_active && keyboard_translation_active &&
        !input.x_down && !input.z_down;
    const bool orbit_drag_started =
        interaction.drag_started && interaction.mode == interaction::InteractionMode::orbit;
    const bool eye_orbit_started = orbit_drag_started && input.space_down;
    if (left_keyboard_navigation_started || orbit_drag_started) {
        screen_anchor_.reset();
    }
    if (eye_orbit_started) {
        eye_orbit_active_ = true;
    } else if (orbit_drag_started) {
        eye_orbit_active_ = false;
        update_result.orbit_start_position_pixels = input.pointer_position_pixels;
    } else if (left_keyboard_navigation_started) {
        update_result.orbit_start_position_pixels = input.pointer_position_pixels;
    }
    if ((interaction.drag_started && interaction.mode != interaction::InteractionMode::orbit) ||
        interaction.drag_ended || !interaction.drag_active ||
        interaction.mode != interaction::InteractionMode::orbit) {
        eye_orbit_active_ = false;
    }
    if (keyboard_translation_active) {
        keyboard_navigation_used_ = true;
    }
    if (!input.is_focused && !has_hover_wheel) {
        keyboard_navigation_used_ = false;
        return update_result;
    }

    const Result<void> sync = ensure_synchronized(scene, camera);
    if (!sync) {
        return sync.error();
    }

    bool changed = false;
    const BoundsInfo bounds = bounds_info(scene.visible_world_bounds(visibility));
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    if (has_hover_wheel) {
        const float base_multiplier = std::exp(-input.wheel_delta * settings_.zoom_sensitivity);
        if (!std::isfinite(base_multiplier) || base_multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport wheel input produced an invalid zoom multiplier"};
        }
        const float multiplier = scaled_dolly_multiplier(base_multiplier, wheel_dolly_speed_scale);
        if (screen_anchor_.has_value()) {
            const Result<void> dolly = apply_screen_anchor_dolly(
                scene, camera, multiplier, scene.visible_world_bounds(visibility));
            if (!dolly) {
                return dolly.error();
            }
        } else {
            const float motion_reference = local_motion_reference_distance(distance_, bounds);
            const float step =
                dolly_step_from_multiplier(multiplier, distance_, settings_, motion_reference);
            distance_ = clamp_signed_distance(distance_ - step, limits);
            changed = true;
        }
    }
    const Float2 delta = sanitized_delta(interaction.pointer_delta_pixels);
    if (interaction.drag_active && interaction.mode == interaction::InteractionMode::pan) {
        screen_anchor_.reset();
    }
    if (interaction.drag_active && interaction.mode == interaction::InteractionMode::orbit &&
        (delta.x != 0.0F || delta.y != 0.0F)) {
        if (eye_orbit_active_) {
            const Result<void> orbit =
                apply_eye_orbit(scene, camera, delta, scene.visible_world_bounds(visibility));
            if (!orbit) {
                return orbit.error();
            }
        } else if (screen_anchor_.has_value()) {
            const Result<void> orbit = apply_screen_anchor_orbit(
                scene, camera, delta, scene.visible_world_bounds(visibility));
            if (!orbit) {
                return orbit.error();
            }
        } else {
            apply_orbit_delta(delta, settings_, yaw_radians_, pitch_radians_);
            changed = true;
        }
    } else if (interaction.drag_active && interaction.mode == interaction::InteractionMode::pan &&
               (delta.x != 0.0F || delta.y != 0.0F)) {
        const float pointer_speed_scale =
            interaction.active_button == interaction::PointerButton::right
                ? right_button_pan_speed_scale
                : 1.0F;
        const float motion_reference = local_motion_reference_distance(distance_, bounds);
        const Result<PanOffset> offset =
            pan_offset(scene, camera, extent, delta, settings_, distance_, motion_reference,
                       yaw_radians_, pitch_radians_, pointer_speed_scale);
        if (!offset) {
            return offset.error();
        }
        if (offset.value().has_value) {
            pivot_ = math::to_float3(math::to_vector(pivot_) + offset.value().value);
            changed = true;
        }
    } else if (interaction.drag_active && interaction.mode == interaction::InteractionMode::zoom &&
               delta.y != 0.0F) {
        const float multiplier = std::exp(delta.y * settings_.zoom_sensitivity * 0.03F);
        if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport drag input produced an invalid zoom multiplier"};
        }
        if (screen_anchor_.has_value()) {
            const Result<void> dolly = apply_screen_anchor_dolly(
                scene, camera, multiplier, scene.visible_world_bounds(visibility));
            if (!dolly) {
                return dolly.error();
            }
        } else {
            const float motion_reference = local_motion_reference_distance(distance_, bounds);
            const float step =
                dolly_step_from_multiplier(multiplier, distance_, settings_, motion_reference);
            distance_ = clamp_signed_distance(distance_ - step, limits);
            changed = true;
        }
    }
    if (keyboard_forward != 0.0F) {
        const float base_multiplier = std::exp(-keyboard_forward * settings_.zoom_sensitivity);
        if (!std::isfinite(base_multiplier) || base_multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport keyboard input produced an invalid movement multiplier"};
        }
        const float reference_multiplier = scaled_dolly_multiplier(
            base_multiplier, wheel_dolly_speed_scale * keyboard_forward_to_wheel_speed_scale);
        const float multiplier = std::pow(reference_multiplier, keyboard_time_scale(input));
        if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport frame time produced an invalid movement multiplier"};
        }
        if (screen_anchor_.has_value()) {
            const Result<void> dolly = apply_screen_anchor_dolly(
                scene, camera, multiplier, scene.visible_world_bounds(visibility));
            if (!dolly) {
                return dolly.error();
            }
        } else {
            const float motion_reference = local_motion_reference_distance(distance_, bounds);
            const float step =
                dolly_step_from_multiplier(multiplier, distance_, settings_, motion_reference);
            distance_ = clamp_signed_distance(distance_ - step, limits);
            changed = true;
        }
    }

    const Float2 sanitized_keyboard_view_pan =
        sanitized_delta({keyboard_pan.view_horizontal_pixels, 0.0F});
    const float sanitized_keyboard_world_vertical_pan =
        std::clamp(keyboard_pan.world_vertical_pixels, -maximum_pointer_delta_pixels,
                   maximum_pointer_delta_pixels);
    if (sanitized_keyboard_view_pan.x != 0.0F || sanitized_keyboard_world_vertical_pan != 0.0F) {
        screen_anchor_.reset();
    }
    if (sanitized_keyboard_view_pan.x != 0.0F) {
        const float motion_reference = local_motion_reference_distance(distance_, bounds);
        const Result<PanOffset> offset =
            pan_offset(scene, camera, extent, sanitized_keyboard_view_pan, settings_, distance_,
                       motion_reference, yaw_radians_, pitch_radians_, 1.0F);
        if (!offset) {
            return offset.error();
        }
        if (offset.value().has_value) {
            pivot_ = math::to_float3(math::to_vector(pivot_) + offset.value().value);
            changed = true;
        }
    }
    if (sanitized_keyboard_world_vertical_pan != 0.0F) {
        const float motion_reference = local_motion_reference_distance(distance_, bounds);
        const Result<PanOffset> offset =
            world_vertical_pan_offset(scene, camera, extent, sanitized_keyboard_world_vertical_pan,
                                      settings_, distance_, motion_reference);
        if (!offset) {
            return offset.error();
        }
        if (offset.value().has_value) {
            pivot_ = math::to_float3(math::to_vector(pivot_) + offset.value().value);
            changed = true;
        }
    }

    if (!changed) {
        return update_result;
    }
    const Result<void> apply_result = apply_camera(scene, camera);
    if (!apply_result) {
        return apply_result.error();
    }
    return update_result;
}

} // namespace elf3d::navigation
