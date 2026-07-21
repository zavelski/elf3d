#include "viewer_internal.hpp"

#include <elf3d/imgui/texture.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace elf3d::viewer {

elf3d::GraphicsProcedure load_opengl_procedure(const char* name) noexcept {
    return glfwGetProcAddress(name);
}

std::uint32_t to_pixel_dimension(float logical_size, float framebuffer_scale) noexcept {
    if (!std::isfinite(logical_size) || !std::isfinite(framebuffer_scale) || logical_size <= 0.0F ||
        framebuffer_scale <= 0.0F) {
        return 0;
    }
    const double pixel_size =
        static_cast<double>(logical_size) * static_cast<double>(framebuffer_scale);
    const double maximum = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    if (pixel_size >= maximum) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(std::floor(pixel_size + 0.5));
}

elf3d::Extent2D content_extent_in_pixels(ImVec2 logical_size) noexcept {
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    return elf3d::Extent2D{to_pixel_dimension(logical_size.x, scale.x),
                           to_pixel_dimension(logical_size.y, scale.y)};
}

[[nodiscard]] bool has_nonzero_extent(elf3d::Extent2D extent) noexcept {
    return extent.width != 0 && extent.height != 0;
}

[[nodiscard]] const char* tool_name(elf3d::ViewportTool tool) noexcept {
    switch (tool) {
    case elf3d::ViewportTool::selection:
        return "Select";
    case elf3d::ViewportTool::distance_measurement:
        return "Measure Distance";
    }
    return "Select";
}

