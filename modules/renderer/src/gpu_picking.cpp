module;

#include <elf3d/rendering.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

module elf.renderer;

import elf.clipping;
import elf.graphics;
import elf.math;
import elf.scene;

namespace elf3d::renderer {
namespace {

[[nodiscard]] std::optional<GpuPickHit>
make_gpu_pick_hit(const RenderList& list, const RenderItem& item, graphics::PickingPixel pixel,
                  Extent2D extent, Float2 position_pixels) noexcept {
    if (extent.width == 0 || extent.height == 0 || !std::isfinite(pixel.depth) ||
        pixel.depth < 0.0F || pixel.depth >= 1.0F) {
        return std::nullopt;
    }
    const Result<Float3> world = math::unproject_viewport_point(
        list.view_matrix, list.projection_matrix, extent, position_pixels, pixel.depth);
    if (!world) {
        return std::nullopt;
    }
    const Float3 world_position = world.value();
    const float world_distance = math::distance(world_position, list.camera_world_position);
    if (!std::isfinite(world_distance) || world_distance < 0.0F) {
        return std::nullopt;
    }
    return GpuPickHit{item.entity,    item.mesh,   pixel.primitive_index, pixel.triangle_index,
                      world_position, pixel.depth, world_distance};
}

[[nodiscard]] double focus_depth_weight(Extent2D extent, std::uint32_t x,
                                        std::uint32_t y) noexcept {
    if (extent.width == 0U || extent.height == 0U) {
        return 0.0;
    }
    const double sample_x =
        (static_cast<double>(x) + 0.5) * 2.0 / static_cast<double>(extent.width) - 1.0;
    const double sample_y =
        (static_cast<double>(y) + 0.5) * 2.0 / static_cast<double>(extent.height) - 1.0;
    const double radius_squared = (sample_x * sample_x + sample_y * sample_y) * 0.5;
    if (!std::isfinite(radius_squared)) {
        return 0.0;
    }
    const double mass = 1.0 - std::min(radius_squared, 1.0);
    return mass * mass;
}

void apply_clipping_description(const clipping::ClippingFilter& filter,
                                graphics::PickingDrawDescription& draw) noexcept {
    draw.clipping_section_plane_enabled = filter.section_plane_enabled;
    draw.clipping_section_plane_normal = filter.section_plane_normal;
    draw.clipping_section_plane_offset = filter.section_plane_offset;
    draw.clipping_retain_positive_half_space = filter.retain_positive_half_space;
    draw.clipping_box_count = filter.enabled_box_count;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        draw.clipping_boxes[index] = filter.boxes[index];
    }
}

[[nodiscard]] bool valid_viewport_position(Extent2D extent, Float2 position) noexcept {
    return extent.width != 0U && extent.height != 0U && std::isfinite(position.x) &&
           std::isfinite(position.y) && position.x >= 0.0F && position.y >= 0.0F &&
           position.x < static_cast<float>(extent.width) &&
           position.y < static_cast<float>(extent.height);
}

[[nodiscard]] bool valid_pick_coordinates(Extent2D target_extent,
                                          const GpuPickRequest& request) noexcept {
    return valid_viewport_position(target_extent, request.target_position_pixels) &&
           valid_viewport_position(request.viewport_extent, request.viewport_position_pixels);
}

[[nodiscard]] bool valid_focus_extents(Extent2D target_extent,
                                       const GpuFocusDepthRequest& request) noexcept {
    return target_extent.width != 0U && target_extent.height != 0U &&
           request.viewport_extent.width != 0U && request.viewport_extent.height != 0U;
}

[[nodiscard]] Result<void> validate_pick_item_count(const RenderList& list) noexcept {
    if (list.items.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1U) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "GPU picking cannot encode the number of visible render items"};
    }
    return {};
}

