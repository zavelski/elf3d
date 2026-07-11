module;

#include <elf3d/clipping.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>

module elf.viewport;

import elf.clipping;
import elf.graphics;
import elf.math;
import elf.navigation;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.tool.clipping;
import elf.tool.measurement;
import elf.tool.selection;
import elf.tool.visibility;

namespace elf3d::viewport {
namespace {

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

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

[[nodiscard]] bool valid_tool(ViewportTool tool) noexcept {
    return tool == ViewportTool::selection || tool == ViewportTool::distance_measurement;
}

[[nodiscard]] bool measurement_preview_allowed(const ViewportInput& input,
                                               const NavigationSnapshot& navigation,
                                               ViewportTool active_tool) noexcept {
    return active_tool == ViewportTool::distance_measurement && input.is_hovered &&
           input.is_focused && !input.left_button_down && !input.middle_button_down &&
           !input.right_button_down && !navigation.is_orbiting && !navigation.is_panning &&
           !navigation.is_pointer_captured;
}

void append_overlay_lines(std::array<OverlayLineSegment, 2 + 4 + maximum_clipping_boxes * 12>& out,
                          std::size_t& count, std::span<const OverlayLineSegment> lines) noexcept {
    for (const OverlayLineSegment& line : lines) {
        if (count >= out.size()) {
            return;
        }
        out[count++] = line;
    }
}

void append_overlay_markers(std::array<OverlayPointMarker, 3>& out, std::size_t& count,
                            std::span<const OverlayPointMarker> markers) noexcept {
    for (const OverlayPointMarker& marker : markers) {
        if (count >= out.size()) {
            return;
        }
        out[count++] = marker;
    }
}

} // namespace

Result<std::unique_ptr<OffscreenViewport>>
OffscreenViewport::create(graphics::Device& device, Extent2D initial_extent) {
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

    return std::make_unique<OffscreenViewport>(ConstructionKey{},
                                               std::move(target_result).value(),
                                               std::move(picking_target_result).value());
}

OffscreenViewport::OffscreenViewport(
    ConstructionKey, std::unique_ptr<graphics::RenderTarget> render_target,
    std::unique_ptr<graphics::PickingTarget> picking_target) noexcept
    : render_target_(std::move(render_target)), picking_target_(std::move(picking_target)) {
    set_basic_lighting(BasicLighting{});
}

Extent2D OffscreenViewport::extent() const noexcept {
    return render_target_ != nullptr ? render_target_->extent() : Extent2D{};
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

void OffscreenViewport::set_clear_color(Color4 color) noexcept {
    clear_color_ = math::clamp_color(color);
}

Color4 OffscreenViewport::clear_color() const noexcept {
    return clear_color_;
}

void OffscreenViewport::set_basic_lighting(const BasicLighting& lighting) noexcept {
    const float direction_length_squared = lighting.direction.x * lighting.direction.x +
                                           lighting.direction.y * lighting.direction.y +
                                           lighting.direction.z * lighting.direction.z;
    const Float3 direction = math::is_finite(lighting.direction) &&
                                     std::isfinite(direction_length_squared) &&
                                     direction_length_squared > 0.000001F
                                 ? lighting.direction
                                 : Float3{-0.5F, -1.0F, -0.3F};
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    lighting_.direction = {direction.x / length, direction.y / length, direction.z / length};
    lighting_.color = math::clamp_color(lighting.color);
    lighting_.ambient_intensity = std::isfinite(lighting.ambient_intensity)
                                      ? std::clamp(lighting.ambient_intensity, 0.0F, 2.0F)
                                      : 0.08F;
    lighting_.diffuse_intensity = std::isfinite(lighting.diffuse_intensity)
                                      ? std::clamp(lighting.diffuse_intensity, 0.0F, 10.0F)
                                      : 3.0F;
}

BasicLighting OffscreenViewport::basic_lighting() const noexcept {
    return lighting_;
}

Result<std::optional<PickHit>>
OffscreenViewport::pick_gpu_first(renderer::Renderer& renderer,
                                  picking::PickingService& picking,
                                  const scene::Storage& scene, EntityId camera,
                                  Float2 position_pixels, const PickOptions& options,
                                  const scene::VisibilityFilter& visibility,
                                  const clipping::ClippingFilter& clipping_filter) {
    const Extent2D viewport_extent = extent();
    reset_latest_gpu_picking_statistics(gpu_picking_statistics_);

    const auto cpu_fallback = [&]() -> Result<std::optional<PickHit>> {
        ++gpu_picking_statistics_.latest_cpu_fallbacks;
        ++gpu_picking_statistics_.lifetime_cpu_fallbacks;
        return picking.pick(scene, camera, viewport_extent, position_pixels, options, visibility,
                            clipping_filter);
    };

    if (picking_target_ == nullptr) {
        return cpu_fallback();
    }
    if (viewport_extent.width == 0 || viewport_extent.height == 0) {
        return std::optional<PickHit>{};
    }
    if (!contains_viewport_position(viewport_extent, position_pixels)) {
        return cpu_fallback();
    }
    const Result<void> resize_result =
        picking_target_->resize(picking_target_extent(viewport_extent));
    if (!resize_result) {
        return cpu_fallback();
    }
    const Extent2D target_extent = picking_target_->extent();
    if (target_extent.width == 0 || target_extent.height == 0) {
        return std::optional<PickHit>{};
    }
    const Float2 gpu_position =
        scale_viewport_position_to_picking_target(position_pixels, viewport_extent, target_extent);

    ++gpu_picking_statistics_.latest_gpu_requests;
    ++gpu_picking_statistics_.lifetime_gpu_requests;
    Result<renderer::GpuPickResult> gpu_result =
        renderer.gpu_pick(scene, camera, *picking_target_, gpu_position, viewport_extent,
                          position_pixels, visibility, clipping_filter);
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
    Result<std::optional<PickHit>> refined =
        picking.refine_candidate(scene, camera, viewport_extent, position_pixels, options,
                                 visibility, clipping_filter, candidate);
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
OffscreenViewport::focus_depth_anchor(renderer::Renderer& renderer,
                                      const scene::Storage& scene, EntityId camera,
                                      const scene::VisibilityFilter& visibility,
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
    Result<renderer::GpuFocusDepthAnchorResult> anchor_result =
        renderer.gpu_focus_depth_anchor(scene, camera, *picking_target_, viewport_extent,
                                        visibility, clipping_filter);
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

Result<void> OffscreenViewport::update_navigation(renderer::Renderer& renderer,
                                                  picking::PickingService& picking,
                                                  scene::Storage& scene, EntityId camera,
                                                  const ViewportInput& input) {
    selection_.validate_against(scene);
    visibility_.validate_against(scene);
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const scene::VisibilityFilter& visibility_filter = visibility.value();
    const clipping::ClippingFilter& clipping_filter_value = clipping_filter.value();
    Result<navigation::NavigationUpdate> navigation_result =
        navigation_.update(scene, camera, extent(), input,
                           selection_.settings().click_drag_threshold_pixels, visibility_filter);
    if (!navigation_result) {
        return navigation_result.error();
    }
    const navigation::NavigationUpdate& navigation_update = navigation_result.value();
    if (navigation_update.orbit_start_position_pixels.has_value() &&
        !navigation_.has_screen_anchor()) {
        Result<std::optional<Float3>> anchor_position =
            focus_depth_anchor(renderer, scene, camera, visibility_filter, clipping_filter_value);
        if (anchor_position && anchor_position.value().has_value()) {
            const std::optional<Float3>& anchor_value = anchor_position.value();
            Result<void> anchor =
                navigation_.set_screen_anchor(scene, camera, *anchor_value);
            if (!anchor) {
                return anchor.error();
            }
        }
    }
    if (navigation_update.click_position_pixels.has_value()) {
        const Float2 click_position = *navigation_update.click_position_pixels;
        if (active_tool_ == ViewportTool::distance_measurement) {
            Result<std::optional<PickHit>> hit_result =
                pick_gpu_first(renderer, picking, scene, camera, click_position, PickOptions{},
                               visibility_filter, clipping_filter_value);
            if (!hit_result) {
                return hit_result.error();
            }
            const std::optional<PickHit>& hit = hit_result.value();
            if (hit.has_value()) {
                Result<void> placed = measurement_.place_hit(scene, *hit);
                if (!placed) {
                    return placed.error();
                }
            }
        } else {
            if (input.control_down || input.shift_down) {
                Result<std::optional<PickHit>> hit_result =
                    pick_gpu_first(renderer, picking, scene, camera, click_position, PickOptions{},
                                   visibility_filter, clipping_filter_value);
                if (!hit_result) {
                    return hit_result.error();
                }
                const std::optional<PickHit>& hit = hit_result.value();
                if (input.shift_down && hit.has_value()) {
                    Result<void> hidden = scene.set_entity_visible(hit->entity, false);
                    if (!hidden) {
                        return hidden.error();
                    }
                    selection_.validate_against(scene);
                    return {};
                }
                if (input.control_down && active_tool_ == ViewportTool::selection &&
                    selection_.enabled()) {
                    Result<std::optional<PickHit>> selection_result =
                        selection_.select_hit(scene, hit);
                    if (!selection_result) {
                        return selection_result.error();
                    }
                }
            } else {
                Result<std::optional<PickHit>> hit_result =
                    pick_gpu_first(renderer, picking, scene, camera, click_position, PickOptions{},
                                   visibility_filter, clipping_filter_value);
                if (!hit_result) {
                    return hit_result.error();
                }
                const std::optional<PickHit>& hit = hit_result.value();
                if (hit.has_value()) {
                    Result<void> anchor =
                        navigation_.set_screen_anchor(scene, camera, hit->world_position);
                    if (!anchor) {
                        return anchor.error();
                    }
                }
            }
        }
    }

    const bool allow_preview =
        navigation_.has_state() &&
        measurement_preview_allowed(input, navigation_.snapshot(), active_tool_);
    if (!allow_preview) {
        measurement_.clear_preview();
        return {};
    }
    if (measurement_.wants_preview_pick(scene, visibility_filter, clipping_filter_value,
                                        input.pointer_position_pixels, allow_preview)) {
        measurement_.record_preview_pick(scene, visibility_filter, clipping_filter_value,
                                         input.pointer_position_pixels);
        Result<std::optional<PickHit>> hit_result =
            pick_gpu_first(renderer, picking, scene, camera, input.pointer_position_pixels,
                           PickOptions{}, visibility_filter, clipping_filter_value);
        if (!hit_result) {
            return hit_result.error();
        }
        const std::optional<PickHit>& hit = hit_result.value();
        if (hit.has_value()) {
            Result<void> preview = measurement_.update_preview(scene, *hit);
            if (!preview) {
                return preview.error();
            }
        } else {
            measurement_.clear_preview();
        }
    }
    return {};
}

Result<void> OffscreenViewport::set_examine_pivot(scene::Storage& scene, EntityId camera,
                                                  Float3 world_position) {
    return navigation_.set_screen_anchor(scene, camera, world_position);
}

Result<void> OffscreenViewport::fit_to_scene(scene::Storage& scene, EntityId camera) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const std::optional<Bounds3> bounds =
        tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
    if (!bounds.has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera fitting requires visible content after clipping"};
    }
    return navigation_.fit_to_bounds(scene, camera, extent(), *bounds);
}

Result<void> OffscreenViewport::reset_view(scene::Storage& scene, EntityId camera) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const std::optional<Bounds3> bounds =
        tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
    if (!bounds.has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera reset requires visible content after clipping"};
    }
    return navigation_.reset_to_bounds(scene, camera, extent(), *bounds);
}

Result<void> OffscreenViewport::synchronize_navigation(const scene::Storage& scene,
                                                       EntityId camera) {
    return navigation_.synchronize(scene, camera);
}

void OffscreenViewport::cancel_interaction() noexcept {
    navigation_.cancel_interaction();
}

void OffscreenViewport::set_navigation_enabled(bool enabled) noexcept {
    navigation_.set_enabled(enabled);
}

bool OffscreenViewport::navigation_enabled() const noexcept {
    return navigation_.enabled();
}

Result<void> OffscreenViewport::set_navigation_settings(const OrbitNavigationSettings& settings) {
    return navigation_.set_settings(settings);
}

OrbitNavigationSettings OffscreenViewport::navigation_settings() const noexcept {
    return navigation_.settings();
}

std::optional<NavigationSnapshot> OffscreenViewport::navigation_snapshot() const noexcept {
    return navigation_.has_state() ? std::optional{navigation_.snapshot()} : std::nullopt;
}

void OffscreenViewport::set_active_tool(ViewportTool tool) noexcept {
    if (!valid_tool(tool)) {
        tool = ViewportTool::selection;
    }
    if (active_tool_ == tool) {
        return;
    }
    navigation_.cancel_interaction();
    if (active_tool_ == ViewportTool::distance_measurement &&
        tool != ViewportTool::distance_measurement) {
        measurement_.cancel_incomplete();
    }
    active_tool_ = tool;
}

ViewportTool OffscreenViewport::active_tool() const noexcept {
    return active_tool_;
}

Result<Ray3> OffscreenViewport::make_picking_ray(picking::PickingService& picking,
                                                 const scene::Storage& scene, EntityId camera,
                                                 Float2 position_pixels) const {
    return picking.make_picking_ray(scene, camera, extent(), position_pixels);
}

Result<std::optional<PickHit>>
OffscreenViewport::pick(renderer::Renderer& renderer, picking::PickingService& picking,
                        const scene::Storage& scene, EntityId camera, Float2 position_pixels,
                        const PickOptions& options) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return pick_gpu_first(renderer, picking, scene, camera, position_pixels, options,
                          visibility.value(), clipping_filter.value());
}

Result<std::optional<PickHit>>
OffscreenViewport::select_at(renderer::Renderer& renderer, picking::PickingService& picking,
                             const scene::Storage& scene, EntityId camera,
                             Float2 position_pixels) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    Result<std::optional<PickHit>> hit_result =
        pick_gpu_first(renderer, picking, scene, camera, position_pixels, PickOptions{},
                       visibility.value(), clipping_filter.value());
    if (!hit_result) {
        return hit_result.error();
    }
    return selection_.select_hit(scene, hit_result.value());
}

