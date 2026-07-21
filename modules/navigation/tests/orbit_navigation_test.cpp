#include <elf3d/assets.h>
#include <elf3d/core/result.h>
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

int elf3d_navigation_keyboard_test();

namespace {

[[nodiscard]] bool has_reset_view(const NavigationTestContext& context,
                                  const elf3d::NavigationSnapshot& snapshot) {
    const elf3d::Float3 expected_center{1.5F, 1.5F, 1.5F};
    return nearly_equal(snapshot.pivot, expected_center) &&
           camera_looks_at(context.fixture.scene, context.fixture.camera, snapshot.pivot) &&
           bounds_visible(context.fixture.scene, context.fixture.camera, {800, 600});
}

[[nodiscard]] bool has_anchored_state(const NavigationTestContext& context,
                                      const elf3d::NavigationSnapshot& anchored,
                                      const elf3d::NavigationSnapshot& reset,
                                      elf3d::Float3 camera_position_before,
                                      elf3d::Float3 camera_forward_before) {
    return nearly_equal(anchored.pivot, reset.pivot) &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                        camera_position_before) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        camera_forward_before) &&
           anchored.distance > 0.0F;
}

[[nodiscard]] bool
has_unchanged_camera(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                     const NavigationTestContext& context, elf3d::Float3 position,
                     elf3d::Float3 forward) {
    return update &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera), position) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera), forward);
}

[[nodiscard]] int verify_reset_and_anchor(NavigationTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 1;
    }
    const elf3d::NavigationSnapshot reset = context.navigation.snapshot();
    if (!has_reset_view(context, reset)) {
        return 2;
    }
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              {2.0F, 1.0F, 0.0F})) {
        return 28;
    }
    if (!has_anchored_state(context, context.navigation.snapshot(), reset, position, forward)) {
        return 29;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(update_navigation(context, input));
    input.pointer_position_pixels = {20.0F, 10.0F};
    input.pointer_delta_pixels = {10.0F, 0.0F};
    if (!has_unchanged_camera(update_navigation(context, input), context, position, forward)) {
        return 30;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 31;
    }
    return 0;
}

[[nodiscard]] int verify_fit_to_scene(NavigationTestContext& context) {
    if (!context.navigation.fit_to_scene(context.fixture.scene, context.fixture.camera,
                                         {360, 800}) ||
        !bounds_visible(context.fixture.scene, context.fixture.camera, {360, 800})) {
        return 3;
    }
    const float extent =
        maximum_projected_bounds_extent(context.fixture.scene, context.fixture.camera, {360, 800});
    if (extent < 0.85F || extent > 1.001F) {
        return 96;
    }
    return 0;
}

[[nodiscard]] bool has_orbit_start(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                                   bool expected_anchor) {
    return update && update.value().orbit_start_position_pixels.has_value() == expected_anchor;
}

[[nodiscard]] bool has_initial_drag(const elf3d::NavigationSnapshot& drag,
                                    const elf3d::NavigationSnapshot& before) {
    return drag.is_orbiting && drag.is_pointer_captured &&
           nearly_equal(drag.yaw_radians, before.yaw_radians) &&
           nearly_equal(drag.pitch_radians, before.pitch_radians);
}

[[nodiscard]] bool has_yawed(const NavigationTestContext& context,
                             const elf3d::NavigationSnapshot& yawed,
                             const elf3d::NavigationSnapshot& before) {
    return !nearly_equal(yawed.yaw_radians, before.yaw_radians) &&
           yawed.yaw_radians < before.yaw_radians &&
           nearly_equal(yawed.distance, before.distance) &&
           camera_looks_at(context.fixture.scene, context.fixture.camera, yawed.pivot);
}

[[nodiscard]] bool has_valid_pitch(const NavigationTestContext& context,
                                   const elf3d::NavigationSnapshot& pitched,
                                   const elf3d::NavigationSnapshot& before) {
    return pitched.pitch_radians > before.pitch_radians &&
           pitched.pitch_radians >= context.navigation.settings().minimum_pitch_radians &&
           pitched.pitch_radians <= context.navigation.settings().maximum_pitch_radians;
}

