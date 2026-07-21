#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>
#include <elf3d/viewport.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

import elf.assets;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

#include "offscreen_viewport_test_support.h"

namespace {

using elf3d::viewport::tests::FakeDevice;
using elf3d::viewport::tests::FakeDeviceState;
using elf3d::viewport::tests::FakePickingTarget;

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

struct ViewportContext {
    ViewportContext()
        : scene_id(elf3d::detail::SceneHandleAccess::create_scene(1, 1)), scene(scene_id) {}

    elf3d::SceneId scene_id;
    elf3d::scene::Storage scene;
    std::unique_ptr<elf3d::renderer::Renderer> renderer;
    elf3d::picking::PickingService picking_service;
    elf3d::EntityId camera;
    elf3d::EntityId model;
    std::unique_ptr<elf3d::viewport::OffscreenViewport> viewport;

    [[nodiscard]] FakeDeviceState& device_state() noexcept {
        return static_cast<FakeDevice&>(renderer->device()).state();
    }
};

[[nodiscard]] int prepare_viewport_context(ViewportContext& context) {
    auto owned_device = std::make_unique<FakeDevice>();
    auto renderer = elf3d::renderer::Renderer::create(std::move(owned_device), 1);
    if (!renderer) {
        return 1;
    }
    context.renderer = std::move(renderer).value();

    const auto camera =
        context.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-0.5F, -0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.5F, -0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = context.scene.create_mesh({vertices, indices});
    const auto material = context.scene.create_material({});
    if (!camera) {
        return 2;
    }
    if (!mesh || !material) {
        return 21;
    }
    const auto model = context.scene.create_model(mesh.value(), material.value());
    if (!model) {
        return 21;
    }
    context.camera = camera.value();
    context.model = model.value();

    auto viewport =
        elf3d::viewport::OffscreenViewport::create(context.renderer->device(), elf3d::Extent2D{});
    if (!viewport) {
        return 3;
    }
    context.viewport = std::move(viewport).value();
    return 0;
}

[[nodiscard]] elf3d::Result<void> update_navigation(ViewportContext& context,
                                                    const elf3d::ViewportInput& input) {
    return context.viewport->update_navigation(*context.renderer, context.picking_service,
                                               context.scene, context.camera, input);
}

[[nodiscard]] int verify_empty_and_resize(ViewportContext& context) {
    if (context.viewport->framebuffer_valid() || context.viewport->color_texture().is_valid()) {
        return 4;
    }
    if (!context.viewport->render(*context.renderer, context.scene, context.camera)) {
        return 5;
    }
    if (!context.viewport->resize({640, 360}) || !context.viewport->resize({640, 360}) ||
        !context.viewport->framebuffer_valid() ||
        context.viewport->extent() != elf3d::Extent2D{640, 360}) {
        return 6;
    }
    return 0;
}

[[nodiscard]] int verify_viewport_settings(ViewportContext& context) {
    context.viewport->set_clear_color({-1.0F, 2.0F, std::numeric_limits<float>::quiet_NaN(),
                                       std::numeric_limits<float>::infinity()});
    const elf3d::Color4 expected{0.0F, 1.0F, 0.0F, 1.0F};
    if (context.viewport->clear_color() != expected ||
        !context.viewport->render(*context.renderer, context.scene, context.camera)) {
        return 7;
    }
    elf3d::BasicLighting lighting;
    lighting.direction = {0.0F, 3.0F, 4.0F};
    lighting.ambient_intensity = 5.0F;
    lighting.diffuse_intensity = 20.0F;
    context.viewport->set_basic_lighting(lighting);
    const elf3d::BasicLighting sanitized = context.viewport->basic_lighting();
    if (sanitized.direction != elf3d::Float3{0.0F, 0.6F, 0.8F} ||
        sanitized.ambient_intensity != 2.0F || sanitized.diffuse_intensity != 10.0F) {
        return 8;
    }
    return 0;
}

struct DynamicAnchorContext {
    elf3d::Extent2D target_extent{256U, 144U};
    std::size_t pixel_count = static_cast<std::size_t>(target_extent.width) *
                              static_cast<std::size_t>(target_extent.height);
    elf3d::Float3 anchor;
    elf3d::ProjectedViewportPoint projected_before;
    elf3d::ViewportInput input;
};

[[nodiscard]] bool has_expected_focus_statistics(const FakeDeviceState& state,
                                                 const elf3d::PickingStatistics& statistics,
                                                 elf3d::Extent2D expected_extent) {
    return state.picking_depths_read_count == 1 && state.picking_pixel_read_count == 0 &&
           state.last_picking_read_extent == expected_extent &&
           statistics.latest_gpu_pixels_read <= 65536U;
}

[[nodiscard]] bool same_screen_position(const elf3d::ProjectedViewportPoint& left,
                                        const elf3d::ProjectedViewportPoint& right,
                                        float tolerance) {
    return nearly_equal(left.position_pixels.x, right.position_pixels.x, tolerance) &&
           nearly_equal(left.position_pixels.y, right.position_pixels.y, tolerance);
}

[[nodiscard]] int prepare_dynamic_anchor(ViewportContext& context,
                                         DynamicAnchorContext& anchor_context) {
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
                                      DynamicAnchorContext& anchor_context) {
    FakeDeviceState& state = context.device_state();
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    anchor_context.input.is_focused = true;
    anchor_context.input.is_hovered = true;
    anchor_context.input.pointer_position_pixels = {16.0F, 16.0F};
    anchor_context.input.left_button_down = true;
    if (!update_navigation(context, anchor_context.input)) {
        return 843;
    }
    anchor_context.input.pointer_position_pixels = {32.0F, 16.0F};
    anchor_context.input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!update_navigation(context, anchor_context.input)) {
        return 844;
    }
    const elf3d::PickingStatistics statistics =
        context.viewport->picking_statistics(context.picking_service);
    if (!has_expected_focus_statistics(state, statistics, anchor_context.target_extent)) {
        return 845;
    }
    return 0;
}