Result<void> OffscreenViewport::set_selected_entity(const scene::Storage& scene, EntityId entity) {
    return selection_.set_selected_entity(scene, entity);
}

void OffscreenViewport::clear_selection() noexcept {
    selection_.clear();
}

void OffscreenViewport::clear_scene_selection(SceneId scene) noexcept {
    selection_.clear_scene(scene);
}

bool OffscreenViewport::has_selection() const noexcept {
    return selection_.has_selection();
}

std::optional<EntityId> OffscreenViewport::selected_entity() const noexcept {
    return selection_.selected_entity();
}

std::optional<PickHit> OffscreenViewport::selection_hit() const noexcept {
    return selection_.selection_hit();
}

SelectionSnapshot OffscreenViewport::selection_snapshot() const noexcept {
    return selection_.snapshot();
}

void OffscreenViewport::set_selection_enabled(bool enabled) noexcept {
    selection_.set_enabled(enabled);
    if (!enabled) {
        navigation_.cancel_interaction();
    }
}

bool OffscreenViewport::selection_enabled() const noexcept {
    return selection_.enabled();
}

Result<void> OffscreenViewport::set_selection_settings(const SelectionSettings& settings) {
    return selection_.set_settings(settings);
}

SelectionSettings OffscreenViewport::selection_settings() const noexcept {
    return selection_.settings();
}

