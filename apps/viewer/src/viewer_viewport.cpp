#include "viewer_viewport.hpp"
#include "viewer_browser.hpp"
#include "viewer_input_math.hpp"

#include "viewer_ui.hpp"

#include <elf3d/imgui/texture.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace elf3d::viewer {

namespace {

[[nodiscard]] std::string viewport_window_title(const SceneSession& scene) {
    if (!scene.is_imported()) {
        return "3D View";
    }

    return file_name_label(scene.source_path) + "###3D View";
}

} // namespace

elf3d::Extent2D content_extent_in_pixels(ImVec2 logical_size) noexcept {
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    return content_extent_in_pixels(Float2{logical_size.x, logical_size.y},
                                    Float2{scale.x, scale.y});
}

[[nodiscard]] bool has_nonzero_extent(elf3d::Extent2D extent) noexcept {
    return extent.width != 0 && extent.height != 0;
}

[[nodiscard]] std::string clipping_status(const elf3d::ClippingSnapshot& snapshot,
                                          bool has_visible_content) {
    const bool plane_enabled = snapshot.section_plane.enabled;
    std::uint32_t enabled_boxes = 0;
    for (std::uint32_t index = 0; index < snapshot.box_count; ++index) {
        if (snapshot.boxes[index].enabled) {
            ++enabled_boxes;
        }
    }
    std::string result = "Clipping: ";
    if (!plane_enabled && enabled_boxes == 0) {
        result += "off";
    } else {
        bool wrote = false;
        if (plane_enabled) {
            result += "plane";
            wrote = true;
        }
        if (enabled_boxes != 0) {
            if (wrote) {
                result += " + ";
            }
            result += std::to_string(enabled_boxes);
            result += enabled_boxes == 1 ? " box" : " boxes";
        }
    }
    if (!has_visible_content) {
        result += " | no visible content";
    }
    return result;
}

