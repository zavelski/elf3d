#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>
#include <elf3d/navigation.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

import elf.assets;
import elf.math;
import elf.navigation;
import elf.scene;

#include "orbit_navigation_test_support.h"

using namespace elf3d::navigation::test_support;

int elf3d_navigation_continuous_motion_test();
int elf3d_navigation_anchor_depth_test();

int elf3d_navigation_test() {
    constexpr float click_threshold = 4.0F;
    SceneFixture fixture = make_scene(1, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController navigation;
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 1;
    }
    const elf3d::NavigationSnapshot reset = navigation.snapshot();
    const elf3d::Float3 expected_center{1.5F, 1.5F, 1.5F};
    if (!nearly_equal(reset.pivot, expected_center) ||
        !camera_looks_at(fixture.scene, fixture.camera, reset.pivot) ||
        !bounds_visible(fixture.scene, fixture.camera, {800, 600})) {
        return 2;
    }
    const elf3d::Float3 pivot_target{2.0F, 1.0F, 0.0F};
    const elf3d::Float3 position_before_pivot = camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_pivot = camera_forward(fixture.scene, fixture.camera);
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, pivot_target)) {
        return 28;
    }
    const elf3d::NavigationSnapshot anchored = navigation.snapshot();
    if (!nearly_equal(anchored.pivot, reset.pivot) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_pivot) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_pivot) ||
        !(anchored.distance > 0.0F)) {
        return 29;
    }
    elf3d::ViewportInput pivot_input = hovered_input();
    pivot_input.left_button_down = true;
    pivot_input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input, click_threshold));
    pivot_input.pointer_position_pixels = {20.0F, 10.0F};
    pivot_input.pointer_delta_pixels = {10.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input,
                           click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_pivot) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_pivot)) {
        return 30;
    }
    pivot_input.left_button_down = false;
    pivot_input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input, click_threshold));
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 31;
    }
    if (!navigation.fit_to_scene(fixture.scene, fixture.camera, {360, 800}) ||
        !bounds_visible(fixture.scene, fixture.camera, {360, 800})) {
        return 3;
    }
    const float fitted_bounds_extent =
        maximum_projected_bounds_extent(fixture.scene, fixture.camera, {360, 800});
    if (fitted_bounds_extent < 0.85F || fitted_bounds_extent > 1.001F) {
        return 96;
    }

    const elf3d::NavigationSnapshot before_orbit = navigation.snapshot();
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 4;
    }
    input.pointer_position_pixels = {300.0F, 300.0F};
    input.pointer_delta_pixels = {290.0F, 290.0F};
    const elf3d::Result<elf3d::navigation::NavigationUpdate> orbit_start =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!orbit_start || !orbit_start.value().orbit_start_position_pixels.has_value()) {
        return 4;
    }
    const elf3d::NavigationSnapshot first_drag = navigation.snapshot();
    if (!first_drag.is_orbiting || !first_drag.is_pointer_captured ||
        !nearly_equal(first_drag.yaw_radians, before_orbit.yaw_radians) ||
        !nearly_equal(first_drag.pitch_radians, before_orbit.pitch_radians)) {
        return 5;
    }
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {100.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 6;
    }
    const elf3d::NavigationSnapshot yawed = navigation.snapshot();
    if (nearly_equal(yawed.yaw_radians, first_drag.yaw_radians) ||
        !(yawed.yaw_radians < first_drag.yaw_radians) ||
        !nearly_equal(yawed.distance, first_drag.distance) ||
        !camera_looks_at(fixture.scene, fixture.camera, yawed.pivot)) {
        return 7;
    }
    input.pointer_position_pixels = {400.0F, -9700.0F};
    input.pointer_delta_pixels = {0.0F, -10000.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 8;
    }
    const elf3d::NavigationSnapshot pitched = navigation.snapshot();
    if (!(pitched.pitch_radians > yawed.pitch_radians) ||
        pitched.pitch_radians < navigation.settings().minimum_pitch_radians ||
        pitched.pitch_radians > navigation.settings().maximum_pitch_radians) {
        return 9;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (navigation.snapshot().is_pointer_captured) {
        return 10;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 142;
    }
    const elf3d::NavigationSnapshot before_eye_orbit = navigation.snapshot();
    const elf3d::Float3 eye_orbit_position = camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 eye_orbit_forward = camera_forward(fixture.scene, fixture.camera);
    input = hovered_input();
    input.left_button_down = true;
    input.space_down = true;
    input.pointer_position_pixels = {20.0F, 20.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 143;
    }
    input.pointer_position_pixels = {40.0F, 20.0F};
    input.pointer_delta_pixels = {20.0F, 0.0F};
    const elf3d::Result<elf3d::navigation::NavigationUpdate> eye_orbit_start =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!eye_orbit_start || eye_orbit_start.value().orbit_start_position_pixels.has_value() ||
        !navigation.snapshot().is_orbiting || !navigation.snapshot().is_pointer_captured ||
        navigation.has_screen_anchor()) {
        return 144;
    }
    input.space_down = false;
    input.pointer_position_pixels = {100.0F, 20.0F};
    input.pointer_delta_pixels = {60.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 145;
    }
    const elf3d::NavigationSnapshot after_eye_orbit = navigation.snapshot();
    if (!nearly_equal(camera_position(fixture.scene, fixture.camera), eye_orbit_position) ||
        nearly_equal(camera_forward(fixture.scene, fixture.camera), eye_orbit_forward) ||
        nearly_equal(after_eye_orbit.yaw_radians, before_eye_orbit.yaw_radians) ||
        !camera_looks_at(fixture.scene, fixture.camera, after_eye_orbit.pivot) ||
        navigation.has_screen_anchor()) {
        return 146;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 147;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {40.0F, 10.0F};
    input.pointer_delta_pixels = {30.0F, 0.0F};
    const elf3d::Result<elf3d::navigation::NavigationUpdate> normal_orbit_after_eye =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!normal_orbit_after_eye ||
        !normal_orbit_after_eye.value().orbit_start_position_pixels.has_value()) {
        return 148;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 104;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {40.0F, 10.0F};
    input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 105;
    }
    input.right_button_down = true;
    input.pointer_delta_pixels = {0.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 106;
    }
    input.left_button_down = false;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan) {
        return 107;
    }
    const elf3d::Float3 position_before_handoff_pan =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_handoff_pan = camera_forward(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_handoff_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_handoff_pan) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan) {
        return 108;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 109;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 110;
    }
    input = hovered_input();
    input.right_button_down = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan ||
        !navigation.snapshot().is_pointer_captured) {
        return 111;
    }
    input.left_button_down = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan ||
        !navigation.snapshot().is_pointer_captured) {
        return 112;
    }
    input.right_button_down = false;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> orbit_handoff =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!orbit_handoff ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit ||
        !navigation.snapshot().is_pointer_captured ||
        !orbit_handoff.value().orbit_start_position_pixels.has_value()) {
        return 113;
    }
    const float yaw_before_handoff_orbit = navigation.snapshot().yaw_radians;
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(navigation.snapshot().yaw_radians, yaw_before_handoff_orbit) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 114;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 115;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 11;
    }
    const elf3d::NavigationSnapshot before_pan = navigation.snapshot();
    const elf3d::Float3 before_forward = camera_forward(fixture.scene, fixture.camera);
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 12;
    }
    const elf3d::NavigationSnapshot after_pan = navigation.snapshot();
    if (nearly_equal(after_pan.pivot, before_pan.pivot) ||
        !nearly_equal(after_pan.distance, before_pan.distance) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), before_forward)) {
        return 13;
    }
    input.left_button_down = false;
    input.x_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    const float distance_before_zoom = navigation.snapshot().distance;
    input = hovered_input();
    input.left_button_down = true;
    input.z_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {0.0F, -80.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < distance_before_zoom)) {
        return 32;
    }
    input.left_button_down = false;
    input.z_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    const float distance_before_wheel_zoom = navigation.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 14;
    }
    const float distance_after_zoom_in = navigation.snapshot().distance;
    input.wheel_delta = -1.0F;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    const float distance_after_zoom_out = navigation.snapshot().distance;
    input.is_hovered = false;
    input.wheel_delta = 20.0F;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!(distance_after_zoom_in < distance_before_wheel_zoom) ||
        !(distance_after_zoom_out > distance_after_zoom_in) ||
        !nearly_equal(navigation.snapshot().distance, distance_after_zoom_out)) {
        return 15;
    }
    input = hovered_input();
    input.is_focused = false;
    input.wheel_delta = 1.0F;
    const float distance_before_hover_wheel = navigation.snapshot().distance;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < distance_before_hover_wheel)) {
        return 33;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 68;
    }
    const float distance_before_scaled_wheel = navigation.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 69;
    }
    const float scaled_wheel_step = distance_before_scaled_wheel - navigation.snapshot().distance;
    const float full_wheel_step =
        distance_before_scaled_wheel * (1.0F - std::exp(-navigation.settings().zoom_sensitivity));
    if (!nearly_equal(scaled_wheel_step, full_wheel_step * 0.5F)) {
        return 70;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 75;
    }
    const elf3d::NavigationSnapshot before_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 position_before_keyboard_forward =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_keyboard_forward =
        camera_forward(fixture.scene, fixture.camera);
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera,
                                      before_keyboard_forward.pivot)) {
        return 130;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_forward =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_forward || !keyboard_forward.value().orbit_start_position_pixels.has_value() ||
        navigation.has_screen_anchor()) {
        return 76;
    }
    const elf3d::NavigationSnapshot after_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 keyboard_forward_offset =
        subtract(camera_position(fixture.scene, fixture.camera), position_before_keyboard_forward);
    const float keyboard_forward_step =
        before_keyboard_forward.distance - after_keyboard_forward.distance;
    if (!nearly_equal(after_keyboard_forward.pivot, before_keyboard_forward.pivot) ||
        !nearly_equal(keyboard_forward_step, scaled_wheel_step * 0.025F) ||
        !nearly_equal(keyboard_forward_offset,
                      multiply(forward_before_keyboard_forward, keyboard_forward_step))) {
        return 77;
    }
    input.w_pressed = false;
    input.s_pressed = true;
    const elf3d::Float3 position_before_keyboard_backward =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_keyboard_backward =
        camera_forward(fixture.scene, fixture.camera);
    const float distance_before_keyboard_backward = navigation.snapshot().distance;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance > distance_before_keyboard_backward) ||
        !(dot(subtract(camera_position(fixture.scene, fixture.camera),
                       position_before_keyboard_backward),
              forward_before_keyboard_backward) < 0.0F)) {
        return 78;
    }
    input.left_button_down = false;
    input.s_pressed = false;
    const elf3d::Float3 position_before_keyboard_release =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_release =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_release || keyboard_release.value().click_position_pixels.has_value() ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_keyboard_release)) {
        return 86;
    }
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_reentry =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_reentry || !keyboard_reentry.value().orbit_start_position_pixels.has_value()) {
        return 131;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 138;
    }
    const elf3d::NavigationSnapshot before_local_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 local_keyboard_position_before =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 far_anchor = add(
        local_keyboard_position_before, multiply(camera_forward(fixture.scene, fixture.camera),
                                                 before_local_keyboard_forward.distance * 1000.0F));
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, far_anchor)) {
        return 139;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> local_keyboard_forward =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    const float keyboard_base_multiplier = std::exp(-navigation.settings().zoom_sensitivity);
    const float keyboard_reference_multiplier =
        1.0F + (keyboard_base_multiplier - 1.0F) * 0.5F * 0.025F;
    const float expected_local_keyboard_step =
        before_local_keyboard_forward.distance * (1.0F - keyboard_reference_multiplier);
    const float local_keyboard_step =
        before_local_keyboard_forward.distance - navigation.snapshot().distance;
    if (!local_keyboard_forward ||
        !local_keyboard_forward.value().orbit_start_position_pixels.has_value() ||
        navigation.has_screen_anchor() ||
        !nearly_equal(local_keyboard_step, expected_local_keyboard_step, 0.0005F)) {
        return 140;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 99;
    }
    input = hovered_input();
    input.right_button_down = true;
    input.w_pressed = true;
    const elf3d::NavigationSnapshot before_right_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 position_before_right_keyboard_forward =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < before_right_keyboard_forward.distance) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_right_keyboard_forward)) {
        return 100;
    }
    input.w_pressed = false;
    input.a_pressed = true;
    const elf3d::Float3 position_before_right_keyboard_pan =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_right_keyboard_pan)) {
        return 101;
    }
    input.a_pressed = false;
    input.e_pressed = true;
    const elf3d::Float3 position_before_right_keyboard_up =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 102;
    }
    const elf3d::Float3 position_after_right_keyboard_up =
        camera_position(fixture.scene, fixture.camera);
    if (!(position_after_right_keyboard_up.y > position_before_right_keyboard_up.y) ||
        !nearly_equal(position_after_right_keyboard_up.x, position_before_right_keyboard_up.x) ||
        !nearly_equal(position_after_right_keyboard_up.z, position_before_right_keyboard_up.z)) {
        return 103;
    }
    input.right_button_down = false;
    input.e_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 71;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 72;
    }
    const elf3d::Float3 mouse_pan_delta_position = camera_position(fixture.scene, fixture.camera);

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 73;
    }
    input = hovered_input();
    input.right_button_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), mouse_pan_delta_position)) {
        return 74;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    constexpr float keyboard_pan_test_step = 800.0F / 400.0F;
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 79;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {keyboard_pan_test_step, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 80;
    }
    const elf3d::Float3 keyboard_horizontal_pan_delta_position =
        camera_position(fixture.scene, fixture.camera);

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 87;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.a_pressed = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      keyboard_horizontal_pan_delta_position)) {
        return 88;
    }
    input.left_button_down = false;
    input.a_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 81;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.e_pressed = true;
    const elf3d::Float3 before_keyboard_up_pan = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 82;
    }
    const elf3d::Float3 after_keyboard_up_pan = camera_position(fixture.scene, fixture.camera);
    if (!(after_keyboard_up_pan.y > before_keyboard_up_pan.y) ||
        !nearly_equal(after_keyboard_up_pan.x, before_keyboard_up_pan.x) ||
        !nearly_equal(after_keyboard_up_pan.z, before_keyboard_up_pan.z)) {
        return 84;
    }
    input.e_pressed = false;
    input.q_pressed = true;
    const elf3d::Float3 before_keyboard_down_pan = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(camera_position(fixture.scene, fixture.camera).y < before_keyboard_down_pan.y) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera).x,
                      before_keyboard_down_pan.x) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera).z,
                      before_keyboard_down_pan.z)) {
        return 85;
    }
    input.left_button_down = false;
    input.q_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));


    if (const int continuous_motion = elf3d_navigation_continuous_motion_test();
        continuous_motion != 0) {
        return continuous_motion;
    }
    if (const int anchor_depth = elf3d_navigation_anchor_depth_test(); anchor_depth != 0) {
        return anchor_depth;
    }

    return 0;
}
