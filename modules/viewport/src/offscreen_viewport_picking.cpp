module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

module elf.viewport;

import elf.clipping;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;

namespace elf3d::viewport {
namespace {

constexpr std::uint32_t picking_target_downsample = 2U;
constexpr std::uint32_t focus_depth_max_side = 256U;

[[nodiscard]] std::uint32_t picking_dimension(std::uint32_t dimension) noexcept {
    if (dimension == 0U) {
        return 0U;
    }
    return dimension / picking_target_downsample +
           (dimension % picking_target_downsample == 0U ? 0U : 1U);
}

[[nodiscard]] Extent2D picking_target_extent(Extent2D viewport_extent) noexcept {
    return {picking_dimension(viewport_extent.width), picking_dimension(viewport_extent.height)};
}

[[nodiscard]] Extent2D focus_depth_target_extent(Extent2D viewport_extent) noexcept {
    if (viewport_extent.width == 0U || viewport_extent.height == 0U) {
        return {};
    }
    if (viewport_extent.width <= focus_depth_max_side &&
        viewport_extent.height <= focus_depth_max_side) {
        return viewport_extent;
    }

    if (viewport_extent.width >= viewport_extent.height) {
        const std::uint32_t height = static_cast<std::uint32_t>(std::max<std::uint64_t>(
            1U, (static_cast<std::uint64_t>(focus_depth_max_side) * viewport_extent.height +
                 viewport_extent.width / 2U) /
                    viewport_extent.width));
        return {focus_depth_max_side, height};
    }

    const std::uint32_t width = static_cast<std::uint32_t>(std::max<std::uint64_t>(
        1U, (static_cast<std::uint64_t>(focus_depth_max_side) * viewport_extent.width +
             viewport_extent.height / 2U) /
                viewport_extent.height));
    return {width, focus_depth_max_side};
}

[[nodiscard]] bool contains_viewport_position(Extent2D extent, Float2 position_pixels) noexcept {
    return std::isfinite(position_pixels.x) && std::isfinite(position_pixels.y) &&
           position_pixels.x >= 0.0F && position_pixels.y >= 0.0F &&
           position_pixels.x < static_cast<float>(extent.width) &&
           position_pixels.y < static_cast<float>(extent.height);
}

[[nodiscard]] float scale_pixel_coordinate(float position, std::uint32_t source_dimension,
                                           std::uint32_t target_dimension) noexcept {
    if (source_dimension == 0U || target_dimension == 0U || source_dimension == target_dimension) {
        return position;
    }
    const double source = static_cast<double>(source_dimension);
    const double target = static_cast<double>(target_dimension);
    const double scaled = (static_cast<double>(position) + 0.5) * target / source - 0.5;
    const float maximum = std::nextafter(static_cast<float>(target_dimension), 0.0F);
    return std::clamp(static_cast<float>(scaled), 0.0F, maximum);
}

[[nodiscard]] Float2 scale_viewport_position_to_picking_target(Float2 position_pixels,
                                                               Extent2D viewport_extent,
                                                               Extent2D target_extent) noexcept {
    return {
        scale_pixel_coordinate(position_pixels.x, viewport_extent.width, target_extent.width),
        scale_pixel_coordinate(position_pixels.y, viewport_extent.height, target_extent.height)};
}

void reset_latest_gpu_picking_statistics(PickingStatistics& statistics) noexcept {
    statistics.latest_gpu_requests = 0;
    statistics.latest_gpu_hits = 0;
    statistics.latest_gpu_misses = 0;
    statistics.latest_gpu_draw_calls = 0;
    statistics.latest_gpu_pixels_read = 0;
    statistics.latest_cpu_refinements = 0;
    statistics.latest_cpu_fallbacks = 0;
}

enum class GpuPickAction : std::uint8_t {
    render,
    empty,
    cpu_fallback,
};

struct GpuPickPreparation final {
    GpuPickAction action{GpuPickAction::empty};
    Float2 target_position{};
};

[[nodiscard]] GpuPickPreparation prepare_gpu_pick_target(graphics::PickingTarget* target,
                                                         Extent2D viewport_extent,
                                                         Float2 position_pixels) {
    if (target == nullptr) {
        return {GpuPickAction::cpu_fallback, {}};
    }
    if (viewport_extent.width == 0U || viewport_extent.height == 0U) {
        return {};
    }
    if (!contains_viewport_position(viewport_extent, position_pixels)) {
        return {GpuPickAction::cpu_fallback, {}};
    }
    if (!target->resize(picking_target_extent(viewport_extent))) {
        return {GpuPickAction::cpu_fallback, {}};
    }
    const Extent2D target_extent = target->extent();
    if (target_extent.width == 0U || target_extent.height == 0U) {
        return {};
    }
    return {GpuPickAction::render, scale_viewport_position_to_picking_target(
                                       position_pixels, viewport_extent, target_extent)};
}

} // namespace

Result<std::unique_ptr<OffscreenViewport>> OffscreenViewport::create(graphics::Device& device,
                                                                     Extent2D initial_extent) {
    Result<std::unique_ptr<graphics::RenderTarget>> target_result =
        device.create_render_target(initial_extent);
    if (!target_result) {
        return target_result.error();
    }
    Result<std::unique_ptr<graphics::PickingTarget>> picking_target_result =
        device.create_picking_target(picking_target_extent(initial_extent));
    if (!picking_target_result) {
        return picking_target_result.error();
    }

    return std::make_unique<OffscreenViewport>(ConstructionKey{}, std::move(target_result).value(),
                                               std::move(picking_target_result).value());
}

Extent2D OffscreenViewport::extent() const noexcept {
    return render_target_ != nullptr ? render_target_->extent() : Extent2D{};
}

ViewportInput
OffscreenViewport::normalized_pointer_hover(const ViewportInput& input) const noexcept {
    ViewportInput normalized = input;
    normalized.is_hovered =
        input.is_hovered && contains_viewport_position(extent(), input.pointer_position_pixels);
    return normalized;
}

Result<void> OffscreenViewport::resize(Extent2D extent) {
    if (render_target_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (extent == render_target_->extent()) {
        if (picking_target_ == nullptr ||
            picking_target_->extent() == picking_target_extent(extent)) {
            return {};
        }
        return picking_target_->resize(picking_target_extent(extent));
    }
    const Result<void> render_resize = render_target_->resize(extent);
    if (!render_resize) {
        return render_resize.error();
    }
    if (picking_target_ == nullptr) {
        return {};
    }
    return picking_target_->resize(picking_target_extent(extent));
}

Result<std::optional<PickHit>> OffscreenViewport::pick_gpu_first(
    renderer::Renderer& renderer, picking::PickingService& picking, const scene::Storage& scene,
    const scene::VisibilityFilter& visibility, const PickOperation& operation) {
    const Extent2D viewport_extent = extent();
    reset_latest_gpu_picking_statistics(gpu_picking_statistics_);
    const picking::PickRequest pick_request{operation.camera, viewport_extent,
                                            operation.position_pixels, operation.options};

    const auto cpu_fallback = [&]() -> Result<std::optional<PickHit>> {
        ++gpu_picking_statistics_.latest_cpu_fallbacks;
        ++gpu_picking_statistics_.lifetime_cpu_fallbacks;
        return picking.pick(scene, pick_request, visibility, operation.clipping_filter);
    };

    const GpuPickPreparation preparation =
        prepare_gpu_pick_target(picking_target_.get(), viewport_extent, operation.position_pixels);
    if (preparation.action == GpuPickAction::cpu_fallback) {
        return cpu_fallback();
    }
    if (preparation.action == GpuPickAction::empty) {
        return std::optional<PickHit>{};
    }

    ++gpu_picking_statistics_.latest_gpu_requests;
    ++gpu_picking_statistics_.lifetime_gpu_requests;
    const renderer::GpuPickRequest request{operation.camera, preparation.target_position,
                                           viewport_extent, operation.position_pixels};
    Result<renderer::GpuPickResult> gpu_result =
        renderer.gpu_pick(scene, *picking_target_, visibility, operation.clipping_filter, request);
    if (!gpu_result) {
        return cpu_fallback();
    }

    gpu_picking_statistics_.latest_gpu_draw_calls = gpu_result.value().draw_calls;
    gpu_picking_statistics_.latest_gpu_pixels_read = gpu_result.value().pixels_read;
    if (!gpu_result.value().hit.has_value()) {
        ++gpu_picking_statistics_.latest_gpu_misses;
        ++gpu_picking_statistics_.lifetime_gpu_misses;
        return std::optional<PickHit>{};
    }

    ++gpu_picking_statistics_.latest_gpu_hits;
    ++gpu_picking_statistics_.lifetime_gpu_hits;
    const renderer::GpuPickHit& gpu_hit = *gpu_result.value().hit;
    const picking::PickCandidate candidate{gpu_hit.entity, gpu_hit.mesh, gpu_hit.primitive_index,
                                           gpu_hit.triangle_index};
    Result<std::optional<PickHit>> refined = picking.refine_candidate(
        scene, pick_request, visibility, operation.clipping_filter, candidate);
    if (!refined) {
        return refined.error();
    }
    ++gpu_picking_statistics_.latest_cpu_refinements;
    ++gpu_picking_statistics_.lifetime_cpu_refinements;
    const std::optional<PickHit>& refined_hit = refined.value();
    if (!refined_hit.has_value()) {
        return cpu_fallback();
    }
    return refined;
}

Result<std::optional<Float3>>
OffscreenViewport::focus_depth_anchor(renderer::Renderer& renderer, const scene::Storage& scene,
                                      EntityId camera, const scene::VisibilityFilter& visibility,
                                      const clipping::ClippingFilter& clipping_filter) {
    reset_latest_gpu_picking_statistics(gpu_picking_statistics_);

    if (picking_target_ == nullptr) {
        return std::optional<Float3>{};
    }
    const Extent2D viewport_extent = extent();
    if (viewport_extent.width == 0U || viewport_extent.height == 0U) {
        return std::optional<Float3>{};
    }
    const Result<void> resize_result =
        picking_target_->resize(focus_depth_target_extent(viewport_extent));
    if (!resize_result) {
        return resize_result.error();
    }
    if (picking_target_->extent().width == 0U || picking_target_->extent().height == 0U) {
        return std::optional<Float3>{};
    }

    ++gpu_picking_statistics_.latest_gpu_requests;
    ++gpu_picking_statistics_.lifetime_gpu_requests;
    const renderer::GpuFocusDepthRequest request{camera, viewport_extent};
    Result<renderer::GpuFocusDepthAnchorResult> anchor_result = renderer.gpu_focus_depth_anchor(
        scene, *picking_target_, visibility, clipping_filter, request);
    if (!anchor_result) {
        return anchor_result.error();
    }
    gpu_picking_statistics_.latest_gpu_draw_calls = anchor_result.value().draw_calls;
    gpu_picking_statistics_.latest_gpu_pixels_read = anchor_result.value().pixels_read;
    if (!anchor_result.value().world_position.has_value()) {
        ++gpu_picking_statistics_.latest_gpu_misses;
        ++gpu_picking_statistics_.lifetime_gpu_misses;
        return std::optional<Float3>{};
    }

    ++gpu_picking_statistics_.latest_gpu_hits;
    ++gpu_picking_statistics_.lifetime_gpu_hits;
    return anchor_result.value().world_position;
}

} // namespace elf3d::viewport