[[nodiscard]] graphics::PickingDrawDescription
picking_draw_description(const RenderList& list, const RenderItem& item,
                         const scene::RuntimePrimitiveView& primitive, std::uint32_t object_id,
                         const clipping::ClippingFilter& clipping_filter) noexcept {
    graphics::PickingDrawDescription draw;
    draw.model_matrix = item.model_matrix.elements;
    draw.view_matrix = list.view_matrix.elements;
    draw.projection_matrix = list.projection_matrix.elements;
    draw.object_id = object_id;
    draw.primitive_index = item.primitive_index;
    draw.double_sided = primitive.material_view.double_sided;
    draw.front_face_clockwise = item.orientation_reversed;
    apply_clipping_description(clipping_filter, draw);
    return draw;
}

[[nodiscard]] GpuPickResult resolve_gpu_pick(const RenderList& list,
                                             const std::optional<graphics::PickingPixel>& pixel,
                                             const GpuPickRequest& request,
                                             GpuPickResult result) noexcept {
    if (!pixel.has_value() || pixel->object_id == 0U || pixel->object_id > list.items.size()) {
        return result;
    }
    const RenderItem& item = list.items[pixel->object_id - 1U];
    result.hit = make_gpu_pick_hit(list, item, *pixel, request.viewport_extent,
                                   request.viewport_position_pixels);
    return result;
}

[[nodiscard]] std::optional<float> weighted_focus_depth(std::span<const float> depths,
                                                        Extent2D extent) noexcept {
    double weighted_depth = 0.0;
    double total_weight = 0.0;
    for (std::uint32_t y = 0; y < extent.height; ++y) {
        for (std::uint32_t x = 0; x < extent.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * extent.width + x;
            const float depth = depths[index];
            if (!std::isfinite(depth) || depth <= 0.0F || depth >= 1.0F) {
                continue;
            }
            const double weight = focus_depth_weight(extent, x, y);
            if (weight <= 0.0) {
                continue;
            }
            weighted_depth += static_cast<double>(depth) * weight;
            total_weight += weight;
        }
    }
    if (total_weight <= 0.0 || !std::isfinite(total_weight)) {
        return std::nullopt;
    }
    return static_cast<float>(weighted_depth / total_weight);
}

[[nodiscard]] std::optional<Float3> focus_world_position(const RenderList& list,
                                                         Extent2D viewport_extent,
                                                         std::optional<float> depth) noexcept {
    if (!depth.has_value()) {
        return std::nullopt;
    }
    const Float2 position{static_cast<float>(viewport_extent.width) * 0.5F,
                          static_cast<float>(viewport_extent.height) * 0.5F};
    const Result<Float3> anchor = math::unproject_viewport_point(
        list.view_matrix, list.projection_matrix, viewport_extent, position, *depth);
    return anchor && math::is_finite(anchor.value()) ? std::optional<Float3>{anchor.value()}
                                                     : std::nullopt;
}

} // namespace

Result<void>
Renderer::validate_gpu_picking_context(const scene::Storage& scene_storage) const noexcept {
    if (!scene_storage.belongs_to_engine(engine_token_)) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }
    return {};
}

Result<std::uint64_t>
Renderer::draw_picking_items(const scene::Storage& scene_storage, graphics::PickingTarget& target,
                             const RenderList& list,
                             const clipping::ClippingFilter& clipping_filter) {
    const Result<void> clear_result = target.clear();
    if (!clear_result) {
        return clear_result.error();
    }
    std::vector<graphics::PickingDrawBatchItem> batch;
    batch.reserve(list.items.size());
    for (std::size_t item_index = 0; item_index < list.items.size(); ++item_index) {
        const RenderItem& item = list.items[item_index];
        const Result<scene::RuntimePrimitiveView> primitive =
            scene_storage.runtime_primitive(item.entity, item.primitive_index);
        if (!primitive) {
            return primitive.error();
        }
        RenderStatistics ignored_statistics;
        Result<graphics::StaticMesh*> gpu_mesh =
            cached_mesh(scene_storage.id(), primitive.value(), ignored_statistics);
        if (!gpu_mesh) {
            return gpu_mesh.error();
        }
        const graphics::PickingDrawDescription draw =
            picking_draw_description(list, item, primitive.value(),
                                     static_cast<std::uint32_t>(item_index + 1U), clipping_filter);
        batch.push_back(graphics::PickingDrawBatchItem{gpu_mesh.value(), draw});
    }
    const Result<void> drawn = device_->draw_picking_batch(target, batch);
    return drawn ? Result<std::uint64_t>{static_cast<std::uint64_t>(batch.size())}
                 : Result<std::uint64_t>{drawn.error()};
}

