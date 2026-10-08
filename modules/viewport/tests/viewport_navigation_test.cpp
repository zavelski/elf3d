#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/projection.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>
#include <elf3d/surface_anchor.h>
#include <elf3d/viewport.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <elf3d/internal/assets.h>
#include <elf3d/internal/graphics.h>
#include <elf3d/internal/picking.h>
#include <elf3d/internal/renderer.h>
#include <elf3d/internal/scene.h>
#include <elf3d/internal/viewport.h>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "../../renderer/tests/studio_environment_test_source.h"
#include "viewport_scenario_support.h"

namespace elf3d::viewport::tests::scenario {

struct DynamicAnchorContext {
    elf3d::Extent2D target_extent{256U, 144U};
    std::size_t pixel_count = static_cast<std::size_t>(target_extent.width) *
                              static_cast<std::size_t>(target_extent.height);
    elf3d::Float3 anchor;
    elf3d::ProjectedViewportPoint projected_before;
    elf3d::NavigationInput input;
};

[[nodiscard]] bool has_expected_focus_statistics(const FakeDeviceState& state,
                                                 const elf3d::PickingStatistics& statistics,
                                                 elf3d::Extent2D expected_extent)
{
    return state.picking_depths_read_count == 1 && state.picking_pixel_read_count == 0 &&
           state.last_picking_read_extent == expected_extent &&
           statistics.latest_gpu_pixels_read <= 65536U &&
           statistics.latest_target_allocations == 0 && statistics.latest_pass_milliseconds > 0.0 &&
           statistics.latest_readback_milliseconds > 0.0 &&
           statistics.latest_cpu_milliseconds > 0.0;
}

[[nodiscard]] bool same_screen_position(const elf3d::ProjectedViewportPoint& left,
                                        const elf3d::ProjectedViewportPoint& right, float tolerance)
{
    return nearly_equal(left.position_pixels.x, right.position_pixels.x, tolerance) &&
           nearly_equal(left.position_pixels.y, right.position_pixels.y, tolerance);
}

[[nodiscard]] int prepare_dynamic_anchor(ViewportContext& context,
                                         DynamicAnchorContext& anchor_context)
{
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 840;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_pixel = elf3d::graphics::PickingPixel{1U, 0U, 0U, 0.5F};
    state.picking_depths.assign(anchor_context.pixel_count, 0.5F);
    FakePickingTarget reference_target{anchor_context.target_extent};
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::renderer::GpuFocusDepthRequest request{context.camera, {640, 360}};
    const auto anchor_result = context.renderer->gpu_focus_depth_anchor(
        context.scene, reference_target, visibility, elf3d::clipping::disabled_filter(), request);
    if (!anchor_result || !anchor_result.value().world_position.has_value() ||
        anchor_result.value().pixels_read != anchor_context.pixel_count) {
        return 841;
    }
    anchor_context.anchor = *anchor_result.value().world_position;
    const auto projected = context.viewport->project_world_to_viewport(
        context.scene, context.camera, anchor_context.anchor);
    if (!projected || !projected.value().is_inside_viewport ||
        !nearly_equal(projected.value().position_pixels.x, 320.0F, 0.5F) ||
        !nearly_equal(projected.value().position_pixels.y, 180.0F, 0.5F)) {
        return 842;
    }
    anchor_context.projected_before = projected.value();
    return 0;
}

[[nodiscard]] int begin_dynamic_orbit(ViewportContext& context,
                                      DynamicAnchorContext& anchor_context)
{
    FakeDeviceState& state = context.device_state();
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    anchor_context.input.region_focused = true;
    anchor_context.input.pointer_hovered = true;
    anchor_context.input.pointer_position_pixels = {16.0F, 16.0F};
    anchor_context.input.orbit_down = true;
    if (!update_navigation(context, anchor_context.input)) {
        return 843;
    }
    anchor_context.input.pointer_position_pixels = {48.0F, 16.0F};
    anchor_context.input.pointer_delta_pixels = {32.0F, 0.0F};
    if (!update_navigation(context, anchor_context.input)) {
        return 844;
    }

    const elf3d::PickingStatistics statistics =
        context.viewport->picking_statistics(context.picking_service);
    if (!has_expected_focus_statistics(state, statistics, anchor_context.target_extent)) {
        return 845;
    }
    if (state.picking_targets.size() != 2 || state.picking_targets[0].front().resize_count != 1 ||
        state.picking_targets[1].front().resize_count != 1) {
        return 847;
    }
    return 0;
}

[[nodiscard]] int finish_dynamic_orbit(ViewportContext& context,
                                       DynamicAnchorContext& anchor_context)
{
    anchor_context.input.pointer_position_pixels = {80.0F, 16.0F};
    anchor_context.input.pointer_delta_pixels = {32.0F, 0.0F};
    if (!update_navigation(context, anchor_context.input) ||
        context.device_state().picking_depths_read_count != 1) {
        return 846;
    }

    const auto projected_after = context.viewport->project_world_to_viewport(
        context.scene, context.camera, anchor_context.anchor);
    if (!projected_after ||
        !same_screen_position(projected_after.value(), anchor_context.projected_before, 0.1F)) {
        return 848;
    }
    anchor_context.input.orbit_down = false;
    anchor_context.input.pointer_delta_pixels = {};
    if (!update_navigation(context, anchor_context.input)) {
        return 849;
    }
    return 0;
}

[[nodiscard]] int verify_dynamic_anchor_navigation(ViewportContext& context)
{
    DynamicAnchorContext anchor_context;
    const int prepared = prepare_dynamic_anchor(context, anchor_context);
    if (prepared != 0) {
        return prepared;
    }

    const int begun = begin_dynamic_orbit(context, anchor_context);
    if (begun != 0) {
        return begun;
    }
    return finish_dynamic_orbit(context, anchor_context);
}

[[nodiscard]] bool has_started_eye_orbit(const FakeDeviceState& state,
                                         const std::optional<elf3d::NavigationSnapshot>& snapshot)
{
    return state.picking_depths_read_count == 0 && state.picking_pixel_read_count == 0 &&
           snapshot.has_value() && snapshot->is_pointer_captured &&
           snapshot->interaction_mode == elf3d::NavigationInteractionMode::orbit;
}

[[nodiscard]] bool has_continued_eye_orbit(const FakeDeviceState& state,
                                           const std::optional<elf3d::NavigationSnapshot>& snapshot,
                                           float initial_yaw)
{
    return state.picking_depths_read_count == 0 && state.picking_pixel_read_count == 0 &&
           snapshot.has_value() && !nearly_equal(snapshot->yaw_radians, initial_yaw);
}

[[nodiscard]] int verify_eye_orbit(ViewportContext& context)
{
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 867;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_depths.assign(256U * 144U, 0.5F);
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    elf3d::NavigationInput input;
    input.region_focused = true;
    input.pointer_hovered = true;
    input.eye_orbit_modifier_down = true;
    input.orbit_down = true;
    input.pointer_position_pixels = {16.0F, 16.0F};
    if (!update_navigation(context, input)) {
        return 868;
    }
    input.pointer_position_pixels = {32.0F, 16.0F};
    input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 869;
    }