[[nodiscard]] int verify_orbit(NavigationTestContext& context) {
    const elf3d::NavigationSnapshot before = context.navigation.snapshot();
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    if (!update_navigation(context, input)) {
        return 4;
    }
    input.pointer_position_pixels = {300.0F, 300.0F};
    input.pointer_delta_pixels = {290.0F, 290.0F};
    if (!has_orbit_start(update_navigation(context, input), true)) {
        return 4;
    }
    const elf3d::NavigationSnapshot first_drag = context.navigation.snapshot();
    if (!has_initial_drag(first_drag, before)) {
        return 5;
    }
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {100.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 6;
    }
    const elf3d::NavigationSnapshot yawed = context.navigation.snapshot();
    if (!has_yawed(context, yawed, first_drag)) {
        return 7;
    }
    input.pointer_position_pixels = {400.0F, -9700.0F};
    input.pointer_delta_pixels = {0.0F, -10000.0F};
    if (!update_navigation(context, input)) {
        return 8;
    }
    if (!has_valid_pitch(context, context.navigation.snapshot(), yawed)) {
        return 9;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    if (context.navigation.snapshot().is_pointer_captured) {
        return 10;
    }
    return 0;
}

[[nodiscard]] bool
has_eye_orbit_start(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                    const NavigationTestContext& context) {
    const elf3d::NavigationSnapshot snapshot = context.navigation.snapshot();
    return update && !update.value().orbit_start_position_pixels.has_value() &&
           snapshot.is_orbiting && snapshot.is_pointer_captured &&
           !context.navigation.has_screen_anchor();
}

[[nodiscard]] bool has_eye_orbit_result(const NavigationTestContext& context,
                                        const elf3d::NavigationSnapshot& after,
                                        const elf3d::NavigationSnapshot& before,
                                        elf3d::Float3 position, elf3d::Float3 forward) {
    return nearly_equal(camera_position(context.fixture.scene, context.fixture.camera), position) &&
           !nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera), forward) &&
           !nearly_equal(after.yaw_radians, before.yaw_radians) &&
           camera_looks_at(context.fixture.scene, context.fixture.camera, after.pivot) &&
           !context.navigation.has_screen_anchor();
}

[[nodiscard]] int verify_eye_orbit(NavigationTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 142;
    }
    const elf3d::NavigationSnapshot before = context.navigation.snapshot();
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.space_down = true;
    input.pointer_position_pixels = {20.0F, 20.0F};
    if (!update_navigation(context, input)) {
        return 143;
    }
    input.pointer_position_pixels = {40.0F, 20.0F};
    input.pointer_delta_pixels = {20.0F, 0.0F};
    if (!has_eye_orbit_start(update_navigation(context, input), context)) {
        return 144;
    }
    input.space_down = false;
    input.pointer_position_pixels = {100.0F, 20.0F};
    input.pointer_delta_pixels = {60.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 145;
    }
    if (!has_eye_orbit_result(context, context.navigation.snapshot(), before, position, forward)) {
        return 146;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) || context.navigation.snapshot().is_pointer_captured) {
        return 147;
    }
    return 0;
}

[[nodiscard]] int verify_normal_orbit_after_eye(NavigationTestContext& context) {
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(update_navigation(context, input));
    input.pointer_position_pixels = {40.0F, 10.0F};
    input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!has_orbit_start(update_navigation(context, input), true)) {
        return 148;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] bool updated_in_mode(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                                   const NavigationTestContext& context,
                                   elf3d::NavigationInteractionMode mode) {
    return update && context.navigation.snapshot().interaction_mode == mode;
}

[[nodiscard]] bool captured_in_mode(const NavigationTestContext& context,
                                    elf3d::NavigationInteractionMode mode) {
    const elf3d::NavigationSnapshot snapshot = context.navigation.snapshot();
    return snapshot.is_pointer_captured && snapshot.interaction_mode == mode;
}