[[nodiscard]] const char* measurement_state_name(elf3d::DistanceMeasurementState state) noexcept {
    switch (state) {
    case elf3d::DistanceMeasurementState::empty:
        return "Empty";
    case elf3d::DistanceMeasurementState::awaiting_first_point:
        return "Select first point";
    case elf3d::DistanceMeasurementState::awaiting_second_point:
        return "Select second point";
    case elf3d::DistanceMeasurementState::complete:
        return "Complete";
    }
    return "Empty";
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

[[nodiscard]] const char* unit_name(elf3d::LengthDisplayUnit unit) noexcept {
    switch (unit) {
    case elf3d::LengthDisplayUnit::automatic_metric:
        return "Automatic metric";
    case elf3d::LengthDisplayUnit::meters:
        return "Meters";
    case elf3d::LengthDisplayUnit::centimeters:
        return "Centimeters";
    case elf3d::LengthDisplayUnit::millimeters:
        return "Millimeters";
    case elf3d::LengthDisplayUnit::feet:
        return "Feet";
    case elf3d::LengthDisplayUnit::inches:
        return "Inches";
    }
    return "Meters";
}

[[nodiscard]] const char* unit_suffix(elf3d::LengthDisplayUnit unit) noexcept {
    switch (unit) {
    case elf3d::LengthDisplayUnit::meters:
        return "m";
    case elf3d::LengthDisplayUnit::centimeters:
        return "cm";
    case elf3d::LengthDisplayUnit::millimeters:
        return "mm";
    case elf3d::LengthDisplayUnit::feet:
        return "ft";
    case elf3d::LengthDisplayUnit::inches:
        return "in";
    case elf3d::LengthDisplayUnit::automatic_metric:
        break;
    }
    return "m";
}

struct DisplayDistance {
    double value = 0.0;
    elf3d::LengthDisplayUnit unit = elf3d::LengthDisplayUnit::meters;
};

[[nodiscard]] DisplayDistance display_distance(double meters,
                                               elf3d::LengthDisplayUnit unit) noexcept {
    elf3d::LengthDisplayUnit resolved = unit;
    if (resolved == elf3d::LengthDisplayUnit::automatic_metric) {
        const double absolute = std::abs(meters);
        if (absolute >= 1.0) {
            resolved = elf3d::LengthDisplayUnit::meters;
        } else if (absolute >= 0.01) {
            resolved = elf3d::LengthDisplayUnit::centimeters;
        } else {
            resolved = elf3d::LengthDisplayUnit::millimeters;
        }
    }

    switch (resolved) {
    case elf3d::LengthDisplayUnit::meters:
        return DisplayDistance{meters, resolved};
    case elf3d::LengthDisplayUnit::centimeters:
        return DisplayDistance{meters * 100.0, resolved};
    case elf3d::LengthDisplayUnit::millimeters:
        return DisplayDistance{meters * 1000.0, resolved};
    case elf3d::LengthDisplayUnit::feet:
        return DisplayDistance{meters * 3.280839895, resolved};
    case elf3d::LengthDisplayUnit::inches:
        return DisplayDistance{meters * 39.37007874, resolved};
    case elf3d::LengthDisplayUnit::automatic_metric:
        break;
    }
    return DisplayDistance{meters, elf3d::LengthDisplayUnit::meters};
}

[[nodiscard]] std::string format_distance(double meters, elf3d::LengthDisplayUnit unit) {
    const DisplayDistance display = display_distance(meters, unit);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.4g %s", display.value, unit_suffix(display.unit));
    return std::string{buffer};
}

[[nodiscard]] bool navigation_blocked_by_modal() noexcept {
    return ImGui::GetTopMostPopupModal() != nullptr;
}

void disable_imgui_cursor_updates(ViewerState& state) noexcept {
    if (state.imgui_cursor_change_disabled_by_capture) {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    state.imgui_cursor_change_was_disabled =
        (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) != 0;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    state.imgui_cursor_change_disabled_by_capture = true;
}

void restore_imgui_cursor_updates(ViewerState& state) noexcept {
    if (!state.imgui_cursor_change_disabled_by_capture) {
        return;
    }
    if (!state.imgui_cursor_change_was_disabled) {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
    state.imgui_cursor_change_disabled_by_capture = false;
    state.imgui_cursor_change_was_disabled = false;
}

[[nodiscard]] std::optional<ImVec2> glfw_cursor_position(GLFWwindow* window) noexcept {
    if (window == nullptr) {
        return std::nullopt;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }
    return ImVec2{static_cast<float>(x), static_cast<float>(y)};
}

void reset_navigation_cursor_sample(GLFWwindow* window, ViewerState& state) noexcept {
    state.navigation_cursor_position = glfw_cursor_position(window);
}

struct NavigationCursorSample {
    ImVec2 position;
    ImVec2 delta;
};

[[nodiscard]] NavigationCursorSample
navigation_cursor_sample(GLFWwindow* window, ViewerState& state, bool tracking_enabled) noexcept {
    const ImGuiIO& io = ImGui::GetIO();
    NavigationCursorSample sample{io.MousePos, ImVec2{0.0F, 0.0F}};
    if (!tracking_enabled) {
        state.navigation_cursor_position.reset();
        return sample;
    }

    const std::optional<ImVec2> cursor_position = glfw_cursor_position(window);
    if (!cursor_position.has_value()) {
        state.navigation_cursor_position.reset();
        return sample;
    }

    sample.position = *cursor_position;
    if (state.navigation_cursor_position.has_value()) {
        sample.delta = ImVec2{sample.position.x - state.navigation_cursor_position->x,
                              sample.position.y - state.navigation_cursor_position->y};
    }
    state.navigation_cursor_position = sample.position;
    return sample;
}

void acquire_navigation_cursor(GLFWwindow* window, ViewerState& state) noexcept {
    if (window == nullptr) {
        return;
    }

    disable_imgui_cursor_updates(state);
    if (!state.cursor_captured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        state.cursor_captured = true;
        reset_navigation_cursor_sample(window, state);
    }

#if defined(GLFW_RAW_MOUSE_MOTION)
    if (!state.raw_mouse_motion_enabled && glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        state.raw_mouse_motion_enabled = true;
    }
#endif
}

void release_navigation_cursor(GLFWwindow* window, ViewerState& state) noexcept {
#if defined(GLFW_RAW_MOUSE_MOTION)
    if (window != nullptr && state.raw_mouse_motion_enabled) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }
#endif
    state.raw_mouse_motion_enabled = false;

    if (window != nullptr && state.cursor_captured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    state.cursor_captured = false;
    state.navigation_cursor_position.reset();
    restore_imgui_cursor_updates(state);
}

void sync_navigation_cursor(GLFWwindow* window, ViewerState& state,
                            const elf3d::Viewport& engine_viewport, bool view_available) noexcept {
    const std::optional<elf3d::NavigationSnapshot> snapshot = engine_viewport.navigation_snapshot();
    const bool should_capture = view_available && state.application_focused &&
                                !navigation_blocked_by_modal() && snapshot.has_value() &&
                                snapshot->is_pointer_captured;
    if (should_capture) {
        acquire_navigation_cursor(window, state);
    } else {
        release_navigation_cursor(window, state);
    }
}

[[nodiscard]] bool glfw_key_down(GLFWwindow* window, int key) noexcept {
    return window != nullptr && glfwGetKey(window, key) == GLFW_PRESS;
}

[[nodiscard]] bool glfw_mouse_button_down(GLFWwindow* window, int button) noexcept {
    return window != nullptr && glfwGetMouseButton(window, button) == GLFW_PRESS;
}

[[nodiscard]] float navigation_wheel_delta_for_view(const ViewerState& state, ImVec2 item_min,
                                                    ImVec2 item_size,
                                                    bool input_available) noexcept {
    if (!input_available || !state.navigation_wheel_position.has_value()) {
        return 0.0F;
    }

    const ImVec2 position = *state.navigation_wheel_position;
    const ImVec2 item_max{item_min.x + item_size.x, item_min.y + item_size.y};
    const bool inside = position.x >= item_min.x && position.x < item_max.x &&
                        position.y >= item_min.y && position.y < item_max.y;
    return inside ? state.navigation_wheel_delta : 0.0F;
}

struct ViewportInputRegion {
    ImVec2 minimum;
    ImVec2 size;
    elf3d::Extent2D render_extent;
    bool hovered = false;
    bool focused = false;
    bool pointer_captured = false;
};

[[nodiscard]] bool navigation_tracking_enabled(const ViewportInputRegion& region,
                                               bool left_button_down, bool middle_button_down,
                                               bool right_button_down) noexcept {
    return region.hovered || region.focused || region.pointer_captured || left_button_down ||
           middle_button_down || right_button_down;
}

[[nodiscard]] elf3d::ViewportInput
viewport_input_from_imgui(GLFWwindow* window, ViewerState& state,
                          const ViewportInputRegion& region) noexcept {
    const ImGuiIO& io = ImGui::GetIO();
    const float x_scale = region.size.x > 0.0F
                              ? static_cast<float>(region.render_extent.width) / region.size.x
                              : 0.0F;
    const float y_scale = region.size.y > 0.0F
                              ? static_cast<float>(region.render_extent.height) / region.size.y
                              : 0.0F;
    const bool left_button_down = glfw_mouse_button_down(window, GLFW_MOUSE_BUTTON_LEFT);
    const bool middle_button_down = glfw_mouse_button_down(window, GLFW_MOUSE_BUTTON_MIDDLE);
    const bool right_button_down = glfw_mouse_button_down(window, GLFW_MOUSE_BUTTON_RIGHT);
    const bool tracking_enabled = navigation_tracking_enabled(
        region, left_button_down, middle_button_down, right_button_down);
    const NavigationCursorSample cursor_sample =
        navigation_cursor_sample(window, state, tracking_enabled);
    elf3d::ViewportInput input;
    input.frame_delta_seconds = io.DeltaTime;
    input.pointer_position_pixels = {
        (cursor_sample.position.x - region.minimum.x) * x_scale,
        (cursor_sample.position.y - region.minimum.y) * y_scale,
    };
    input.pointer_delta_pixels = {cursor_sample.delta.x * x_scale, cursor_sample.delta.y * y_scale};
    input.wheel_delta = navigation_wheel_delta_for_view(state, region.minimum, region.size,
                                                        state.application_focused &&
                                                            !navigation_blocked_by_modal());
    input.is_hovered = region.hovered || input.wheel_delta != 0.0F;
    input.is_focused = region.focused || region.pointer_captured;
    input.left_button_down = left_button_down;
    input.middle_button_down = middle_button_down;
    input.right_button_down = right_button_down;
    input.shift_down = io.KeyShift;
    input.control_down = io.KeyCtrl;
    input.alt_down = io.KeyAlt;
    input.x_down = glfw_key_down(window, GLFW_KEY_X);
    input.z_down = glfw_key_down(window, GLFW_KEY_Z);
    if (!io.WantTextInput) {
        input.space_down = glfw_key_down(window, GLFW_KEY_SPACE);
        input.w_pressed = glfw_key_down(window, GLFW_KEY_W);
        input.s_pressed = glfw_key_down(window, GLFW_KEY_S);
        input.a_pressed = glfw_key_down(window, GLFW_KEY_A);
        input.d_pressed = glfw_key_down(window, GLFW_KEY_D);
        input.q_pressed = glfw_key_down(window, GLFW_KEY_Q);
        input.e_pressed = glfw_key_down(window, GLFW_KEY_E);
    }
    return input;
}

void set_viewport_error(ViewerState& state, const elf3d::Error& error) {
    state.viewport_error = error.message();
    state.framebuffer_valid = false;
}

void reset_demo_cube_transform(ViewerState& state, ViewerScene& scene) {
    state.rotation_angle = 0.0F;
    if (!scene.cube.has_value()) {
        return;
    }

    const elf3d::Result<void> reset_result =
        scene.scene->set_local_transform(*scene.cube, elf3d::Transform{});
    if (!reset_result) {
        set_viewport_error(state, reset_result.error());
    }
}

void update_demo_cube_animation(ViewerState& state, ViewerScene& scene) {
    if (scene.is_imported() || !state.rotate_cube || !scene.cube.has_value()) {
        return;
    }

    state.rotation_angle = std::fmod(
        state.rotation_angle + state.rotation_speed * ImGui::GetIO().DeltaTime, 6.2831853072F);
    elf3d::Transform cube_transform;
    cube_transform.rotation = axis_angle({0.0F, 1.0F, 0.0F}, state.rotation_angle);
    const elf3d::Result<void> transform_result =
        scene.scene->set_local_transform(*scene.cube, cube_transform);
    if (!transform_result) {
        set_viewport_error(state, transform_result.error());
    }
}

void apply_demo_cube_color(ViewerState& state, ViewerScene& scene) {
    if (scene.is_imported() || !scene.cube_material.has_value()) {
        return;
    }

    const elf3d::Result<void> material_result = scene.scene->set_material_description(
        *scene.cube_material,
        elf3d::MaterialDescription{elf3d::Color4{state.cube_color[0], state.cube_color[1],
                                                 state.cube_color[2], state.cube_color[3]}});
    if (!material_result) {
        set_viewport_error(state, material_result.error());
    }
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

void draw_measurement_label(ViewerState& state, elf3d::Viewport& engine_viewport,
                            const ViewerScene& scene, ImVec2 image_min, ImVec2 area_size) {
    const elf3d::DistanceMeasurementSnapshot measurement =
        engine_viewport.distance_measurement_snapshot(*scene.scene);
    if (!measurement.overlay_visible || !measurement.midpoint_world_position.has_value()) {
        return;
    }

    const double meters = measurement.state == elf3d::DistanceMeasurementState::complete
                              ? measurement.distance_meters
                              : measurement.preview_distance_meters;
    const std::string label =
        format_distance(meters, engine_viewport.measurement_settings().display_unit);
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

    const float logical_x = image_min.x + ((projected.value().position_pixels.x + 0.5F) /
                                           static_cast<float>(state.view_dimensions.width)) *
                                              area_size.x;
    const float logical_y = image_min.y + ((projected.value().position_pixels.y + 0.5F) /
                                           static_cast<float>(state.view_dimensions.height)) *
                                              area_size.y;
    const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    ImVec2 label_min{logical_x + 8.0F, logical_y - text_size.y - 8.0F};
    const ImVec2 image_max{image_min.x + area_size.x, image_min.y + area_size.y};
    const float minimum_x = image_min.x + 4.0F;
    const float minimum_y = image_min.y + 4.0F;
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
    bool hovered = false;
};

void deactivate_3d_view(const ViewPanelContext& context) {
    context.state->view_dimensions = {};
    context.state->framebuffer_valid = false;
    context.state->statistics = {};
    context.viewport->cancel_interaction();
    release_navigation_cursor(context.window, *context.state);
    const elf3d::Result<void> result = context.viewport->resize({});
    if (!result) {
        set_viewport_error(*context.state, result.error());
    }
}

[[nodiscard]] ViewportCanvas begin_viewport_canvas(ViewerState& state) {
    ViewportCanvas canvas;
    canvas.area_size = ImGui::GetContentRegionAvail();
    canvas.image_min = ImGui::GetCursorScreenPos();
    state.view_dimensions = content_extent_in_pixels(canvas.area_size);
    canvas.has_area = canvas.area_size.x > 0.0F && canvas.area_size.y > 0.0F &&
                      has_nonzero_extent(state.view_dimensions);
    if (!canvas.has_area) {
        ImGui::Dummy(
            ImVec2{std::max(canvas.area_size.x, 0.0F), std::max(canvas.area_size.y, 0.0F)});
        return canvas;
    }
    constexpr ImGuiButtonFlags input_flags = ImGuiButtonFlags_MouseButtonLeft |
                                             ImGuiButtonFlags_MouseButtonMiddle |
                                             ImGuiButtonFlags_MouseButtonRight;
    ImGui::InvisibleButton("##Elf3DViewportInput", canvas.area_size, input_flags);
    canvas.image_min = ImGui::GetItemRectMin();
    canvas.hovered = !navigation_blocked_by_modal() && ImGui::IsItemHovered();
    return canvas;
}

[[nodiscard]] bool resize_3d_view(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    const elf3d::Result<void> result = context.viewport->resize(
        canvas.has_area ? context.state->view_dimensions : elf3d::Extent2D{});
    if (!result) {
        set_viewport_error(*context.state, result.error());
        return false;
    }
    if (canvas.has_area) {
        return true;
    }
    context.viewport->cancel_interaction();
    release_navigation_cursor(context.window, *context.state);
    context.state->framebuffer_valid = false;
    context.state->statistics = {};
    return false;
}

void reset_view_camera_if_needed(const ViewPanelContext& context) {
    if (!context.scene->camera_needs_reset) {
        return;
    }
    const elf3d::Result<void> result =
        context.viewport->reset_view(*context.scene->scene, context.scene->camera);
    context.scene->camera_needs_reset = false;
    if (!result) {
        set_viewport_error(*context.state, result.error());
    }
}

[[nodiscard]] bool
measurement_cursor_requested(const ViewPanelContext& context, const ViewportCanvas& canvas,
                             const std::optional<elf3d::NavigationSnapshot>& snapshot) noexcept {
    return canvas.hovered &&
           context.viewport->active_tool() == elf3d::ViewportTool::distance_measurement &&
           (!snapshot.has_value() || !snapshot->is_pointer_captured);
}

[[nodiscard]] bool viewport_input_focused(const ViewPanelContext& context) noexcept {
    return context.state->application_focused &&
           ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
           !navigation_blocked_by_modal();
}

[[nodiscard]] bool
viewport_pointer_captured(const ViewPanelContext& context,
                          const std::optional<elf3d::NavigationSnapshot>& snapshot) noexcept {
    return context.state->application_focused && !navigation_blocked_by_modal() &&
           (context.state->cursor_captured ||
            (snapshot.has_value() && snapshot->is_pointer_captured));
}

void update_viewport_input(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    const std::optional<elf3d::NavigationSnapshot> snapshot =
        context.viewport->navigation_snapshot();
    if (measurement_cursor_requested(context, canvas, snapshot)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    const ViewportInputRegion region{canvas.image_min,
                                     canvas.area_size,
                                     context.state->view_dimensions,
                                     canvas.hovered,
                                     viewport_input_focused(context),
                                     viewport_pointer_captured(context, snapshot)};
    const elf3d::ViewportInput input =
        viewport_input_from_imgui(context.window, *context.state, region);
    const elf3d::Result<void> result =
        context.viewport->update_navigation(*context.scene->scene, context.scene->camera, input);
    if (!result) {
        set_viewport_error(*context.state, result.error());
    }
    sync_navigation_cursor(context.window, *context.state, *context.viewport, true);
}

void present_viewport_texture(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    if (!context.state->framebuffer_valid) {
        return;
    }
    const elf3d::Result<elf3d::NativeTextureView> texture =
        context.engine->native_texture_view(context.viewport->color_texture());
    if (!texture) {
        set_viewport_error(*context.state, texture.error());
        return;
    }
    const elf3d::Result<void> image = elf3d::imgui::draw_image(
        texture.value(), elf3d::Float2{canvas.image_min.x, canvas.image_min.y},
        elf3d::Float2{canvas.area_size.x, canvas.area_size.y});
    if (!image) {
        set_viewport_error(*context.state, image.error());
        return;
    }
    draw_measurement_label(*context.state, *context.viewport, *context.scene, canvas.image_min,
                           canvas.area_size);
}

void render_3d_view(const ViewPanelContext& context, const ViewportCanvas& canvas) {
    context.viewport->set_clear_color(
        elf3d::Color4{context.state->clear_color[0], context.state->clear_color[1],
                      context.state->clear_color[2], context.state->clear_color[3]});
    context.viewport->set_basic_lighting(context.state->lighting);
    const elf3d::Result<void> result =
        context.viewport->render(*context.scene->scene, context.scene->camera);
    if (!result) {
        set_viewport_error(*context.state, result.error());
        return;
    }
    context.state->statistics = context.viewport->render_statistics();
    context.state->framebuffer_valid = context.viewport->framebuffer_valid();
    present_viewport_texture(context, canvas);
}

void draw_3d_view_content(const ViewPanelContext& context) {
    const ViewportCanvas canvas = begin_viewport_canvas(*context.state);
    if (resize_3d_view(context, canvas)) {
        reset_view_camera_if_needed(context);
        update_viewport_input(context, canvas);
        render_3d_view(context, canvas);
    }
    if (!context.state->viewport_error.empty()) {
        draw_viewport_error_overlay(context.state->viewport_error, canvas.image_min,
                                    canvas.area_size);
    }
}

void build_3d_view(const ViewPanelContext& context) {
    if (!context.state->show_3d_view) {
        deactivate_3d_view(context);
        return;
    }
    set_default_dock(context.state->dock_center_id != 0 ? context.state->dock_center_id
                                                        : context.dockspace_id,
                     context.state->apply_dock_layout);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0F, 0.0F});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.94F, 0.94F, 0.94F, 1.0F});
    const bool visible = begin_panel_window("3D View", &context.state->show_3d_view,
                                            context.state->panel_title_font, flags);
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
