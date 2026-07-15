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

namespace {

constexpr float click_threshold = 4.0F;

[[nodiscard]] bool
has_expected_dynamic_turn(const SceneFixture& fixture,
                          const elf3d::navigation::OrbitNavigationController& navigation,
                          const elf3d::NavigationSnapshot& before,
                          const elf3d::NavigationSnapshot& after, elf3d::Float3 fixed_center) {
    return !nearly_equal(after.yaw_radians, before.yaw_radians) &&
           after.distance < before.distance &&
           length(subtract(after.pivot, fixed_center)) <= 32.0F && navigation.has_screen_anchor() &&
           camera_looks_at(fixture.scene, fixture.camera, fixed_center);
}

[[nodiscard]] int prepare_forward_turn(SceneFixture& fixture,
                                       elf3d::navigation::OrbitNavigationController& navigation,
                                       elf3d::ViewportInput& input) {
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 116;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 117;
    }
    input.pointer_position_pixels = {80.0F, 10.0F};
    input.pointer_delta_pixels = {70.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !navigation.set_screen_anchor(fixture.scene, fixture.camera, navigation.snapshot().pivot)) {
        return 118;
    }
    return 0;
}

[[nodiscard]] int verify_forward_turn() {
    SceneFixture forward_turn_fixture = make_scene(4, {99'000'000.0F, -1'000'000.0F, -1'000'000.0F},
                                                   {101'000'000.0F, 1'000'000.0F, 1'000'000.0F});
    elf3d::navigation::OrbitNavigationController forward_turn_navigation;
    elf3d::ViewportInput forward_turn_input;
    const int prepared =
        prepare_forward_turn(forward_turn_fixture, forward_turn_navigation, forward_turn_input);
    if (prepared != 0) {
        return prepared;
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
    if (!has_expected_dynamic_turn(forward_turn_fixture, forward_turn_navigation,
                                   before_first_forward_turn, after_first_forward_turn,
                                   fixed_dynamic_center)) {
        return 120;
    }

    forward_turn_input.pointer_position_pixels = {150.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 121;
    }
    const elf3d::NavigationSnapshot after_second_forward_turn = forward_turn_navigation.snapshot();
    if (!has_expected_dynamic_turn(forward_turn_fixture, forward_turn_navigation,
                                   after_first_forward_turn, after_second_forward_turn,
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
    return 0;
}

struct FrameRateContext {
    SceneFixture low_fixture;
    SceneFixture high_fixture;
    elf3d::navigation::OrbitNavigationController low_navigation;
    elf3d::navigation::OrbitNavigationController high_navigation;
    elf3d::ViewportInput low_input;
    elf3d::ViewportInput high_input;
};

[[nodiscard]] FrameRateContext make_frame_rate_context() {
    return FrameRateContext{
        make_scene(5, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F}),
        make_scene(8, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F}),
    };
}

[[nodiscard]] int prepare_frame_rate_context(FrameRateContext& context) {
    if (!context.low_navigation.reset_view(context.low_fixture.scene, context.low_fixture.camera,
                                           {800, 600}) ||
        !context.high_navigation.reset_view(context.high_fixture.scene, context.high_fixture.camera,
                                            {800, 600})) {
        return 132;
    }
    context.low_input = hovered_input();
    context.low_input.left_button_down = true;
    context.low_input.pointer_position_pixels = {10.0F, 10.0F};
    context.high_input = context.low_input;
    if (!context.low_navigation.update(context.low_fixture.scene, context.low_fixture.camera,
                                       {800, 600}, context.low_input, click_threshold) ||
        !context.high_navigation.update(context.high_fixture.scene, context.high_fixture.camera,
                                        {800, 600}, context.high_input, click_threshold)) {
        return 133;
    }
    context.low_input.pointer_position_pixels = {80.0F, 10.0F};
    context.low_input.pointer_delta_pixels = {70.0F, 0.0F};
    context.high_input = context.low_input;
    if (!context.low_navigation.update(context.low_fixture.scene, context.low_fixture.camera,
                                       {800, 600}, context.low_input, click_threshold) ||
        !context.high_navigation.update(context.high_fixture.scene, context.high_fixture.camera,
                                        {800, 600}, context.high_input, click_threshold) ||
        !context.low_navigation.set_screen_anchor(context.low_fixture.scene,
                                                  context.low_fixture.camera,
                                                  context.low_navigation.snapshot().pivot) ||
        !context.high_navigation.set_screen_anchor(context.high_fixture.scene,
                                                   context.high_fixture.camera,
                                                   context.high_navigation.snapshot().pivot)) {
        return 134;
    }
    return 0;
}

[[nodiscard]] bool frame_rates_match(const FrameRateContext& context) {
    const elf3d::NavigationSnapshot low = context.low_navigation.snapshot();
    const elf3d::NavigationSnapshot high = context.high_navigation.snapshot();
    return nearly_equal(low.yaw_radians, high.yaw_radians) &&
           nearly_equal(low.pitch_radians, high.pitch_radians) &&
           nearly_equal(low.distance, high.distance, 0.002F) &&
           nearly_equal(low.pivot, high.pivot, 0.002F) &&
           nearly_equal(camera_position(context.low_fixture.scene, context.low_fixture.camera),
                        camera_position(context.high_fixture.scene, context.high_fixture.camera),
                        0.002F) &&
           context.low_navigation.has_screen_anchor() &&
           context.high_navigation.has_screen_anchor();
}

[[nodiscard]] int verify_frame_rate_invariance() {
    FrameRateContext context = make_frame_rate_context();
    const int prepared = prepare_frame_rate_context(context);
    if (prepared != 0) {
        return prepared;
    }
    context.low_input.frame_delta_seconds = 1.0F / 30.0F;
    context.low_input.pointer_position_pixels = {120.0F, 10.0F};
    context.low_input.pointer_delta_pixels = {40.0F, 0.0F};
    context.low_input.w_pressed = true;
    context.high_input.frame_delta_seconds = 1.0F / 60.0F;
    context.high_input.pointer_position_pixels = {100.0F, 10.0F};
    context.high_input.pointer_delta_pixels = {20.0F, 0.0F};
    context.high_input.w_pressed = true;
    if (!context.low_navigation.update(context.low_fixture.scene, context.low_fixture.camera,
                                       {800, 600}, context.low_input, click_threshold) ||
        !context.high_navigation.update(context.high_fixture.scene, context.high_fixture.camera,
                                        {800, 600}, context.high_input, click_threshold)) {
        return 135;
    }
    context.high_input.pointer_position_pixels = {120.0F, 10.0F};
    if (!context.high_navigation.update(context.high_fixture.scene, context.high_fixture.camera,
                                        {800, 600}, context.high_input, click_threshold)) {
        return 136;
    }
    if (!frame_rates_match(context)) {
        return 137;
    }
    return 0;
}

struct PanContext {
    SceneFixture combined_fixture;
    SceneFixture mouse_fixture;
    elf3d::navigation::OrbitNavigationController combined_navigation;
    elf3d::navigation::OrbitNavigationController mouse_navigation;
    elf3d::ViewportInput combined_input;
    elf3d::ViewportInput mouse_input;
};

[[nodiscard]] PanContext make_pan_context() {
    return PanContext{
        make_scene(6, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F}),
        make_scene(7, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F}),
    };
}

