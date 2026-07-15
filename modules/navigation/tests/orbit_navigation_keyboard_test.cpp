#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>
#include <elf3d/navigation.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>
#include <cstdint>

import elf.assets;
import elf.math;
import elf.navigation;
import elf.scene;

#include "orbit_navigation_test_support.h"

using namespace elf3d::navigation::test_support;

namespace {

struct KeyboardTestContext : NavigationTestContext {
    KeyboardTestContext() : NavigationTestContext(2) {}

    float scaled_wheel_step = 0.0F;
};

[[nodiscard]] bool has_pan_result(const KeyboardTestContext& context,
                                  const elf3d::NavigationSnapshot& after,
                                  const elf3d::NavigationSnapshot& before,
                                  elf3d::Float3 forward_before) {
    return !nearly_equal(after.pivot, before.pivot) &&
           nearly_equal(after.distance, before.distance) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        forward_before);
}

[[nodiscard]] int verify_drag_pan(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 11;
    }
    const elf3d::NavigationSnapshot before = context.navigation.snapshot();
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(update_navigation(context, input));
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!update_navigation(context, input)) {
        return 12;
    }
    if (!has_pan_result(context, context.navigation.snapshot(), before, forward)) {
        return 13;
    }
    input.left_button_down = false;
    input.x_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] int verify_drag_zoom(KeyboardTestContext& context) {
    const float distance_before = context.navigation.snapshot().distance;
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.z_down = true;
    static_cast<void>(update_navigation(context, input));
    input.pointer_delta_pixels = {0.0F, -80.0F};
    if (!update_navigation(context, input) ||
        context.navigation.snapshot().distance >= distance_before) {
        return 32;
    }
    input.left_button_down = false;
    input.z_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] bool has_expected_wheel_zoom(float before, float after_in, float after_out,
                                           float current) {
    return after_in < before && after_out > after_in && nearly_equal(current, after_out);
}

[[nodiscard]] int verify_wheel_zoom(KeyboardTestContext& context) {
    const float before = context.navigation.snapshot().distance;
    elf3d::ViewportInput input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!update_navigation(context, input)) {
        return 14;
    }
    const float after_in = context.navigation.snapshot().distance;
    input.wheel_delta = -1.0F;
    static_cast<void>(update_navigation(context, input));
    const float after_out = context.navigation.snapshot().distance;
    input.is_hovered = false;
    input.wheel_delta = 20.0F;
    static_cast<void>(update_navigation(context, input));
    if (!has_expected_wheel_zoom(before, after_in, after_out,
                                 context.navigation.snapshot().distance)) {
        return 15;
    }
    input = hovered_input();
    input.is_focused = false;
    input.wheel_delta = 1.0F;
    const float before_hover_wheel = context.navigation.snapshot().distance;
    if (!update_navigation(context, input) ||
        context.navigation.snapshot().distance >= before_hover_wheel) {
        return 33;
    }
    return 0;
}

[[nodiscard]] int verify_scaled_wheel(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 68;
    }
    const float before = context.navigation.snapshot().distance;
    elf3d::ViewportInput input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!update_navigation(context, input)) {
        return 69;
    }
    context.scaled_wheel_step = before - context.navigation.snapshot().distance;
    const float full_step =
        before * (1.0F - std::exp(-context.navigation.settings().zoom_sensitivity));
    if (!nearly_equal(context.scaled_wheel_step, full_step * 0.5F)) {
        return 70;
    }
    return 0;
}

[[nodiscard]] bool
has_keyboard_start(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                   const KeyboardTestContext& context) {
    return update && update.value().orbit_start_position_pixels.has_value() &&
           !context.navigation.has_screen_anchor();
}

[[nodiscard]] bool has_expected_keyboard_forward(const KeyboardTestContext& context,
                                                 const elf3d::NavigationSnapshot& before,
                                                 elf3d::Float3 position_before,
                                                 elf3d::Float3 forward_before) {
    const elf3d::NavigationSnapshot after = context.navigation.snapshot();
    const float step = before.distance - after.distance;
    const elf3d::Float3 offset =
        subtract(camera_position(context.fixture.scene, context.fixture.camera), position_before);
    return nearly_equal(after.pivot, before.pivot) &&
           nearly_equal(step, context.scaled_wheel_step * 0.0125F) &&
           nearly_equal(offset, multiply(forward_before, step));
}

[[nodiscard]] bool
has_keyboard_backward(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                      const KeyboardTestContext& context, elf3d::Float3 position_before,
                      elf3d::Float3 forward_before, float distance_before) {
    return update && context.navigation.snapshot().distance > distance_before &&
           dot(subtract(camera_position(context.fixture.scene, context.fixture.camera),
                        position_before),
               forward_before) < 0.0F;
}

[[nodiscard]] bool
has_keyboard_release(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                     const KeyboardTestContext& context, elf3d::Float3 position_before) {
    return update && !update.value().click_position_pixels.has_value() &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                        position_before);
}

