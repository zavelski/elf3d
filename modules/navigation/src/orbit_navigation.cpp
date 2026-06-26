#include <elf3d/navigation/orbit_navigation.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace elf3d::navigation {
namespace {

constexpr float pi = 3.14159265358979323846F;
constexpr float half_pi = pi * 0.5F;
constexpr float minimum_axis_length = 0.000001F;
constexpr float fit_margin = 1.15F;
constexpr float maximum_pointer_delta_pixels = 10000.0F;

struct BoundsInfo {
    bool has_bounds = false;
    math::Vector3 center{};
    float radius = 1.0F;
};

struct CameraBasis {
    math::Vector3 position{};
    math::Vector3 forward{0.0F, 0.0F, -1.0F};
};

[[nodiscard]] bool finite_vector(const math::Vector3 &value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool finite_input(const ViewportInput &input) noexcept {
    return std::isfinite(input.pointer_position_pixels.x) &&
           std::isfinite(input.pointer_position_pixels.y) &&
           std::isfinite(input.pointer_delta_pixels.x) &&
           std::isfinite(input.pointer_delta_pixels.y) && std::isfinite(input.wheel_delta);
}

[[nodiscard]] Float2 sanitized_delta(Float2 delta) noexcept {
    return Float2{std::clamp(delta.x, -maximum_pointer_delta_pixels, maximum_pointer_delta_pixels),
                  std::clamp(delta.y, -maximum_pointer_delta_pixels, maximum_pointer_delta_pixels)};
}

[[nodiscard]] BoundsInfo bounds_info(Bounds3 bounds) noexcept {
    if (!bounds.is_valid || !math::is_finite(bounds.minimum) || !math::is_finite(bounds.maximum) ||
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

[[nodiscard]] bool valid_settings(const OrbitNavigationSettings &settings) noexcept {
    return std::isfinite(settings.orbit_sensitivity) && settings.orbit_sensitivity >= 0.0F &&
           std::isfinite(settings.pan_sensitivity) && settings.pan_sensitivity >= 0.0F &&
           std::isfinite(settings.zoom_sensitivity) && settings.zoom_sensitivity >= 0.0F &&
           std::isfinite(settings.minimum_distance) && settings.minimum_distance > 0.0F &&
           std::isfinite(settings.maximum_distance) &&
           settings.maximum_distance > settings.minimum_distance &&
           std::isfinite(settings.minimum_pitch_radians) &&
           std::isfinite(settings.maximum_pitch_radians) &&
           settings.minimum_pitch_radians < settings.maximum_pitch_radians &&
           settings.minimum_pitch_radians > -half_pi && settings.maximum_pitch_radians < half_pi;
}

struct DistanceLimits {
    float minimum = 0.001F;
    float maximum = 1.0e9F;
};

[[nodiscard]] DistanceLimits effective_distance_limits(const OrbitNavigationSettings &settings,
                                                       const BoundsInfo &bounds) noexcept {
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

[[nodiscard]] math::Vector3 canonical_direction() noexcept {
    return glm::normalize(math::Vector3{-1.0F, -0.75F, -1.0F});
}

[[nodiscard]] math::Vector3 direction_from_angles(float yaw, float pitch) noexcept {
    const float cosine_pitch = std::cos(pitch);
    return glm::normalize(
        math::Vector3{std::sin(yaw) * cosine_pitch, std::sin(pitch), std::cos(yaw) * cosine_pitch});
}

void angles_from_direction(const math::Vector3 &direction, float &yaw, float &pitch) noexcept {
    const math::Vector3 normalized = glm::normalize(direction);
    pitch = std::asin(std::clamp(normalized.y, -1.0F, 1.0F));
    yaw = std::atan2(normalized.x, normalized.z);
}

[[nodiscard]] Result<CameraBasis> camera_basis(const scene::Storage &scene, EntityId camera) {
    const Result<math::Matrix4> camera_world = scene.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<math::Matrix4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }

    const math::Vector3 position{camera_world.value()[3]};
    math::Vector3 right{camera_world.value()[0]};
    math::Vector3 up{camera_world.value()[1]};
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
    return CameraBasis{position, -backward};
}

[[nodiscard]] Transform look_at_transform(const math::Vector3 &position,
                                          const math::Vector3 &direction) noexcept {
    math::Vector3 forward = finite_vector(direction) && glm::length(direction) > minimum_axis_length
                                ? glm::normalize(direction)
                                : canonical_direction();
    math::Vector3 right = glm::cross(forward, math::Vector3{0.0F, 1.0F, 0.0F});
    if (glm::length(right) <= minimum_axis_length) {
        right = glm::cross(forward, math::Vector3{0.0F, 0.0F, 1.0F});
    }
    right = glm::normalize(right);
    const math::Vector3 up = glm::normalize(glm::cross(right, forward));

    math::Matrix4 rotation_matrix{1.0F};
    rotation_matrix[0] = math::Vector4{right, 0.0F};
    rotation_matrix[1] = math::Vector4{up, 0.0F};
    rotation_matrix[2] = math::Vector4{-forward, 0.0F};
    const math::Rotation rotation = glm::normalize(glm::quat_cast(math::Matrix3{rotation_matrix}));
    return Transform{math::to_float3(position), math::to_quaternion(rotation),
                     Float3{1.0F, 1.0F, 1.0F}};
}

[[nodiscard]] Result<void> set_camera_world_transform(scene::Storage &scene, EntityId camera,
                                                      const Transform &world_transform) {
    const Result<const scene::EntityRecord *> record = scene.entity(camera);
    if (!record) {
        return record.error();
    }
    const math::Matrix4 camera_world = math::transform_matrix(world_transform);
    if (!record.value()->parent.has_value()) {
        return scene.set_local_transform(camera, world_transform);
    }

    const Result<math::Matrix4> parent_world = scene.world_matrix(record.value()->parent.value());
    if (!parent_world) {
        return parent_world.error();
    }
    const float determinant = glm::determinant(math::Matrix3{parent_world.value()});
    if (!std::isfinite(determinant) || std::abs(determinant) <= minimum_axis_length) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "The camera parent transform is not invertible"};
    }
    return scene.set_local_matrix(camera, glm::inverse(parent_world.value()) * camera_world);
}

[[nodiscard]] Result<void> validate_camera(const scene::Storage &scene, EntityId camera) {
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

[[nodiscard]] Result<void> update_clip_planes(scene::Storage &scene, EntityId camera,
                                              float distance, float radius) {
    Result<PerspectiveCameraDescription> description = scene.perspective_camera(camera);
    if (!description) {
        return description.error();
    }
    const float useful_radius = std::max(radius, 0.000001F);
    const float minimum_near = std::max(0.00001F, useful_radius * 1.0e-6F);
    const float minimum_range = std::max(0.0001F, useful_radius * 0.001F);
    float near_plane = std::max(minimum_near, distance - useful_radius * 2.0F);
    float far_plane = std::max(near_plane + minimum_range, distance + useful_radius * 3.0F);
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

Result<NavigationUpdate> OrbitNavigationController::update(scene::Storage &scene, EntityId camera,
                                                           Extent2D extent,
                                                           const ViewportInput &input,
                                                           float click_drag_threshold_pixels) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return update(scene, camera, extent, input, click_drag_threshold_pixels, visibility.value());
}

Result<NavigationUpdate>
OrbitNavigationController::update(scene::Storage &scene, EntityId camera, Extent2D extent,
                                  const ViewportInput &input,
                                  float click_drag_threshold_pixels,
                                  const scene::VisibilityFilter &visibility) {
    if (!finite_input(input)) {
        cancel_interaction();
        return Error{ErrorCode::invalid_viewport_input,
                     "Viewport navigation input must contain finite pointer and wheel values"};
    }
    if (!enabled_) {
        cancel_interaction();
        return NavigationUpdate{};
    }

    const interaction::ViewportInteractionFrame interaction =
        interaction_.update(input, click_drag_threshold_pixels);
    NavigationUpdate update_result;
    if (interaction.click_released) {
        update_result.click_position_pixels = interaction.click_position_pixels;
    }
    if (!input.is_focused) {
        return update_result;
    }

    const Result<void> sync = ensure_synchronized(scene, camera);
    if (!sync) {
        return sync.error();
    }

    bool changed = false;
    const BoundsInfo bounds = bounds_info(scene.visible_world_bounds(visibility));
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    if (input.is_hovered && input.wheel_delta != 0.0F) {
        const float multiplier = std::exp(-input.wheel_delta * settings_.zoom_sensitivity);
        if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport wheel input produced an invalid zoom multiplier"};
        }
        distance_ = std::clamp(distance_ * multiplier, limits.minimum, limits.maximum);
        changed = true;
    }

    const Float2 delta = sanitized_delta(interaction.pointer_delta_pixels);
    if (interaction.drag_active && interaction.mode == NavigationInteractionMode::orbit &&
        (delta.x != 0.0F || delta.y != 0.0F)) {
        yaw_radians_ -= delta.x * settings_.orbit_sensitivity;
        const float vertical_sign = settings_.invert_vertical_orbit ? 1.0F : -1.0F;
        pitch_radians_ += delta.y * vertical_sign * settings_.orbit_sensitivity;
        pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                    settings_.maximum_pitch_radians);
        yaw_radians_ = std::remainder(yaw_radians_, pi * 2.0F);
        changed = true;
    } else if (interaction.drag_active && interaction.mode == NavigationInteractionMode::pan &&
               (delta.x != 0.0F || delta.y != 0.0F)) {
        if (extent.height != 0) {
            const Result<PerspectiveCameraDescription> camera_description =
                scene.perspective_camera(camera);
            if (!camera_description) {
                return camera_description.error();
            }
            const math::Vector3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
            math::Vector3 right =
                glm::normalize(glm::cross(direction, math::Vector3{0.0F, 1.0F, 0.0F}));
            if (!finite_vector(right) || glm::length(right) <= minimum_axis_length) {
                right = math::Vector3{1.0F, 0.0F, 0.0F};
            }
            const math::Vector3 up = glm::normalize(glm::cross(right, direction));
            const float visible_height =
                2.0F * distance_ *
                std::tan(camera_description.value().vertical_field_of_view_radians * 0.5F);
            const float world_units_per_pixel = visible_height / static_cast<float>(extent.height);
            if (std::isfinite(world_units_per_pixel) && world_units_per_pixel > 0.0F) {
                const math::Vector3 offset = (-right * delta.x + up * delta.y) *
                                             world_units_per_pixel * settings_.pan_sensitivity;
                pivot_ = math::to_float3(math::to_vector(pivot_) + offset);
                changed = true;
            }
        }
    } else if (interaction.drag_active && interaction.mode == NavigationInteractionMode::zoom &&
               delta.y != 0.0F) {
        const float multiplier = std::exp(delta.y * settings_.zoom_sensitivity * 0.03F);
        if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
            return Error{ErrorCode::invalid_viewport_input,
                         "Viewport drag input produced an invalid zoom multiplier"};
        }
        distance_ = std::clamp(distance_ * multiplier, limits.minimum, limits.maximum);
        changed = true;
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

Result<void> OrbitNavigationController::set_pivot(scene::Storage &scene, EntityId camera,
                                                  Float3 world_position) {
    if (!math::is_finite(world_position)) {
        return Error{ErrorCode::invalid_argument,
                     "Navigation pivot requires a finite world-space position"};
    }

    const Result<void> sync = ensure_synchronized(scene, camera);
    if (!sync) {
        return sync.error();
    }
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    if (!basis) {
        return basis.error();
    }

    math::Vector3 pivot = math::to_vector(world_position);
    math::Vector3 direction = pivot - basis.value().position;
    float distance = glm::length(direction);
    if (!finite_vector(direction) || !std::isfinite(distance) ||
        distance <= minimum_axis_length) {
        direction = basis.value().forward;
        distance = settings_.minimum_distance;
        pivot = basis.value().position + direction * distance;
    }

    angles_from_direction(direction, yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);

    const BoundsInfo bounds = bounds_info(scene.world_bounds());
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = std::clamp(distance, limits.minimum, limits.maximum);
    pivot_ = math::to_float3(pivot);
    scene_ = scene.id();
    camera_ = camera;
    scene_revision_ = scene.revision();
    has_valid_state_ = true;
    return {};
}

Result<void> OrbitNavigationController::fit_to_scene(scene::Storage &scene, EntityId camera,
                                                     Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return fit_to_scene(scene, camera, extent, visibility.value());
}

Result<void> OrbitNavigationController::fit_to_scene(scene::Storage &scene, EntityId camera,
                                                     Extent2D extent,
                                                     const scene::VisibilityFilter &visibility) {
    cancel_interaction();
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    const math::Vector3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, math::to_float3(direction),
                              scene.visible_world_bounds(visibility));
}

Result<void> OrbitNavigationController::reset_view(scene::Storage &scene, EntityId camera,
                                                   Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return reset_view(scene, camera, extent, visibility.value());
}

Result<void> OrbitNavigationController::reset_view(scene::Storage &scene, EntityId camera,
                                                   Extent2D extent,
                                                   const scene::VisibilityFilter &visibility) {
    cancel_interaction();
    return fit_with_direction(scene, camera, extent, math::to_float3(canonical_direction()),
                              scene.visible_world_bounds(visibility));
}

Result<void> OrbitNavigationController::fit_to_bounds(scene::Storage &scene, EntityId camera,
                                                      Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    const Result<CameraBasis> basis = camera_basis(scene, camera);
    const math::Vector3 direction = basis ? basis.value().forward : canonical_direction();
    return fit_with_direction(scene, camera, extent, math::to_float3(direction), bounds);
}

Result<void> OrbitNavigationController::reset_to_bounds(scene::Storage &scene, EntityId camera,
                                                        Extent2D extent, Bounds3 bounds) {
    cancel_interaction();
    return fit_with_direction(scene, camera, extent, math::to_float3(canonical_direction()),
                              bounds);
}

Result<void> OrbitNavigationController::synchronize(const scene::Storage &scene, EntityId camera) {
    return synchronize_from_camera(scene, camera, true);
}

void OrbitNavigationController::cancel_interaction() noexcept {
    interaction_.cancel();
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

Result<void>
OrbitNavigationController::set_settings(const OrbitNavigationSettings &settings) noexcept {
    if (!valid_settings(settings)) {
        return Error{
            ErrorCode::invalid_navigation_settings,
            "Orbit navigation settings require finite, ordered distances and pitch limits"};
    }
    settings_ = settings;
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);
    distance_ = std::clamp(distance_, settings_.minimum_distance, settings_.maximum_distance);
    return {};
}

OrbitNavigationSettings OrbitNavigationController::settings() const noexcept {
    return settings_;
}

NavigationSnapshot OrbitNavigationController::snapshot() const noexcept {
    const NavigationInteractionMode mode = interaction_.mode();
    return NavigationSnapshot{pivot_,
                              distance_,
                              yaw_radians_,
                              pitch_radians_,
                              mode == NavigationInteractionMode::orbit,
                              mode == NavigationInteractionMode::pan,
                              interaction_.pointer_captured(),
                              has_valid_state_,
                              mode};
}

Result<void> OrbitNavigationController::ensure_synchronized(const scene::Storage &scene,
                                                            EntityId camera) {
    if (!has_valid_state_ || scene.id() != scene_ || camera != camera_ ||
        scene.revision() != scene_revision_) {
        const bool preserve_existing_pivot =
            has_valid_state_ && scene.id() == scene_ && camera == camera_;
        return synchronize_from_camera(scene, camera, preserve_existing_pivot);
    }
    return {};
}

Result<void> OrbitNavigationController::synchronize_from_camera(const scene::Storage &scene,
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
    scene_revision_ = scene.revision();
    has_valid_state_ = true;
    return {};
}

Result<void> OrbitNavigationController::apply_camera(scene::Storage &scene, EntityId camera) {
    const BoundsInfo bounds = bounds_info(scene.world_bounds());
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = std::clamp(distance_, limits.minimum, limits.maximum);
    const math::Vector3 direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const math::Vector3 position = math::to_vector(pivot_) - direction * distance_;
    const Transform transform = look_at_transform(position, direction);
    const Result<void> transform_result = set_camera_world_transform(scene, camera, transform);
    if (!transform_result) {
        return transform_result.error();
    }
    scene_ = scene.id();
    camera_ = camera;
    scene_revision_ = scene.revision();
    has_valid_state_ = true;
    return {};
}

Result<void> OrbitNavigationController::fit_with_direction(scene::Storage &scene, EntityId camera,
                                                           Extent2D extent, Float3 direction,
                                                           Bounds3 bounds_value) {
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

    const Result<math::Matrix4> old_matrix = scene.local_matrix(camera);
    if (!old_matrix) {
        return old_matrix.error();
    }
    const Result<PerspectiveCameraDescription> old_description = scene.perspective_camera(camera);
    if (!old_description) {
        return old_description.error();
    }

    const float vertical_half_angle = old_description.value().vertical_field_of_view_radians * 0.5F;
    const float horizontal_half_angle = std::atan(std::tan(vertical_half_angle) * aspect.value());
    const float limiting_half_angle = std::min(vertical_half_angle, horizontal_half_angle);
    const float sine = std::sin(limiting_half_angle);
    if (!std::isfinite(sine) || sine <= 0.0F) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Camera fitting requires a valid field of view and aspect ratio"};
    }