PickingStatistics
OffscreenViewport::picking_statistics(const picking::PickingService& picking) const noexcept {
    PickingStatistics result = picking.statistics();
    result.latest_gpu_requests = gpu_picking_statistics_.latest_gpu_requests;
    result.latest_gpu_hits = gpu_picking_statistics_.latest_gpu_hits;
    result.latest_gpu_misses = gpu_picking_statistics_.latest_gpu_misses;
    result.latest_gpu_draw_calls = gpu_picking_statistics_.latest_gpu_draw_calls;
    result.latest_gpu_pixels_read = gpu_picking_statistics_.latest_gpu_pixels_read;
    result.latest_cpu_refinements = gpu_picking_statistics_.latest_cpu_refinements;
    result.latest_cpu_fallbacks = gpu_picking_statistics_.latest_cpu_fallbacks;
    if (result.latest_gpu_requests != 0 && result.latest_cpu_refinements == 0 &&
        result.latest_cpu_fallbacks == 0) {
        result.latest_instance_bounds_tests = 0;
        result.latest_mesh_bounds_tests = 0;
        result.latest_bvh_node_tests = 0;
        result.latest_triangle_tests = 0;
        result.latest_bvh_builds = 0;
        result.latest_clipping_bounds_rejected = 0;
        result.latest_clipping_hits_rejected = 0;
        result.latest_clipping_hits_accepted = 0;
    }
    result.lifetime_gpu_requests = gpu_picking_statistics_.lifetime_gpu_requests;
    result.lifetime_gpu_hits = gpu_picking_statistics_.lifetime_gpu_hits;
    result.lifetime_gpu_misses = gpu_picking_statistics_.lifetime_gpu_misses;
    result.lifetime_cpu_refinements = gpu_picking_statistics_.lifetime_cpu_refinements;
    result.lifetime_cpu_fallbacks = gpu_picking_statistics_.lifetime_cpu_fallbacks;
    return result;
}