[[nodiscard]] bool has_pan_update(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                                  const NavigationTestContext& context, elf3d::Float3 position,
                                  elf3d::Float3 forward) {
    return update &&
           !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                         position) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera), forward) &&
           context.navigation.snapshot().interaction_mode == elf3d::NavigationInteractionMode::pan;
}

[[nodiscard]] bool
released_pointer(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                 const NavigationTestContext& context) {
    return update && !context.navigation.snapshot().is_pointer_captured;
}

[[nodiscard]] int verify_orbit_to_pan_handoff(NavigationTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 104;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(update_navigation(context, input));
    input.pointer_position_pixels = {40.0F, 10.0F};
    input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!updated_in_mode(update_navigation(context, input), context,
                         elf3d::NavigationInteractionMode::orbit)) {
        return 105;
    }
    input.right_button_down = true;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) ||
        !captured_in_mode(context, elf3d::NavigationInteractionMode::orbit)) {
        return 106;
    }
    input.left_button_down = false;
    if (!update_navigation(context, input) ||
        !captured_in_mode(context, elf3d::NavigationInteractionMode::pan)) {
        return 107;
    }
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!has_pan_update(update_navigation(context, input), context, position, forward)) {
        return 108;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    if (!released_pointer(update_navigation(context, input), context)) {
        return 109;
    }
    return 0;
}

[[nodiscard]] bool
has_orbit_handoff(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                  const NavigationTestContext& context) {
    return update &&
           context.navigation.snapshot().interaction_mode ==
               elf3d::NavigationInteractionMode::orbit &&
           context.navigation.snapshot().is_pointer_captured &&
           update.value().orbit_start_position_pixels.has_value();
}

[[nodiscard]] bool
changed_yaw_in_orbit(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                     const NavigationTestContext& context, float previous_yaw) {
    return update && !nearly_equal(context.navigation.snapshot().yaw_radians, previous_yaw) &&
           context.navigation.snapshot().interaction_mode ==
               elf3d::NavigationInteractionMode::orbit;
}

[[nodiscard]] int verify_pan_to_orbit_handoff(NavigationTestContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera, {800, 600})) {
        return 110;
    }
    elf3d::ViewportInput input = hovered_input();
    input.right_button_down = true;
    if (!update_navigation(context, input) ||
        !captured_in_mode(context, elf3d::NavigationInteractionMode::pan)) {
        return 111;
    }
    input.left_button_down = true;
    if (!update_navigation(context, input) ||
        !captured_in_mode(context, elf3d::NavigationInteractionMode::pan)) {
        return 112;
    }
    input.right_button_down = false;
    if (!has_orbit_handoff(update_navigation(context, input), context)) {
        return 113;
    }
    const float yaw = context.navigation.snapshot().yaw_radians;
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!changed_yaw_in_orbit(update_navigation(context, input), context, yaw)) {
        return 114;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!released_pointer(update_navigation(context, input), context)) {
        return 115;
    }
    return 0;
}

using NavigationStep = int (*)(NavigationTestContext&);

[[nodiscard]] int run_navigation_steps(NavigationTestContext& context) {
    constexpr std::array<NavigationStep, 7> steps{{
        verify_reset_and_anchor,
        verify_fit_to_scene,
        verify_orbit,
        verify_eye_orbit,
        verify_normal_orbit_after_eye,
        verify_orbit_to_pan_handoff,
        verify_pan_to_orbit_handoff,
    }};
    for (const NavigationStep step : steps) {
        const int result = step(context);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_navigation_test() {
    NavigationTestContext context{1};
    const int interaction = run_navigation_steps(context);
    if (interaction != 0) {
        return interaction;
    }
    const int keyboard = elf3d_navigation_keyboard_test();
    if (keyboard != 0) {
        return keyboard;
    }
    const int continuous = elf3d_navigation_continuous_motion_test();
    if (continuous != 0) {
        return continuous;
    }
    return elf3d_navigation_anchor_depth_test();
}