[[nodiscard]] int verify_keyboard_forward(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 75;
    }
    const elf3d::NavigationSnapshot before = context.navigation.snapshot();
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              before.pivot)) {
        return 130;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    if (!has_keyboard_start(update_navigation(context, input), context)) {
        return 76;
    }
    if (!has_expected_keyboard_forward(context, before, position, forward)) {
        return 77;
    }
    input.w_pressed = false;
    input.s_pressed = true;
    const elf3d::Float3 backward_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 backward_forward =
        camera_forward(context.fixture.scene, context.fixture.camera);
    const float backward_distance = context.navigation.snapshot().distance;
    if (!has_keyboard_backward(update_navigation(context, input), context, backward_position,
                               backward_forward, backward_distance)) {
        return 78;
    }
    input.left_button_down = false;
    input.s_pressed = false;
    const elf3d::Float3 release_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!has_keyboard_release(update_navigation(context, input), context, release_position)) {
        return 86;
    }
    input.left_button_down = true;
    input.w_pressed = true;
    const auto reentry = update_navigation(context, input);
    if (!reentry || !reentry.value().orbit_start_position_pixels.has_value()) {
        return 131;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] int verify_local_keyboard_step(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 138;
    }
    const elf3d::NavigationSnapshot before = context.navigation.snapshot();
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 far_anchor =
        add(position, multiply(camera_forward(context.fixture.scene, context.fixture.camera),
                               before.distance * 1000.0F));
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              far_anchor)) {
        return 139;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    const auto update = update_navigation(context, input);
    const float base_multiplier = std::exp(-context.navigation.settings().zoom_sensitivity);
    const float reference_multiplier = 1.0F + (base_multiplier - 1.0F) * 0.5F * 0.0125F;
    const float expected_step = before.distance * (1.0F - reference_multiplier);
    const float actual_step = before.distance - context.navigation.snapshot().distance;
    if (!update || !update.value().orbit_start_position_pixels.has_value() ||
        context.navigation.has_screen_anchor() ||
        !nearly_equal(actual_step, expected_step, 0.0005F)) {
        return 140;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] bool moved_camera(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                                const KeyboardTestContext& context, elf3d::Float3 position_before) {
    return update && !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                                   position_before);
}

[[nodiscard]] bool moved_up(const KeyboardTestContext& context, elf3d::Float3 position_before) {
    const elf3d::Float3 after = camera_position(context.fixture.scene, context.fixture.camera);
    return after.y > position_before.y && nearly_equal(after.x, position_before.x) &&
           nearly_equal(after.z, position_before.z);
}

[[nodiscard]] int verify_right_button_keyboard(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 99;
    }
    elf3d::ViewportInput input = hovered_input();
    input.right_button_down = true;
    input.w_pressed = true;
    const float distance = context.navigation.snapshot().distance;
    const elf3d::Float3 forward_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!moved_camera(update_navigation(context, input), context, forward_position) ||
        context.navigation.snapshot().distance >= distance) {
        return 100;
    }
    input.w_pressed = false;
    input.a_pressed = true;
    const elf3d::Float3 pan_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!moved_camera(update_navigation(context, input), context, pan_position)) {
        return 101;
    }
    input.a_pressed = false;
    input.e_pressed = true;
    const elf3d::Float3 up_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!update_navigation(context, input)) {
        return 102;
    }
    if (!moved_up(context, up_position)) {
        return 103;
    }
    input.right_button_down = false;
    input.e_pressed = false;
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] int verify_mouse_pan_scaling(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 71;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(update_navigation(context, input));
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 72;
    }
    const elf3d::Float3 expected_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 73;
    }
    input = hovered_input();
    input.right_button_down = true;
    static_cast<void>(update_navigation(context, input));
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!update_navigation(context, input) ||
        !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                      expected_position)) {
        return 74;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] int verify_keyboard_pan_scaling(KeyboardTestContext& context) {
    constexpr float pan_step = 800.0F / 800.0F;
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 79;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(update_navigation(context, input));
    input.pointer_delta_pixels = {pan_step, 0.0F};
    if (!update_navigation(context, input)) {
        return 80;
    }
    const elf3d::Float3 expected_position =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 87;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.a_pressed = true;
    if (!update_navigation(context, input) ||
        !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                      expected_position)) {
        return 88;
    }
    input.left_button_down = false;
    input.a_pressed = false;
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] bool moved_down(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                              const KeyboardTestContext& context, elf3d::Float3 position_before) {
    const elf3d::Float3 after = camera_position(context.fixture.scene, context.fixture.camera);
    return update && after.y < position_before.y && nearly_equal(after.x, position_before.x) &&
           nearly_equal(after.z, position_before.z);
}

[[nodiscard]] int verify_vertical_keyboard_pan(KeyboardTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 81;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.e_pressed = true;
    const elf3d::Float3 before_up = camera_position(context.fixture.scene, context.fixture.camera);
    if (!update_navigation(context, input)) {
        return 82;
    }
    if (!moved_up(context, before_up)) {
        return 84;
    }
    input.e_pressed = false;
    input.q_pressed = true;
    const elf3d::Float3 before_down =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!moved_down(update_navigation(context, input), context, before_down)) {
        return 85;
    }
    input.left_button_down = false;
    input.q_pressed = false;
    static_cast<void>(update_navigation(context, input));
    return 0;
}

using KeyboardStep = int (*)(KeyboardTestContext&);

[[nodiscard]] int run_keyboard_steps(KeyboardTestContext& context) {
    constexpr std::array<KeyboardStep, 10> steps{{
        verify_drag_pan,
        verify_drag_zoom,
        verify_wheel_zoom,
        verify_scaled_wheel,
        verify_keyboard_forward,
        verify_local_keyboard_step,
        verify_right_button_keyboard,
        verify_mouse_pan_scaling,
        verify_keyboard_pan_scaling,
        verify_vertical_keyboard_pan,
    }};
    for (const KeyboardStep step : steps) {
        const int result = step(context);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_navigation_keyboard_test() {
    KeyboardTestContext context;
    return run_keyboard_steps(context);
}
