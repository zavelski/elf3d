#include <elf3d/viewport/offscreen_viewport.h>

#include <elf3d/math/conventions.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <utility>

namespace elf3d::viewport {
namespace {

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool valid_tool(ViewportTool tool) noexcept {
    return tool == ViewportTool::selection || tool == ViewportTool::distance_measurement;
}

[[nodiscard]] bool measurement_preview_allowed(const ViewportInput &input,
                                               const NavigationSnapshot &navigation,
                                               ViewportTool active_tool) noexcept {
    return active_tool == ViewportTool::distance_measurement && input.is_hovered &&
           input.is_focused && !input.left_button_down && !input.middle_button_down &&
           !input.right_button_down && !navigation.is_orbiting && !navigation.is_panning &&
           !navigation.is_pointer_captured;
}

void append_overlay_lines(std::array<OverlayLineSegment, 2 + 4 + maximum_clipping_boxes * 12> &out,
                          std::size_t &count,
                          std::span<const OverlayLineSegment> lines) noexcept {
    for (const OverlayLineSegment &line : lines) {
        if (count >= out.size()) {
            return;
        }
        out[count++] = line;
    }
}

void append_overlay_markers(std::array<OverlayPointMarker, 3> &out, std::size_t &count,
                            std::span<const OverlayPointMarker> markers) noexcept {
    for (const OverlayPointMarker &marker : markers) {
        if (count >= out.size()) {
            return;
        }
        out[count++] = marker;
    }
}

} // namespace

Result<std::unique_ptr<OffscreenViewport>> OffscreenViewport::create(
    std::shared_ptr<graphics::Device> device, std::shared_ptr<renderer::Renderer> renderer,
    std::shared_ptr<picking::PickingService> picking, Extent2D initial_extent) noexcept {
    if (!device || !renderer || !picking) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport creation requires active graphics, renderer, and picking services"};
    }

    try {
        Result<std::unique_ptr<graphics::RenderTarget>> target_result =
            device->create_render_target(initial_extent);
        if (!target_result) {
            return target_result.error();
        }

        return std::unique_ptr<OffscreenViewport>{
            new OffscreenViewport{std::move(device), std::move(renderer), std::move(picking),
                                  std::move(target_result).value()}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport creation threw an exception"};
    }
}

OffscreenViewport::OffscreenViewport(std::shared_ptr<graphics::Device> device,
                                     std::shared_ptr<renderer::Renderer> renderer,
                                     std::shared_ptr<picking::PickingService> picking,
                                     std::unique_ptr<graphics::RenderTarget> render_target) noexcept
    : device_(std::move(device)), renderer_(std::move(renderer)), picking_(std::move(picking)),
      render_target_(std::move(render_target)) {
    set_basic_lighting(BasicLighting{});
}

Extent2D OffscreenViewport::extent() const noexcept {
    return render_target_ != nullptr ? render_target_->extent() : Extent2D{};
}

Result<void> OffscreenViewport::resize(Extent2D extent) {
    if (render_target_ == nullptr || device_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "Viewport graphics resources are unavailable"};
    }
    if (extent == render_target_->extent()) {
        return {};
    }
    return render_target_->resize(extent);
}

void OffscreenViewport::set_clear_color(Color4 color) noexcept {
    clear_color_ = math::clamp_color(color);
}

Color4 OffscreenViewport::clear_color() const noexcept {
    return clear_color_;
}