Result<GpuPickResult> Renderer::gpu_pick(const scene::Storage& scene_storage,
                                         graphics::PickingTarget& target,
                                         const scene::VisibilityFilter& visibility,
                                         const clipping::ClippingFilter& clipping_filter,
                                         const GpuPickRequest& request) {
    const Result<void> context = validate_gpu_picking_context(scene_storage);
    if (!context) {
        return context.error();
    }
    const Extent2D target_extent = target.extent();
    if (target_extent.width == 0U || target_extent.height == 0U) {
        return GpuPickResult{};
    }
    if (!valid_pick_coordinates(target_extent, request)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Picking coordinates are outside the viewport extent"};
    }
    const double pass_begin = device_->monotonic_time_milliseconds();
    Result<RenderList> list_result = build_render_list(
        scene_storage, request.camera, request.viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuPickResult{};
    }
    const Result<void> item_count = validate_pick_item_count(list);
    if (!item_count) {
        return item_count.error();
    }
    Result<std::uint64_t> draw_calls =
        draw_picking_items(scene_storage, target, list, clipping_filter);
    if (!draw_calls) {
        return draw_calls.error();
    }
    const double pass_end = device_->monotonic_time_milliseconds();

    const double readback_begin = device_->monotonic_time_milliseconds();
    Result<std::optional<graphics::PickingPixel>> pixel =
        device_->read_picking_pixel(target, request.target_position_pixels);
    if (!pixel) {
        return pixel.error();
    }
    GpuPickResult result;
    result.draw_calls = draw_calls.value();
    result.pixels_read = 1;
    result.pass_milliseconds = pass_end - pass_begin;
    result.readback_milliseconds = device_->monotonic_time_milliseconds() - readback_begin;
    return resolve_gpu_pick(list, pixel.value(), request, result);
}

Result<GpuFocusDepthAnchorResult> Renderer::gpu_focus_depth_anchor(
    const scene::Storage& scene_storage, graphics::PickingTarget& target,
    const scene::VisibilityFilter& visibility, const clipping::ClippingFilter& clipping_filter,
    const GpuFocusDepthRequest& request) {
    const Result<void> context = validate_gpu_picking_context(scene_storage);
    if (!context) {
        return context.error();
    }
    const Extent2D target_extent = target.extent();
    if (!valid_focus_extents(target_extent, request)) {
        return GpuFocusDepthAnchorResult{};
    }
    const double pass_begin = device_->monotonic_time_milliseconds();
    Result<RenderList> list_result = build_render_list(
        scene_storage, request.camera, request.viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuFocusDepthAnchorResult{};
    }
    const Result<void> item_count = validate_pick_item_count(list);
    if (!item_count) {
        return item_count.error();
    }
    Result<std::uint64_t> draw_calls =
        draw_picking_items(scene_storage, target, list, clipping_filter);
    if (!draw_calls) {
        return draw_calls.error();
    }
    const double pass_end = device_->monotonic_time_milliseconds();
    const double readback_begin = device_->monotonic_time_milliseconds();
    Result<std::vector<float>> depths = device_->read_picking_depths(target);
    if (!depths) {
        return depths.error();
    }
    const std::size_t expected_pixels =
        static_cast<std::size_t>(target_extent.width) * target_extent.height;
    if (depths.value().size() < expected_pixels) {
        return Error{ErrorCode::draw_submission_failed,
                     "Picking depth readback returned fewer pixels than the picking target extent"};
    }

    GpuFocusDepthAnchorResult result;
    result.draw_calls = draw_calls.value();
    result.pixels_read = static_cast<std::uint64_t>(depths.value().size());
    result.pass_milliseconds = pass_end - pass_begin;
    result.readback_milliseconds = device_->monotonic_time_milliseconds() - readback_begin;
    result.world_position = focus_world_position(
        list, request.viewport_extent, weighted_focus_depth(depths.value(), target_extent));
    return result;
}

} // namespace elf3d::renderer
