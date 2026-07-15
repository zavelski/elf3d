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

constexpr elf3d::Extent2D viewport_extent{800, 600};

struct AnchorDepthContext : NavigationTestContext {
    AnchorDepthContext() : NavigationTestContext(9) {}

    elf3d::NavigationSnapshot initial_reset;
    elf3d::navigation::OrbitNavigationController first_viewport;
    elf3d::navigation::OrbitNavigationController second_viewport;
};

struct OffAxisAnchor {
    elf3d::Float3 forward;
    elf3d::Float3 position;
    elf3d::Float3 pivot;
    elf3d::Float2 projected;
};

[[nodiscard]] OffAxisAnchor make_off_axis_anchor(const AnchorDepthContext& context) {
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 right = camera_right(context.fixture.scene, context.fixture.camera);
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    const float distance = context.navigation.snapshot().distance;
    const elf3d::Float3 pivot{
        position.x + forward.x * distance + right.x * 2.0F,
        position.y + forward.y * distance + right.y * 2.0F,
        position.z + forward.z * distance + right.z * 2.0F,
    };
    return OffAxisAnchor{
        forward,
        position,
        pivot,
        project_to_ndc(context.fixture.scene, context.fixture.camera, viewport_extent, pivot),
    };
}

[[nodiscard]] bool is_inside_view(const elf3d::Float2 projected) {
    return std::isfinite(projected.x) && std::isfinite(projected.y) &&
           std::abs(projected.x) < 1.0F && std::abs(projected.y) < 1.0F;
}

[[nodiscard]] bool
released_pointer(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                 const AnchorDepthContext& context) {
    return update && !context.navigation.snapshot().is_pointer_captured;
}

[[nodiscard]] int prepare_anchor_context(AnchorDepthContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 149;
    }
    context.initial_reset = context.navigation.snapshot();
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 34;
    }
    return 0;
}

[[nodiscard]] int establish_click_anchor(AnchorDepthContext& context, const OffAxisAnchor& anchor) {
    if (!is_inside_view(anchor.projected)) {
        return 35;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {
        (anchor.projected.x + 1.0F) * 0.5F * static_cast<float>(viewport_extent.width),
        (1.0F - anchor.projected.y) * 0.5F * static_cast<float>(viewport_extent.height),
    };
    if (!update_navigation(context, input)) {
        return 36;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!released_pointer(update_navigation(context, input), context)) {
        return 37;
    }
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              anchor.pivot)) {
        return 38;
    }
    const std::uint64_t revision = context.fixture.scene.revision();
    if (!context.fixture.scene.set_local_transform(context.fixture.model, elf3d::Transform{}) ||
        context.fixture.scene.revision() == revision) {
        return 39;
    }
    return 0;
}

[[nodiscard]] bool
has_idle_anchor_update(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                       const AnchorDepthContext& context, const OffAxisAnchor& anchor) {
    return update && !context.navigation.snapshot().is_pointer_captured &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                        anchor.position);
}

[[nodiscard]] bool has_expected_anchor_dolly(const AnchorDepthContext& context,
                                             const OffAxisAnchor& anchor, float distance_before) {
    const elf3d::Float2 projected = project_to_ndc(context.fixture.scene, context.fixture.camera,
                                                   viewport_extent, anchor.pivot);
    return nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        anchor.forward) &&
           context.navigation.snapshot().distance < distance_before &&
           nearly_equal(projected.x, anchor.projected.x, 0.002F) &&
           nearly_equal(projected.y, anchor.projected.y, 0.002F);
}

[[nodiscard]] bool
started_static_pan(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                   const AnchorDepthContext& context, elf3d::Float3 position,
                   elf3d::Float3 forward) {
    return update &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera), position) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera), forward);
}

[[nodiscard]] bool has_bounded_pan_step(const AnchorDepthContext& context,
                                        elf3d::Float3 position_before, elf3d::Float3 forward) {
    const float step = length(
        subtract(camera_position(context.fixture.scene, context.fixture.camera), position_before));
    return step > 0.0F && step <= 0.25F &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera), forward);
}

