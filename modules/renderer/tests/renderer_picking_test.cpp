#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/model.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <elf3d/internal/assets.h>
#include <elf3d/internal/graphics.h>
#include <elf3d/internal/math.h>
#include <elf3d/internal/renderer.h>
#include <elf3d/internal/scene.h>
#include <memory>
#include <optional>
#include <string>

#include "renderer_scenario_support.h"

namespace elf3d::renderer::tests::scenario {

[[nodiscard]] double test_focus_depth_weight(elf3d::Extent2D extent, std::uint32_t x,
                                             std::uint32_t y) noexcept
{
    const double sample_x =
        (static_cast<double>(x) + 0.5) * 2.0 / static_cast<double>(extent.width) - 1.0;
    const double sample_y =
        (static_cast<double>(y) + 0.5) * 2.0 / static_cast<double>(extent.height) - 1.0;
    const double radius_squared = (sample_x * sample_x + sample_y * sample_y) * 0.5;
    const double mass = 1.0 - std::min(radius_squared, 1.0);
    return mass * mass;
}
[[nodiscard]] bool
has_expected_gpu_pick_summary(const elf3d::Result<elf3d::renderer::GpuPickResult>& pick,
                              const FakePickingTarget& target, const FakeDeviceState& device)
{
    return pick && pick.value().hit.has_value() && pick.value().draw_calls == 2 &&
           pick.value().pixels_read == 1 && target.clear_count == 1 &&
           device.picking_batch_count == 1 && device.picking_draw_count == 2 &&
           device.picking_draws.size() == 2;
}

[[nodiscard]] bool has_expected_gpu_pick_draws(const FakeDeviceState& device)
{
    return device.picking_draws.size() >= 2 && device.picking_draws[0].object_id == 1U &&
           device.picking_draws[0].primitive_index == 0U &&
           device.picking_draws[1].object_id == 2U && device.picking_draws[1].primitive_index == 1U;
}

[[nodiscard]] bool has_expected_gpu_hit(const elf3d::renderer::GpuPickResult& pick,
                                        const RendererContext& context)
{
    return pick.hit->entity == context.model && pick.hit->mesh == context.mesh &&
           pick.hit->primitive_index == 1U && pick.hit->triangle_index == 0U;
}

[[nodiscard]] int verify_gpu_pick(RendererContext& context)
{
    FakePickingTarget picking_target;
    context.device_state().picking_pixel = elf3d::graphics::PickingPixel{2U, 1U, 0U, 0.5F};
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::renderer::GpuPickRequest request{
        context.camera, {319.5F, 179.5F}, picking_target.extent_value, {319.5F, 179.5F}};
    const auto gpu_pick = context.renderer->gpu_pick(context.scene, picking_target, visibility,
                                                     elf3d::clipping::disabled_filter(), request);
    if (!has_expected_gpu_pick_summary(gpu_pick, picking_target, context.device_state()) ||
        !has_expected_gpu_pick_draws(context.device_state()) ||
        !has_expected_gpu_hit(gpu_pick.value(), context)) {
        return 45;
    }
    return 0;
}

[[nodiscard]] bool has_expected_focus_anchor(
    const elf3d::Result<elf3d::renderer::GpuFocusDepthAnchorResult>& focus_anchor,
    const elf3d::Result<elf3d::Float3>& expected_anchor)
{
    return expected_anchor && focus_anchor && focus_anchor.value().world_position.has_value() &&
           focus_anchor.value().draw_calls == 2 && focus_anchor.value().pixels_read == 16 &&
           nearly_equal(*focus_anchor.value().world_position, expected_anchor.value(), 0.0001F);
}

[[nodiscard]] int verify_focus_anchor(RendererContext& context)
{
    FakePickingTarget anchor_target;
    anchor_target.extent_value = {4, 4};
    const elf3d::Extent2D viewport_extent{640, 360};
    context.device_state().picking_depths.assign(16U, 1.0F);
    context.device_state().picking_depths[static_cast<std::size_t>(1U * 4U + 1U)] = 0.25F;
    context.device_state().picking_depths[static_cast<std::size_t>(1U * 4U + 3U)] = 0.75F;
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::renderer::GpuFocusDepthRequest request{context.camera, viewport_extent};
    const auto focus_anchor = context.renderer->gpu_focus_depth_anchor(
        context.scene, anchor_target, visibility, elf3d::clipping::disabled_filter(), request);
    const auto render_list =
        elf3d::renderer::build_render_list(context.scene, context.camera, viewport_extent,
                                           visibility, elf3d::clipping::disabled_filter());
    if (!render_list) {
        return 46;
    }

    const double center_weight = test_focus_depth_weight(anchor_target.extent_value, 1U, 1U);
    const double edge_weight = test_focus_depth_weight(anchor_target.extent_value, 3U, 1U);
    const float expected_depth = static_cast<float>((0.25 * center_weight + 0.75 * edge_weight) /
                                                    (center_weight + edge_weight));
    const elf3d::Float2 expected_screen{static_cast<float>(viewport_extent.width) * 0.5F,
                                        static_cast<float>(viewport_extent.height) * 0.5F};
    const elf3d::Result<elf3d::Float3> expected_anchor = elf3d::math::unproject_viewport_point(
        render_list.value().view_matrix, render_list.value().projection_matrix, viewport_extent,
        expected_screen, expected_depth);
    if (!has_expected_focus_anchor(focus_anchor, expected_anchor)) {
        return 46;
    }

    const auto projected_anchor = elf3d::math::project_world_to_viewport_point(
        render_list.value().view_matrix, render_list.value().projection_matrix, viewport_extent,
        *focus_anchor.value().world_position);
    if (!projected_anchor ||
        !nearly_equal(projected_anchor.value().position_pixels.x, expected_screen.x, 0.001F) ||
        !nearly_equal(projected_anchor.value().position_pixels.y, expected_screen.y, 0.001F)) {
        return 47;
    }

    context.device_state().picking_depths.clear();
    return 0;
}

} // namespace elf3d::renderer::tests::scenario