[[nodiscard]] int finish_dynamic_orbit(ViewportContext& context,
                                       DynamicAnchorContext& anchor_context) {
    anchor_context.input.pointer_position_pixels = {48.0F, 16.0F};
    anchor_context.input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!update_navigation(context, anchor_context.input)) {
        return 846;
    }
    const auto projected_after = context.viewport->project_world_to_viewport(
        context.scene, context.camera, anchor_context.anchor);
    if (!projected_after ||
        !same_screen_position(projected_after.value(), anchor_context.projected_before, 0.1F)) {
        return 848;
    }
    anchor_context.input.left_button_down = false;
    anchor_context.input.pointer_delta_pixels = {};
    if (!update_navigation(context, anchor_context.input)) {
        return 849;
    }
    return 0;
}

[[nodiscard]] int verify_dynamic_anchor_navigation(ViewportContext& context) {
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
                                         const std::optional<elf3d::NavigationSnapshot>& snapshot) {
    return state.picking_depths_read_count == 0 && state.picking_pixel_read_count == 0 &&
           snapshot.has_value() && snapshot->is_pointer_captured &&
           snapshot->interaction_mode == elf3d::NavigationInteractionMode::orbit;
}

[[nodiscard]] bool has_continued_eye_orbit(const FakeDeviceState& state,
                                           const std::optional<elf3d::NavigationSnapshot>& snapshot,
                                           float initial_yaw) {
    return state.picking_depths_read_count == 0 && state.picking_pixel_read_count == 0 &&
           snapshot.has_value() && !nearly_equal(snapshot->yaw_radians, initial_yaw);
}

[[nodiscard]] int verify_eye_orbit(ViewportContext& context) {
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 867;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_depths.assign(256U * 144U, 0.5F);
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.space_down = true;
    input.left_button_down = true;
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
    input.space_down = false;
    input.pointer_position_pixels = {64.0F, 16.0F};
    input.pointer_delta_pixels = {32.0F, 0.0F};
    if (!update_navigation(context, input)) {
        return 871;
    }
    snapshot = context.viewport->navigation_snapshot();
    if (!has_continued_eye_orbit(state, snapshot, initial_yaw)) {
        return 872;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) ||
        context.viewport->navigation_snapshot()->is_pointer_captured) {
        return 873;
    }
    return 0;
}

[[nodiscard]] bool has_pick_hit(const elf3d::Result<std::optional<elf3d::PickHit>>& pick) {
    return pick && pick.value().has_value();
}