    std::optional<elf3d::NavigationSnapshot> snapshot = context.viewport->navigation_snapshot();
    if (!has_started_eye_orbit(state, snapshot)) {
        return 870;
    }

    const float initial_yaw = snapshot->yaw_radians;
    input.eye_orbit_modifier_down = false;
    input.pointer_position_pixels = {64.0F, 16.0F};
    input.pointer_delta_pixels = {32.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 871;
    }
    snapshot = context.viewport->navigation_snapshot();
    if (!has_continued_eye_orbit(state, snapshot, initial_yaw)) {
        return 872;
    }
    input.orbit_down = false;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) ||
        context.viewport->navigation_snapshot()->is_pointer_captured) {
        return 873;
    }
    return 0;
}

[[nodiscard]] int verify_disabled_focus_depth(ViewportContext& context)
{
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 877;
    }
    elf3d::OrbitNavigationSettings settings = context.viewport->navigation_settings();
    settings.focus_depth_anchor_enabled = false;
    if (!context.viewport->set_navigation_settings(settings)) {
        return 878;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_depths_read_count = 0;
    elf3d::NavigationInput input;
    input.region_focused = true;
    input.pointer_hovered = true;
    input.orbit_down = true;
    input.pointer_position_pixels = {16.0F, 16.0F};
    if (!update_navigation(context, input) || state.picking_depths_read_count != 0) {
        return 879;
    }
    input.orbit_down = false;
    if (!update_navigation(context, input)) {
        return 880;
    }
    settings.focus_depth_anchor_enabled = true;
    return context.viewport->set_navigation_settings(settings) ? 0 : 881;
}

[[nodiscard]] bool has_pick_hit(const elf3d::Result<std::optional<elf3d::PickHit>>& pick)
{
    return pick && pick.value().has_value();
}

[[nodiscard]] int verify_explicit_examine_pivot(ViewportContext& context)
{
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 858;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    elf3d::NavigationInput input;
    input.region_focused = true;
    input.pointer_hovered = true;
    input.pointer_position_pixels = {319.5F, 179.5F};
    const elf3d::viewport::ViewportPickRequest request{
        context.camera, input.pointer_position_pixels, {}};
    const auto pick =
        context.viewport->pick(*context.renderer, context.picking_service, context.scene, request);
    if (!has_pick_hit(pick)) {
        return 859;
    }

    const elf3d::Float3 anchor = pick.value()->world_position;
    if (!context.viewport->set_examine_pivot(context.scene, context.camera, anchor)) {
        return 860;
    }

    const auto projected_before =
        context.viewport->project_world_to_viewport(context.scene, context.camera, anchor);
    if (!projected_before) {
        return 864;
    }
    input.pointer_delta_pixels = {};
    input.wheel_delta = 1.0F;
    if (!update_navigation(context, input)) {
        return 864;
    }

    const auto projected_after =
        context.viewport->project_world_to_viewport(context.scene, context.camera, anchor);
    if (!projected_after ||
        !same_screen_position(projected_after.value(), projected_before.value(), 0.05F)) {
        return 866;
    }
    return 0;
}

} // namespace elf3d::viewport::tests::scenario
