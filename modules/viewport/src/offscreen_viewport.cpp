module;

#include <elf3d/clipping.h>
#include <elf3d/viewport.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>

module elf.viewport;

import elf.math;
import elf.navigation;
import elf.tool.clipping;
import elf.tool.measurement;
import elf.tool.selection;
import elf.tool.visibility;

namespace elf3d::viewport {
namespace {

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
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

[[nodiscard]] std::optional<Bounds3>
clipping_helper_bounds(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                       const clipping::ClippingFilter& filter) noexcept {
    return filter.has_clipping() ? scene.visible_world_bounds(visibility) : std::nullopt;
}

} // namespace

class OffscreenViewport::State final {
  public:
    [[nodiscard]] Result<std::optional<Bounds3>>
    visible_bounds(const scene::Storage& scene, const scene::VisibilityFilter& visibility_filter,
                   const clipping::ClippingFilter& clipping_filter) {
        const bool matches =
            visible_bounds_valid && visible_bounds_scene == scene.id() &&
            visible_bounds_spatial_revision == scene.model_spatial_revision() &&
            visible_bounds_hierarchy_revision == visibility_filter.hierarchy_revision &&
            visible_bounds_visibility_revision == visibility_filter.visibility_revision &&
            visible_bounds_isolated_root == visibility_filter.isolated_root &&
            visible_bounds_clipping_revision == clipping_filter.revision;
        if (!matches) {
            cached_visible_bounds =
                tools::clipping::visible_bounds(scene, visibility_filter, clipping_filter);
            visible_bounds_scene = scene.id();
            visible_bounds_spatial_revision = scene.model_spatial_revision();
            visible_bounds_hierarchy_revision = visibility_filter.hierarchy_revision;
            visible_bounds_visibility_revision = visibility_filter.visibility_revision;
            visible_bounds_isolated_root = visibility_filter.isolated_root;
            visible_bounds_clipping_revision = clipping_filter.revision;
            visible_bounds_valid = true;
        }
        return cached_visible_bounds;
    }

    void begin_interaction_frame() noexcept {
        interaction_pick_consumed = false;
    }

    [[nodiscard]] bool consume_interaction_pick() noexcept {
        if (interaction_pick_consumed) {
            return false;
        }
        interaction_pick_consumed = true;
        return true;
    }