[[nodiscard]] bool used_gpu_pixel_pick(const FakeDeviceState& state) {
    return state.picking_depths_read_count == 0 && state.picking_pixel_read_count != 0;
}

[[nodiscard]] int verify_quick_click_anchor(ViewportContext& context) {
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 858;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.pointer_position_pixels = {319.5F, 179.5F};
    const elf3d::viewport::ViewportPickRequest request{
        context.camera, input.pointer_position_pixels, {}};
    const auto pick =
        context.viewport->pick(*context.renderer, context.picking_service, context.scene, request);
    if (!has_pick_hit(pick)) {
        return 859;
    }
    const elf3d::Float3 anchor = pick.value()->world_position;
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 860;
    }
    input.left_button_down = false;
    if (!update_navigation(context, input)) {
        return 861;
    }
    if (!used_gpu_pixel_pick(state)) {
        return 862;
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

[[nodiscard]] int verify_missed_click(ViewportContext& context) {
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 850;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_pixel.reset();
    state.picking_depths.clear();
    state.picking_depths_read_count = 0;
    state.picking_pixel_read_count = 0;
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.pointer_position_pixels = {319.5F, 179.5F};
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 852;
    }
    input.left_button_down = false;
    if (!update_navigation(context, input)) {
        return 853;
    }
    if (!used_gpu_pixel_pick(state)) {
        return 856;
    }
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 857;
    }
    return 0;
}

[[nodiscard]] int verify_outside_release_cancels_click(ViewportContext& context) {
    if (!context.viewport->reset_view(context.scene, context.camera)) {
        return 874;
    }
    FakeDeviceState& state = context.device_state();
    state.picking_pixel_read_count = 0;
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.pointer_position_pixels = {1.0F, 1.0F};
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 875;
    }
    input.pointer_position_pixels = {-0.5F, 1.0F};
    input.left_button_down = false;
    if (!update_navigation(context, input) || state.picking_pixel_read_count != 0) {
        return 876;
    }
    return 0;
}

[[nodiscard]] bool has_expected_selection(const ViewportContext& context) {
    return context.viewport->has_selection() &&
           context.viewport->selected_entity() == context.model;
}

[[nodiscard]] bool has_expected_pick_statistics(const elf3d::PickingStatistics& statistics) {
    return statistics.latest_gpu_requests == 1 && statistics.latest_gpu_hits == 1 &&
           statistics.latest_gpu_misses == 0 && statistics.latest_cpu_refinements == 1 &&
           statistics.latest_cpu_fallbacks == 0 && statistics.latest_triangle_tests == 1;
}

[[nodiscard]] bool has_expected_scaled_pick_position(const FakeDeviceState& state) {
    return nearly_equal(state.last_picking_read_position.x, 159.5F, 0.001F) &&
           nearly_equal(state.last_picking_read_position.y, 89.5F, 0.001F);
}

[[nodiscard]] int verify_selection_click(ViewportContext& context) {
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.pointer_position_pixels = {319.5F, 179.5F};
    context.device_state().picking_pixel = elf3d::graphics::PickingPixel{1U, 0U, 0U, 0.5F};
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 81;
    }
    if (context.viewport->has_selection()) {
        return 81;
    }
    input.control_down = true;
    input.left_button_down = false;
    if (!update_navigation(context, input)) {
        return 82;
    }
    if (!has_expected_selection(context)) {
        return 82;
    }
    const elf3d::PickingStatistics statistics =
        context.viewport->picking_statistics(context.picking_service);
    if (!has_expected_pick_statistics(statistics)) {
        return 824;
    }
    if (!has_expected_scaled_pick_position(context.device_state())) {
        return 865;
    }
    return 0;
}

[[nodiscard]] bool has_hidden_selection(ViewportContext& context,
                                        const elf3d::Result<void>& hidden) {
    return hidden && context.viewport->has_selection() &&
           !context.scene.entity_effective_visibility(context.model).value();
}

[[nodiscard]] bool has_hidden_render(ViewportContext& context, const elf3d::Result<void>& render) {
    return render && context.viewport->statistics().draw_calls == 0;
}

[[nodiscard]] bool has_shown_selection(ViewportContext& context, const elf3d::Result<void>& shown) {
    return shown && context.scene.entity_effective_visibility(context.model).value();
}

