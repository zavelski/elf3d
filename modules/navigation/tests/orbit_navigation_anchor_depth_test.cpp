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

int elf3d_navigation_anchor_depth_test() {
    constexpr float click_threshold = 4.0F;
    SceneFixture fixture = make_scene(9, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController navigation;
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 149;
    }
    const elf3d::NavigationSnapshot reset = navigation.snapshot();
    elf3d::ViewportInput input = hovered_input();
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 34;
    }
    constexpr elf3d::Extent2D viewport_extent{800, 600};
    const elf3d::Float3 forward_before_click_pivot = camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot = camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot{
        position_before_click_pivot.x + forward_before_click_pivot.x * click_pivot_distance +
            right_before_click_pivot.x * 2.0F,
        position_before_click_pivot.y + forward_before_click_pivot.y * click_pivot_distance +
            right_before_click_pivot.y * 2.0F,
        position_before_click_pivot.z + forward_before_click_pivot.z * click_pivot_distance +
            right_before_click_pivot.z * 2.0F,
    };
    const elf3d::Float2 projected_click_pivot_before =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot);
    if (!std::isfinite(projected_click_pivot_before.x) ||
        !std::isfinite(projected_click_pivot_before.y) ||
        std::abs(projected_click_pivot_before.x) >= 1.0F ||
        std::abs(projected_click_pivot_before.y) >= 1.0F) {
        return 35;
    }

    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {
        (projected_click_pivot_before.x + 1.0F) * 0.5F * static_cast<float>(viewport_extent.width),
        (1.0F - projected_click_pivot_before.y) * 0.5F * static_cast<float>(viewport_extent.height),
    };
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 36;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 37;
    }
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot)) {
        return 38;
    }
    const float distance_before_click_pivot_wheel =
        length(subtract(off_axis_click_pivot, position_before_click_pivot));
    const std::uint64_t scene_revision_before_model_update = fixture.scene.revision();
    if (!fixture.scene.set_local_transform(fixture.model, elf3d::Transform{}) ||
        fixture.scene.revision() == scene_revision_before_model_update) {
        return 39;
    }

    input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot)) {
        return 40;
    }
    input.pointer_delta_pixels = {};
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 41;
    }
    const elf3d::Float3 forward_after_click_pivot_wheel =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float2 projected_click_pivot_after =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot);
    if (!nearly_equal(forward_after_click_pivot_wheel, forward_before_click_pivot) ||
        !(navigation.snapshot().distance < distance_before_click_pivot_wheel) ||
        !nearly_equal(projected_click_pivot_after.x, projected_click_pivot_before.x, 0.002F) ||
        !nearly_equal(projected_click_pivot_after.y, projected_click_pivot_before.y, 0.002F)) {
        return 42;
    }
    input = hovered_input();
    input.middle_button_down = true;
    const elf3d::Float3 before_post_dolly_pan_start =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      before_post_dolly_pan_start) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_after_click_pivot_wheel)) {
        return 65;
    }
    const elf3d::Float3 before_post_dolly_pan = camera_position(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {1.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 66;
    }
    const float post_dolly_pan_step =
        length(subtract(camera_position(fixture.scene, fixture.camera), before_post_dolly_pan));
    if (post_dolly_pan_step <= 0.0F || post_dolly_pan_step > 0.25F ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_after_click_pivot_wheel)) {
        return 67;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 43;
    }
    const elf3d::Float3 forward_before_click_pivot_pan =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot_pan = camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot_pan =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_pan_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot_pan{
        position_before_click_pivot_pan.x +
            forward_before_click_pivot_pan.x * click_pivot_pan_distance +
            right_before_click_pivot_pan.x * 2.0F,
        position_before_click_pivot_pan.y +
            forward_before_click_pivot_pan.y * click_pivot_pan_distance +
            right_before_click_pivot_pan.y * 2.0F,
        position_before_click_pivot_pan.z +
            forward_before_click_pivot_pan.z * click_pivot_pan_distance +
            right_before_click_pivot_pan.z * 2.0F,
    };
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_pan)) {
        return 44;
    }
    input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan)) {
        return 45;
    }
    input.middle_button_down = true;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan)) {
        return 46;
    }
    input.pointer_position_pixels = {480.0F, 340.0F};
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_click_pivot_pan)) {
        return 47;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 48;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 49;
    }
    const elf3d::Float3 forward_before_click_pivot_orbit =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot_orbit =
        camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot_orbit =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_orbit_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot_orbit{
        position_before_click_pivot_orbit.x +
            forward_before_click_pivot_orbit.x * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.x * 2.0F,
        position_before_click_pivot_orbit.y +
            forward_before_click_pivot_orbit.y * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.y * 2.0F,
        position_before_click_pivot_orbit.z +
            forward_before_click_pivot_orbit.z * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.z * 2.0F,
    };
    const elf3d::Float2 projected_click_pivot_orbit_before =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot_orbit);
    if (!std::isfinite(projected_click_pivot_orbit_before.x) ||
        !std::isfinite(projected_click_pivot_orbit_before.y) ||
        std::abs(projected_click_pivot_orbit_before.x) >= 1.0F ||
        std::abs(projected_click_pivot_orbit_before.y) >= 1.0F) {
        return 50;
    }
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_orbit)) {
        return 51;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 52;
    }
    input.pointer_position_pixels = {460.0F, 330.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    const elf3d::Result<elf3d::navigation::NavigationUpdate> click_pivot_orbit_start =
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold);
    if (!click_pivot_orbit_start ||
        !click_pivot_orbit_start.value().orbit_start_position_pixels.has_value() ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_orbit) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_orbit) ||
        !navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_orbit)) {
        return 53;
    }
    input.pointer_position_pixels = {520.0F, 360.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 54;
    }
    const elf3d::Float2 projected_click_pivot_orbit_after =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot_orbit);
    if (!navigation.snapshot().is_pointer_captured ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_click_pivot_orbit) ||
        nearly_equal(camera_forward(fixture.scene, fixture.camera),
                     forward_before_click_pivot_orbit) ||
        camera_looks_at(fixture.scene, fixture.camera, off_axis_click_pivot_orbit) ||
        !nearly_equal(projected_click_pivot_orbit_after.x, projected_click_pivot_orbit_before.x,
                      0.002F) ||
        !nearly_equal(projected_click_pivot_orbit_after.y, projected_click_pivot_orbit_before.y,
                      0.002F)) {
        return 55;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 56;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 57;
    }
    const elf3d::NavigationSnapshot crossing_start = navigation.snapshot();
    const elf3d::Float3 crossing_forward = camera_forward(fixture.scene, fixture.camera);
    const float scaled_wheel_step_ratio =
        (1.0F - std::exp(-navigation.settings().zoom_sensitivity)) * 0.5F;
    const std::optional<elf3d::Bounds3> crossing_bounds = fixture.scene.world_bounds();
    if (!crossing_bounds.has_value()) {
        return 141;
    }
    const elf3d::Float3 crossing_bounds_center =
        multiply(add(crossing_bounds->minimum, crossing_bounds->maximum), 0.5F);
    const float crossing_reference =
        length(subtract(crossing_bounds->maximum, crossing_bounds_center));
    const float minimum_motion = crossing_reference * navigation.settings().minimum_motion_scale;
    const float expected_crossing_step = minimum_motion * scaled_wheel_step_ratio;
    const float starting_signed_distance = expected_crossing_step * 1.5F;
    set_camera_position(
        fixture.scene, fixture.camera,
        add(crossing_start.pivot, multiply(crossing_forward, -starting_signed_distance)));
    if (!navigation.synchronize(fixture.scene, fixture.camera) ||
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <=
            expected_crossing_step) {
        return 58;
    }
    input = hovered_input();
    input.wheel_delta = 1.0F;
    const elf3d::Float3 before_crossing_step = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 59;
    }
    const elf3d::Float3 before_crossing = camera_position(fixture.scene, fixture.camera);
    const float initial_crossing_step = length(subtract(before_crossing, before_crossing_step));
    if (signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <= 0.0F) {
        return 60;
    }
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 61;
    }
    const elf3d::Float3 after_crossing = camera_position(fixture.scene, fixture.camera);
    const float crossing_step = length(subtract(after_crossing, before_crossing));
    const float step_ratio = crossing_step / initial_crossing_step;
    const float local_reference_step_ratio = crossing_step / expected_crossing_step;
    if (signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) >= 0.0F ||
        step_ratio < 0.45F || step_ratio > 1.05F || local_reference_step_ratio < 0.45F ||
        local_reference_step_ratio > 1.05F ||
        !depth_ratio_within_limit(fixture.scene, fixture.camera)) {
        return 62;
    }
    input.wheel_delta = -1.0F;
    const float signed_after_crossing =
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <=
            signed_after_crossing) {
        return 63;
    }
    input = hovered_input();
    input.middle_button_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));
    const elf3d::Float3 before_post_crossing_pan = camera_position(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        length(subtract(camera_position(fixture.scene, fixture.camera),
                        before_post_crossing_pan)) <= minimum_motion * 0.01F) {
        return 64;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));

    elf3d::OrbitNavigationSettings invalid_settings = navigation.settings();
    invalid_settings.maximum_distance = invalid_settings.minimum_distance;
    if (navigation.set_settings(invalid_settings).error().code() !=
        elf3d::ErrorCode::invalid_navigation_settings) {
        return 16;
    }
    elf3d::OrbitNavigationSettings inverted = navigation.settings();
    inverted.invert_vertical_orbit = true;
    if (!navigation.set_settings(inverted) ||
        !navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 17;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {10.0F, 60.0F};
    input.pointer_delta_pixels = {0.0F, 50.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {10.0F, 110.0F};
    input.pointer_delta_pixels = {0.0F, 50.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!(navigation.snapshot().pitch_radians > reset.pitch_radians)) {
        return 18;
    }

    SceneFixture tiny = make_scene(2, {0.0F, 0.0F, 0.0F}, {0.000001F, 0.000001F, 0.000001F});
    if (!navigation.reset_view(tiny.scene, tiny.camera, {800, 600}) ||
        !std::isfinite(navigation.snapshot().distance)) {
        return 19;
    }
    SceneFixture large = make_scene(3, {-1.0e6F, -1.0e6F, -1.0e6F}, {1.0e6F, 1.0e6F, 1.0e6F});
    if (!navigation.reset_view(large.scene, large.camera, {800, 600}) ||
        !std::isfinite(navigation.snapshot().distance)) {
        return 20;
    }

    elf3d::scene::Storage empty{scene_id(4)};
    const elf3d::EntityId empty_camera =
        empty.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    const elf3d::Float4x4 empty_camera_matrix = empty.local_matrix(empty_camera).value();
    const elf3d::Result<void> empty_fit = navigation.fit_to_scene(empty, empty_camera, {800, 600});
    if (empty_fit || empty_fit.error().code() != elf3d::ErrorCode::scene_has_no_bounds ||
        empty.local_matrix(empty_camera).value() != empty_camera_matrix) {
        return 21;
    }

    elf3d::navigation::OrbitNavigationController first_viewport;
    elf3d::navigation::OrbitNavigationController second_viewport;
    if (!first_viewport.reset_view(fixture.scene, fixture.camera, {800, 600}) ||
        !second_viewport.reset_view(fixture.scene, fixture.second_camera, {800, 600})) {
        return 22;
    }
    const float second_distance = second_viewport.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (nearly_equal(first_viewport.snapshot().distance, second_distance) ||
        !nearly_equal(second_viewport.snapshot().distance, second_distance)) {
        return 23;
    }
    input = hovered_input();
    input.left_button_down = true;
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!first_viewport.snapshot().is_pointer_captured ||
        second_viewport.snapshot().is_pointer_captured) {
        return 24;
    }
    first_viewport.cancel_interaction();
    if (first_viewport.snapshot().is_pointer_captured ||
        second_viewport.snapshot().is_pointer_captured) {
        return 25;
    }

    elf3d::Transform external;
    external.translation = {10.0F, 2.0F, 3.0F};
    if (!fixture.scene.set_local_transform(fixture.camera, external) ||
        !first_viewport.synchronize(fixture.scene, fixture.camera)) {
        return 26;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_delta_pixels = {500.0F, 500.0F};
    const elf3d::NavigationSnapshot before_external_drag = first_viewport.snapshot();
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!nearly_equal(first_viewport.snapshot().yaw_radians, before_external_drag.yaw_radians) ||
        !nearly_equal(first_viewport.snapshot().pitch_radians,
                      before_external_drag.pitch_radians)) {
        return 27;
    }


    return 0;
}