    angles_from_direction(math::to_vector(direction), yaw_radians_, pitch_radians_);
    pitch_radians_ = std::clamp(pitch_radians_, settings_.minimum_pitch_radians,
                                settings_.maximum_pitch_radians);

    pivot_ = math::to_float3(bounds.center);
    const DistanceLimits limits = effective_distance_limits(settings_, bounds);
    distance_ = std::clamp(bounds.radius / sine * fit_margin, limits.minimum, limits.maximum);
    const math::Vector3 fit_direction = direction_from_angles(yaw_radians_, pitch_radians_);
    const math::Vector3 position = math::to_vector(pivot_) - fit_direction * distance_;
    const Transform transform = look_at_transform(position, fit_direction);
    const Result<void> transform_result = set_camera_world_transform(scene, camera, transform);
    if (!transform_result) {
        return transform_result.error();
    }
    scene_ = scene.id();
    camera_ = camera;
    scene_revision_ = scene.revision();
    has_valid_state_ = true;
    const Result<void> clip_result = update_clip_planes(scene, camera, distance_, bounds.radius);
    if (!clip_result) {
        const Result<void> restore_matrix = scene.set_local_matrix(camera, old_matrix.value());
        const Result<void> restore_camera =
            scene.set_perspective_camera(camera, old_description.value());
        (void)restore_matrix;
        (void)restore_camera;
        return clip_result.error();
    }
    scene_revision_ = scene.revision();
    return {};
}

} // namespace elf3d::navigation
