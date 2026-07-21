module;

#include <elf3d/core/result.h>
#include <elf3d/navigation.h>

#include "orbit_navigation_detail.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

module elf.navigation;

import elf.interaction;
import elf.math;
import elf.scene;

namespace elf3d::navigation {
namespace navigation_detail {

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
    if (extent.width == 0U) {
        return {};
    }
    const float step = static_cast<float>(extent.width) / keyboard_pan_step_width_divisor *
                       keyboard_time_scale(input);
    KeyboardPanDelta delta;
    if (input.a_pressed) {
        delta.view_horizontal_pixels += step;
    }
    if (input.d_pressed) {
        delta.view_horizontal_pixels -= step;
    }
    if (input.q_pressed) {
        delta.world_vertical_pixels -= step;
    }
    if (input.e_pressed) {
        delta.world_vertical_pixels += step;
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
    const Float3 minimum = bounds.minimum;
    const Float3 maximum = bounds.maximum;
    const Float3 center = math::scale(math::add(minimum, maximum), 0.5F);
    float radius = math::vector_length(math::scale(math::subtract(maximum, minimum), 0.5F));
    if (!finite_vector(center) || !std::isfinite(radius)) {
        return {};
    }
    radius = std::max(radius, 0.000001F);
    return BoundsInfo{true, center, radius};
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

[[nodiscard]] Float3 direction_from_angles(float yaw, float pitch) noexcept {
    const float cosine = std::cos(pitch);
    return math::normalized(
        Float3{cosine * std::sin(yaw), std::sin(pitch), cosine * std::cos(yaw)});
}

void apply_orbit_delta(Float2 delta, const OrbitNavigationSettings& settings, float& yaw,
                       float& pitch) noexcept {
    yaw -= delta.x * settings.orbit_sensitivity;
    const float vertical_sign = settings.invert_vertical_orbit ? 1.0F : -1.0F;
    pitch += delta.y * vertical_sign * settings.orbit_sensitivity;
    pitch = std::clamp(pitch, settings.minimum_pitch_radians, settings.maximum_pitch_radians);
    yaw = std::remainder(yaw, pi * 2.0F);
}

struct PanScaleRequest final {
    EntityId camera;
    Extent2D extent;
    OrbitNavigationSettings settings;
    float distance = 0.0F;
    float motion_reference_distance = 0.0F;
};

struct PanRequest final {
    PanScaleRequest scale;
    float yaw_radians = 0.0F;
    float pitch_radians = 0.0F;
    float speed_scale = 1.0F;
};

[[nodiscard]] Result<float> world_units_per_pixel(const scene::Storage& scene,
                                                  const PanScaleRequest& request) {
    if (request.extent.height == 0U) {
        return 0.0F;
    }
    const Result<PerspectiveCameraDescription> camera = scene.perspective_camera(request.camera);
    if (!camera) {
        return camera.error();
    }
    const float visible_height =
        2.0F *
        std::max(std::abs(request.distance),
                 minimum_motion_distance(request.settings, request.motion_reference_distance)) *
        std::tan(camera.value().vertical_field_of_view_radians * 0.5F);
    const float scale = visible_height / static_cast<float>(request.extent.height);
    return std::isfinite(scale) && scale > 0.0F ? scale : 0.0F;
}

[[nodiscard]] Result<PanOffset> pan_offset(const scene::Storage& scene, Float2 delta,
                                           const PanRequest& request) {
    if ((delta.x == 0.0F && delta.y == 0.0F) || !std::isfinite(request.speed_scale) ||
        request.speed_scale <= 0.0F) {
        return PanOffset{};
    }
    const Result<float> scale = world_units_per_pixel(scene, request.scale);
    if (!scale) {
        return scale.error();
    }
    if (scale.value() == 0.0F) {
        return PanOffset{};
    }
    const Float3 direction = direction_from_angles(request.yaw_radians, request.pitch_radians);
    Float3 right = math::normalized(math::cross(direction, Float3{0.0F, 1.0F, 0.0F}));
    if (!finite_vector(right) || math::vector_length(right) <= minimum_axis_length) {
        right = Float3{1.0F, 0.0F, 0.0F};
    }
    const Float3 up = math::normalized(math::cross(right, direction));
    const Float3 offset =
        math::scale(math::add(math::scale(right, -delta.x), math::scale(up, delta.y)),
                    scale.value() * request.scale.settings.pan_sensitivity * request.speed_scale);
    return PanOffset{true, offset};
}

[[nodiscard]] Result<PanOffset> world_vertical_pan_offset(const scene::Storage& scene,
                                                          float delta_pixels,
                                                          const PanScaleRequest& request) {
    if (delta_pixels == 0.0F) {
        return PanOffset{};
    }
    const Result<float> scale = world_units_per_pixel(scene, request);
    if (!scale) {
        return scale.error();
    }
    if (scale.value() == 0.0F) {
        return PanOffset{};
    }
    return PanOffset{true,
                     {0.0F, delta_pixels * scale.value() * request.settings.pan_sensitivity, 0.0F}};
}

struct KeyboardMotion final {
    float forward = 0.0F;
    float view_pan = 0.0F;
    float world_vertical_pan = 0.0F;
    bool active = false;
};

[[nodiscard]] KeyboardMotion keyboard_motion(const ViewportInput& input, Extent2D extent) noexcept {
    const bool navigation_active =
        input.is_focused && (input.left_button_down || input.right_button_down);
    const float forward = navigation_active ? keyboard_forward_delta(input) : 0.0F;
    const KeyboardPanDelta pan =
        navigation_active ? keyboard_pan_delta_pixels(input, extent) : KeyboardPanDelta{};
    const Float2 view_pan = sanitized_delta({pan.view_horizontal_pixels, 0.0F});
    const float vertical = std::clamp(pan.world_vertical_pixels, -maximum_pointer_delta_pixels,
                                      maximum_pointer_delta_pixels);
    return KeyboardMotion{forward, view_pan.x, vertical,
                          forward != 0.0F || view_pan.x != 0.0F || vertical != 0.0F};
}

[[nodiscard]] bool
left_keyboard_navigation_started(const interaction::ViewportInteractionFrame& frame,
                                 const ViewportInput& input,
                                 bool keyboard_translation_active) noexcept {
    return frame.left_pressed && !frame.drag_active && keyboard_translation_active &&
           !input.x_down && !input.z_down;
}

[[nodiscard]] bool
orbit_navigation_started(const interaction::ViewportInteractionFrame& frame) noexcept {
    return frame.drag_started && frame.mode == interaction::InteractionMode::orbit;
}

[[nodiscard]] bool
orbit_navigation_ended(const interaction::ViewportInteractionFrame& frame) noexcept {
    return frame.drag_ended || !frame.drag_active ||
           frame.mode != interaction::InteractionMode::orbit;
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
    const NavigationUpdateRequest request{camera, extent, input, click_drag_threshold_pixels};
    return update(scene, request, visibility.value());
}

OrbitNavigationController::UpdateFrame
OrbitNavigationController::make_update_frame(const NavigationUpdateRequest& request) {
    UpdateFrame frame;
    frame.interaction =
        interaction_.update(interaction_input(request.input), request.click_drag_threshold_pixels);
    if (frame.interaction.click_released) {
        if (!keyboard_navigation_used_) {
            frame.result.click_position_pixels = frame.interaction.click_position_pixels;
        }
        keyboard_navigation_used_ = false;
    }
    frame.has_hover_wheel = request.input.is_hovered && request.input.wheel_delta != 0.0F;
    const KeyboardMotion keyboard = keyboard_motion(request.input, request.extent);
    frame.keyboard_forward = keyboard.forward;
    frame.keyboard_view_pan = keyboard.view_pan;
    frame.keyboard_world_vertical_pan = keyboard.world_vertical_pan;
    update_orbit_activation(request, frame, keyboard.active);
    if (keyboard.active) {
        keyboard_navigation_used_ = true;
    }
    frame.stop = !request.input.is_focused && !frame.has_hover_wheel;
    return frame;
}

void OrbitNavigationController::update_orbit_activation(const NavigationUpdateRequest& request,
                                                        UpdateFrame& frame,
                                                        bool keyboard_translation_active) noexcept {
    const bool left_started = left_keyboard_navigation_started(frame.interaction, request.input,
                                                               keyboard_translation_active);
    const bool orbit_started = orbit_navigation_started(frame.interaction);
    if (left_started || orbit_started) {
        screen_anchor_.reset();
    }
    if (orbit_started && request.input.space_down) {
        eye_orbit_active_ = true;
    } else if (orbit_started) {
        eye_orbit_active_ = false;
        frame.result.orbit_start_position_pixels = request.input.pointer_position_pixels;
    } else if (left_started) {
        frame.result.orbit_start_position_pixels = request.input.pointer_position_pixels;
    }
    if (orbit_navigation_ended(frame.interaction)) {
        eye_orbit_active_ = false;
    }
}

Result<bool> OrbitNavigationController::apply_dolly(scene::Storage& scene, EntityId camera,
                                                    float multiplier,
                                                    std::optional<Bounds3> bounds) {
    if (screen_anchor_.has_value()) {
        const Result<void> dolly = apply_screen_anchor_dolly(scene, camera, multiplier, bounds);
        if (!dolly) {
            return dolly.error();
        }
        return false;
    }
    const BoundsInfo info = bounds_info(bounds);
    const DistanceLimits limits = effective_distance_limits(settings_, info);
    const float reference = local_motion_reference_distance(distance_, info);
    const float step = dolly_step_from_multiplier(multiplier, distance_, settings_, reference);
    distance_ = clamp_signed_distance(distance_ - step, limits);
    return true;
}

Result<void> OrbitNavigationController::apply_wheel_navigation(
    scene::Storage& scene, const NavigationUpdateRequest& request, UpdateFrame& frame) {
    if (!frame.has_hover_wheel) {
        return {};
    }
    const float base_multiplier = std::exp(-request.input.wheel_delta * settings_.zoom_sensitivity);
    if (!std::isfinite(base_multiplier) || base_multiplier <= 0.0F) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Viewport wheel input produced an invalid zoom multiplier"};
    }
    const float multiplier = scaled_dolly_multiplier(base_multiplier, wheel_dolly_speed_scale);
    const Result<bool> changed =
        apply_dolly(scene, request.camera, multiplier, frame.visible_bounds);
    if (!changed) {
        return changed.error();
    }
    frame.changed = frame.changed || changed.value();
    return {};
}

Result<void> OrbitNavigationController::apply_pointer_orbit(scene::Storage& scene,
                                                            const NavigationUpdateRequest& request,
                                                            UpdateFrame& frame) {
    if (eye_orbit_active_) {
        return apply_eye_orbit(scene, request.camera, frame.pointer_delta, frame.visible_bounds);
    }
    if (screen_anchor_.has_value()) {
        return apply_screen_anchor_orbit(scene, request.camera, frame.pointer_delta,
                                         frame.visible_bounds);
    }
    apply_orbit_delta(frame.pointer_delta, settings_, yaw_radians_, pitch_radians_);
    frame.changed = true;
    return {};
}

Result<void> OrbitNavigationController::apply_pointer_pan(scene::Storage& scene,
                                                          const NavigationUpdateRequest& request,
                                                          UpdateFrame& frame) {
    const float speed = frame.interaction.active_button == interaction::PointerButton::right
                            ? right_button_pan_speed_scale
                            : 1.0F;
    const BoundsInfo bounds = bounds_info(frame.visible_bounds);
    const PanScaleRequest scale{request.camera, request.extent, settings_, distance_,
                                local_motion_reference_distance(distance_, bounds)};
    const Result<PanOffset> offset = pan_offset(
        scene, frame.pointer_delta, PanRequest{scale, yaw_radians_, pitch_radians_, speed});
    if (!offset) {
        return offset.error();
    }
    if (offset.value().has_value) {
        pivot_ = math::add(pivot_, offset.value().value);
        frame.changed = true;
    }
    return {};
}

Result<void> OrbitNavigationController::apply_pointer_zoom(scene::Storage& scene,
                                                           const NavigationUpdateRequest& request,
                                                           UpdateFrame& frame) {
    const float multiplier = std::exp(frame.pointer_delta.y * settings_.zoom_sensitivity * 0.03F);
    if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Viewport drag input produced an invalid zoom multiplier"};
    }
    const Result<bool> changed =
        apply_dolly(scene, request.camera, multiplier, frame.visible_bounds);
    if (!changed) {
        return changed.error();
    }
    frame.changed = frame.changed || changed.value();
    return {};
}

Result<void> OrbitNavigationController::apply_pointer_navigation(
    scene::Storage& scene, const NavigationUpdateRequest& request, UpdateFrame& frame) {
    if (frame.interaction.drag_active &&
        frame.interaction.mode == interaction::InteractionMode::pan) {
        screen_anchor_.reset();
    }
    if (!frame.interaction.drag_active ||
        (frame.pointer_delta.x == 0.0F && frame.pointer_delta.y == 0.0F)) {
        return {};
    }
    switch (frame.interaction.mode) {
    case interaction::InteractionMode::orbit:
        return apply_pointer_orbit(scene, request, frame);
    case interaction::InteractionMode::pan:
        return apply_pointer_pan(scene, request, frame);
    case interaction::InteractionMode::zoom:
        return apply_pointer_zoom(scene, request, frame);
    case interaction::InteractionMode::none:
        return {};
    }
    return {};
}

Result<void> OrbitNavigationController::apply_keyboard_forward(
    scene::Storage& scene, const NavigationUpdateRequest& request, UpdateFrame& frame) {
    if (frame.keyboard_forward == 0.0F) {
        return {};
    }
    const float base_multiplier = std::exp(-frame.keyboard_forward * settings_.zoom_sensitivity);
    if (!std::isfinite(base_multiplier) || base_multiplier <= 0.0F) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Viewport keyboard input produced an invalid movement multiplier"};
    }
    const float reference_multiplier = scaled_dolly_multiplier(
        base_multiplier, wheel_dolly_speed_scale * keyboard_forward_to_wheel_speed_scale);
    const float multiplier = std::pow(reference_multiplier, keyboard_time_scale(request.input));
    if (!std::isfinite(multiplier) || multiplier <= 0.0F) {
        return Error{ErrorCode::invalid_viewport_input,
                     "Viewport frame time produced an invalid movement multiplier"};
    }
    const Result<bool> changed =
        apply_dolly(scene, request.camera, multiplier, frame.visible_bounds);
    if (!changed) {
        return changed.error();
    }
    frame.changed = frame.changed || changed.value();
    return {};
}