[[nodiscard]] bool finite_float3(elf3d::Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool valid_box_for_commit(const elf3d::ClippingBox& box) noexcept {
    return finite_float3(box.minimum) && finite_float3(box.maximum) &&
           box.maximum.x - box.minimum.x > 0.00001F && box.maximum.y - box.minimum.y > 0.00001F &&
           box.maximum.z - box.minimum.z > 0.00001F;
}

[[nodiscard]] elf3d::Float3 bounds_center(const elf3d::Bounds3& bounds) noexcept {
    return elf3d::Float3{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                         (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                         (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

[[nodiscard]] const char* unit_name(LengthDisplayUnit unit) noexcept {
    switch (unit) {
    case LengthDisplayUnit::automatic_metric:
        return "Automatic metric";
    case LengthDisplayUnit::meters:
        return "Meters";
    case LengthDisplayUnit::centimeters:
        return "Centimeters";
    case LengthDisplayUnit::millimeters:
        return "Millimeters";
    case LengthDisplayUnit::feet:
        return "Feet";
    case LengthDisplayUnit::inches:
        return "Inches";
    }
    return "Meters";
}

[[nodiscard]] const char* unit_suffix(LengthDisplayUnit unit) noexcept {
    switch (unit) {
    case LengthDisplayUnit::meters:
        return "m";
    case LengthDisplayUnit::centimeters:
        return "cm";
    case LengthDisplayUnit::millimeters:
        return "mm";
    case LengthDisplayUnit::feet:
        return "ft";
    case LengthDisplayUnit::inches:
        return "in";
    case LengthDisplayUnit::automatic_metric:
        break;
    }
    return "m";
}

struct DisplayDistance {
    double value = 0.0;
    LengthDisplayUnit unit = LengthDisplayUnit::meters;
};

[[nodiscard]] DisplayDistance display_distance(double meters, LengthDisplayUnit unit) noexcept {
    LengthDisplayUnit resolved = unit;
    if (resolved == LengthDisplayUnit::automatic_metric) {
        const double absolute = std::abs(meters);
        if (absolute >= 1.0) {
            resolved = LengthDisplayUnit::meters;
        } else if (absolute >= 0.01) {
            resolved = LengthDisplayUnit::centimeters;
        } else {
            resolved = LengthDisplayUnit::millimeters;
        }
    }

    switch (resolved) {
    case LengthDisplayUnit::meters:
        return DisplayDistance{meters, resolved};
    case LengthDisplayUnit::centimeters:
        return DisplayDistance{meters * 100.0, resolved};
    case LengthDisplayUnit::millimeters:
        return DisplayDistance{meters * 1000.0, resolved};
    case LengthDisplayUnit::feet:
        return DisplayDistance{meters * 3.280839895, resolved};
    case LengthDisplayUnit::inches:
        return DisplayDistance{meters * 39.37007874, resolved};
    case LengthDisplayUnit::automatic_metric:
        break;
    }
    return DisplayDistance{meters, LengthDisplayUnit::meters};
}

[[nodiscard]] std::string format_distance(double meters, LengthDisplayUnit unit) {
    const DisplayDistance display = display_distance(meters, unit);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.4g %s", display.value, unit_suffix(display.unit));
    return std::string{buffer};
}

[[nodiscard]] bool navigation_blocked_by_modal() noexcept {
    return ImGui::GetTopMostPopupModal() != nullptr;
}

[[nodiscard]] elf3d::NavigationInput
viewport_input_from_framework(const ViewerFrameContext& state, const InputSnapshot& snapshot,
                              const InteractionRegionInput& region) noexcept {
    elf3d::NavigationInput input;
    input.frame_delta_seconds = static_cast<float>(state.interaction.frame_delta_seconds);
    input.pointer_position_pixels = region.pointer_position_pixels;
    input.pointer_delta_pixels = region.pointer_delta_pixels;
    input.wheel_delta = region.wheel_delta;
    input.pointer_hovered = region.hovered;
    input.region_focused = region.focused || region.pointer_captured;
    input.orbit_down = region.buttons[static_cast<std::size_t>(InputButton::left)].down;
    input.pan_down = region.buttons[static_cast<std::size_t>(InputButton::middle)].down;
    input.zoom_down = region.buttons[static_cast<std::size_t>(InputButton::right)].down;
    input.pan_modifier_down = snapshot.key(InputKey::x).down;
    input.zoom_modifier_down = snapshot.key(InputKey::z).down;
    if (!snapshot.text_input_owned) {
        input.eye_orbit_modifier_down = snapshot.key(InputKey::space).down;
        input.move_forward_down = snapshot.key(InputKey::w).down;
        input.move_backward_down = snapshot.key(InputKey::s).down;
        input.view_left_down = snapshot.key(InputKey::a).down;
        input.view_right_down = snapshot.key(InputKey::d).down;
        input.world_down_down = snapshot.key(InputKey::q).down;
        input.world_up_down = snapshot.key(InputKey::e).down;
    }
    return input;
}

void set_viewport_error(ViewerFrameContext state, const elf3d::Error& error) {
    state.notifications.viewport_error = error.message();
    state.rendering.framebuffer_valid = false;
}

bool color_control(const char* label, std::array<float, 4>& rgba) {
    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(72.0F, ImGui::GetContentRegionAvail().x));
    const bool changed = ImGui::ColorEdit4("##value", rgba.data(), ImGuiColorEditFlags_NoInputs);
    ImGui::PopID();
    return changed;
}

struct MeasurementLabelArea {
    ImVec2 image_min;
    ImVec2 area_size;
};

void draw_measurement_label(ViewerFrameContext& state, elf3d::Viewport& engine_viewport,
                            const SceneSession& scene, const ToolCoordinator& tools,
                            MeasurementLabelArea area) {
    const DistanceMeasurementSnapshot measurement = tools.measurement().snapshot(
        *scene.scene, engine_viewport, tools.active_tool() == ViewerTool::distance_measurement);
    if (!measurement.overlay_visible || !measurement.midpoint_world_position.has_value()) {
        return;
    }

    const double meters = measurement.state == DistanceMeasurementState::complete
                              ? measurement.distance_meters
                              : measurement.preview_distance_meters;
    const std::string label = format_distance(meters, tools.measurement().settings().display_unit);
    const elf3d::Result<elf3d::ProjectedViewportPoint> projected =
        engine_viewport.project_world_to_viewport(*scene.scene, scene.camera,
                                                  *measurement.midpoint_world_position);
    if (!projected) {
        set_viewport_error(state, projected.error());
        return;
    }
    if (!projected.value().is_in_front || !projected.value().is_inside_viewport) {
        return;
    }

    const float logical_x =
        area.image_min.x + ((projected.value().position_pixels.x + 0.5F) /
                            static_cast<float>(state.rendering.view_dimensions.width)) *
                               area.area_size.x;
    const float logical_y =
        area.image_min.y + ((projected.value().position_pixels.y + 0.5F) /
                            static_cast<float>(state.rendering.view_dimensions.height)) *
                               area.area_size.y;
    const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    ImVec2 label_min{logical_x + 8.0F, logical_y - text_size.y - 8.0F};
    const ImVec2 image_max{area.image_min.x + area.area_size.x,
                           area.image_min.y + area.area_size.y};
    const float minimum_x = area.image_min.x + 4.0F;
    const float minimum_y = area.image_min.y + 4.0F;
    const float maximum_x = std::max(minimum_x, image_max.x - text_size.x - 12.0F);
    const float maximum_y = std::max(minimum_y, image_max.y - text_size.y - 8.0F);
    label_min.x = std::clamp(label_min.x, minimum_x, maximum_x);
    label_min.y = std::clamp(label_min.y, minimum_y, maximum_y);
    const ImVec2 label_max{label_min.x + text_size.x + 8.0F, label_min.y + text_size.y + 4.0F};

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(label_min, label_max, IM_COL32(20, 24, 28, 220), 4.0F);
    draw_list->AddRect(label_min, label_max, IM_COL32(255, 255, 255, 90), 4.0F);
    draw_list->AddText(ImVec2{label_min.x + 4.0F, label_min.y + 2.0F}, IM_COL32(255, 255, 255, 255),
                       label.c_str());
}

void draw_viewport_error_overlay(const std::string& error, ImVec2 image_min, ImVec2 area_size) {
    if (error.empty() || area_size.x < 48.0F || area_size.y < 32.0F) {
        return;
    }

    const std::string message = std::string{"Viewport error: "} + error;
    const float wrap_width = std::max(96.0F, std::min(area_size.x - 24.0F, 520.0F));
    const ImVec2 text_size = ImGui::CalcTextSize(message.c_str(), nullptr, false, wrap_width);
    const ImVec2 overlay_min{image_min.x + 8.0F, image_min.y + 8.0F};
    const ImVec2 overlay_max{overlay_min.x + text_size.x + 14.0F,
                             overlay_min.y + text_size.y + 10.0F};
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(overlay_min, overlay_max, IM_COL32(24, 30, 34, 218), 3.0F);
    draw_list->AddRect(overlay_min, overlay_max, IM_COL32(255, 255, 255, 110), 3.0F);
    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                       ImVec2{overlay_min.x + 7.0F, overlay_min.y + 5.0F},
                       IM_COL32(255, 255, 255, 255), message.c_str(), nullptr, wrap_width);
}

struct ViewportCanvas {
    ImVec2 area_size;
    ImVec2 image_min;
    bool has_area = false;
};

void deactivate_3d_view(const ViewPanelContext& context) {
    context.interaction_region = {};
    context.state.rendering.view_dimensions = {};
    context.state.rendering.render_target_dimensions = {};
    context.state.rendering.framebuffer_valid = false;
    context.state.rendering.statistics = {};
    context.state.rendering.retained_viewport_frame.reset();
    context.viewport.cancel_interaction();
    const elf3d::Result<void> result = context.viewport.resize({});
    if (!result) {
        set_viewport_error(context.state, result.error());
    }
}

[[nodiscard]] ViewportCanvas begin_viewport_canvas(ViewerFrameContext& state) {
    ViewportCanvas canvas;
    canvas.area_size = ImGui::GetContentRegionAvail();
    canvas.image_min = ImGui::GetCursorScreenPos();
    state.rendering.view_dimensions = content_extent_in_pixels(canvas.area_size);
    canvas.has_area = canvas.area_size.x > 0.0F && canvas.area_size.y > 0.0F &&
                      has_nonzero_extent(state.rendering.view_dimensions);
    if (!canvas.has_area) {
        ImGui::Dummy(
            ImVec2{std::max(canvas.area_size.x, 0.0F), std::max(canvas.area_size.y, 0.0F)});
        return canvas;
    }
    constexpr ImGuiButtonFlags input_flags = ImGuiButtonFlags_MouseButtonLeft |
                                             ImGuiButtonFlags_MouseButtonMiddle |
                                             ImGuiButtonFlags_MouseButtonRight;
    ImGui::InvisibleButton("##Elf3DViewportInteraction", canvas.area_size, input_flags);
    canvas.image_min = ImGui::GetItemRectMin();
    return canvas;
}

[[nodiscard]] bool resize_3d_view(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    elf3d::Extent2D target_extent;
    if (canvas.has_area) {
        const std::uint32_t scale =
            static_cast<std::uint32_t>(context.state.rendering.diagnostic_render_scale_percent);
        target_extent.width = std::max(
            std::uint32_t{1}, (context.state.rendering.view_dimensions.width * scale + 99U) / 100U);
        target_extent.height =
            std::max(std::uint32_t{1},
                     (context.state.rendering.view_dimensions.height * scale + 99U) / 100U);
    }
    context.state.rendering.render_target_dimensions = target_extent;
    const elf3d::Result<void> result = context.viewport.resize(target_extent);
    if (!result) {
        set_viewport_error(context.state, result.error());
        return false;
    }
    if (canvas.has_area) {
        return true;
    }
    context.viewport.cancel_interaction();
    context.state.rendering.framebuffer_valid = false;
    context.state.rendering.statistics = {};
    return false;
}

void reset_view_camera_if_needed(const ViewPanelContext& context) {
    if (!context.scene.camera_needs_reset) {
        return;
    }
    const elf3d::Result<void> result =
        context.viewport.reset_view(*context.scene.scene, context.scene.camera);
    context.scene.camera_needs_reset = false;
    if (!result) {
        set_viewport_error(context.state, result.error());
    }
}

[[nodiscard]] bool
measurement_cursor_requested(const ViewPanelContext& context, bool hovered,
                             const std::optional<elf3d::NavigationSnapshot>& snapshot) noexcept {
    return hovered && context.tools.active_tool() == ViewerTool::distance_measurement &&
           (!snapshot.has_value() || !snapshot->is_pointer_captured);
}

[[nodiscard]] Result<InteractionRegionInput> routed_viewport_input(const ViewPanelContext& context,
                                                                   const ViewportCanvas& canvas) {
    if (!context.interaction_owner.is_valid()) {
        return elf3d::Error{elf3d::ErrorCode::invalid_interaction_owner,
                            "The viewer viewport interaction owner is unavailable"};
    }
    InteractionArbiter& arbiter = context.application.interaction_arbiter();
    const Result<InteractionRegionId> registered = arbiter.register_region(
        context.interaction_owner, context.interaction_region,
        InteractionRegionDescription{Float2{canvas.image_min.x, canvas.image_min.y},
                                     Float2{canvas.area_size.x, canvas.area_size.y},
                                     context.state.rendering.render_target_dimensions,
                                     context.state.interaction.application_focused &&
                                         !navigation_blocked_by_modal()});
    if (!registered) {
        return registered.error();
    }
    context.interaction_region = registered.value();
    arbiter.finalize_regions();
    return arbiter.region_input(context.interaction_owner, context.interaction_region);
}

void synchronize_navigation_capture(const ViewPanelContext& context, InteractionArbiter& arbiter) {
    const std::optional<elf3d::NavigationSnapshot> navigation =
        context.viewport.navigation_snapshot();
    const bool capture_requested = navigation.has_value() && navigation->is_pointer_captured;
    const InteractionSnapshot arbitration = arbiter.snapshot(context.interaction_owner);
    if (capture_requested) {
        const Result<void> requested = arbiter.request(
            context.interaction_owner, context.interaction_region, InteractionRequest::navigation);
        if (!requested) {
            context.viewport.cancel_interaction();
            set_viewport_error(context.state, requested.error());
        }
        return;
    }
    if (arbitration.active_owner == context.interaction_owner &&
        arbitration.state == InteractionState::navigation) {
        arbiter.release(context.interaction_owner);
    }
}

void update_viewport_input(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    const Result<InteractionRegionInput> routed = routed_viewport_input(context, canvas);
    if (!routed) {
        set_viewport_error(context.state, routed.error());
        return;
    }
    InteractionArbiter& arbiter = context.application.interaction_arbiter();
    const std::optional<elf3d::NavigationSnapshot> before = context.viewport.navigation_snapshot();
    if (measurement_cursor_requested(context, routed.value().hovered, before)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (arbiter.snapshot(context.interaction_owner).cancellation_reason !=
        InteractionCancellationReason::none) {
        context.viewport.cancel_interaction();
    }
    const elf3d::NavigationInput input =
        viewport_input_from_framework(context.state, context.application.input(), routed.value());
    const elf3d::Result<void> result =
        context.viewport.update_navigation(*context.scene.scene, context.scene.camera, input);
    if (!result) {
        set_viewport_error(context.state, result.error());
        return;
    }
    const std::optional<elf3d::NavigationSnapshot> after = context.viewport.navigation_snapshot();
    const bool navigation_captured = after.has_value() && after->is_pointer_captured;
    const Result<void> tool_result = context.tools.update(ToolUpdateContext{
        *context.scene.scene, context.viewport, context.scene.camera, routed.value(),
        context.application.input().modifiers, navigation_captured});
    if (!tool_result) {
        set_viewport_error(context.state, tool_result.error());
        return;
    }
    synchronize_navigation_capture(context, arbiter);
}

void present_viewport_texture(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    const elf3d::Result<void> image = context.application.draw_viewport_image(
        context.viewport, elf3d::Float2{canvas.image_min.x, canvas.image_min.y},
        elf3d::Float2{canvas.area_size.x, canvas.area_size.y});
    if (!image) {
        set_viewport_error(context.state, image.error());
        return;
    }
    draw_measurement_label(context.state, context.viewport, context.scene, context.tools,
                           MeasurementLabelArea{canvas.image_min, canvas.area_size});
}

void render_3d_view(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    context.viewport.set_clear_color(elf3d::Color4{
        context.state.rendering.clear_color[0], context.state.rendering.clear_color[1],
        context.state.rendering.clear_color[2], context.state.rendering.clear_color[3]});
    context.viewport.set_basic_lighting(context.state.rendering.lighting);
    context.viewport.set_environment_lighting(context.state.rendering.environment_lighting);
    context.viewport.set_display_transform(context.state.rendering.display_transform);
    context.viewport.set_render_shading_mode(context.state.rendering.shading_mode);
    const RetainedViewportFrameKey key =
        viewport_frame_key(context.state, context.scene, context.viewport, context.tools);
    if (viewport_frame_render_required(context.state, key, context.viewport)) {
        ViewportRenderOptions options;
        options.shading_mode = context.state.rendering.shading_mode;
        options.highlight = context.tools.selection().render_feedback(context.viewport);
        const Result<MeasurementOverlay> measurement_overlay =
            context.tools.measurement().overlay(*context.scene.scene, context.viewport);
        if (!measurement_overlay) {
            set_viewport_error(context.state, measurement_overlay.error());
            return;
        }
        const Result<ClippingToolOverlay> clipping_overlay =
            context.tools.clipping().overlay(*context.scene.scene, context.viewport);
        if (!clipping_overlay) {
            set_viewport_error(context.state, clipping_overlay.error());
            return;
        }
        std::array<OverlayLineSegment, 2 + 4 + maximum_clipping_boxes * 12> overlay_lines;
        std::size_t overlay_line_count = 0;
        for (const OverlayLineSegment& line : measurement_overlay.value().line_span()) {
            overlay_lines[overlay_line_count++] = line;
        }
        for (const OverlayLineSegment& line : clipping_overlay.value().line_span()) {
            overlay_lines[overlay_line_count++] = line;
        }
        options.overlay_lines =
            std::span<const OverlayLineSegment>{overlay_lines.data(), overlay_line_count};
        options.overlay_markers = measurement_overlay.value().marker_span();
        const elf3d::Result<void> result = context.application.queue_viewport_render(
            context.viewport, *context.scene.scene, context.scene.camera, options);
        if (!result) {
            set_viewport_error(context.state, result.error());
            return;
        }
        context.state.rendering.retained_viewport_frame = key;
        context.state.rendering.viewport_rendered_this_frame = true;
        ++context.state.performance.rendered_3d_frame_count;
    } else {
        ++context.state.performance.reused_3d_frame_count;
    }
    present_viewport_texture(context, canvas);
}

void draw_3d_view_content(const ViewPanelContext& context) {
    const ViewportCanvas canvas = begin_viewport_canvas(context.state);
    if (resize_3d_view(context, canvas)) {
        reset_view_camera_if_needed(context);
        update_viewport_input(context, canvas);
        render_3d_view(context, canvas);
    }
    if (!context.state.notifications.viewport_error.empty()) {
        draw_viewport_error_overlay(context.state.notifications.viewport_error, canvas.image_min,
                                    canvas.area_size);
    }
}

void build_3d_view(const ViewPanelContext& context) {
    if (!context.state.shell.show_3d_view) {
        deactivate_3d_view(context);
        return;
    }
    set_default_dock(context.state.shell.dock_center_id != 0 ? context.state.shell.dock_center_id
                                                             : context.dockspace_id,
                     context.state.shell.apply_dock_layout);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0F, 0.0F});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.94F, 0.94F, 0.94F, 1.0F});
    const std::string window_title = viewport_window_title(context.scene);
    const bool visible = begin_panel_window(window_title.c_str(), &context.state.shell.show_3d_view,
                                            context.state.presentation.panel_title_font, flags);
    if (visible) {
        draw_3d_view_content(context);
    } else {
        deactivate_3d_view(context);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace elf3d::viewer