    navigation::OrbitNavigationController navigation;
    tools::selection::SelectionController selection;
    tools::visibility::VisibilityController visibility;
    tools::clipping::ClippingController clipping;
    tools::measurement::DistanceMeasurementController measurement;
    tools::measurement::MeasurementOverlay measurement_overlay;
    tools::clipping::ClippingOverlay clipping_overlay;
    SceneId visible_bounds_scene;
    std::optional<EntityId> visible_bounds_isolated_root;
    std::optional<Bounds3> cached_visible_bounds;
    std::uint64_t visible_bounds_spatial_revision = 0;
    std::uint64_t visible_bounds_hierarchy_revision = 0;
    std::uint64_t visible_bounds_visibility_revision = 0;
    std::uint64_t visible_bounds_clipping_revision = 0;
    bool visible_bounds_valid = false;
    bool interaction_pick_consumed = false;
};

OffscreenViewport::OffscreenViewport(ConstructionKey, Resources resources) noexcept
    : render_target_(std::move(resources.render_target)),
      picking_target_(std::move(resources.picking_target)),
      focus_depth_target_(std::move(resources.focus_depth_target)),
      state_(std::make_unique<State>()) {
    set_basic_lighting(BasicLighting{});
}

OffscreenViewport::~OffscreenViewport() noexcept = default;

Result<OffscreenViewport::InteractionFrame>
OffscreenViewport::interaction_frame(scene::Storage& scene, EntityId camera,
                                     const ViewportInput& input) {
    state_->selection.validate_against(scene);
    state_->visibility.validate_against(scene);
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return InteractionFrame{camera, input, std::move(visibility).value(),
                            std::move(clipping_filter).value()};
}

Result<void>
OffscreenViewport::update_orbit_screen_anchor(renderer::Renderer& renderer, scene::Storage& scene,
                                              const InteractionFrame& frame,
                                              std::optional<Float2> orbit_start_position) {
    if (!state_->navigation.settings().focus_depth_anchor_enabled ||
        !orbit_start_position.has_value() || state_->navigation.has_screen_anchor()) {
        return {};
    }
    if (!state_->consume_interaction_pick()) {
        return {};
    }
    Result<std::optional<Float3>> anchor =
        focus_depth_anchor(renderer, scene, frame.camera, frame.visibility, frame.clipping_filter);
    if (!anchor || !anchor.value().has_value()) {
        return {};
    }
    return state_->navigation.set_screen_anchor(scene, frame.camera, *anchor.value());
}

Result<void> OffscreenViewport::handle_measurement_click(renderer::Renderer& renderer,
                                                         picking::PickingService& picking,
                                                         scene::Storage& scene,
                                                         const InteractionFrame& frame,
                                                         Float2 click_position) {
    if (!state_->consume_interaction_pick()) {
        return {};
    }
    const PickOperation operation{frame.camera, click_position, PickOptions{},
                                  frame.clipping_filter};
    Result<std::optional<PickHit>> hit =
        pick_gpu_first(renderer, picking, scene, frame.visibility, operation);
    if (!hit) {
        return hit.error();
    }
    if (!hit.value().has_value()) {
        return {};
    }
    return state_->measurement.place_hit(scene, *hit.value());
}

Result<bool> OffscreenViewport::hide_clicked_entity(scene::Storage& scene,
                                                    const InteractionFrame& frame,
                                                    const std::optional<PickHit>& hit) {
    if (!frame.input.shift_down || !hit.has_value()) {
        return false;
    }
    Result<void> hidden = scene.set_entity_visible(hit->entity, false);
    if (!hidden) {
        return hidden.error();
    }
    state_->selection.validate_against(scene);
    return true;
}

Result<void> OffscreenViewport::select_control_click(const scene::Storage& scene,
                                                     const InteractionFrame& frame,
                                                     const std::optional<PickHit>& hit) {
    if (!frame.input.control_down || active_tool_ != ViewportTool::selection ||
        !state_->selection.enabled()) {
        return {};
    }
    Result<std::optional<PickHit>> selected = state_->selection.select_hit(scene, hit);
    return selected ? Result<void>{} : Result<void>{selected.error()};
}

Result<void> OffscreenViewport::anchor_plain_click(scene::Storage& scene,
                                                   const InteractionFrame& frame,
                                                   const std::optional<PickHit>& hit) {
    if (frame.input.control_down || frame.input.shift_down || !hit.has_value()) {
        return {};
    }
    return state_->navigation.set_screen_anchor(scene, frame.camera, hit->world_position);
}

Result<void> OffscreenViewport::handle_selection_click(renderer::Renderer& renderer,
                                                       picking::PickingService& picking,
                                                       scene::Storage& scene,
                                                       const InteractionFrame& frame,
                                                       Float2 click_position) {
    if (!state_->consume_interaction_pick()) {
        return {};
    }
    const PickOperation operation{frame.camera, click_position, PickOptions{},
                                  frame.clipping_filter};
    Result<std::optional<PickHit>> hit =
        pick_gpu_first(renderer, picking, scene, frame.visibility, operation);
    if (!hit) {
        return hit.error();
    }
    Result<bool> hidden = hide_clicked_entity(scene, frame, hit.value());
    if (!hidden) {
        return hidden.error();
    }
    if (hidden.value()) {
        return {};
    }
    Result<void> selected = select_control_click(scene, frame, hit.value());
    if (!selected) {
        return selected.error();
    }
    return anchor_plain_click(scene, frame, hit.value());
}

Result<void> OffscreenViewport::handle_navigation_click(renderer::Renderer& renderer,
                                                        picking::PickingService& picking,
                                                        scene::Storage& scene,
                                                        const InteractionFrame& frame,
                                                        std::optional<Float2> click_position) {
    if (!click_position.has_value()) {
        return {};
    }
    if (active_tool_ == ViewportTool::distance_measurement) {
        return handle_measurement_click(renderer, picking, scene, frame, *click_position);
    }
    return handle_selection_click(renderer, picking, scene, frame, *click_position);
}

Result<void> OffscreenViewport::update_measurement_preview(renderer::Renderer& renderer,
                                                           picking::PickingService& picking,
                                                           scene::Storage& scene,
                                                           const InteractionFrame& frame) {
    const bool allowed =
        state_->navigation.has_state() &&
        measurement_preview_allowed(frame.input, state_->navigation.snapshot(), active_tool_);
    if (!allowed) {
        state_->measurement.clear_preview();
        return {};
    }
    if (!state_->measurement.wants_preview_pick(scene, frame.visibility, frame.clipping_filter,
                                                frame.input.pointer_position_pixels, allowed)) {
        return {};
    }
    if (!state_->consume_interaction_pick()) {
        return {};
    }
    state_->measurement.record_preview_pick(scene, frame.visibility, frame.clipping_filter,
                                            frame.input.pointer_position_pixels);
    const PickOperation operation{frame.camera, frame.input.pointer_position_pixels, PickOptions{},
                                  frame.clipping_filter};
    Result<std::optional<PickHit>> hit =
        pick_gpu_first(renderer, picking, scene, frame.visibility, operation);
    if (!hit) {
        return hit.error();
    }
    if (!hit.value().has_value()) {
        state_->measurement.clear_preview();
        return {};
    }
    return state_->measurement.update_preview(scene, *hit.value());
}

Result<void> OffscreenViewport::update_navigation(renderer::Renderer& renderer,
                                                  picking::PickingService& picking,
                                                  scene::Storage& scene, EntityId camera,
                                                  const ViewportInput& input) {
    state_->begin_interaction_frame();
    const ViewportInput normalized_input = normalized_pointer_hover(input);
    Result<InteractionFrame> frame = interaction_frame(scene, camera, normalized_input);
    if (!frame) {
        return frame.error();
    }
    const navigation::NavigationUpdateRequest request{
        camera, extent(), normalized_input,
        state_->selection.settings().click_drag_threshold_pixels};
    Result<navigation::NavigationUpdate> navigation =
        state_->navigation.update(scene, request, frame.value().visibility);
    if (!navigation) {
        return navigation.error();
    }
    Result<void> anchored = update_orbit_screen_anchor(
        renderer, scene, frame.value(), navigation.value().orbit_start_position_pixels);
    if (!anchored) {
        return anchored.error();
    }
    Result<void> clicked = handle_navigation_click(renderer, picking, scene, frame.value(),
                                                   navigation.value().click_position_pixels);
    if (!clicked) {
        return clicked.error();
    }
    return update_measurement_preview(renderer, picking, scene, frame.value());
}

Result<void> OffscreenViewport::set_examine_pivot(scene::Storage& scene, EntityId camera,
                                                  Float3 world_position) {
    return state_->navigation.set_screen_anchor(scene, camera, world_position);
}

Result<void> OffscreenViewport::fit_to_scene(scene::Storage& scene, EntityId camera) {
    const Result<std::optional<Bounds3>> bounds = visible_bounds(scene);
    if (!bounds) {
        return bounds.error();
    }
    if (!bounds.value().has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera fitting requires visible content after clipping"};
    }
    return state_->navigation.fit_to_bounds(scene, camera, extent(), *bounds.value());
}

Result<void> OffscreenViewport::reset_view(scene::Storage& scene, EntityId camera) {
    const Result<std::optional<Bounds3>> bounds = visible_bounds(scene);
    if (!bounds) {
        return bounds.error();
    }
    if (!bounds.value().has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera reset requires visible content after clipping"};
    }
    return state_->navigation.reset_to_bounds(scene, camera, extent(), *bounds.value());
}

Result<void> OffscreenViewport::synchronize_navigation(const scene::Storage& scene,
                                                       EntityId camera) {
    return state_->navigation.synchronize(scene, camera);
}

void OffscreenViewport::cancel_interaction() noexcept {
    state_->navigation.cancel_interaction();
}

void OffscreenViewport::set_navigation_enabled(bool enabled) noexcept {
    state_->navigation.set_enabled(enabled);
}

bool OffscreenViewport::navigation_enabled() const noexcept {
    return state_->navigation.enabled();
}

Result<void> OffscreenViewport::set_navigation_settings(const OrbitNavigationSettings& settings) {
    return state_->navigation.set_settings(settings);
}

OrbitNavigationSettings OffscreenViewport::navigation_settings() const noexcept {
    return state_->navigation.settings();
}

std::optional<NavigationSnapshot> OffscreenViewport::navigation_snapshot() const noexcept {
    return state_->navigation.has_state() ? std::optional{state_->navigation.snapshot()}
                                          : std::nullopt;
}

void OffscreenViewport::set_active_tool(ViewportTool tool) noexcept {
    if (!valid_tool(tool)) {
        tool = ViewportTool::selection;
    }
    if (active_tool_ == tool) {
        return;
    }
    state_->navigation.cancel_interaction();
    if (active_tool_ == ViewportTool::distance_measurement &&
        tool != ViewportTool::distance_measurement) {
        state_->measurement.cancel_incomplete();
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

Result<std::optional<PickHit>> OffscreenViewport::pick(renderer::Renderer& renderer,
                                                       picking::PickingService& picking,
                                                       const scene::Storage& scene,
                                                       const ViewportPickRequest& request) {
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const PickOperation operation{request.camera, request.position_pixels, request.options,
                                  clipping_filter.value()};
    return pick_gpu_first(renderer, picking, scene, visibility.value(), operation);
}

Result<std::optional<PickHit>>
OffscreenViewport::select_at(renderer::Renderer& renderer, picking::PickingService& picking,
                             const scene::Storage& scene, EntityId camera, Float2 position_pixels) {
    const ViewportPickRequest request{camera, position_pixels, PickOptions{}};
    Result<std::optional<PickHit>> hit_result = pick(renderer, picking, scene, request);
    if (!hit_result) {
        return hit_result.error();
    }
    return state_->selection.select_hit(scene, hit_result.value());
}

Result<void> OffscreenViewport::set_selected_entity(const scene::Storage& scene, EntityId entity) {
    return state_->selection.set_selected_entity(scene, entity);
}

void OffscreenViewport::clear_selection() noexcept {
    state_->selection.clear();
}

void OffscreenViewport::clear_scene_selection(SceneId scene) noexcept {
    state_->selection.clear_scene(scene);
}

bool OffscreenViewport::has_selection() const noexcept {
    return state_->selection.has_selection();
}

std::optional<EntityId> OffscreenViewport::selected_entity() const noexcept {
    return state_->selection.selected_entity();
}

std::optional<PickHit> OffscreenViewport::selection_hit() const noexcept {
    return state_->selection.selection_hit();
}

SelectionSnapshot OffscreenViewport::selection_snapshot() const noexcept {
    return state_->selection.snapshot();
}

void OffscreenViewport::set_selection_enabled(bool enabled) noexcept {
    state_->selection.set_enabled(enabled);
    if (!enabled) {
        state_->navigation.cancel_interaction();
    }
}

bool OffscreenViewport::selection_enabled() const noexcept {
    return state_->selection.enabled();
}

Result<void> OffscreenViewport::set_selection_settings(const SelectionSettings& settings) {
    return state_->selection.set_settings(settings);
}

SelectionSettings OffscreenViewport::selection_settings() const noexcept {
    return state_->selection.settings();
}

Result<void> OffscreenViewport::begin_distance_measurement() {
    set_active_tool(ViewportTool::distance_measurement);
    return {};
}

void OffscreenViewport::cancel_distance_measurement() noexcept {
    state_->measurement.cancel_incomplete();
}

void OffscreenViewport::clear_distance_measurement() noexcept {
    state_->measurement.clear();
}

void OffscreenViewport::clear_scene_measurement(SceneId scene) noexcept {
    state_->measurement.clear_scene(scene);
}

DistanceMeasurementSnapshot
OffscreenViewport::distance_measurement_snapshot(const scene::Storage& scene) noexcept {
    const Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        DistanceMeasurementSnapshot result;
        result.diagnostic = visibility.error();
        return result;
    }
    const Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        DistanceMeasurementSnapshot result;
        result.diagnostic = clipping_filter.error();
        return result;
    }
    return state_->measurement.snapshot(scene, visibility.value(), clipping_filter.value(),
                                        active_tool_);
}

Result<void>
OffscreenViewport::set_measurement_settings(const DistanceMeasurementSettings& settings) {
    return state_->measurement.set_settings(settings);
}

DistanceMeasurementSettings OffscreenViewport::measurement_settings() const noexcept {
    return state_->measurement.settings();
}

MeasurementStatistics OffscreenViewport::measurement_statistics() const noexcept {
    return state_->measurement.statistics();
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
    return state_->visibility.isolate_entity(scene, entity);
}

void OffscreenViewport::clear_isolation() noexcept {
    state_->visibility.clear_isolation();
}

void OffscreenViewport::clear_scene_isolation(SceneId scene) noexcept {
    state_->visibility.clear_scene(scene);
}

bool OffscreenViewport::is_isolating() const noexcept {
    return state_->visibility.is_isolating();
}

std::optional<EntityId> OffscreenViewport::isolated_entity() const noexcept {
    return state_->visibility.isolated_entity();
}

Result<void> OffscreenViewport::hide_selected(scene::Storage& scene) {
    state_->selection.validate_against(scene);
    const std::optional<EntityId> selected = state_->selection.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to hide"};
    }
    return scene.set_entity_visible(*selected, false);
}

