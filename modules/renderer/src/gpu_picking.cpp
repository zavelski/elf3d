module;

#include <elf3d/viewport.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

} // namespace

Result<GpuPickResult> Renderer::gpu_pick(const scene::Storage& scene_storage, EntityId camera,
                                         graphics::PickingTarget& target,
                                         Float2 target_position_pixels, Extent2D viewport_extent,
                                         Float2 viewport_position_pixels,
                                         const scene::VisibilityFilter& visibility,
                                         const clipping::ClippingFilter& clipping_filter) {
    if (!scene_storage.belongs_to_engine(engine_token_)) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    const Extent2D target_extent = target.extent();
    if (target_extent.width == 0 || target_extent.height == 0) {
        return GpuPickResult{};
    }
    if (!std::isfinite(target_position_pixels.x) || !std::isfinite(target_position_pixels.y) ||
        target_position_pixels.x < 0.0F || target_position_pixels.y < 0.0F ||
        target_position_pixels.x >= static_cast<float>(target_extent.width) ||
        target_position_pixels.y >= static_cast<float>(target_extent.height) ||
        viewport_extent.width == 0U || viewport_extent.height == 0U ||
        !std::isfinite(viewport_position_pixels.x) || !std::isfinite(viewport_position_pixels.y) ||
        viewport_position_pixels.x < 0.0F || viewport_position_pixels.y < 0.0F ||
        viewport_position_pixels.x >= static_cast<float>(viewport_extent.width) ||
        viewport_position_pixels.y >= static_cast<float>(viewport_extent.height)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Picking coordinates are outside the viewport extent"};
    }

    Result<RenderList> list_result =
        build_render_list(scene_storage, camera, viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuPickResult{};
    }
    if (list.items.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1U) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "GPU picking cannot encode the number of visible render items"};
    }

    GpuPickResult result;
    const Result<void> clear_result = target.clear();
    if (!clear_result) {
        return clear_result.error();
    }

    std::vector<std::size_t> item_indices;
    item_indices.reserve(list.items.size() + 1U);
    item_indices.push_back(0U);

    for (std::size_t item_index = 0; item_index < list.items.size(); ++item_index) {
        const RenderItem& item = list.items[item_index];
        const Result<scene::RuntimePrimitiveView> primitive =
            scene_storage.runtime_primitive(item.entity, item.primitive_index);
        if (!primitive) {
            return primitive.error();
        }
        Result<graphics::StaticMesh*> gpu_mesh_result =
            cached_mesh(scene_storage.id(), primitive.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }
        graphics::StaticMesh* const gpu_mesh = gpu_mesh_result.value();

        item_indices.push_back(item_index);
        graphics::PickingDrawDescription draw;
        draw.model_matrix = item.model_matrix.elements;
        draw.view_matrix = list.view_matrix.elements;
        draw.projection_matrix = list.projection_matrix.elements;
        draw.object_id = static_cast<std::uint32_t>(item_indices.size() - 1U);
        draw.primitive_index = item.primitive_index;
        draw.double_sided = primitive.value().material_view.double_sided;
        draw.front_face_clockwise = item.orientation_reversed;
        apply_clipping_description(clipping_filter, draw);
        const Result<void> draw_result = device_->draw_picking_indexed(target, *gpu_mesh, draw);
        if (!draw_result) {
            return draw_result.error();
        }
        ++result.draw_calls;
    }

    Result<std::optional<graphics::PickingPixel>> pixel_result =
        device_->read_picking_pixel(target, target_position_pixels);
    if (!pixel_result) {
        return pixel_result.error();
    }
    result.pixels_read = 1;

    const std::optional<graphics::PickingPixel>& pixel_value = pixel_result.value();
    if (!pixel_value.has_value()) {
        return result;
    }
    const graphics::PickingPixel pixel = *pixel_value;
    if (pixel.object_id == 0U || pixel.object_id >= item_indices.size()) {
        return result;
    }

    const std::size_t item_index = item_indices[pixel.object_id];
    result.hit = make_gpu_pick_hit(list, list.items[item_index], pixel, viewport_extent,
                                   viewport_position_pixels);
    return result;
}