[[nodiscard]] bool has_isolated_selection(ViewportContext& context,
                                          const elf3d::Result<void>& isolated) {
    return isolated && context.viewport->is_isolating() &&
           context.viewport->isolated_entity() == context.model;
}

[[nodiscard]] int verify_visibility_commands(ViewportContext& context) {
    const auto hidden = context.viewport->hide_selected(context.scene);
    if (!has_hidden_selection(context, hidden)) {
        return 821;
    }
    const auto hidden_render =
        context.viewport->render(*context.renderer, context.scene, context.camera);
    if (!has_hidden_render(context, hidden_render)) {
        return 822;
    }
    const auto shown = context.viewport->show_selected(context.scene);
    if (!has_shown_selection(context, shown)) {
        return 823;
    }
    const auto isolated = context.viewport->isolate_selected(context.scene);
    if (!has_isolated_selection(context, isolated)) {
        return 824;
    }
    return 0;
}

[[nodiscard]] bool has_bounds(const elf3d::Result<std::optional<elf3d::Bounds3>>& bounds) {
    return bounds && bounds.value().has_value();
}

[[nodiscard]] bool has_x_bounds(const elf3d::Result<std::optional<elf3d::Bounds3>>& bounds,
                                float minimum, float maximum) {
    return has_bounds(bounds) && nearly_equal(bounds.value()->minimum.x, minimum) &&
           nearly_equal(bounds.value()->maximum.x, maximum);
}

[[nodiscard]] int verify_visible_bounds_and_plane(ViewportContext& context) {
    if (!has_bounds(context.viewport->visible_bounds(context.scene))) {
        return 825;
    }
    elf3d::SectionPlane plane;
    plane.enabled = true;
    plane.point = {0.0F, 0.0F, -2.0F};
    plane.normal = {1.0F, 0.0F, 0.0F};
    const std::uint64_t revision = context.viewport->clipping_snapshot().revision;
    if (!context.viewport->set_section_plane(plane) ||
        context.viewport->clipping_snapshot().revision != revision + 1U) {
        return 827;
    }
    if (!has_x_bounds(context.viewport->visible_bounds(context.scene), 0.0F, 0.5F)) {
        return 828;
    }
    return 0;
}

[[nodiscard]] bool
has_independent_box_configuration(const elf3d::Result<std::uint32_t>& added_box,
                                  const ViewportContext& first,
                                  const elf3d::viewport::OffscreenViewport& second) {
    return added_box && added_box.value() == 0 &&
           first.viewport->clipping_snapshot().box_count == 0 &&
           second.clipping_snapshot().box_count == 1;
}

[[nodiscard]] int verify_independent_clipping(ViewportContext& context) {
    auto second_device = std::make_unique<FakeDevice>();
    auto second_renderer = elf3d::renderer::Renderer::create(std::move(second_device), 1);
    if (!second_renderer) {
        return 829;
    }
    auto second_viewport =
        elf3d::viewport::OffscreenViewport::create(second_renderer.value()->device(), {640, 360});
    if (!second_viewport) {
        return 829;
    }
    if (!has_x_bounds(second_viewport.value()->visible_bounds(context.scene), -0.5F, 0.5F)) {
        return 830;
    }
    const elf3d::ClippingBox centered_box{{-0.25F, -0.25F, -2.25F}, {0.25F, 0.25F, -1.75F}, true};
    const auto added_box = second_viewport.value()->add_clipping_box(centered_box);
    if (!has_independent_box_configuration(added_box, context, *second_viewport.value())) {
        return 831;
    }
    if (!has_x_bounds(second_viewport.value()->visible_bounds(context.scene), -0.25F, 0.25F)) {
        return 832;
    }
    context.viewport->clear_clipping();
    if (!has_x_bounds(context.viewport->visible_bounds(context.scene), -0.5F, 0.5F)) {
        return 833;
    }
    context.viewport->clear_isolation();
    if (context.viewport->is_isolating()) {
        return 826;
    }
    return 0;
}

[[nodiscard]] elf3d::ViewportInput center_input() {
    elf3d::ViewportInput input;
    input.is_focused = true;
    input.is_hovered = true;
    input.pointer_position_pixels = {319.5F, 179.5F};
    return input;
}

