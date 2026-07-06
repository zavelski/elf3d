module;

#include <elf3d/core/assert.h>
#include <elf3d/math/detail/glm_helpers.h>

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
namespace {

constexpr float pi = 3.14159265358979323846F;
constexpr float half_pi = pi * 0.5F;
constexpr float minimum_axis_length = 0.000001F;
constexpr float matrix_comparison_epsilon = 0.0001F;
constexpr float fit_margin = 1.05F;
constexpr float maximum_pointer_delta_pixels = 10000.0F;
constexpr float wheel_dolly_speed_scale = 0.5F;
constexpr float right_button_pan_speed_scale = 0.5F;
constexpr float keyboard_forward_to_wheel_speed_scale = 0.025F;
constexpr float keyboard_pan_step_width_divisor = 400.0F;
constexpr float keyboard_reference_updates_per_second = 60.0F;
constexpr float maximum_keyboard_frame_delta_seconds = 0.25F;

struct BoundsInfo {
    bool has_bounds = false;
    math::Vector3 center{};
    float radius = 1.0F;
};

struct CameraBasis {
    math::Vector3 position{};
    math::Vector3 right{1.0F, 0.0F, 0.0F};
    math::Vector3 up{0.0F, 1.0F, 0.0F};
    math::Vector3 forward{0.0F, 0.0F, -1.0F};
};

struct PanOffset {
    bool has_value = false;
    math::Vector3 value{};
};

struct KeyboardPanDelta {
    float view_horizontal_pixels = 0.0F;
    float world_vertical_pixels = 0.0F;
};

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
    const Bounds3 bounds = bounds_value.value();
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

struct DistanceLimits {
    float minimum = 0.001F;
    float maximum = 1.0e9F;
};

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

    const Result<Float4x4> parent_world_result = scene.world_matrix(record.value()->parent.value());
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

    const Result<Float4x4> parent_world_result = scene.world_matrix(record.value()->parent.value());
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

} // namespace

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
        bounds_value.value(), bounds.center, direction_from_angles(yaw_radians_, pitch_radians_),
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