[[nodiscard]] int verify_click_anchor_dolly(AnchorDepthContext& context) {
    const OffAxisAnchor anchor = make_off_axis_anchor(context);
    const int established = establish_click_anchor(context, anchor);
    if (established != 0) {
        return established;
    }
    const float distance_before = length(subtract(anchor.pivot, anchor.position));
    elf3d::ViewportInput input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!has_idle_anchor_update(update_navigation(context, input), context, anchor)) {
        return 40;
    }
    input.pointer_delta_pixels = {};
    input.wheel_delta = 1.0F;
    if (!update_navigation(context, input)) {
        return 41;
    }
    if (!has_expected_anchor_dolly(context, anchor, distance_before)) {
        return 42;
    }
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    input = hovered_input();
    input.middle_button_down = true;
    const elf3d::Float3 pan_start = camera_position(context.fixture.scene, context.fixture.camera);
    if (!started_static_pan(update_navigation(context, input), context, pan_start, forward)) {
        return 65;
    }
    const elf3d::Float3 before_pan = camera_position(context.fixture.scene, context.fixture.camera);
    input.pointer_delta_pixels = {1.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 66;
    }
    if (!has_bounded_pan_step(context, before_pan, forward)) {
        return 67;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] bool captured_static_camera(const AnchorDepthContext& context,
                                          const OffAxisAnchor& anchor) {
    return context.navigation.snapshot().is_pointer_captured &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                        anchor.position) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        anchor.forward);
}

[[nodiscard]] bool captured_pan_moved(const AnchorDepthContext& context,
                                      const OffAxisAnchor& anchor) {
    return context.navigation.snapshot().is_pointer_captured &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        anchor.forward) &&
           !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                         anchor.position);
}

[[nodiscard]] int verify_click_anchor_pan(AnchorDepthContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 43;
    }
    const OffAxisAnchor anchor = make_off_axis_anchor(context);
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              anchor.pivot)) {
        return 44;
    }
    elf3d::ViewportInput input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!has_idle_anchor_update(update_navigation(context, input), context, anchor) ||
        !nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                      anchor.forward)) {
        return 45;
    }
    input.middle_button_down = true;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) || !captured_static_camera(context, anchor)) {
        return 46;
    }
    input.pointer_position_pixels = {480.0F, 340.0F};
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!update_navigation(context, input) || !captured_pan_moved(context, anchor)) {
        return 47;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    if (!released_pointer(update_navigation(context, input), context)) {
        return 48;
    }
    return 0;
}

[[nodiscard]] bool
has_static_orbit_start(const elf3d::Result<elf3d::navigation::NavigationUpdate>& update,
                       const AnchorDepthContext& context, const OffAxisAnchor& anchor) {
    return update && update.value().orbit_start_position_pixels.has_value() &&
           nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                        anchor.position) &&
           nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                        anchor.forward);
}

[[nodiscard]] bool has_expected_anchor_orbit(const AnchorDepthContext& context,
                                             const OffAxisAnchor& anchor) {
    const elf3d::Float2 projected = project_to_ndc(context.fixture.scene, context.fixture.camera,
                                                   viewport_extent, anchor.pivot);
    return context.navigation.snapshot().is_pointer_captured &&
           !nearly_equal(camera_position(context.fixture.scene, context.fixture.camera),
                         anchor.position) &&
           !nearly_equal(camera_forward(context.fixture.scene, context.fixture.camera),
                         anchor.forward) &&
           !camera_looks_at(context.fixture.scene, context.fixture.camera, anchor.pivot) &&
           nearly_equal(projected.x, anchor.projected.x, 0.002F) &&
           nearly_equal(projected.y, anchor.projected.y, 0.002F);
}