[[nodiscard]] int verify_drag_does_not_select(ViewportContext& context) {
    context.viewport->clear_selection();
    elf3d::ViewportInput input = center_input();
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 83;
    }
    input.pointer_delta_pixels = {10.0F, 0.0F};
    input.pointer_position_pixels = {329.5F, 179.5F};
    if (!update_navigation(context, input)) {
        return 84;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!update_navigation(context, input) || context.viewport->has_selection()) {
        return 85;
    }
    return 0;
}

[[nodiscard]] int begin_distance_measurement(ViewportContext& context) {
    context.viewport->set_active_tool(elf3d::ViewportTool::distance_measurement);
    if (context.viewport->active_tool() != elf3d::ViewportTool::distance_measurement) {
        return 86;
    }
    elf3d::ViewportInput input = center_input();
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 87;
    }
    input.left_button_down = false;
    if (!update_navigation(context, input) || context.viewport->has_selection()) {
        return 88;
    }
    const elf3d::DistanceMeasurementSnapshot measurement =
        context.viewport->distance_measurement_snapshot(context.scene);
    if (measurement.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        !measurement.first_point.has_value()) {
        return 89;
    }
    return 0;
}

[[nodiscard]] bool has_complete_measurement(const elf3d::DistanceMeasurementSnapshot& measurement) {
    return measurement.state == elf3d::DistanceMeasurementState::complete &&
           measurement.second_point.has_value() && measurement.distance_meters == 0.0;
}

[[nodiscard]] bool has_measurement_overlay(ViewportContext& context,
                                           const elf3d::Result<void>& render) {
    const elf3d::RenderStatistics statistics = context.viewport->statistics();
    const FakeDeviceState& state = context.device_state();
    return render && statistics.overlay_lines == 1 && statistics.overlay_markers == 2 &&
           state.latest_overlay_lines == 1 && state.latest_overlay_markers == 2;
}

[[nodiscard]] int finish_distance_measurement(ViewportContext& context) {
    elf3d::ViewportInput input = center_input();
    input.left_button_down = true;
    if (!update_navigation(context, input)) {
        return 90;
    }
    input.left_button_down = false;
    if (!update_navigation(context, input)) {
        return 91;
    }
    const elf3d::DistanceMeasurementSnapshot measurement =
        context.viewport->distance_measurement_snapshot(context.scene);
    if (!has_complete_measurement(measurement)) {
        return 92;
    }
    const auto render = context.viewport->render(*context.renderer, context.scene, context.camera);
    if (!has_measurement_overlay(context, render)) {
        return 93;
    }
    return 0;
}

[[nodiscard]] int clear_distance_measurement(ViewportContext& context) {
    context.viewport->set_active_tool(elf3d::ViewportTool::selection);
    if (context.viewport->active_tool() != elf3d::ViewportTool::selection ||
        context.viewport->distance_measurement_snapshot(context.scene).state !=
            elf3d::DistanceMeasurementState::complete) {
        return 94;
    }
    context.viewport->clear_distance_measurement();
    if (context.viewport->distance_measurement_snapshot(context.scene).state !=
        elf3d::DistanceMeasurementState::empty) {
        return 95;
    }
    return 0;
}

[[nodiscard]] int verify_zero_width(ViewportContext& context) {
    if (!context.viewport->resize({0, 360}) || context.viewport->framebuffer_valid() ||
        context.viewport->color_texture().is_valid()) {
        return 10;
    }
    return 0;
}

using ViewportStep = int (*)(ViewportContext&);

[[nodiscard]] int run_viewport_steps(ViewportContext& context) {
    constexpr std::array<ViewportStep, 16> steps{{
        verify_empty_and_resize,
        verify_viewport_settings,
        verify_dynamic_anchor_navigation,
        verify_eye_orbit,
        verify_quick_click_anchor,
        verify_missed_click,
        verify_outside_release_cancels_click,
        verify_selection_click,
        verify_visibility_commands,
        verify_visible_bounds_and_plane,
        verify_independent_clipping,
        verify_drag_does_not_select,
        begin_distance_measurement,
        finish_distance_measurement,
        clear_distance_measurement,
        verify_zero_width,
    }};
    for (const ViewportStep step : steps) {
        const int result = step(context);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_viewport_lifetime_test() {
    ViewportContext context;
    const int prepared = prepare_viewport_context(context);
    if (prepared != 0) {
        return prepared;
    }
    return run_viewport_steps(context);
}