Result<void> OffscreenViewport::begin_distance_measurement() {
    set_active_tool(ViewportTool::distance_measurement);
    return {};
}

void OffscreenViewport::cancel_distance_measurement() noexcept {
    measurement_.cancel_incomplete();
}

void OffscreenViewport::clear_distance_measurement() noexcept {
    measurement_.clear();
}

void OffscreenViewport::clear_scene_measurement(SceneId scene) noexcept {
    measurement_.clear_scene(scene);
}

DistanceMeasurementSnapshot
OffscreenViewport::distance_measurement_snapshot(const scene::Storage& scene) noexcept {
    const Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        DistanceMeasurementSnapshot result;
        result.diagnostic = visibility.error();
        return result;
    }
    const Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        DistanceMeasurementSnapshot result;
        result.diagnostic = clipping_filter.error();
        return result;
    }
    return measurement_.snapshot(scene, visibility.value(), clipping_filter.value(), active_tool_);
}

Result<void>
OffscreenViewport::set_measurement_settings(const DistanceMeasurementSettings& settings) {
    return measurement_.set_settings(settings);
}

DistanceMeasurementSettings OffscreenViewport::measurement_settings() const noexcept {
    return measurement_.settings();
}

MeasurementStatistics OffscreenViewport::measurement_statistics() const noexcept {
    return measurement_.statistics();
}