void OffscreenViewport::set_basic_lighting(const BasicLighting &lighting) noexcept {
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

Result<void> OffscreenViewport::update_navigation(scene::Storage &scene, EntityId camera,
                                                  const ViewportInput &input) {
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
    Result<navigation::NavigationUpdate> navigation_result =
        navigation_.update(scene, camera, extent(), input,
                           selection_.settings().click_drag_threshold_pixels, visibility.value());
    if (!navigation_result) {
        return navigation_result.error();
    }
    if (navigation_result.value().click_position_pixels.has_value()) {
        const Float2 click_position = navigation_result.value().click_position_pixels.value();
        if (active_tool_ == ViewportTool::selection && selection_.enabled()) {
            Result<std::optional<PickHit>> selection_result = selection_.select_at(
                *picking_, scene, camera, extent(), click_position, visibility.value(),
                clipping_filter.value());
            if (!selection_result) {
                return selection_result.error();
            }
        } else if (active_tool_ == ViewportTool::distance_measurement) {
            Result<std::optional<PickHit>> hit_result = picking_->pick(
                scene, camera, extent(), click_position, PickOptions{}, visibility.value(),
                clipping_filter.value());
            if (!hit_result) {
                return hit_result.error();
            }
            if (hit_result.value().has_value()) {
                Result<void> placed = measurement_.place_hit(scene, hit_result.value().value());
                if (!placed) {
                    return placed.error();
                }
            }
        }
    }

    const NavigationSnapshot navigation = navigation_.snapshot();
    const bool allow_preview = measurement_preview_allowed(input, navigation, active_tool_);
    if (!allow_preview) {
        measurement_.clear_preview();
        return {};
    }
    if (measurement_.wants_preview_pick(scene, visibility.value(), clipping_filter.value(),
                                        input.pointer_position_pixels, allow_preview)) {
        measurement_.record_preview_pick(scene, visibility.value(), clipping_filter.value(),
                                         input.pointer_position_pixels);
        Result<std::optional<PickHit>> hit_result =
            picking_->pick(scene, camera, extent(), input.pointer_position_pixels, PickOptions{},
                           visibility.value(), clipping_filter.value());
        if (!hit_result) {
            return hit_result.error();
        }
        if (hit_result.value().has_value()) {
            Result<void> preview = measurement_.update_preview(scene, hit_result.value().value());
            if (!preview) {
                return preview.error();
            }
        } else {
            measurement_.clear_preview();
        }
    }
    return {};
}

Result<void> OffscreenViewport::fit_to_scene(scene::Storage &scene, EntityId camera) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const Bounds3 bounds =
        tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
    if (!bounds.is_valid) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera fitting requires visible content after clipping"};
    }
    return navigation_.fit_to_bounds(scene, camera, extent(), bounds);
}

Result<void> OffscreenViewport::reset_view(scene::Storage &scene, EntityId camera) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const Bounds3 bounds =
        tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
    if (!bounds.is_valid) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Camera reset requires visible content after clipping"};
    }
    return navigation_.reset_to_bounds(scene, camera, extent(), bounds);
}

Result<void> OffscreenViewport::synchronize_navigation(const scene::Storage &scene,
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

Result<void> OffscreenViewport::set_navigation_settings(const OrbitNavigationSettings &settings) {
    return navigation_.set_settings(settings);
}

OrbitNavigationSettings OffscreenViewport::navigation_settings() const noexcept {
    return navigation_.settings();
}

NavigationSnapshot OffscreenViewport::navigation_snapshot() const noexcept {
    return navigation_.snapshot();
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

Result<Ray3> OffscreenViewport::make_picking_ray(const scene::Storage &scene, EntityId camera,
                                                 Float2 position_pixels) const {
    return picking_->make_picking_ray(scene, camera, extent(), position_pixels);
}

Result<std::optional<PickHit>> OffscreenViewport::pick(const scene::Storage &scene, EntityId camera,
                                                       Float2 position_pixels,
                                                       const PickOptions &options) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return picking_->pick(scene, camera, extent(), position_pixels, options, visibility.value(),
                          clipping_filter.value());
}

Result<std::optional<PickHit>>
OffscreenViewport::select_at(const scene::Storage &scene, EntityId camera, Float2 position_pixels) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    return selection_.select_at(*picking_, scene, camera, extent(), position_pixels,
                                visibility.value(), clipping_filter.value());
}

Result<void> OffscreenViewport::set_selected_entity(const scene::Storage &scene, EntityId entity) {
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

Result<void> OffscreenViewport::set_selection_settings(const SelectionSettings &settings) {
    return selection_.set_settings(settings);
}

SelectionSettings OffscreenViewport::selection_settings() const noexcept {
    return selection_.settings();
}

PickingStatistics OffscreenViewport::picking_statistics() const noexcept {
    return picking_ != nullptr ? picking_->statistics() : PickingStatistics{};
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
OffscreenViewport::distance_measurement_snapshot(const scene::Storage &scene) noexcept {
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
OffscreenViewport::set_measurement_settings(const DistanceMeasurementSettings &settings) {
    return measurement_.set_settings(settings);
}

DistanceMeasurementSettings OffscreenViewport::measurement_settings() const noexcept {
    return measurement_.settings();
}

MeasurementStatistics OffscreenViewport::measurement_statistics() const noexcept {
    return measurement_.statistics();
}

Result<ProjectedViewportPoint>
OffscreenViewport::project_world_to_viewport(const scene::Storage &scene, EntityId camera,
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
    const Result<math::Matrix4> camera_world = scene.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<math::Matrix4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect =
        static_cast<float>(target_extent.width) / static_cast<float>(target_extent.height);
    const Result<math::Matrix4> projection = math::perspective_matrix(
        camera_description.value().vertical_field_of_view_radians, aspect,
        camera_description.value().near_plane, camera_description.value().far_plane);
    if (!projection) {
        return projection.error();
    }

    const math::Vector4 clip =
        projection.value() * view.value() *
        math::Vector4{world_position.x, world_position.y, world_position.z, 1.0F};
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) ||
        !std::isfinite(clip.w) || std::abs(clip.w) <= 0.000001F) {
        return Error{ErrorCode::projection_failed,
                     "World-to-viewport projection produced an invalid homogeneous point"};
    }

    const float inverse_w = 1.0F / clip.w;
    const float ndc_x = clip.x * inverse_w;
    const float ndc_y = clip.y * inverse_w;
    const float ndc_z = clip.z * inverse_w;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) {
        return Error{ErrorCode::projection_failed,
                     "World-to-viewport projection produced non-finite clip coordinates"};
    }

    ProjectedViewportPoint result;
    result.position_pixels = {
        (ndc_x * 0.5F + 0.5F) * static_cast<float>(target_extent.width) - 0.5F,
        (1.0F - (ndc_y * 0.5F + 0.5F)) * static_cast<float>(target_extent.height) - 0.5F};
    result.depth = ndc_z;
    result.is_in_front = clip.w > 0.0F;
    result.is_inside_viewport = result.is_in_front && ndc_x >= -1.0F && ndc_x <= 1.0F &&
                                ndc_y >= -1.0F && ndc_y <= 1.0F && ndc_z >= -1.0F && ndc_z <= 1.0F;
    return result;
}

