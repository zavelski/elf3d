#include "viewer_tools.hpp"
#include "viewer_tool_style.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace elf3d::viewer {
using namespace tool_detail;
namespace {

[[nodiscard]] bool valid_selection_settings(const SelectionToolSettings& settings) noexcept
{
    return std::isfinite(settings.click_drag_threshold_pixels) &&
           settings.click_drag_threshold_pixels >= 0.0F && finite_color(settings.highlight_color) &&
           std::isfinite(settings.highlight_strength) && settings.highlight_strength >= 0.0F &&
           settings.highlight_strength <= 1.0F;
}

[[nodiscard]] float pointer_distance_squared(Float2 first, Float2 second) noexcept
{
    const float x = second.x - first.x;
    const float y = second.y - first.y;
    return x * x + y * y;
}

[[nodiscard]] bool requests_direct_selection(const ToolUpdateContext& context,
                                             ViewerTool active_tool) noexcept
{
    return active_tool == ViewerTool::selection && context.modifiers.control &&
           !context.modifiers.shift;
}

} // namespace

Result<void> SelectionTool::set_settings(const SelectionToolSettings& settings) noexcept
{
    if (!valid_selection_settings(settings)) {
        return Error{ErrorCode::invalid_argument,
                     "Selection Tool settings require finite color, non-negative click threshold, "
                     "and highlight strength in [0, 1]"};
    }
    settings_ = settings;
    settings_.highlight_color = sanitized_color(settings.highlight_color);
    return {};
}

SelectionToolSettings SelectionTool::settings() const noexcept
{
    return settings_;
}

void SelectionTool::set_enabled(bool enabled) noexcept
{
    enabled_ = enabled;
}

bool SelectionTool::enabled() const noexcept
{
    return enabled_;
}

Result<std::optional<PickHit>> SelectionTool::select_at(Scene& scene, Viewport& viewport,
                                                        EntityId camera,
                                                        Float2 position_pixels) noexcept
{
    if (!enabled_) {
        return std::optional<PickHit>{};
    }
    return viewport.select_at(scene, camera, position_pixels);
}

std::optional<EntityHighlight>
SelectionTool::render_feedback(const Viewport& viewport) const noexcept
{
    const std::optional<EntityId> selected = enabled_ ? viewport.selected_entity() : std::nullopt;
    if (!selected.has_value()) {
        return std::nullopt;
    }
    return EntityHighlight{*selected, settings_.highlight_color, settings_.highlight_strength};
}

ViewerTool ToolCoordinator::active_tool() const noexcept
{
    return active_tool_;
}

void ToolCoordinator::activate(ViewerTool tool) noexcept
{
    active_tool_ = tool;
}

void ToolCoordinator::cancel() noexcept
{
    measurement_.cancel_incomplete();
    primary_press_position_.reset();
}

void ToolCoordinator::clear_scene(SceneId scene) noexcept
{
    measurement_.clear_scene(scene);
    primary_press_position_.reset();
}

Result<void> ToolCoordinator::update(const ToolUpdateContext& context) noexcept
{
    const InputTransition& primary =
        context.input.buttons[static_cast<std::size_t>(InputButton::left)];
    if (primary.pressed && context.input.hovered) {
        primary_press_position_ = context.input.pointer_position_pixels;
    }

    const float click_threshold = selection_.settings().click_drag_threshold_pixels;
    if (primary.down && primary_press_position_.has_value() &&
        pointer_distance_squared(*primary_press_position_, context.input.pointer_position_pixels) >
            click_threshold * click_threshold) {
        primary_press_position_.reset();
    }
    if (primary.released) {
        const Result<void> release = handle_primary_release(context);
        if (!release) {
            return release.error();
        }
    }
    return update_measurement_preview(context, primary.down);
}

Result<void> ToolCoordinator::handle_primary_release(const ToolUpdateContext& context) noexcept
{
    const bool is_click = primary_press_position_.has_value() && context.input.hovered &&
                          !context.navigation_captured;
    primary_press_position_.reset();
    if (!is_click) {
        return {};
    }
    if (requests_direct_selection(context, active_tool_)) {
        const Result<std::optional<PickHit>> selected = selection_.select_at(
            context.scene, context.viewport, context.camera, context.input.pointer_position_pixels);
        return selected ? Result<void>{} : Result<void>{selected.error()};
    }

    Result<std::optional<PickHit>> hit =
        context.viewport.pick(context.scene, context.camera, context.input.pointer_position_pixels);
    if (!hit || !hit.value().has_value()) {
        return hit ? Result<void>{} : Result<void>{hit.error()};
    }
    return apply_primary_hit(context, *hit.value());
}

Result<void> ToolCoordinator::apply_primary_hit(const ToolUpdateContext& context,
                                                const PickHit& hit) noexcept
{
    if (active_tool_ == ViewerTool::distance_measurement) {
        return measurement_.place_hit(context.scene, hit);
    }
    if (context.modifiers.shift) {
        return context.scene.set_entity_local_visibility(hit.entity, false);
    }
    return context.viewport.set_examine_pivot(context.scene, context.camera, hit.world_position);
}

Result<void> ToolCoordinator::update_measurement_preview(const ToolUpdateContext& context,
                                                         bool primary_down) noexcept
{
    if (active_tool_ != ViewerTool::distance_measurement) {
        measurement_.clear_preview();
        return {};
    }

    const bool preview_allowed =
        context.input.hovered && !primary_down && !context.navigation_captured;
    if (!measurement_.wants_preview_pick(context.scene, context.viewport,
                                         context.input.pointer_position_pixels, preview_allowed)) {
        if (!preview_allowed) {
            measurement_.clear_preview();
        }
        return {};
    }

    measurement_.record_preview_pick(context.scene, context.viewport,
                                     context.input.pointer_position_pixels);
    Result<std::optional<PickHit>> preview =
        context.viewport.pick(context.scene, context.camera, context.input.pointer_position_pixels);
    if (!preview) {
        return preview.error();
    }
    if (!preview.value().has_value()) {
        measurement_.clear_preview();
        return {};
    }
    return measurement_.update_preview(context.scene, *preview.value());
}

MeasurementTool& ToolCoordinator::measurement() noexcept
{
    return measurement_;
}

const MeasurementTool& ToolCoordinator::measurement() const noexcept
{
    return measurement_;
}

SelectionTool& ToolCoordinator::selection() noexcept
{
    return selection_;
}

const SelectionTool& ToolCoordinator::selection() const noexcept
{
    return selection_;
}

ClippingTool& ToolCoordinator::clipping() noexcept
{
    return clipping_;
}

const ClippingTool& ToolCoordinator::clipping() const noexcept
{
    return clipping_;
}

const char* tool_name(ViewerTool tool) noexcept
{
    switch (tool) {
    case ViewerTool::selection:
        return "Select";
    case ViewerTool::distance_measurement:
        return "Measure Distance";
    }
    return "Select";
}

const char* measurement_state_name(DistanceMeasurementState state) noexcept
{
    switch (state) {
    case DistanceMeasurementState::empty:
        return "Empty";
    case DistanceMeasurementState::awaiting_first_point:
        return "Select first point";
    case DistanceMeasurementState::awaiting_second_point:
        return "Select second point";
    case DistanceMeasurementState::complete:
        return "Complete";
    }
    return "Empty";
}

} // namespace elf3d::viewer
