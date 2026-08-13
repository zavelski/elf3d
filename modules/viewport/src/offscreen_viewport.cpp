module;

#include <elf3d/clipping.h>
#include <elf3d/viewport.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

module elf.viewport;

import elf.math;
import elf.navigation;
import elf.clipping.runtime;
import elf.selection;
import elf.visibility;

namespace elf3d::viewport {
class OffscreenViewport::State::SelectionState final {
  public:
    selection::SelectionController controller;
};

class OffscreenViewport::State::VisibilityState final {
  public:
    visibility::VisibilityController controller;
};

OffscreenViewport::State::State()
    : selection(std::make_unique<SelectionState>()),
      visibility(std::make_unique<VisibilityState>()) {}

OffscreenViewport::State::~State() noexcept = default;

Result<std::optional<Bounds3>>
OffscreenViewport::State::visible_bounds(const scene::Storage& scene,
                                         const scene::VisibilityFilter& visibility_filter,
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
            clipping_runtime::visible_bounds(scene, visibility_filter, clipping_filter);
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

void OffscreenViewport::State::validate_visibility(const scene::Storage& scene) noexcept {
    visibility->controller.validate_against(scene);
}

Result<scene::VisibilityFilter>
OffscreenViewport::State::visibility_filter(const scene::Storage& scene) {
    return visibility->controller.filter_for(scene);
}

Result<void> OffscreenViewport::State::isolate_entity(const scene::Storage& scene,
                                                      EntityId entity) {
    return visibility->controller.isolate_entity(scene, entity);
}

void OffscreenViewport::State::clear_isolation() noexcept {
    visibility->controller.clear_isolation();
}

void OffscreenViewport::State::clear_scene_isolation(SceneId scene) noexcept {
    visibility->controller.clear_scene(scene);
}

bool OffscreenViewport::State::is_isolating() const noexcept {
    return visibility->controller.is_isolating();
}

std::optional<EntityId> OffscreenViewport::State::isolated_entity() const noexcept {
    return visibility->controller.isolated_entity();
}

OffscreenViewport::OffscreenViewport(ConstructionKey, Resources resources) noexcept
    : render_target_(std::move(resources.render_target)),
      picking_target_(std::move(resources.picking_target)),
      focus_depth_target_(std::move(resources.focus_depth_target)),
      state_(std::make_unique<State>()) {
    set_basic_lighting(BasicLighting{});
}

OffscreenViewport::~OffscreenViewport() noexcept = default;

Result<void>
OffscreenViewport::update_orbit_screen_anchor(renderer::Renderer& renderer, scene::Storage& scene,
                                              const InteractionFrame& frame,
                                              std::optional<Float2> orbit_start_position) {
    if (!state_->navigation.settings().focus_depth_anchor_enabled ||
        !orbit_start_position.has_value() || state_->navigation.has_screen_anchor()) {
        return {};
    }
    Result<std::optional<Float3>> anchor =
        focus_depth_anchor(renderer, scene, frame.camera, frame.visibility, frame.clipping_filter);
    if (!anchor || !anchor.value().has_value()) {
        return {};
    }
    return state_->navigation.set_screen_anchor(scene, frame.camera, *anchor.value());
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

Result<Ray3> OffscreenViewport::make_picking_ray(picking::PickingService& picking,
                                                 const scene::Storage& scene, EntityId camera,
                                                 Float2 position_pixels) const {
    return picking.make_picking_ray(scene, camera, extent(), position_pixels);
}

Result<std::optional<PickHit>> OffscreenViewport::pick(renderer::Renderer& renderer,
                                                       picking::PickingService& picking,
                                                       const scene::Storage& scene,
                                                       const ViewportPickRequest& request) {
    Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
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
    return state_->selection->controller.select_hit(scene, hit_result.value());
}

Result<void> OffscreenViewport::set_selected_entity(const scene::Storage& scene, EntityId entity) {
    return state_->selection->controller.set_selected_entity(scene, entity);
}

void OffscreenViewport::clear_selection() noexcept {
    state_->selection->controller.clear();
}

void OffscreenViewport::clear_scene_selection(SceneId scene) noexcept {
    state_->selection->controller.clear_scene(scene);
}

bool OffscreenViewport::has_selection() const noexcept {
    return state_->selection->controller.has_selection();
}

std::optional<EntityId> OffscreenViewport::selected_entity() const noexcept {
    return state_->selection->controller.selected_entity();
}

std::optional<PickHit> OffscreenViewport::selection_hit() const noexcept {
    return state_->selection->controller.selection_hit();
}

SelectionSnapshot OffscreenViewport::selection_snapshot() const noexcept {
    return state_->selection->controller.snapshot();
}

Result<void> OffscreenViewport::isolate_entity(const scene::Storage& scene, EntityId entity) {
    return state_->isolate_entity(scene, entity);
}

void OffscreenViewport::clear_isolation() noexcept {
    state_->clear_isolation();
}

void OffscreenViewport::clear_scene_isolation(SceneId scene) noexcept {
    state_->clear_scene_isolation(scene);
}

bool OffscreenViewport::is_isolating() const noexcept {
    return state_->is_isolating();
}

std::optional<EntityId> OffscreenViewport::isolated_entity() const noexcept {
    return state_->isolated_entity();
}

Result<std::optional<Bounds3>> OffscreenViewport::visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
    if (!visibility) {
        return visibility.error();
    }
    const Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return state_->visible_bounds(scene, visibility.value(), clipping_filter.value());
}

Result<std::optional<Bounds3>>
OffscreenViewport::unclipped_visible_bounds(const scene::Storage& scene) {
    Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
    if (!visibility) {
        return visibility.error();
    }
    return scene.visible_world_bounds(visibility.value());
}

Result<bool> OffscreenViewport::surface_anchor_visible(const scene::Storage& scene,
                                                       const ResolvedSurfaceAnchor& anchor) {
    const Result<ResolvedSurfaceAnchor> current = scene.resolve_surface_anchor(anchor.anchor);
    if (!current) {
        return current.error();
    }
    const Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
    if (!visibility) {
        return visibility.error();
    }
    const Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return scene::entity_visible_in_filter(scene, visibility.value(), anchor.anchor.entity) &&
           clipping::contains_point(clipping_filter.value(), current.value().world_position);
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

ClippingSnapshot OffscreenViewport::clipping_snapshot() const noexcept {
    return state_->clipping.snapshot();
}

Result<void> OffscreenViewport::render(renderer::Renderer& renderer, const scene::Storage& scene,
                                       EntityId camera) {
    ViewportRenderOptions options;
    options.shading_mode = shading_mode_;
    return render(renderer, scene, camera, options);
}

Result<void> OffscreenViewport::render(renderer::Renderer& renderer, const scene::Storage& scene,
                                       EntityId camera,
                                       const ViewportRenderOptions& requested_options) {
    statistics_ = {};
    if (render_target_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (render_target_->extent().width == 0 || render_target_->extent().height == 0) {
        return {};
    }
    state_->selection->controller.validate_against(scene);
    state_->validate_visibility(scene);
    Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const renderer::RenderRequest request{camera, clear_color_, lighting_, requested_options};
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