Result<ProjectedViewportPoint>
OffscreenViewport::project_world_to_viewport(const scene::Storage& scene, EntityId camera,
                                             Float3 world_position) const {
    const Extent2D target_extent = extent();
    if (target_extent.width == 0 || target_extent.height == 0) {
        return Error{ErrorCode::invalid_viewport_dimensions,
                     "World-to-viewport projection requires a nonzero viewport extent"};
    }
    if (!finite_float3(world_position)) {
        return Error{ErrorCode::projection_failed,
                     "World-to-viewport projection requires a finite world position"};
    }

    const Result<PerspectiveCameraDescription> camera_description =
        scene.perspective_camera(camera);
    if (!camera_description) {
        return camera_description.error();
    }
    if (!scene::valid_camera_description(camera_description.value())) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "World-to-viewport projection requires a valid perspective camera"};
    }
    const Result<Float4x4> camera_world = scene.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<Float4x4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect =
        static_cast<float>(target_extent.width) / static_cast<float>(target_extent.height);
    const Result<Float4x4> projection = math::perspective_matrix(
        camera_description.value().vertical_field_of_view_radians, aspect,
        camera_description.value().near_plane, camera_description.value().far_plane);
    if (!projection) {
        return projection.error();
    }

    const Result<math::ViewportProjection> projected = math::project_world_to_viewport_point(
        view.value(), projection.value(), target_extent, world_position);
    if (!projected) {
        return projected.error();
    }

    return ProjectedViewportPoint{projected.value().position_pixels, projected.value().depth,
                                  projected.value().is_in_front,
                                  projected.value().is_inside_viewport};
}

Result<void> OffscreenViewport::isolate_entity(const scene::Storage& scene, EntityId entity) {
    return visibility_.isolate_entity(scene, entity);
}

void OffscreenViewport::clear_isolation() noexcept {
    visibility_.clear_isolation();
}

void OffscreenViewport::clear_scene_isolation(SceneId scene) noexcept {
    visibility_.clear_scene(scene);
}

bool OffscreenViewport::is_isolating() const noexcept {
    return visibility_.is_isolating();
}

std::optional<EntityId> OffscreenViewport::isolated_entity() const noexcept {
    return visibility_.isolated_entity();
}

Result<void> OffscreenViewport::hide_selected(scene::Storage& scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to hide"};
    }
    return scene.set_entity_visible(*selected, false);
}

Result<void> OffscreenViewport::show_selected(scene::Storage& scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to show"};
    }
    return scene.show_entity_and_ancestors(*selected);
}

Result<void> OffscreenViewport::isolate_selected(const scene::Storage& scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity,
                     "The viewport has no selected entity to isolate"};
    }
    return visibility_.isolate_entity(scene, *selected);
}