Result<void> OffscreenViewport::show_selected(scene::Storage& scene) {
    state_->selection.validate_against(scene);
    const std::optional<EntityId> selected = state_->selection.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to show"};
    }
    return scene.show_entity_and_ancestors(*selected);
}

Result<void> OffscreenViewport::isolate_selected(const scene::Storage& scene) {
    state_->selection.validate_against(scene);
    const std::optional<EntityId> selected = state_->selection.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity,
                     "The viewport has no selected entity to isolate"};
    }
    return state_->visibility.isolate_entity(scene, *selected);
}

Result<std::optional<Bounds3>> OffscreenViewport::visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    const Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return state_->visible_bounds(scene, visibility.value(), clipping_filter.value());
}

Result<void> OffscreenViewport::set_section_plane(const SectionPlane& plane) {
    return state_->clipping.set_section_plane(plane);
}

void OffscreenViewport::clear_section_plane() noexcept {
    state_->clipping.clear_section_plane();
}

Result<std::uint32_t> OffscreenViewport::add_clipping_box(const ClippingBox& box) {
    return state_->clipping.add_box(box);
}

Result<void> OffscreenViewport::set_clipping_box(std::uint32_t index, const ClippingBox& box) {
    return state_->clipping.set_box(index, box);
}

Result<void> OffscreenViewport::remove_clipping_box(std::uint32_t index) {
    return state_->clipping.remove_box(index);
}