Result<GpuFocusDepthAnchorResult>
Renderer::gpu_focus_depth_anchor(const scene::Storage& scene_storage, EntityId camera,
                                 graphics::PickingTarget& target, Extent2D viewport_extent,
                                 const scene::VisibilityFilter& visibility,
                                 const clipping::ClippingFilter& clipping_filter) {
    if (!scene_storage.belongs_to_engine(engine_token_)) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    const Extent2D target_extent = target.extent();
    if (target_extent.width == 0 || target_extent.height == 0 || viewport_extent.width == 0U ||
        viewport_extent.height == 0U) {
        return GpuFocusDepthAnchorResult{};
    }

    Result<RenderList> list_result =
        build_render_list(scene_storage, camera, viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuFocusDepthAnchorResult{};
    }
    if (list.items.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1U) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "GPU picking cannot encode the number of visible render items"};
    }

    GpuFocusDepthAnchorResult result;
    const Result<void> clear_result = target.clear();
    if (!clear_result) {
        return clear_result.error();
    }

    for (std::size_t item_index = 0; item_index < list.items.size(); ++item_index) {
        const RenderItem& item = list.items[item_index];
        const Result<scene::RuntimePrimitiveView> primitive =
            scene_storage.runtime_primitive(item.entity, item.primitive_index);
        if (!primitive) {
            return primitive.error();
        }
        Result<graphics::StaticMesh*> gpu_mesh_result =
            cached_mesh(scene_storage.id(), primitive.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }
        graphics::StaticMesh* const gpu_mesh = gpu_mesh_result.value();

        graphics::PickingDrawDescription draw;
        draw.model_matrix = item.model_matrix.elements;
        draw.view_matrix = list.view_matrix.elements;
        draw.projection_matrix = list.projection_matrix.elements;
        draw.object_id = static_cast<std::uint32_t>(item_index + 1U);
        draw.primitive_index = item.primitive_index;
        draw.double_sided = primitive.value().material_view.double_sided;
        draw.front_face_clockwise = item.orientation_reversed;
        apply_clipping_description(clipping_filter, draw);
        const Result<void> draw_result = device_->draw_picking_indexed(target, *gpu_mesh, draw);
        if (!draw_result) {
            return draw_result.error();
        }
        ++result.draw_calls;
    }

    Result<std::vector<float>> depths_result = device_->read_picking_depths(target);
    if (!depths_result) {
        return depths_result.error();
    }

    const std::vector<float>& depths = depths_result.value();
    result.pixels_read = static_cast<std::uint64_t>(depths.size());
    const std::size_t expected_pixels = static_cast<std::size_t>(target_extent.width) *
                                        static_cast<std::size_t>(target_extent.height);
    if (depths.size() < expected_pixels) {
        return Error{ErrorCode::draw_submission_failed,
                     "Picking depth readback returned fewer pixels than the picking target extent"};
    }

    double weighted_depth = 0.0;
    double total_weight = 0.0;
    for (std::uint32_t y = 0; y < target_extent.height; ++y) {
        for (std::uint32_t x = 0; x < target_extent.width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(target_extent.width) +
                static_cast<std::size_t>(x);
            const float depth = depths[index];
            if (!std::isfinite(depth) || depth <= 0.0F || depth >= 1.0F) {
                continue;
            }
            const double weight = focus_depth_weight(target_extent, x, y);
            if (weight <= 0.0) {
                continue;
            }
            weighted_depth += static_cast<double>(depth) * weight;
            total_weight += weight;
        }
    }

    if (total_weight > 0.0 && std::isfinite(total_weight)) {
        const Float2 anchor_position{static_cast<float>(viewport_extent.width) * 0.5F,
                                     static_cast<float>(viewport_extent.height) * 0.5F};
        const float anchor_depth = static_cast<float>(weighted_depth / total_weight);
        const Result<Float3> anchor =
            math::unproject_viewport_point(list.view_matrix, list.projection_matrix,
                                           viewport_extent, anchor_position, anchor_depth);
        if (anchor && math::is_finite(anchor.value())) {
            result.world_position = anchor.value();
        }
    }
    return result;
}

} // namespace elf3d::renderer