Result<void> OrbitNavigationController::apply_keyboard_pan(scene::Storage& scene,
                                                           const NavigationUpdateRequest& request,
                                                           UpdateFrame& frame) {
    if (frame.keyboard_view_pan != 0.0F || frame.keyboard_world_vertical_pan != 0.0F) {
        screen_anchor_.reset();
    }
    const BoundsInfo bounds = bounds_info(frame.visible_bounds);
    const PanScaleRequest scale{request.camera, request.extent, settings_, distance_,
                                local_motion_reference_distance(distance_, bounds)};
    if (frame.keyboard_view_pan != 0.0F) {
        const Result<PanOffset> offset =
            pan_offset(scene, {frame.keyboard_view_pan, 0.0F},
                       PanRequest{scale, yaw_radians_, pitch_radians_, 1.0F});
        if (!offset) {
            return offset.error();
        }
        if (offset.value().has_value) {
            pivot_ = math::add(pivot_, offset.value().value);
            frame.changed = true;
        }
    }
    if (frame.keyboard_world_vertical_pan != 0.0F) {
        const Result<PanOffset> offset =
            world_vertical_pan_offset(scene, frame.keyboard_world_vertical_pan, scale);
        if (!offset) {
            return offset.error();
        }
        if (offset.value().has_value) {
            pivot_ = math::add(pivot_, offset.value().value);
            frame.changed = true;
        }
    }
    return {};
}

