module;

#include "orbit_navigation_detail.h"
#include <elf3d/core/assert.h>

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

[[nodiscard]] bool nearly_equal_matrix(const Float4x4& left, const Float4x4& right) noexcept {
    for (std::size_t index = 0; index < left.elements.size(); ++index) {
        if (!std::isfinite(left.elements[index]) || !std::isfinite(right.elements[index]) ||
            std::abs(left.elements[index] - right.elements[index]) > matrix_comparison_epsilon) {
            return false;
        }
    }
    return true;
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

[[nodiscard]] std::array<Float3, 8> bounds_corners(const Bounds3& bounds) noexcept {
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

[[nodiscard]] bool valid_sensitivity_settings(const OrbitNavigationSettings& settings) noexcept {
    return std::isfinite(settings.orbit_sensitivity) && settings.orbit_sensitivity >= 0.0F &&
           std::isfinite(settings.pan_sensitivity) && settings.pan_sensitivity >= 0.0F &&
           std::isfinite(settings.zoom_sensitivity) && settings.zoom_sensitivity >= 0.0F;
}

[[nodiscard]] bool valid_distance_settings(const OrbitNavigationSettings& settings) noexcept {
    return std::isfinite(settings.minimum_distance) && settings.minimum_distance > 0.0F &&
           std::isfinite(settings.maximum_distance) &&
           settings.maximum_distance > settings.minimum_distance &&
           std::isfinite(settings.minimum_motion_scale) && settings.minimum_motion_scale > 0.0F;
}

[[nodiscard]] bool valid_pitch_settings(const OrbitNavigationSettings& settings) noexcept {
    return std::isfinite(settings.minimum_pitch_radians) &&
           std::isfinite(settings.maximum_pitch_radians) &&
           settings.minimum_pitch_radians < settings.maximum_pitch_radians &&
           settings.minimum_pitch_radians > -half_pi && settings.maximum_pitch_radians < half_pi;
}

[[nodiscard]] bool valid_settings(const OrbitNavigationSettings& settings) noexcept {
    return valid_sensitivity_settings(settings) && valid_distance_settings(settings) &&
           valid_pitch_settings(settings);
}

[[nodiscard]] Float3 canonical_direction() noexcept {
    return math::normalized(Float3{-1.0F, -0.75F, -1.0F});
}

[[nodiscard]] CameraBasis basis_from_forward(const Float3& direction) noexcept {
    const Float3 forward =
        finite_vector(direction) && math::vector_length(direction) > minimum_axis_length
            ? math::normalized(direction)
            : canonical_direction();
    Float3 right = math::cross(forward, Float3{0.0F, 1.0F, 0.0F});
    if (math::vector_length(right) <= minimum_axis_length) {
        right = math::cross(forward, Float3{0.0F, 0.0F, 1.0F});
    }
    right = math::normalized(right);
    return CameraBasis{{}, right, math::normalized(math::cross(right, forward)), forward};
}

[[nodiscard]] float fit_distance_to_bounds(const Bounds3& bounds, const Float3& center,
                                           const Float3& direction, float vertical_tangent,
                                           float horizontal_tangent) noexcept {
    if (!std::isfinite(vertical_tangent) || !std::isfinite(horizontal_tangent) ||
        vertical_tangent <= 0.0F || horizontal_tangent <= 0.0F) {
        return 0.0F;
    }

    const CameraBasis basis = basis_from_forward(direction);
    float distance = minimum_axis_length;
    for (const Float3 corner : bounds_corners(bounds)) {
        const Float3 offset = math::subtract(corner, center);
        const float forward_offset = math::dot(offset, basis.forward);
        const float horizontal_distance =
            std::abs(math::dot(offset, basis.right)) / horizontal_tangent - forward_offset;
        const float vertical_distance =
            std::abs(math::dot(offset, basis.up)) / vertical_tangent - forward_offset;
        distance = std::max({distance, horizontal_distance, vertical_distance});
    }
    return std::isfinite(distance) ? distance : 0.0F;
}

void angles_from_direction(const Float3& direction, float& yaw, float& pitch) noexcept {
    const Float3 normalized = math::normalized(direction);
    pitch = std::asin(std::clamp(normalized.y, -1.0F, 1.0F));
    yaw = std::atan2(normalized.x, normalized.z);
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

    const Float4x4& camera_world = camera_world_result.value();
    const Float3 position = math::matrix_column(camera_world, 3);
    Float3 right = math::matrix_column(camera_world, 0);
    Float3 up = math::matrix_column(camera_world, 1);
    if (!finite_vector(position) || !finite_vector(right) || !finite_vector(up) ||
        math::vector_length(right) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Navigation requires a finite camera world transform"};
    }
    right = math::normalized(right);
    up = math::subtract(up, math::scale(right, math::dot(right, up)));
    if (math::vector_length(up) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Navigation requires a non-degenerate camera orientation"};
    }
    up = math::normalized(up);
    const Float3 backward = math::normalized(math::cross(right, up));
    return CameraBasis{position, right, up, math::negate(backward)};
}

[[nodiscard]] Transform look_at_transform(const Float3& position,
                                          const Float3& direction) noexcept {
    const CameraBasis basis = basis_from_forward(direction);
    const Quaternion rotation =
        math::rotation_from_basis(basis.right, basis.up, math::negate(basis.forward));
    return Transform{position, rotation, Float3{1.0F, 1.0F, 1.0F}};
}

[[nodiscard]] Result<void> set_camera_world_transform(scene::Storage& scene, EntityId camera,
                                                      const Transform& world_transform) {
    const Result<const scene::EntityRecord*> record = scene.entity(camera);
    if (!record) {
        return record.error();
    }
    const Float4x4 camera_world = math::transform_matrix(world_transform);
    if (!record.value()->parent.has_value()) {
        return scene.set_local_transform(camera, world_transform);
    }

    const Result<Float4x4> parent_world_result = scene.world_matrix(*record.value()->parent);
    if (!parent_world_result) {
        return parent_world_result.error();
    }
    const Result<Float4x4> inverse_parent =
        math::inverse_affine_matrix(parent_world_result.value());
    if (!inverse_parent) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "The camera parent transform is not invertible"};
    }
    return scene.set_local_matrix(camera,
                                  math::compose_world(inverse_parent.value(), camera_world));
}

[[nodiscard]] Result<void> set_camera_world_matrix(scene::Storage& scene, EntityId camera,
                                                   const Float4x4& camera_world) {
    const Result<const scene::EntityRecord*> record = scene.entity(camera);
    if (!record) {
        return record.error();
    }
    if (!record.value()->parent.has_value()) {
        return scene.set_local_matrix(camera, camera_world);
    }

    const Result<Float4x4> parent_world_result = scene.world_matrix(*record.value()->parent);
    if (!parent_world_result) {
        return parent_world_result.error();
    }
    const Result<Float4x4> inverse_parent =
        math::inverse_affine_matrix(parent_world_result.value());
    if (!inverse_parent) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "The camera parent transform is not invertible"};
    }
    return scene.set_local_matrix(camera,
                                  math::compose_world(inverse_parent.value(), camera_world));
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

} // namespace elf3d::navigation