Result<void> OffscreenViewport::isolate_entity(const scene::Storage &scene, EntityId entity) {
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

Result<void> OffscreenViewport::hide_selected(scene::Storage &scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to hide"};
    }
    return scene.set_entity_visible(selected.value(), false);
}

Result<void> OffscreenViewport::show_selected(scene::Storage &scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity, "The viewport has no selected entity to show"};
    }
    return scene.show_entity_and_ancestors(selected.value());
}

Result<void> OffscreenViewport::isolate_selected(const scene::Storage &scene) {
    selection_.validate_against(scene);
    const std::optional<EntityId> selected = selection_.selected_entity();
    if (!selected.has_value()) {
        return Error{ErrorCode::no_selected_entity,
                     "The viewport has no selected entity to isolate"};
    }
    return visibility_.isolate_entity(scene, selected.value());
}

Result<Bounds3> OffscreenViewport::visible_bounds(const scene::Storage &scene) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    const Result<clipping::ClippingFilter> clipping_filter = clipping_.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }
    const Bounds3 clipped_bounds =
        tools::clipping::visible_bounds(scene, visibility.value(), clipping_filter.value());
    if (!clipped_bounds.is_valid) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Visible bounds require at least one visible renderable entity"};
    }
    return clipped_bounds;
}

Result<void> OffscreenViewport::set_section_plane(const SectionPlane &plane) {
    return clipping_.set_section_plane(plane);
}

void OffscreenViewport::clear_section_plane() noexcept {
    clipping_.clear_section_plane();
}

Result<std::uint32_t> OffscreenViewport::add_clipping_box(const ClippingBox &box) {
    return clipping_.add_box(box);
}

Result<void> OffscreenViewport::set_clipping_box(std::uint32_t index, const ClippingBox &box) {
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

Result<void> OffscreenViewport::set_clipping_helper_settings(
    const ClippingHelperSettings &settings) noexcept {
    return clipping_.set_helper_settings(settings);
}

Result<void> OffscreenViewport::reset_clipping_box_to_visible_bounds(const scene::Storage &scene,
                                                                     std::uint32_t index) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return clipping_.reset_box_to_visible_bounds(scene, visibility.value(), index);
}

Result<std::uint32_t>
OffscreenViewport::add_clipping_box_from_visible_bounds(const scene::Storage &scene) {
    Result<scene::VisibilityFilter> visibility = visibility_.filter_for(scene);
    if (!visibility) {
        return visibility.error();
    }
    return clipping_.add_box_from_visible_bounds(scene, visibility.value());
}

ClippingSnapshot OffscreenViewport::clipping_snapshot() const noexcept {
    return clipping_.snapshot();
}

Result<void> OffscreenViewport::render(const scene::Storage &scene, EntityId camera) {
    statistics_ = {};
    if (render_target_ == nullptr || device_ == nullptr || renderer_ == nullptr) {
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
        options.highlight = EntityHighlight{selected.value(), settings.highlight_color,
                                            settings.highlight_strength};
    }
    Result<tools::measurement::MeasurementOverlay> overlay =
        measurement_.overlay(scene, visibility.value(), clipping_filter.value());
    if (!overlay) {
        return overlay.error();
    }
    measurement_overlay_ = overlay.value();
    clipping_overlay_ = {};
    const Bounds3 helper_bounds = scene.visible_world_bounds(visibility.value());
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
    Result<RenderStatistics> render_result = renderer_->render(
        scene, camera, *render_target_, clear_color_, lighting_, options, visibility.value(),
        clipping_filter.value());
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