[[nodiscard]] int prepare_pan_context(PanContext& context) {
    if (!context.combined_navigation.reset_view(context.combined_fixture.scene,
                                                context.combined_fixture.camera, {800, 600}) ||
        !context.mouse_navigation.reset_view(context.mouse_fixture.scene,
                                             context.mouse_fixture.camera, {800, 600})) {
        return 124;
    }
    context.combined_input = hovered_input();
    context.combined_input.right_button_down = true;
    context.mouse_input = context.combined_input;
    if (!context.combined_navigation.update(context.combined_fixture.scene,
                                            context.combined_fixture.camera, {800, 600},
                                            context.combined_input, click_threshold) ||
        !context.mouse_navigation.update(context.mouse_fixture.scene, context.mouse_fixture.camera,
                                         {800, 600}, context.mouse_input, click_threshold)) {
        return 125;
    }
    context.combined_input.pointer_delta_pixels = {80.0F, 40.0F};
    context.combined_input.w_pressed = true;
    context.mouse_input = context.combined_input;
    context.mouse_input.w_pressed = false;
    if (!context.combined_navigation.update(context.combined_fixture.scene,
                                            context.combined_fixture.camera, {800, 600},
                                            context.combined_input, click_threshold) ||
        !context.mouse_navigation.update(context.mouse_fixture.scene, context.mouse_fixture.camera,
                                         {800, 600}, context.mouse_input, click_threshold)) {
        return 126;
    }
    return 0;
}