[[nodiscard]] int verify_click_anchor_orbit(AnchorDepthContext& context) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 49;
    }
    const OffAxisAnchor anchor = make_off_axis_anchor(context);
    if (!is_inside_view(anchor.projected)) {
        return 50;
    }
    if (!context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              anchor.pivot)) {
        return 51;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {400.0F, 300.0F};
    if (!update_navigation(context, input)) {
        return 52;
    }
    input.pointer_position_pixels = {460.0F, 330.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    if (!has_static_orbit_start(update_navigation(context, input), context, anchor) ||
        !context.navigation.set_screen_anchor(context.fixture.scene, context.fixture.camera,
                                              anchor.pivot)) {
        return 53;
    }
    input.pointer_position_pixels = {520.0F, 360.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    if (!update_navigation(context, input)) {
        return 54;
    }
    if (!has_expected_anchor_orbit(context, anchor)) {
        return 55;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!released_pointer(update_navigation(context, input), context)) {
        return 56;
    }
    return 0;
}

struct CrossingContext {
    elf3d::NavigationSnapshot start;
    float expected_step = 0.0F;
    float minimum_motion = 0.0F;
};

[[nodiscard]] int prepare_crossing(AnchorDepthContext& context, CrossingContext& crossing) {
    if (!context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 57;
    }
    crossing.start = context.navigation.snapshot();
    const std::optional<elf3d::Bounds3> bounds = context.fixture.scene.world_bounds();
    if (!bounds.has_value()) {
        return 141;
    }
    const elf3d::Float3 center = multiply(add(bounds->minimum, bounds->maximum), 0.5F);
    const float reference = length(subtract(bounds->maximum, center));
    crossing.minimum_motion = reference * context.navigation.settings().minimum_motion_scale;
    const float wheel_ratio =
        (1.0F - std::exp(-context.navigation.settings().zoom_sensitivity)) * 0.5F;
    crossing.expected_step = crossing.minimum_motion * wheel_ratio;
    const elf3d::Float3 forward = camera_forward(context.fixture.scene, context.fixture.camera);
    set_camera_position(
        context.fixture.scene, context.fixture.camera,
        add(crossing.start.pivot, multiply(forward, -crossing.expected_step * 1.5F)));
    if (!context.navigation.synchronize(context.fixture.scene, context.fixture.camera) ||
        signed_camera_distance_to(context.fixture.scene, context.fixture.camera,
                                  crossing.start.pivot) <= crossing.expected_step) {
        return 58;
    }
    return 0;
}

[[nodiscard]] bool has_expected_crossing(const AnchorDepthContext& context,
                                         const CrossingContext& crossing, float initial_step,
                                         float crossing_step) {
    const float step_ratio = crossing_step / initial_step;
    const float reference_ratio = crossing_step / crossing.expected_step;
    return signed_camera_distance_to(context.fixture.scene, context.fixture.camera,
                                     crossing.start.pivot) < 0.0F &&
           step_ratio >= 0.45F && step_ratio <= 1.05F && reference_ratio >= 0.45F &&
           reference_ratio <= 1.05F &&
           depth_ratio_within_limit(context.fixture.scene, context.fixture.camera);
}

[[nodiscard]] int cross_anchor_depth(AnchorDepthContext& context, const CrossingContext& crossing) {
    elf3d::ViewportInput input = hovered_input();
    input.wheel_delta = 1.0F;
    const elf3d::Float3 before_step =
        camera_position(context.fixture.scene, context.fixture.camera);
    if (!update_navigation(context, input)) {
        return 59;
    }
    const elf3d::Float3 before_crossing =
        camera_position(context.fixture.scene, context.fixture.camera);
    const float initial_step = length(subtract(before_crossing, before_step));
    if (signed_camera_distance_to(context.fixture.scene, context.fixture.camera,
                                  crossing.start.pivot) <= 0.0F) {
        return 60;
    }
    if (!update_navigation(context, input)) {
        return 61;
    }
    const float crossing_step = length(
        subtract(camera_position(context.fixture.scene, context.fixture.camera), before_crossing));
    if (!has_expected_crossing(context, crossing, initial_step, crossing_step)) {
        return 62;
    }
    return 0;
}

[[nodiscard]] int recover_after_crossing(AnchorDepthContext& context,
                                         const CrossingContext& crossing) {
    elf3d::ViewportInput input = hovered_input();
    input.wheel_delta = -1.0F;
    const float signed_before = signed_camera_distance_to(
        context.fixture.scene, context.fixture.camera, crossing.start.pivot);
    if (!update_navigation(context, input) ||
        signed_camera_distance_to(context.fixture.scene, context.fixture.camera,
                                  crossing.start.pivot) <= signed_before) {
        return 63;
    }
    input = hovered_input();
    input.middle_button_down = true;
    static_cast<void>(update_navigation(context, input));
    const elf3d::Float3 position = camera_position(context.fixture.scene, context.fixture.camera);
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!update_navigation(context, input) ||
        length(subtract(camera_position(context.fixture.scene, context.fixture.camera),
                        position)) <= crossing.minimum_motion * 0.01F) {
        return 64;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(update_navigation(context, input));
    return 0;
}

[[nodiscard]] int verify_anchor_crossing(AnchorDepthContext& context) {
    CrossingContext crossing;
    const int prepared = prepare_crossing(context, crossing);
    if (prepared != 0) {
        return prepared;
    }
    const int crossed = cross_anchor_depth(context, crossing);
    if (crossed != 0) {
        return crossed;
    }
    return recover_after_crossing(context, crossing);
}

[[nodiscard]] int verify_settings_contract(AnchorDepthContext& context) {
    elf3d::OrbitNavigationSettings invalid = context.navigation.settings();
    invalid.maximum_distance = invalid.minimum_distance;
    if (context.navigation.set_settings(invalid).error().code() !=
        elf3d::ErrorCode::invalid_navigation_settings) {
        return 16;
    }
    elf3d::OrbitNavigationSettings inverted = context.navigation.settings();
    inverted.invert_vertical_orbit = true;
    if (!context.navigation.set_settings(inverted) ||
        !context.navigation.reset_view(context.fixture.scene, context.fixture.camera,
                                       viewport_extent)) {
        return 17;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(update_navigation(context, input));
    input.pointer_position_pixels = {10.0F, 60.0F};
    input.pointer_delta_pixels = {0.0F, 50.0F};
    static_cast<void>(update_navigation(context, input));
    input.pointer_position_pixels = {10.0F, 110.0F};
    static_cast<void>(update_navigation(context, input));
    if (context.navigation.snapshot().pitch_radians <= context.initial_reset.pitch_radians) {
        return 18;
    }
    return 0;
}

[[nodiscard]] int verify_extreme_bounds(AnchorDepthContext& context) {
    SceneFixture tiny = make_scene(2, {0.0F, 0.0F, 0.0F}, {0.000001F, 0.000001F, 0.000001F});
    if (!context.navigation.reset_view(tiny.scene, tiny.camera, viewport_extent) ||
        !std::isfinite(context.navigation.snapshot().distance)) {
        return 19;
    }
    SceneFixture large = make_scene(3, {-1.0e6F, -1.0e6F, -1.0e6F}, {1.0e6F, 1.0e6F, 1.0e6F});
    if (!context.navigation.reset_view(large.scene, large.camera, viewport_extent) ||
        !std::isfinite(context.navigation.snapshot().distance)) {
        return 20;
    }
    return 0;
}

[[nodiscard]] int verify_empty_fit(AnchorDepthContext& context) {
    elf3d::scene::Storage empty{scene_id(4)};
    const elf3d::EntityId camera =
        empty.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    const elf3d::Float4x4 matrix = empty.local_matrix(camera).value();
    const elf3d::Result<void> fit = context.navigation.fit_to_scene(empty, camera, viewport_extent);
    if (fit || fit.error().code() != elf3d::ErrorCode::scene_has_no_bounds ||
        empty.local_matrix(camera).value() != matrix) {
        return 21;
    }
    return 0;
}

[[nodiscard]] int verify_independent_controllers(AnchorDepthContext& context) {
    if (!context.first_viewport.reset_view(context.fixture.scene, context.fixture.camera,
                                           viewport_extent) ||
        !context.second_viewport.reset_view(context.fixture.scene, context.fixture.second_camera,
                                            viewport_extent)) {
        return 22;
    }
    const float second_distance = context.second_viewport.snapshot().distance;
    elf3d::ViewportInput input = hovered_input();
    input.wheel_delta = 1.0F;
    static_cast<void>(context.first_viewport.update(context.fixture.scene, context.fixture.camera,
                                                    viewport_extent, input,
                                                    navigation_test_click_threshold));
    if (nearly_equal(context.first_viewport.snapshot().distance, second_distance) ||
        !nearly_equal(context.second_viewport.snapshot().distance, second_distance)) {
        return 23;
    }
    input = hovered_input();
    input.left_button_down = true;
    static_cast<void>(context.first_viewport.update(context.fixture.scene, context.fixture.camera,
                                                    viewport_extent, input,
                                                    navigation_test_click_threshold));
    if (!context.first_viewport.snapshot().is_pointer_captured ||
        context.second_viewport.snapshot().is_pointer_captured) {
        return 24;
    }
    context.first_viewport.cancel_interaction();
    if (context.first_viewport.snapshot().is_pointer_captured ||
        context.second_viewport.snapshot().is_pointer_captured) {
        return 25;
    }
    return 0;
}

[[nodiscard]] int verify_external_synchronization(AnchorDepthContext& context) {
    elf3d::Transform external;
    external.translation = {10.0F, 2.0F, 3.0F};
    if (!context.fixture.scene.set_local_transform(context.fixture.camera, external) ||
        !context.first_viewport.synchronize(context.fixture.scene, context.fixture.camera)) {
        return 26;
    }
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_delta_pixels = {500.0F, 500.0F};
    const elf3d::NavigationSnapshot before = context.first_viewport.snapshot();
    static_cast<void>(context.first_viewport.update(context.fixture.scene, context.fixture.camera,
                                                    viewport_extent, input,
                                                    navigation_test_click_threshold));
    if (!nearly_equal(context.first_viewport.snapshot().yaw_radians, before.yaw_radians) ||
        !nearly_equal(context.first_viewport.snapshot().pitch_radians, before.pitch_radians)) {
        return 27;
    }
    return 0;
}

using AnchorStep = int (*)(AnchorDepthContext&);

[[nodiscard]] int run_anchor_steps(AnchorDepthContext& context) {
    constexpr std::array<AnchorStep, 10> steps{{
        prepare_anchor_context,
        verify_click_anchor_dolly,
        verify_click_anchor_pan,
        verify_click_anchor_orbit,
        verify_anchor_crossing,
        verify_settings_contract,
        verify_extreme_bounds,
        verify_empty_fit,
        verify_independent_controllers,
        verify_external_synchronization,
    }};
    for (const AnchorStep step : steps) {
        const int result = step(context);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_navigation_anchor_depth_test() {
    AnchorDepthContext context;
    return run_anchor_steps(context);
}
