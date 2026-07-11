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

int elf3d_navigation_continuous_motion_test() {
    constexpr float click_threshold = 4.0F;
    SceneFixture forward_turn_fixture = make_scene(4, {99'000'000.0F, -1'000'000.0F, -1'000'000.0F},
                                                   {101'000'000.0F, 1'000'000.0F, 1'000'000.0F});
    elf3d::navigation::OrbitNavigationController forward_turn_navigation;
    if (!forward_turn_navigation.reset_view(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                            {800, 600})) {
        return 116;
    }
    elf3d::ViewportInput forward_turn_input = hovered_input();
    forward_turn_input.left_button_down = true;
    forward_turn_input.pointer_position_pixels = {10.0F, 10.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 117;
    }
    forward_turn_input.pointer_position_pixels = {80.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {70.0F, 0.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold) ||
        !forward_turn_navigation.set_screen_anchor(forward_turn_fixture.scene,
                                                   forward_turn_fixture.camera,
                                                   forward_turn_navigation.snapshot().pivot)) {
        return 118;
    }
    const elf3d::NavigationSnapshot before_first_forward_turn = forward_turn_navigation.snapshot();
    const elf3d::Float3 fixed_dynamic_center = before_first_forward_turn.pivot;
    forward_turn_input.pointer_position_pixels = {120.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {40.0F, 0.0F};
    forward_turn_input.w_pressed = true;
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 119;
    }
    const elf3d::NavigationSnapshot after_first_forward_turn = forward_turn_navigation.snapshot();
    if (nearly_equal(after_first_forward_turn.yaw_radians, before_first_forward_turn.yaw_radians) ||
        !(after_first_forward_turn.distance < before_first_forward_turn.distance) ||
        length(subtract(after_first_forward_turn.pivot, fixed_dynamic_center)) > 32.0F ||
        !forward_turn_navigation.has_screen_anchor() ||
        !camera_looks_at(forward_turn_fixture.scene, forward_turn_fixture.camera,
                         fixed_dynamic_center)) {
        return 120;
    }

    const float yaw_before_second_forward_turn = after_first_forward_turn.yaw_radians;
    const float distance_before_second_forward_turn = after_first_forward_turn.distance;
    forward_turn_input.pointer_position_pixels = {150.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 121;
    }
    const elf3d::NavigationSnapshot after_second_forward_turn = forward_turn_navigation.snapshot();
    if (nearly_equal(after_second_forward_turn.yaw_radians, yaw_before_second_forward_turn) ||
        !(after_second_forward_turn.distance < distance_before_second_forward_turn) ||
        length(subtract(after_second_forward_turn.pivot, fixed_dynamic_center)) > 32.0F ||
        !forward_turn_navigation.has_screen_anchor() ||
        !camera_looks_at(forward_turn_fixture.scene, forward_turn_fixture.camera,
                         fixed_dynamic_center)) {
        return 122;
    }
    forward_turn_input.w_pressed = false;
    forward_turn_input.pointer_delta_pixels = {};
    const elf3d::Float3 position_before_forward_release =
        camera_position(forward_turn_fixture.scene, forward_turn_fixture.camera);
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold) ||
        !nearly_equal(camera_position(forward_turn_fixture.scene, forward_turn_fixture.camera),
                      position_before_forward_release)) {
        return 123;
    }
    forward_turn_input.left_button_down = false;
    static_cast<void>(forward_turn_navigation.update(forward_turn_fixture.scene,
                                                     forward_turn_fixture.camera, {800, 600},
                                                     forward_turn_input, click_threshold));

    SceneFixture low_fps_fixture = make_scene(5, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    SceneFixture high_fps_fixture = make_scene(8, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController low_fps_navigation;
    elf3d::navigation::OrbitNavigationController high_fps_navigation;
    if (!low_fps_navigation.reset_view(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600}) ||
        !high_fps_navigation.reset_view(high_fps_fixture.scene, high_fps_fixture.camera,
                                        {800, 600})) {
        return 132;
    }
    elf3d::ViewportInput low_fps_input = hovered_input();
    low_fps_input.left_button_down = true;
    low_fps_input.pointer_position_pixels = {10.0F, 10.0F};
    elf3d::ViewportInput high_fps_input = low_fps_input;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 133;
    }
    low_fps_input.pointer_position_pixels = {80.0F, 10.0F};
    low_fps_input.pointer_delta_pixels = {70.0F, 0.0F};
    high_fps_input = low_fps_input;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold) ||
        !low_fps_navigation.set_screen_anchor(low_fps_fixture.scene, low_fps_fixture.camera,
                                              low_fps_navigation.snapshot().pivot) ||
        !high_fps_navigation.set_screen_anchor(high_fps_fixture.scene, high_fps_fixture.camera,
                                               high_fps_navigation.snapshot().pivot)) {
        return 134;
    }
    low_fps_input.frame_delta_seconds = 1.0F / 30.0F;
    low_fps_input.pointer_position_pixels = {120.0F, 10.0F};
    low_fps_input.pointer_delta_pixels = {40.0F, 0.0F};
    low_fps_input.w_pressed = true;
    high_fps_input.frame_delta_seconds = 1.0F / 60.0F;
    high_fps_input.pointer_position_pixels = {100.0F, 10.0F};
    high_fps_input.pointer_delta_pixels = {20.0F, 0.0F};
    high_fps_input.w_pressed = true;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 135;
    }
    high_fps_input.pointer_position_pixels = {120.0F, 10.0F};
    if (!high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 136;
    }
    const elf3d::NavigationSnapshot low_fps_snapshot = low_fps_navigation.snapshot();
    const elf3d::NavigationSnapshot high_fps_snapshot = high_fps_navigation.snapshot();
    if (!nearly_equal(low_fps_snapshot.yaw_radians, high_fps_snapshot.yaw_radians) ||
        !nearly_equal(low_fps_snapshot.pitch_radians, high_fps_snapshot.pitch_radians) ||
        !nearly_equal(low_fps_snapshot.distance, high_fps_snapshot.distance, 0.002F) ||
        !nearly_equal(low_fps_snapshot.pivot, high_fps_snapshot.pivot, 0.002F) ||
        !nearly_equal(camera_position(low_fps_fixture.scene, low_fps_fixture.camera),
                      camera_position(high_fps_fixture.scene, high_fps_fixture.camera), 0.002F) ||
        !low_fps_navigation.has_screen_anchor() || !high_fps_navigation.has_screen_anchor()) {
        return 137;
    }

    SceneFixture combined_pan_fixture = make_scene(6, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    SceneFixture mouse_pan_fixture = make_scene(7, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController combined_pan_navigation;
    elf3d::navigation::OrbitNavigationController mouse_pan_navigation;
    if (!combined_pan_navigation.reset_view(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                            {800, 600}) ||
        !mouse_pan_navigation.reset_view(mouse_pan_fixture.scene, mouse_pan_fixture.camera,
                                         {800, 600})) {
        return 124;
    }
    elf3d::ViewportInput combined_pan_input = hovered_input();
    combined_pan_input.right_button_down = true;
    elf3d::ViewportInput mouse_pan_input = combined_pan_input;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold)) {
        return 125;
    }
    combined_pan_input.pointer_delta_pixels = {80.0F, 40.0F};
    combined_pan_input.w_pressed = true;
    mouse_pan_input = combined_pan_input;
    mouse_pan_input.w_pressed = false;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold)) {
        return 126;
    }
    const elf3d::Float3 combined_pan_offset =
        subtract(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                 camera_position(mouse_pan_fixture.scene, mouse_pan_fixture.camera));
    const elf3d::Float3 current_pan_forward =
        camera_forward(mouse_pan_fixture.scene, mouse_pan_fixture.camera);
    const float combined_pan_forward_distance = dot(combined_pan_offset, current_pan_forward);
    if (!(combined_pan_forward_distance > 0.0F) ||
        !nearly_equal(combined_pan_offset,
                      multiply(current_pan_forward, combined_pan_forward_distance)) ||
        !(combined_pan_navigation.snapshot().distance < mouse_pan_navigation.snapshot().distance)) {
        return 127;
    }
    combined_pan_input.w_pressed = false;
    combined_pan_input.pointer_delta_pixels = {};
    mouse_pan_input = combined_pan_input;
    const elf3d::Float3 position_before_pan_key_release =
        camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera);
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold) ||
        !nearly_equal(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                      position_before_pan_key_release)) {
        return 128;
    }
    const elf3d::Float3 position_before_mouse_only_pan =
        camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera);
    combined_pan_input.pointer_delta_pixels = {20.0F, 10.0F};
    mouse_pan_input = combined_pan_input;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold) ||
        nearly_equal(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                     position_before_mouse_only_pan)) {
        return 129;
    }
    combined_pan_input.right_button_down = false;
    combined_pan_input.pointer_delta_pixels = {};
    mouse_pan_input = combined_pan_input;
    static_cast<void>(combined_pan_navigation.update(combined_pan_fixture.scene,
                                                     combined_pan_fixture.camera, {800, 600},
                                                     combined_pan_input, click_threshold));
    static_cast<void>(mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera,
                                                  {800, 600}, mouse_pan_input, click_threshold));

    return 0;
}