Result<void> OrbitNavigationController::commit_update(scene::Storage& scene, EntityId camera,
                                                      bool changed) {
    return changed ? apply_camera(scene, camera) : Result<void>{};
}

Result<NavigationUpdate>
OrbitNavigationController::update(scene::Storage& scene, const NavigationUpdateRequest& request,
                                  const scene::VisibilityFilter& visibility) {
    if (!finite_input(request.input)) {
        cancel_interaction();
        return Error{
            ErrorCode::invalid_viewport_input,
            "Viewport navigation input must contain finite pointer, frame, and wheel values"};
    }
    if (!enabled_) {
        cancel_interaction();
        return NavigationUpdate{};
    }
    UpdateFrame frame = make_update_frame(request);
    if (frame.stop) {
        keyboard_navigation_used_ = false;
        return frame.result;
    }
    const Result<void> sync = ensure_synchronized(scene, request.camera);
    if (!sync) {
        return sync.error();
    }
    frame.visible_bounds = scene.visible_world_bounds(visibility);
    frame.pointer_delta = sanitized_delta(frame.interaction.pointer_delta_pixels);
    const Result<void> wheel = apply_wheel_navigation(scene, request, frame);
    if (!wheel) {
        return wheel.error();
    }
    const Result<void> pointer = apply_pointer_navigation(scene, request, frame);
    if (!pointer) {
        return pointer.error();
    }
    const Result<void> forward = apply_keyboard_forward(scene, request, frame);
    if (!forward) {
        return forward.error();
    }
    const Result<void> pan = apply_keyboard_pan(scene, request, frame);
    if (!pan) {
        return pan.error();
    }
    const Result<void> commit = commit_update(scene, request.camera, frame.changed);
    if (!commit) {
        return commit.error();
    }
    return frame.result;
}

} // namespace elf3d::navigation