Result<std::optional<Bounds3>> OffscreenViewport::visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    const Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
}

Result<void> OffscreenViewport::set_section_plane(const SectionPlane& plane) {
    return clipping_.set_section_plane(plane);
}

void OffscreenViewport::clear_section_plane() noexcept {
    clipping_.clear_section_plane();
}

Result<std::uint32_t> OffscreenViewport::add_clipping_box(const ClippingBox& box) {
    return clipping_.add_box(box);
}

Result<void> OffscreenViewport::set_clipping_box(std::uint32_t index, const ClippingBox& box) {
    return clipping_.set_box(index, box);
}

Result<void> OffscreenViewport::remove_clipping_box(std::uint32_t index) {
    return clipping_.remove_box(index);
}

void OffscreenViewport::clear_clipping_boxes() noexcept {
    clipping_.clear_boxes();
}

void OffscreenViewport::clear_clipping() noexcept {
    clipping_.clear();
}

Result<void> OffscreenViewport::set_clipping_helpers_visible(bool visible) noexcept {
    return clipping_.set_helpers_visible(visible);
}

Result<void>
OffscreenViewport::set_clipping_helper_settings(const ClippingHelperSettings& settings) noexcept {
    return clipping_.set_helper_settings(settings);
}

Result<void> OffscreenViewport::reset_clipping_box_to_visible_bounds(const scene::Storage& scene,
                                                                     std::uint32_t index) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return clipping_.reset_box_to_visible_bounds(scene, visibility.value(), index);
}

Result<std::uint32_t>
OffscreenViewport::add_clipping_box_from_visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return clipping_.add_box_from_visible_bounds(scene, visibility.value());
}

ClippingSnapshot OffscreenViewport::clipping_snapshot() const noexcept {
    return clipping_.snapshot();
}

Result<void> OffscreenViewport::render(renderer::Renderer& renderer,
                                       const scene::Storage& scene, EntityId camera) {
    statistics_ = {};
    if (render_target_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (render_target_->extent().width == 0 || render_target_->extent().height == 0) {
        return {};
    }
    selection_.validate_against(scene);
    visibility_.validate_against(scene);
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    ViewportRenderOptions options;
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (selected.has_value()) {
        const SelectionSettings settings = selection_.settings();
        options.highlight = EntityHighlight{*selected, settings.highlight_color,
                                            settings.highlight_strength};
    }
    Result<tools::measurement::MeasurementOverlay> overlay =
        measurement_.overlay(scene, visibility.value(), clipping_filter.value());
    if (!overlay) {
        return overlay.error();
    }
    measurement_overlay_ = overlay.value();
    clipping_overlay_ = {};
    const std::optional<Bounds3> helper_bounds =
        scene.visible_world_bounds(visibility.value());
    const Result<tools::clipping::ClippingOverlay> clipping_overlay =
        clipping_.overlay(helper_bounds);
    if (!clipping_overlay) {
        return clipping_overlay.error();
    }
    clipping_overlay_ = clipping_overlay.value();

    overlay_line_count_ = 0;
    overlay_marker_count_ = 0;
    append_overlay_lines(overlay_lines_, overlay_line_count_, measurement_overlay_.line_span());
    append_overlay_lines(overlay_lines_, overlay_line_count_, clipping_overlay_.line_span());
    append_overlay_markers(overlay_markers_, overlay_marker_count_,
                           measurement_overlay_.marker_span());
    options.overlay_lines =
        std::span<const OverlayLineSegment>{overlay_lines_.data(), overlay_line_count_};
    options.overlay_markers =
        std::span<const OverlayPointMarker>{overlay_markers_.data(), overlay_marker_count_};
    Result<RenderStatistics> render_result =
        renderer.render(scene, camera, *render_target_, clear_color_, lighting_, options,
                        visibility.value(), clipping_filter.value());
    if (!render_result) {
        return render_result.error();
    }
    statistics_ = render_result.value();
    return {};
}

RenderStatistics OffscreenViewport::statistics() const noexcept {
    return statistics_;
}

TextureHandle OffscreenViewport::color_texture() const noexcept {
    return render_target_ != nullptr ? render_target_->color_texture() : TextureHandle{};
}

bool OffscreenViewport::framebuffer_valid() const noexcept {
    return render_target_ != nullptr && render_target_->is_valid();
}

} // namespace elf3d::viewport