[[nodiscard]] int verify_initial_combined_pan(const PanContext& context) {
    const elf3d::Float3 combined_pan_offset =
        subtract(camera_position(context.combined_fixture.scene, context.combined_fixture.camera),
                 camera_position(context.mouse_fixture.scene, context.mouse_fixture.camera));
    const elf3d::Float3 current_pan_forward =
        camera_forward(context.mouse_fixture.scene, context.mouse_fixture.camera);
    const float combined_pan_forward_distance = dot(combined_pan_offset, current_pan_forward);
    if (!(combined_pan_forward_distance > 0.0F) ||
        !nearly_equal(combined_pan_offset,
                      multiply(current_pan_forward, combined_pan_forward_distance)) ||
        !(context.combined_navigation.snapshot().distance <
          context.mouse_navigation.snapshot().distance)) {
        return 127;
    }
    return 0;
}

[[nodiscard]] int verify_pan_key_release(PanContext& context) {
    context.combined_input.w_pressed = false;
    context.combined_input.pointer_delta_pixels = {};
    context.mouse_input = context.combined_input;
    const elf3d::Float3 position_before_pan_key_release =
        camera_position(context.combined_fixture.scene, context.combined_fixture.camera);
    if (!context.combined_navigation.update(context.combined_fixture.scene,
                                            context.combined_fixture.camera, {800, 600},
                                            context.combined_input, click_threshold) ||
        !context.mouse_navigation.update(context.mouse_fixture.scene, context.mouse_fixture.camera,
                                         {800, 600}, context.mouse_input, click_threshold) ||
        !nearly_equal(
            camera_position(context.combined_fixture.scene, context.combined_fixture.camera),
            position_before_pan_key_release)) {
        return 128;
    }
    return 0;
}

[[nodiscard]] int verify_mouse_only_pan(PanContext& context) {
    const elf3d::Float3 position_before_mouse_only_pan =
        camera_position(context.combined_fixture.scene, context.combined_fixture.camera);
    context.combined_input.pointer_delta_pixels = {20.0F, 10.0F};
    context.mouse_input = context.combined_input;
    if (!context.combined_navigation.update(context.combined_fixture.scene,
                                            context.combined_fixture.camera, {800, 600},
                                            context.combined_input, click_threshold) ||
        !context.mouse_navigation.update(context.mouse_fixture.scene, context.mouse_fixture.camera,
                                         {800, 600}, context.mouse_input, click_threshold) ||
        nearly_equal(
            camera_position(context.combined_fixture.scene, context.combined_fixture.camera),
            position_before_mouse_only_pan)) {
        return 129;
    }
    return 0;
}

void release_pan(PanContext& context) {
    context.combined_input.right_button_down = false;
    context.combined_input.pointer_delta_pixels = {};
    context.mouse_input = context.combined_input;
    static_cast<void>(context.combined_navigation.update(
        context.combined_fixture.scene, context.combined_fixture.camera, {800, 600},
        context.combined_input, click_threshold));
    static_cast<void>(context.mouse_navigation.update(context.mouse_fixture.scene,
                                                      context.mouse_fixture.camera, {800, 600},
                                                      context.mouse_input, click_threshold));
}

[[nodiscard]] int verify_combined_pan() {
    PanContext context = make_pan_context();
    const int prepared = prepare_pan_context(context);
    if (prepared != 0) {
        return prepared;
    }
    const int initial_pan = verify_initial_combined_pan(context);
    if (initial_pan != 0) {
        return initial_pan;
    }
    const int released_key = verify_pan_key_release(context);
    if (released_key != 0) {
        return released_key;
    }
    const int mouse_only_pan = verify_mouse_only_pan(context);
    if (mouse_only_pan != 0) {
        return mouse_only_pan;
    }
    release_pan(context);
    return 0;
}

} // namespace

int elf3d_navigation_continuous_motion_test() {
    const int forward = verify_forward_turn();
    if (forward != 0) {
        return forward;
    }
    const int frame_rate = verify_frame_rate_invariance();
    if (frame_rate != 0) {
        return frame_rate;
    }
    return verify_combined_pan();
}