void OffscreenViewport::clear_clipping_boxes() noexcept {
    state_->clipping.clear_boxes();
}

void OffscreenViewport::clear_clipping() noexcept {
    state_->clipping.clear();
}

Result<void> OffscreenViewport::set_clipping_helpers_visible(bool visible) noexcept {
    return state_->clipping.set_helpers_visible(visible);
}

Result<void>
OffscreenViewport::set_clipping_helper_settings(const ClippingHelperSettings& settings) noexcept {
    return state_->clipping.set_helper_settings(settings);
}

Result<void> OffscreenViewport::reset_clipping_box_to_visible_bounds(const scene::Storage& scene,
                                                                     std::uint32_t index) {
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return state_->clipping.reset_box_to_visible_bounds(scene, visibility.value(), index);
}

Result<std::uint32_t>
OffscreenViewport::add_clipping_box_from_visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return state_->clipping.add_box_from_visible_bounds(scene, visibility.value());
}

ClippingSnapshot OffscreenViewport::clipping_snapshot() const noexcept {
    return state_->clipping.snapshot();
}

Result<void> OffscreenViewport::render(renderer::Renderer& renderer, const scene::Storage& scene,
                                       EntityId camera) {
    statistics_ = {};
    if (render_target_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (render_target_->extent().width == 0 || render_target_->extent().height == 0) {
        return {};
    }
    state_->selection.validate_against(scene);
    state_->visibility.validate_against(scene);
    Result<scene::VisibilityFilter> visibility = state_->visibility.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    ViewportRenderOptions options;
    options.shading_mode = shading_mode_;
    const std::optional<EntityId> selected = state_->selection.selected_entity();
    if (selected.has_value()) {
        const SelectionSettings settings = state_->selection.settings();
        options.highlight =
            EntityHighlight{*selected, settings.highlight_color, settings.highlight_strength};
    }
    Result<tools::measurement::MeasurementOverlay> overlay =
        state_->measurement.overlay(scene, visibility.value(), clipping_filter.value());
    if (!overlay) {
        return overlay.error();
    }
    state_->measurement_overlay = overlay.value();
    state_->clipping_overlay = {};
    const std::optional<Bounds3> helper_bounds =
        clipping_helper_bounds(scene, visibility.value(), clipping_filter.value());
    const Result<tools::clipping::ClippingOverlay> clipping_overlay =
        state_->clipping.overlay(helper_bounds);
    if (!clipping_overlay) {
        return clipping_overlay.error();
    }
    state_->clipping_overlay = clipping_overlay.value();

    overlay_line_count_ = 0;
    overlay_marker_count_ = 0;
    append_overlay_lines(overlay_lines_, overlay_line_count_,
                         state_->measurement_overlay.line_span());
    append_overlay_lines(overlay_lines_, overlay_line_count_, state_->clipping_overlay.line_span());
    append_overlay_markers(overlay_markers_, overlay_marker_count_,
                           state_->measurement_overlay.marker_span());
    options.overlay_lines =
        std::span<const OverlayLineSegment>{overlay_lines_.data(), overlay_line_count_};
    options.overlay_markers =
        std::span<const OverlayPointMarker>{overlay_markers_.data(), overlay_marker_count_};
    const renderer::RenderRequest request{camera, clear_color_, lighting_, options};
    Result<RenderStatistics> render_result = renderer.render(
        scene, *render_target_, request, visibility.value(), clipping_filter.value());
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
