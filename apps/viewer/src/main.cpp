#include <elf3d/elf3d.h>
#include <elf3d/imgui/context.h>
#include <elf3d/imgui/texture.h>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr char glsl_version[] = "#version 410 core";

class GlfwRuntime final {
  public:
    [[nodiscard]] bool initialize() noexcept {
        initialized_ = glfwInit() == GLFW_TRUE;
        return initialized_;
    }

    ~GlfwRuntime() {
        if (initialized_) {
            glfwTerminate();
        }
    }

    GlfwRuntime() = default;
    GlfwRuntime(const GlfwRuntime &) = delete;
    GlfwRuntime &operator=(const GlfwRuntime &) = delete;

  private:
    bool initialized_ = false;
};

struct WindowDeleter {
    void operator()(GLFWwindow *window) const noexcept {
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
    }
};

using Window = std::unique_ptr<GLFWwindow, WindowDeleter>;

struct LoadFailure {
    std::string source_path;
    elf3d::Error error;
};

struct ViewerState {
    bool show_3d_view = true;
    bool show_scene_hierarchy = true;
    bool show_model_information = true;
    bool show_selection_panel = true;
    bool show_measurement_panel = true;
    bool show_clipping_panel = true;
    bool show_navigation_settings = false;
    bool show_imgui_demo = false;
    bool show_status_bar = true;
    bool show_about = false;
    bool request_open_modal = false;
    bool request_error_modal = false;
    std::array<char, 2048> open_path{};
    std::optional<std::string> dropped_path;
    bool drop_copy_failed = false;
    elf3d::Extent2D view_dimensions;
    bool framebuffer_valid = false;
    std::array<float, 4> clear_color{0.08F, 0.16F, 0.28F, 1.0F};
    std::array<float, 4> cube_color{0.72F, 0.32F, 0.12F, 1.0F};
    elf3d::BasicLighting lighting;
    bool rotate_cube = true;
    float rotation_speed = 0.8F;
    float rotation_angle = 0.0F;
    elf3d::RenderStatistics statistics;
    std::string viewport_error;
    std::optional<LoadFailure> load_failure;
    bool application_focused = true;
    std::optional<elf3d::EntityId> last_revealed_hierarchy_selection;
};

struct ViewerScene {
    std::unique_ptr<elf3d::Scene> scene;
    elf3d::EntityId camera;
    std::optional<elf3d::EntityId> cube;
    std::optional<elf3d::MaterialHandle> cube_material;
    std::filesystem::path source_path;
    elf3d::SceneStatistics source_statistics;
    elf3d::Bounds3 source_bounds;
    bool camera_needs_reset = true;
    bool hierarchy_snapshot_valid = false;
    elf3d::SceneHierarchySnapshot hierarchy_snapshot;

    [[nodiscard]] bool is_imported() const noexcept {
        return !source_path.empty();
    }
};

struct ViewerCommands {
    bool reload = false;
    bool close_scene = false;
    bool fit_to_scene = false;
    bool reset_view = false;
    bool select_tool = false;
    bool measure_tool = false;
    bool show_clipping_panel = false;
    bool enable_section_plane = false;
    bool flip_section_side = false;
    bool add_clipping_box_from_bounds = false;
    bool clear_clipping = false;
    bool toggle_clipping_helpers = false;
    bool fit_to_clipped_content = false;
    bool clear_selection = false;
    bool cancel_measurement = false;
    bool clear_measurement = false;
    bool hide_selected = false;
    bool show_selected = false;
    bool show_all = false;
    bool isolate_selected = false;
    bool exit_isolation = false;
};

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path &path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

elf3d::Quaternion axis_angle(elf3d::Float3 axis, float radians) noexcept {
    const float half_angle = radians * 0.5F;
    const float sine = std::sin(half_angle);
    return elf3d::Quaternion{axis.x * sine, axis.y * sine, axis.z * sine, std::cos(half_angle)};
}

[[nodiscard]] elf3d::Result<elf3d::EntityId> create_viewer_camera(elf3d::Scene &scene) {
    const elf3d::Result<elf3d::EntityId> camera_result =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!camera_result) {
        return camera_result.error();
    }
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    const elf3d::Result<void> transform_result =
        scene.set_local_transform(camera_result.value(), transform);
    if (!transform_result) {
        return transform_result.error();
    }
    return camera_result.value();
}

[[nodiscard]] elf3d::Result<ViewerScene> create_demo_scene(elf3d::Engine &engine) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();

    constexpr float low = -0.5F;
    constexpr float high = 0.5F;
    const std::array<elf3d::VertexPositionNormal, 24> vertices{{
        {{low, low, high}, {0.0F, 0.0F, 1.0F}},   {{high, low, high}, {0.0F, 0.0F, 1.0F}},
        {{high, high, high}, {0.0F, 0.0F, 1.0F}}, {{low, high, high}, {0.0F, 0.0F, 1.0F}},
        {{high, low, low}, {0.0F, 0.0F, -1.0F}},  {{low, low, low}, {0.0F, 0.0F, -1.0F}},
        {{low, high, low}, {0.0F, 0.0F, -1.0F}},  {{high, high, low}, {0.0F, 0.0F, -1.0F}},
        {{high, low, high}, {1.0F, 0.0F, 0.0F}},  {{high, low, low}, {1.0F, 0.0F, 0.0F}},
        {{high, high, low}, {1.0F, 0.0F, 0.0F}},  {{high, high, high}, {1.0F, 0.0F, 0.0F}},
        {{low, low, low}, {-1.0F, 0.0F, 0.0F}},   {{low, low, high}, {-1.0F, 0.0F, 0.0F}},
        {{low, high, high}, {-1.0F, 0.0F, 0.0F}}, {{low, high, low}, {-1.0F, 0.0F, 0.0F}},
        {{low, high, high}, {0.0F, 1.0F, 0.0F}},  {{high, high, high}, {0.0F, 1.0F, 0.0F}},
        {{high, high, low}, {0.0F, 1.0F, 0.0F}},  {{low, high, low}, {0.0F, 1.0F, 0.0F}},
        {{low, low, low}, {0.0F, -1.0F, 0.0F}},   {{high, low, low}, {0.0F, -1.0F, 0.0F}},
        {{high, low, high}, {0.0F, -1.0F, 0.0F}}, {{low, low, high}, {0.0F, -1.0F, 0.0F}},
    }};
    const std::array<std::uint32_t, 36> indices{{
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    }};

    const elf3d::Result<elf3d::MeshHandle> mesh_result =
        scene->create_mesh(elf3d::MeshDataView{vertices, indices});
    if (!mesh_result) {
        return mesh_result.error();
    }
    const elf3d::Result<elf3d::MaterialHandle> material_result = scene->create_material(
        elf3d::MaterialDescription{elf3d::Color4{0.72F, 0.32F, 0.12F, 1.0F}});
    if (!material_result) {
        return material_result.error();
    }
    const elf3d::Result<elf3d::EntityId> cube_result =
        scene->create_model(mesh_result.value(), material_result.value());
    if (!cube_result) {
        return cube_result.error();
    }

    const elf3d::Result<elf3d::EntityId> camera_result = create_viewer_camera(*scene);
    if (!camera_result) {
        return camera_result.error();
    }
    const elf3d::Bounds3 bounds = scene->world_bounds();

    return ViewerScene{std::move(scene),
                       camera_result.value(),
                       cube_result.value(),
                       material_result.value(),
                       {},
                       elf3d::SceneStatistics{2, 1, 1, 1, 1, 24, 36, 12},
                       bounds,
                       true};
}

[[nodiscard]] elf3d::Result<ViewerScene> load_model_scene(elf3d::Engine &engine,
                                                          const std::filesystem::path &path) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.load_scene(path);
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();
    const elf3d::SceneStatistics source_statistics = scene->statistics();
    const elf3d::Bounds3 bounds = scene->world_bounds();
    const elf3d::Result<elf3d::EntityId> camera_result = create_viewer_camera(*scene);
    if (!camera_result) {
        return camera_result.error();
    }

    return ViewerScene{std::move(scene),
                       camera_result.value(),
                       std::nullopt,
                       std::nullopt,
                       path,
                       source_statistics,
                       bounds,
                       true};
}

void glfw_error_callback(int error_code, const char *description) {
    std::cerr << "GLFW error " << error_code << ": "
              << (description != nullptr ? description : "No description") << '\n';
}

void glfw_drop_callback(GLFWwindow *window, int path_count, const char **paths) {
    auto *state = static_cast<ViewerState *>(glfwGetWindowUserPointer(window));
    if (state == nullptr || path_count <= 0 || paths == nullptr || paths[0] == nullptr) {
        return;
    }
    try {
        state->dropped_path = paths[0];
    } catch (...) {
        state->drop_copy_failed = true;
    }
}

[[nodiscard]] bool has_nonzero_extent(elf3d::Extent2D extent) noexcept;

void build_main_menu(GLFWwindow *window, ViewerState &state, const ViewerScene &scene,
                     elf3d::Viewport &engine_viewport, ViewerCommands &commands) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...")) {
            state.request_open_modal = true;
        }
        ImGui::BeginDisabled(!scene.is_imported());
        commands.reload = ImGui::MenuItem("Reload");
        commands.close_scene = ImGui::MenuItem("Close Scene");
        ImGui::EndDisabled();
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("3D View", nullptr, &state.show_3d_view);
        ImGui::MenuItem("Scene Hierarchy", nullptr, &state.show_scene_hierarchy);
        ImGui::MenuItem("Model Information", nullptr, &state.show_model_information);
        ImGui::MenuItem("Selection", nullptr, &state.show_selection_panel);
        ImGui::MenuItem("Measurement", nullptr, &state.show_measurement_panel);
        ImGui::MenuItem("Clipping", nullptr, &state.show_clipping_panel);
        ImGui::MenuItem("Dear ImGui Demo", nullptr, &state.show_imgui_demo);
        ImGui::MenuItem("Status Bar", nullptr, &state.show_status_bar);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        const elf3d::ViewportTool active_tool = engine_viewport.active_tool();
        commands.select_tool =
            ImGui::MenuItem("Select", "S", active_tool == elf3d::ViewportTool::selection);
        commands.measure_tool = ImGui::MenuItem(
            "Measure Distance", "M", active_tool == elf3d::ViewportTool::distance_measurement);
        commands.show_clipping_panel = ImGui::MenuItem("Clipping");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Clipping")) {
        const elf3d::ClippingSnapshot clipping = engine_viewport.clipping_snapshot();
        commands.enable_section_plane =
            ImGui::MenuItem("Enable Section Plane", nullptr, clipping.section_plane.enabled);
        ImGui::BeginDisabled(!clipping.section_plane.enabled);
        commands.flip_section_side = ImGui::MenuItem("Flip Section Side");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(clipping.box_count >= elf3d::maximum_clipping_boxes);
        commands.add_clipping_box_from_bounds =
            ImGui::MenuItem("Add Box from Visible Bounds");
        ImGui::EndDisabled();
        commands.clear_clipping = ImGui::MenuItem("Clear Clipping");
        commands.toggle_clipping_helpers =
            ImGui::MenuItem("Show Helpers", nullptr, clipping.helpers.visible);
        commands.fit_to_clipped_content = ImGui::MenuItem("Fit to Clipped Content");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Camera")) {
        const elf3d::Result<elf3d::Bounds3> visible_bounds =
            engine_viewport.visible_bounds(*scene.scene);
        ImGui::BeginDisabled(!visible_bounds || !visible_bounds.value().is_valid ||
                             !has_nonzero_extent(state.view_dimensions) || !state.show_3d_view);
        commands.fit_to_scene = ImGui::MenuItem("Fit to Scene", "F");
        commands.reset_view = ImGui::MenuItem("Reset View", "Home");
        ImGui::EndDisabled();
        ImGui::Separator();
        const bool navigation_enabled = engine_viewport.navigation_enabled();
        if (ImGui::MenuItem("Enable Navigation", nullptr, navigation_enabled)) {
            engine_viewport.set_navigation_enabled(!navigation_enabled);
        }
        if (ImGui::MenuItem("Navigation Settings...")) {
            state.show_navigation_settings = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Selection")) {
        ImGui::BeginDisabled(!engine_viewport.has_selection());
        commands.clear_selection = ImGui::MenuItem("Clear Selection");
        commands.hide_selected = ImGui::MenuItem("Hide Selected");
        commands.show_selected = ImGui::MenuItem("Show Selected");
        commands.isolate_selected = ImGui::MenuItem("Isolate Selected");
        ImGui::EndDisabled();
        ImGui::Separator();
        commands.show_all = ImGui::MenuItem("Show All");
        ImGui::BeginDisabled(!engine_viewport.is_isolating());
        commands.exit_isolation = ImGui::MenuItem("Exit Isolation");
        ImGui::EndDisabled();
        ImGui::Separator();
        const bool selection_enabled = engine_viewport.selection_enabled();
        if (ImGui::MenuItem("Enable Selection", nullptr, selection_enabled)) {
            engine_viewport.set_selection_enabled(!selection_enabled);
        }
        if (ImGui::MenuItem("Selection Settings...")) {
            state.show_selection_panel = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Measurement")) {
        const elf3d::DistanceMeasurementSnapshot measurement =
            engine_viewport.distance_measurement_snapshot(*scene.scene);
        const bool incomplete =
            measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point;
        const bool has_measurement = measurement.first_point.has_value() ||
                                     measurement.second_point.has_value() ||
                                     measurement.preview_point.has_value();
        ImGui::BeginDisabled(!incomplete);
        commands.cancel_measurement = ImGui::MenuItem("Cancel Current", "Escape");
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!has_measurement);
        commands.clear_measurement = ImGui::MenuItem("Clear Measurement", "Delete");
        ImGui::EndDisabled();
        if (ImGui::MenuItem("Measurement Settings...")) {
            state.show_measurement_panel = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About Elf3D")) {
            state.show_about = true;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

elf3d::GraphicsProcedure load_opengl_procedure(const char *name) {
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

[[nodiscard]] const char *tool_name(elf3d::ViewportTool tool) noexcept {
    switch (tool) {
    case elf3d::ViewportTool::selection:
        return "Select";
    case elf3d::ViewportTool::distance_measurement:
        return "Measure Distance";
    }
    return "Select";
}

[[nodiscard]] const char *measurement_state_name(elf3d::DistanceMeasurementState state) noexcept {
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

[[nodiscard]] std::string clipping_status(const elf3d::ClippingSnapshot &snapshot,
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

[[nodiscard]] bool valid_box_for_commit(const elf3d::ClippingBox &box) noexcept {
    return finite_float3(box.minimum) && finite_float3(box.maximum) &&
           box.maximum.x - box.minimum.x > 0.00001F &&
           box.maximum.y - box.minimum.y > 0.00001F &&
           box.maximum.z - box.minimum.z > 0.00001F;
}

[[nodiscard]] elf3d::Float3 bounds_center(const elf3d::Bounds3 &bounds) noexcept {
    return elf3d::Float3{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                         (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                         (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

[[nodiscard]] const char *unit_name(elf3d::LengthDisplayUnit unit) noexcept {
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

[[nodiscard]] const char *unit_suffix(elf3d::LengthDisplayUnit unit) noexcept {
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
    return ImGui::IsPopupOpen("Open glTF Model") || ImGui::IsPopupOpen("Model Load Error");
}

[[nodiscard]] elf3d::ViewportInput
viewport_input_from_imgui(ImVec2 item_min, bool item_hovered, bool item_focused,
                          bool pointer_captured, ImVec2 item_size,
                          elf3d::Extent2D render_extent) noexcept {
    const ImGuiIO &io = ImGui::GetIO();
    const float x_scale =
        item_size.x > 0.0F ? static_cast<float>(render_extent.width) / item_size.x : 0.0F;
    const float y_scale =
        item_size.y > 0.0F ? static_cast<float>(render_extent.height) / item_size.y : 0.0F;
    elf3d::ViewportInput input;
    input.pointer_position_pixels = {
        (io.MousePos.x - item_min.x) * x_scale,
        (io.MousePos.y - item_min.y) * y_scale,
    };
    input.pointer_delta_pixels = {io.MouseDelta.x * x_scale, io.MouseDelta.y * y_scale};
    input.wheel_delta = item_hovered ? io.MouseWheel : 0.0F;
    input.is_hovered = item_hovered;
    input.is_focused = item_focused || pointer_captured;
    input.left_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    input.middle_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    input.right_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    input.shift_down = io.KeyShift;
    input.control_down = io.KeyCtrl;
    input.alt_down = io.KeyAlt;
    return input;
}

void set_viewport_error(ViewerState &state, const elf3d::Error &error) {
    state.viewport_error = error.message();
    state.framebuffer_valid = false;
}

void draw_measurement_label(ViewerState &state, elf3d::Viewport &engine_viewport,
                            const ViewerScene &scene, ImVec2 image_min, ImVec2 area_size) {
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
                                                  measurement.midpoint_world_position.value());
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

    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(label_min, label_max, IM_COL32(20, 24, 28, 220), 4.0F);
    draw_list->AddRect(label_min, label_max, IM_COL32(255, 255, 255, 90), 4.0F);
    draw_list->AddText(ImVec2{label_min.x + 4.0F, label_min.y + 2.0F}, IM_COL32(255, 255, 255, 255),
                       label.c_str());
}

void build_3d_view(ImGuiID dockspace_id, ViewerState &state, elf3d::Engine &engine,
                   elf3d::Viewport &engine_viewport, ViewerScene &active_scene) {
    if (!state.show_3d_view) {
        state.view_dimensions = {};
        state.framebuffer_valid = false;
        state.statistics = {};
        engine_viewport.cancel_interaction();
        const elf3d::Result<void> resize_result = engine_viewport.resize({});
        if (!resize_result) {
            set_viewport_error(state, resize_result.error());
        }
        return;
    }

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.055F, 0.06F, 0.07F, 1.0F});
    const bool is_visible = ImGui::Begin("3D View", &state.show_3d_view);
    ImGui::PopStyleColor();

    if (is_visible) {
        state.viewport_error.clear();
        if (!active_scene.is_imported()) {
            ImGui::Checkbox("Rotate cube", &state.rotate_cube);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130.0F);
            ImGui::SliderFloat("Speed", &state.rotation_speed, 0.0F, 3.0F, "%.2f rad/s");
            ImGui::SameLine();
            if (ImGui::Button("Reset cube transform") && active_scene.cube.has_value()) {
                state.rotation_angle = 0.0F;
                const elf3d::Result<void> reset_result = active_scene.scene->set_local_transform(
                    active_scene.cube.value(), elf3d::Transform{});
                if (!reset_result) {
                    set_viewport_error(state, reset_result.error());
                }
            }
            if (state.rotate_cube && active_scene.cube.has_value()) {
                state.rotation_angle = std::fmod(
                    state.rotation_angle + state.rotation_speed * ImGui::GetIO().DeltaTime,
                    6.2831853072F);
                elf3d::Transform cube_transform;
                cube_transform.rotation = axis_angle({0.0F, 1.0F, 0.0F}, state.rotation_angle);
                const elf3d::Result<void> transform_result =
                    active_scene.scene->set_local_transform(active_scene.cube.value(),
                                                            cube_transform);
                if (!transform_result) {
                    set_viewport_error(state, transform_result.error());
                }
            }
        } else {
            ImGui::TextUnformatted(path_to_utf8(active_scene.source_path).c_str());
        }

        ImGui::SetNextItemWidth(220.0F);
        ImGui::ColorEdit4("Clear color", state.clear_color.data(), ImGuiColorEditFlags_NoInputs);
        if (!active_scene.is_imported() && active_scene.cube_material.has_value()) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(220.0F);
            if (ImGui::ColorEdit4("Cube base color", state.cube_color.data(),
                                  ImGuiColorEditFlags_NoInputs)) {
                const elf3d::Result<void> material_result = active_scene.scene->set_material(
                    active_scene.cube_material.value(),
                    elf3d::MaterialDescription{
                        elf3d::Color4{state.cube_color[0], state.cube_color[1], state.cube_color[2],
                                      state.cube_color[3]}});
                if (!material_result) {
                    set_viewport_error(state, material_result.error());
                }
            }
        }

        const ImVec2 area_size = ImGui::GetContentRegionAvail();
        state.view_dimensions = content_extent_in_pixels(area_size);
        const bool has_view_area =
            area_size.x > 0.0F && area_size.y > 0.0F && has_nonzero_extent(state.view_dimensions);
        ImVec2 image_min = ImGui::GetCursorScreenPos();
        bool image_hovered = false;
        if (has_view_area) {
            constexpr ImGuiButtonFlags input_flags = ImGuiButtonFlags_MouseButtonLeft |
                                                     ImGuiButtonFlags_MouseButtonMiddle |
                                                     ImGuiButtonFlags_MouseButtonRight;
            ImGui::InvisibleButton("##Elf3DViewportInput", area_size, input_flags);
            image_min = ImGui::GetItemRectMin();
            image_hovered = !navigation_blocked_by_modal() && ImGui::IsItemHovered();
        } else {
            ImGui::Dummy(ImVec2{std::max(area_size.x, 0.0F), std::max(area_size.y, 0.0F)});
        }

        const elf3d::Result<void> resize_result =
            engine_viewport.resize(has_view_area ? state.view_dimensions : elf3d::Extent2D{});
        if (!resize_result) {
            set_viewport_error(state, resize_result.error());
        } else if (!has_view_area) {
            engine_viewport.cancel_interaction();
            state.framebuffer_valid = false;
            state.statistics = {};
        } else {
            if (active_scene.camera_needs_reset) {
                const elf3d::Result<void> reset_result =
                    engine_viewport.reset_view(*active_scene.scene, active_scene.camera);
                active_scene.camera_needs_reset = false;
                if (!reset_result) {
                    set_viewport_error(state, reset_result.error());
                }
            }

            const elf3d::NavigationSnapshot snapshot = engine_viewport.navigation_snapshot();
            if (image_hovered &&
                engine_viewport.active_tool() == elf3d::ViewportTool::distance_measurement &&
                !snapshot.is_pointer_captured) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            const bool input_focused =
                state.application_focused &&
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                !navigation_blocked_by_modal();
            const bool pointer_captured = state.application_focused &&
                                          !navigation_blocked_by_modal() &&
                                          snapshot.is_pointer_captured;
            const elf3d::ViewportInput input =
                viewport_input_from_imgui(image_min, image_hovered, input_focused, pointer_captured,
                                          area_size, state.view_dimensions);
            const elf3d::Result<void> navigation_result =
                engine_viewport.update_navigation(*active_scene.scene, active_scene.camera, input);
            if (!navigation_result) {
                set_viewport_error(state, navigation_result.error());
            }

            engine_viewport.set_clear_color(
                elf3d::Color4{state.clear_color[0], state.clear_color[1], state.clear_color[2],
                              state.clear_color[3]});
            engine_viewport.set_basic_lighting(state.lighting);
            const elf3d::Result<void> render_result =
                engine_viewport.render(*active_scene.scene, active_scene.camera);
            if (!render_result) {
                set_viewport_error(state, render_result.error());
            } else {
                state.statistics = engine_viewport.statistics();
                state.framebuffer_valid = engine_viewport.framebuffer_valid();
                if (state.framebuffer_valid) {
                    const elf3d::Result<elf3d::NativeTextureView> texture_result =
                        engine.native_texture_view(engine_viewport.color_texture());
                    if (!texture_result) {
                        set_viewport_error(state, texture_result.error());
                    } else {
                        const elf3d::Result<void> image_result = elf3d::imgui::draw_image(
                            texture_result.value(), elf3d::Float2{image_min.x, image_min.y},
                            elf3d::Float2{area_size.x, area_size.y});
                        if (!image_result) {
                            set_viewport_error(state, image_result.error());
                        } else {
                            draw_measurement_label(state, engine_viewport, active_scene, image_min,
                                                   area_size);
                        }
                    }
                }
            }
        }
        if (!state.viewport_error.empty()) {
            ImGui::TextWrapped("Viewport error: %s", state.viewport_error.c_str());
        }
    } else {
        state.view_dimensions = {};
        state.framebuffer_valid = false;
        state.statistics = {};
        engine_viewport.cancel_interaction();
        const elf3d::Result<void> resize_result = engine_viewport.resize({});
        if (!resize_result) {
            set_viewport_error(state, resize_result.error());
        }
    }
    ImGui::End();
}

void build_model_information(ImGuiID dockspace_id, ViewerState &state, const ViewerScene &scene) {
    if (!state.show_model_information) {
        return;
    }
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Model Information", &state.show_model_information)) {
        const std::string source =
            scene.is_imported() ? path_to_utf8(scene.source_path) : "Procedural cube demo";
        const std::string extension = scene.source_path.extension().string();
        ImGui::TextWrapped("Source: %s", source.c_str());
        ImGui::Text("Format: %s",
                    scene.is_imported() ? (extension == ".glb" ? "GLB" : "glTF") : "Procedural");
        ImGui::Separator();
        ImGui::Text("Entities: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.entities));
        ImGui::Text("Model entities: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.model_entities));
        ImGui::Text("Mesh assets: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.mesh_assets));
        ImGui::Text("Materials: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.material_assets));
        ImGui::Text("Images: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.image_assets));
        ImGui::Text("Textures: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.texture_assets));
        ImGui::Text("Sampler descriptions: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.sampler_descriptions));
        ImGui::Text("Decoded image memory: %llu bytes",
                    static_cast<unsigned long long>(scene.source_statistics.decoded_image_bytes));
        ImGui::Text("Base-color textured materials: %llu",
                    static_cast<unsigned long long>(
                        scene.source_statistics.materials_with_base_color_textures));
        ImGui::Text("Metallic-roughness textured materials: %llu",
                    static_cast<unsigned long long>(
                        scene.source_statistics.materials_with_metallic_roughness_textures));
        ImGui::Text("Primitives: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.primitives));
        ImGui::Text("Vertices: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.vertices));
        ImGui::Text("Indices: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.indices));
        ImGui::Text("Triangles: %llu",
                    static_cast<unsigned long long>(scene.source_statistics.triangles));
        if (scene.source_bounds.is_valid) {
            ImGui::Text("Bounds min: %.4g, %.4g, %.4g", scene.source_bounds.minimum.x,
                        scene.source_bounds.minimum.y, scene.source_bounds.minimum.z);
            ImGui::Text("Bounds max: %.4g, %.4g, %.4g", scene.source_bounds.maximum.x,
                        scene.source_bounds.maximum.y, scene.source_bounds.maximum.z);
        } else {
            ImGui::TextUnformatted("Bounds: empty");
        }
        ImGui::Separator();
        ImGui::Text("Latest draw calls: %llu",
                    static_cast<unsigned long long>(state.statistics.draw_calls));
        ImGui::Text("Latest rendered triangles: %llu",
                    static_cast<unsigned long long>(state.statistics.triangles));
        ImGui::Text("Latest texture bindings: %llu",
                    static_cast<unsigned long long>(state.statistics.texture_bindings));
        ImGui::Text("Latest texture uploads: %llu",
                    static_cast<unsigned long long>(state.statistics.gpu_texture_uploads));
        ImGui::Text("Current GPU textures: %llu",
                    static_cast<unsigned long long>(state.statistics.unique_gpu_textures));
        ImGui::Text("Overlay lines: %llu",
                    static_cast<unsigned long long>(state.statistics.overlay_lines));
        ImGui::Text("Overlay markers: %llu",
                    static_cast<unsigned long long>(state.statistics.overlay_markers));
        ImGui::Text("Clipping bounds tested: %llu",
                    static_cast<unsigned long long>(state.statistics.clipping_bounds_tested));
        ImGui::Text("Clipping bounds rejected: %llu",
                    static_cast<unsigned long long>(state.statistics.clipping_bounds_rejected));
        ImGui::Text("Clipping bounds intersecting: %llu",
                    static_cast<unsigned long long>(state.statistics.clipping_bounds_intersecting));
        ImGui::TextUnformatted("Image formats: PNG, JPEG");
        ImGui::TextWrapped("PBR: one directional light, no IBL, shadows, normal maps, emissive, "
                           "occlusion, alpha mask, or alpha blend. Materials are opaque.");

        if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::array<float, 3> direction{state.lighting.direction.x, state.lighting.direction.y,
                                           state.lighting.direction.z};
            if (ImGui::DragFloat3("Light direction", direction.data(), 0.01F, -1.0F, 1.0F)) {
                state.lighting.direction = {direction[0], direction[1], direction[2]};
            }
            ImGui::SliderFloat("Light intensity", &state.lighting.diffuse_intensity, 0.0F, 10.0F,
                               "%.2f");
            ImGui::SliderFloat("Ambient intensity", &state.lighting.ambient_intensity, 0.0F, 2.0F,
                               "%.2f");
            if (ImGui::Button("Reset Lighting")) {
                state.lighting = elf3d::BasicLighting{};
            }
        }
    }
    ImGui::End();
}

[[nodiscard]] float radians_to_degrees(float radians) noexcept {
    return radians * 57.2957795131F;
}

[[nodiscard]] const char *interaction_mode_name(elf3d::NavigationInteractionMode mode) noexcept {
    switch (mode) {
    case elf3d::NavigationInteractionMode::none:
        return "None";
    case elf3d::NavigationInteractionMode::orbit:
        return "Orbit";
    case elf3d::NavigationInteractionMode::pan:
        return "Pan";
    }
    return "None";
}

void build_navigation_settings_window(ViewerState &state, elf3d::Viewport &engine_viewport) {
    if (!state.show_navigation_settings) {
        return;
    }
    if (ImGui::Begin("Navigation Settings", &state.show_navigation_settings)) {
        bool enabled = engine_viewport.navigation_enabled();
        if (ImGui::Checkbox("Enable Navigation", &enabled)) {
            engine_viewport.set_navigation_enabled(enabled);
        }

        elf3d::OrbitNavigationSettings settings = engine_viewport.navigation_settings();
        bool settings_changed = false;
        settings_changed |= ImGui::DragFloat("Orbit sensitivity", &settings.orbit_sensitivity,
                                             0.0001F, 0.0F, 0.05F, "%.4f");
        settings_changed |= ImGui::DragFloat("Pan sensitivity", &settings.pan_sensitivity, 0.01F,
                                             0.0F, 10.0F, "%.2f");
        settings_changed |= ImGui::DragFloat("Zoom sensitivity", &settings.zoom_sensitivity, 0.005F,
                                             0.0F, 1.0F, "%.3f");
        settings_changed |=
            ImGui::Checkbox("Invert vertical orbit", &settings.invert_vertical_orbit);
        if (settings_changed) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_navigation_settings(settings);
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }
        if (ImGui::Button("Reset Navigation Settings")) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_navigation_settings(elf3d::OrbitNavigationSettings{});
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }

        ImGui::Separator();
        const elf3d::NavigationSnapshot snapshot = engine_viewport.navigation_snapshot();
        ImGui::Text("Pivot: %.4g, %.4g, %.4g", snapshot.pivot.x, snapshot.pivot.y,
                    snapshot.pivot.z);
        ImGui::Text("Distance: %.4g", snapshot.distance);
        ImGui::Text("Yaw: %.2f deg", radians_to_degrees(snapshot.yaw_radians));
        ImGui::Text("Pitch: %.2f deg", radians_to_degrees(snapshot.pitch_radians));
        ImGui::Text("Interaction: %s", interaction_mode_name(snapshot.interaction_mode));
    }
    ImGui::End();
}

[[nodiscard]] std::string entity_label(const ViewerScene &scene, elf3d::EntityId entity) {
    const elf3d::Result<std::string_view> name = scene.scene->entity_name(entity);
    if (name && !name.value().empty()) {
        return std::string{name.value()};
    }
    return std::string{"Entity "} + std::to_string(entity.debug_value());
}

[[nodiscard]] std::string selected_entity_label(const ViewerScene &scene,
                                                const elf3d::SelectionSnapshot &selection) {
    return selection.has_selection ? entity_label(scene, selection.entity) : "none";
}

void build_selection_panel(ImGuiID dockspace_id, ViewerState &state, const ViewerScene &scene,
                           elf3d::Viewport &engine_viewport) {
    if (!state.show_selection_panel) {
        return;
    }
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Selection", &state.show_selection_panel)) {
        bool enabled = engine_viewport.selection_enabled();
        if (ImGui::Checkbox("Enable Selection", &enabled)) {
            engine_viewport.set_selection_enabled(enabled);
        }

        elf3d::SelectionSettings settings = engine_viewport.selection_settings();
        bool settings_changed = false;
        settings_changed |= ImGui::DragFloat(
            "Click threshold", &settings.click_drag_threshold_pixels, 0.1F, 0.0F, 32.0F, "%.1f px");
        std::array<float, 4> highlight_color{
            settings.highlight_color.red, settings.highlight_color.green,
            settings.highlight_color.blue, settings.highlight_color.alpha};
        if (ImGui::ColorEdit4("Highlight color", highlight_color.data(),
                              ImGuiColorEditFlags_NoInputs)) {
            settings.highlight_color = {highlight_color[0], highlight_color[1], highlight_color[2],
                                        highlight_color[3]};
            settings_changed = true;
        }
        settings_changed |= ImGui::SliderFloat("Highlight strength", &settings.highlight_strength,
                                               0.0F, 1.0F, "%.2f");
        if (settings_changed) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_selection_settings(settings);
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }

        ImGui::Separator();
        const elf3d::SelectionSnapshot selection = engine_viewport.selection_snapshot();
        if (!selection.has_selection) {
            ImGui::TextUnformatted("Selected: none");
        } else {
            const std::string label = selected_entity_label(scene, selection);
            ImGui::Text("Selected: %s", label.c_str());
            ImGui::Text("Entity ID: %llu",
                        static_cast<unsigned long long>(selection.entity.debug_value()));
            if (selection.has_pick_hit) {
                const elf3d::PickHit &hit = selection.pick_hit;
                ImGui::Text("Mesh ID: %llu",
                            static_cast<unsigned long long>(hit.mesh.debug_value()));
                ImGui::Text("Primitive: %u", hit.primitive_index);
                ImGui::Text("Triangle: %u", hit.triangle_index);
                ImGui::Text("Hit position: %.4g, %.4g, %.4g", hit.world_position.x,
                            hit.world_position.y, hit.world_position.z);
                ImGui::Text("Hit normal: %.4g, %.4g, %.4g", hit.world_normal.x, hit.world_normal.y,
                            hit.world_normal.z);
                ImGui::Text("Barycentric: %.4g, %.4g, %.4g", hit.barycentric_coordinates.x,
                            hit.barycentric_coordinates.y, hit.barycentric_coordinates.z);
                ImGui::Text("Distance: %.4g", hit.world_distance);
            } else {
                ImGui::TextUnformatted("Pick hit: none");
            }
        }

        ImGui::Separator();
        const elf3d::PickingStatistics picking = engine_viewport.picking_statistics();
        ImGui::Text("Instance bounds tests: %llu",
                    static_cast<unsigned long long>(picking.latest_instance_bounds_tests));
        ImGui::Text("Mesh bounds tests: %llu",
                    static_cast<unsigned long long>(picking.latest_mesh_bounds_tests));
        ImGui::Text("BVH node tests: %llu",
                    static_cast<unsigned long long>(picking.latest_bvh_node_tests));
        ImGui::Text("Triangle tests: %llu",
                    static_cast<unsigned long long>(picking.latest_triangle_tests));
        ImGui::Text("Clipping bounds rejected: %llu",
                    static_cast<unsigned long long>(picking.latest_clipping_bounds_rejected));
        ImGui::Text("Clipping hits rejected: %llu",
                    static_cast<unsigned long long>(picking.latest_clipping_hits_rejected));
        ImGui::Text("Clipping hits accepted: %llu",
                    static_cast<unsigned long long>(picking.latest_clipping_hits_accepted));
        ImGui::Text("BVH builds this pick: %llu",
                    static_cast<unsigned long long>(picking.latest_bvh_builds));
        ImGui::Text("Lifetime BVH builds: %llu",
                    static_cast<unsigned long long>(picking.lifetime_bvh_builds));
        ImGui::Text("Cached mesh BVHs: %llu",
                    static_cast<unsigned long long>(picking.cached_mesh_bvhs));
    }
    ImGui::End();
}

void draw_measurement_point(const char *label, const ViewerScene &scene,
                            const std::optional<elf3d::MeasurementPoint> &point) {
    if (!point.has_value()) {
        ImGui::Text("%s: none", label);
        return;
    }
    const std::string entity = entity_label(scene, point->entity);
    ImGui::Text("%s: %s", label, entity.c_str());
    ImGui::Text("  Entity ID: %llu", static_cast<unsigned long long>(point->entity.debug_value()));
    ImGui::Text("  Mesh ID: %llu", static_cast<unsigned long long>(point->mesh.debug_value()));
    ImGui::Text("  Position: %.4g, %.4g, %.4g", point->world_position.x, point->world_position.y,
                point->world_position.z);
    ImGui::Text("  Normal: %.4g, %.4g, %.4g", point->world_normal.x, point->world_normal.y,
                point->world_normal.z);
}

void build_measurement_panel(ImGuiID dockspace_id, ViewerState &state, const ViewerScene &scene,
                             elf3d::Viewport &engine_viewport) {
    if (!state.show_measurement_panel) {
        return;
    }
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Measurement", &state.show_measurement_panel)) {
        const elf3d::ViewportTool active_tool = engine_viewport.active_tool();
        ImGui::Text("Active tool: %s", tool_name(active_tool));
        if (ImGui::RadioButton("Select", active_tool == elf3d::ViewportTool::selection)) {
            engine_viewport.set_active_tool(elf3d::ViewportTool::selection);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Measure Distance",
                               active_tool == elf3d::ViewportTool::distance_measurement)) {
            const elf3d::Result<void> result = engine_viewport.begin_distance_measurement();
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }

        elf3d::DistanceMeasurementSettings settings = engine_viewport.measurement_settings();
        bool settings_changed = false;
        const char *unit_names[] = {"Automatic metric", "Meters", "Centimeters",
                                    "Millimeters",      "Feet",   "Inches"};
        int unit_index = static_cast<int>(settings.display_unit);
        if (ImGui::Combo("Display unit", &unit_index, unit_names,
                         static_cast<int>(std::size(unit_names)))) {
            settings.display_unit = static_cast<elf3d::LengthDisplayUnit>(unit_index);
            settings_changed = true;
        }
        const char *depth_names[] = {"Depth tested", "Always visible"};
        int depth_index = static_cast<int>(settings.depth_mode);
        if (ImGui::Combo("Overlay depth", &depth_index, depth_names,
                         static_cast<int>(std::size(depth_names)))) {
            settings.depth_mode = static_cast<elf3d::OverlayDepthMode>(depth_index);
            settings_changed = true;
        }
        std::array<float, 4> line_color{settings.line_color.red, settings.line_color.green,
                                        settings.line_color.blue, settings.line_color.alpha};
        if (ImGui::ColorEdit4("Line color", line_color.data(), ImGuiColorEditFlags_NoInputs)) {
            settings.line_color = {line_color[0], line_color[1], line_color[2], line_color[3]};
            settings_changed = true;
        }
        std::array<float, 4> first_color{
            settings.first_point_color.red, settings.first_point_color.green,
            settings.first_point_color.blue, settings.first_point_color.alpha};
        if (ImGui::ColorEdit4("First marker color", first_color.data(),
                              ImGuiColorEditFlags_NoInputs)) {
            settings.first_point_color = {first_color[0], first_color[1], first_color[2],
                                          first_color[3]};
            settings_changed = true;
        }
        std::array<float, 4> second_color{
            settings.second_point_color.red, settings.second_point_color.green,
            settings.second_point_color.blue, settings.second_point_color.alpha};
        if (ImGui::ColorEdit4("Second marker color", second_color.data(),
                              ImGuiColorEditFlags_NoInputs)) {
            settings.second_point_color = {second_color[0], second_color[1], second_color[2],
                                           second_color[3]};
            settings_changed = true;
        }
        settings_changed |= ImGui::DragFloat("Line thickness", &settings.line_thickness_pixels,
                                             0.1F, 0.5F, 16.0F, "%.1f px");
        settings_changed |= ImGui::DragFloat("Marker radius", &settings.marker_radius_pixels, 0.1F,
                                             1.0F, 32.0F, "%.1f px");
        if (settings_changed) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_measurement_settings(settings);
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }

        ImGui::Separator();
        const elf3d::DistanceMeasurementSnapshot measurement =
            engine_viewport.distance_measurement_snapshot(*scene.scene);
        ImGui::Text("State: %s", measurement_state_name(measurement.state));
        if (measurement.state == elf3d::DistanceMeasurementState::empty ||
            measurement.state == elf3d::DistanceMeasurementState::awaiting_first_point) {
            ImGui::TextUnformatted("Click a visible surface to set the first point.");
        }
        draw_measurement_point("First point", scene, measurement.first_point);
        draw_measurement_point("Second point", scene, measurement.second_point);
        draw_measurement_point("Preview point", scene, measurement.preview_point);
        if (measurement.state == elf3d::DistanceMeasurementState::complete) {
            ImGui::Text("Distance: %.8g m", measurement.distance_meters);
            ImGui::Text(
                "Display: %s",
                format_distance(measurement.distance_meters, settings.display_unit).c_str());
        } else if (measurement.preview_point.has_value()) {
            ImGui::Text("Preview distance: %.8g m", measurement.preview_distance_meters);
            ImGui::Text("Display: %s",
                        format_distance(measurement.preview_distance_meters, settings.display_unit)
                            .c_str());
        }
        ImGui::Text("Anchors visible: %s", measurement.anchors_currently_visible ? "yes" : "no");
        if (measurement.diagnostic.has_value()) {
            ImGui::TextWrapped("Diagnostic: %s", measurement.diagnostic->message());
        }

        const bool incomplete =
            measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point;
        const bool has_measurement = measurement.first_point.has_value() ||
                                     measurement.second_point.has_value() ||
                                     measurement.preview_point.has_value();
        ImGui::BeginDisabled(!incomplete);
        if (ImGui::Button("Cancel Current")) {
            engine_viewport.cancel_distance_measurement();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!has_measurement);
        if (ImGui::Button("Clear Measurement")) {
            engine_viewport.clear_distance_measurement();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        const elf3d::MeasurementStatistics stats = engine_viewport.measurement_statistics();
        ImGui::Text("Committed points: %llu",
                    static_cast<unsigned long long>(stats.committed_points));
        ImGui::Text("Preview picks: %llu", static_cast<unsigned long long>(stats.preview_picks));
        ImGui::Text("Anchor resolutions: %llu",
                    static_cast<unsigned long long>(stats.anchor_resolutions));
        ImGui::Text("Overlay lines: %llu", static_cast<unsigned long long>(stats.overlay_lines));
        ImGui::Text("Overlay markers: %llu",
                    static_cast<unsigned long long>(stats.overlay_markers));
    }
    ImGui::End();
}

void build_clipping_panel(ImGuiID dockspace_id, ViewerState &state, const ViewerScene &scene,
                          elf3d::Viewport &engine_viewport) {
    if (!state.show_clipping_panel) {
        return;
    }
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Clipping", &state.show_clipping_panel)) {
        elf3d::ClippingSnapshot snapshot = engine_viewport.clipping_snapshot();
        const elf3d::Result<elf3d::Bounds3> visible_bounds =
            engine_viewport.visible_bounds(*scene.scene);
        ImGui::TextUnformatted(
            clipping_status(snapshot, visible_bounds && visible_bounds.value().is_valid).c_str());

        if (ImGui::CollapsingHeader("Section Plane", ImGuiTreeNodeFlags_DefaultOpen)) {
            elf3d::SectionPlane plane = snapshot.section_plane;
            bool changed = false;
            changed |= ImGui::Checkbox("Enabled##SectionPlane", &plane.enabled);
            std::array<float, 3> point{plane.point.x, plane.point.y, plane.point.z};
            if (ImGui::DragFloat3("Point", point.data(), 0.05F, -100000.0F, 100000.0F,
                                  "%.4g")) {
                plane.point = {point[0], point[1], point[2]};
                changed = true;
            }
            std::array<float, 3> normal{plane.normal.x, plane.normal.y, plane.normal.z};
            if (ImGui::DragFloat3("Normal", normal.data(), 0.02F, -1.0F, 1.0F, "%.4g")) {
                plane.normal = {normal[0], normal[1], normal[2]};
                changed = true;
            }
            int retained_side =
                plane.retained_half_space == elf3d::PlaneHalfSpace::positive ? 0 : 1;
            const char *side_names[] = {"Positive", "Negative"};
            if (ImGui::Combo("Retained side", &retained_side, side_names,
                             static_cast<int>(std::size(side_names)))) {
                plane.retained_half_space = retained_side == 0 ? elf3d::PlaneHalfSpace::positive
                                                               : elf3d::PlaneHalfSpace::negative;
                changed = true;
            }
            if (ImGui::SmallButton("X")) {
                plane.normal = {1.0F, 0.0F, 0.0F};
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Y")) {
                plane.normal = {0.0F, 1.0F, 0.0F};
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Z")) {
                plane.normal = {0.0F, 0.0F, 1.0F};
                changed = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!visible_bounds || !visible_bounds.value().is_valid);
            if (ImGui::SmallButton("Center") && visible_bounds) {
                plane.point = bounds_center(visible_bounds.value());
                changed = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear")) {
                engine_viewport.clear_section_plane();
                changed = false;
            }
            if (changed) {
                const elf3d::Result<void> result = engine_viewport.set_section_plane(plane);
                if (!result) {
                    set_viewport_error(state, result.error());
                }
            }
        }

        if (ImGui::CollapsingHeader("Clipping Boxes", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (std::uint32_t index = 0; index < snapshot.box_count; ++index) {
                ImGui::PushID(static_cast<int>(index));
                elf3d::ClippingBox box = snapshot.boxes[index];
                bool changed = false;
                ImGui::Separator();
                ImGui::Text("Box %u", index + 1U);
                changed |= ImGui::Checkbox("Enabled", &box.enabled);
                std::array<float, 3> minimum{box.minimum.x, box.minimum.y, box.minimum.z};
                std::array<float, 3> maximum{box.maximum.x, box.maximum.y, box.maximum.z};
                if (ImGui::DragFloat3("Minimum", minimum.data(), 0.05F, -100000.0F, 100000.0F,
                                      "%.4g")) {
                    box.minimum = {minimum[0], minimum[1], minimum[2]};
                    changed = true;
                }
                if (ImGui::DragFloat3("Maximum", maximum.data(), 0.05F, -100000.0F, 100000.0F,
                                      "%.4g")) {
                    box.maximum = {maximum[0], maximum[1], maximum[2]};
                    changed = true;
                }
                if (!valid_box_for_commit(box)) {
                    ImGui::TextColored(ImVec4{1.0F, 0.45F, 0.25F, 1.0F},
                                       "Box extents must be positive on all axes.");
                } else if (changed) {
                    const elf3d::Result<void> result = engine_viewport.set_clipping_box(index, box);
                    if (!result) {
                        set_viewport_error(state, result.error());
                    }
                }
                ImGui::BeginDisabled(!visible_bounds || !visible_bounds.value().is_valid);
                if (ImGui::SmallButton("Reset to Visible Bounds")) {
                    const elf3d::Result<void> result =
                        engine_viewport.reset_clipping_box_to_visible_bounds(*scene.scene, index);
                    if (!result) {
                        set_viewport_error(state, result.error());
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    const elf3d::Result<void> result = engine_viewport.remove_clipping_box(index);
                    if (!result) {
                        set_viewport_error(state, result.error());
                    }
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }

            ImGui::Separator();
            ImGui::BeginDisabled(snapshot.box_count >= elf3d::maximum_clipping_boxes ||
                                 !visible_bounds || !visible_bounds.value().is_valid);
            if (ImGui::Button("Add Box from Visible Bounds")) {
                const elf3d::Result<std::uint32_t> result =
                    engine_viewport.add_clipping_box_from_visible_bounds(*scene.scene);
                if (!result) {
                    set_viewport_error(state, result.error());
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshot.box_count == 0);
            if (ImGui::Button("Clear Boxes")) {
                engine_viewport.clear_clipping_boxes();
            }
            ImGui::EndDisabled();
        }

        if (ImGui::CollapsingHeader("Helpers", ImGuiTreeNodeFlags_DefaultOpen)) {
            elf3d::ClippingHelperSettings helpers = snapshot.helpers;
            bool helper_changed = false;
            helper_changed |= ImGui::Checkbox("Show helpers", &helpers.visible);
            std::array<float, 4> plane_color{
                helpers.section_plane_color.red, helpers.section_plane_color.green,
                helpers.section_plane_color.blue, helpers.section_plane_color.alpha};
            if (ImGui::ColorEdit4("Plane color", plane_color.data(), ImGuiColorEditFlags_NoInputs)) {
                helpers.section_plane_color =
                    {plane_color[0], plane_color[1], plane_color[2], plane_color[3]};
                helper_changed = true;
            }
            std::array<float, 4> box_color{helpers.box_color.red, helpers.box_color.green,
                                           helpers.box_color.blue, helpers.box_color.alpha};
            if (ImGui::ColorEdit4("Box color", box_color.data(), ImGuiColorEditFlags_NoInputs)) {
                helpers.box_color = {box_color[0], box_color[1], box_color[2], box_color[3]};
                helper_changed = true;
            }
            helper_changed |= ImGui::DragFloat("Line thickness", &helpers.line_thickness_pixels,
                                               0.1F, 0.5F, 16.0F, "%.1f px");
            if (helper_changed) {
                const elf3d::Result<void> result =
                    engine_viewport.set_clipping_helper_settings(helpers);
                if (!result) {
                    set_viewport_error(state, result.error());
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Fit to Clipped Content")) {
            const elf3d::Result<void> result =
                engine_viewport.fit_to_scene(*scene.scene, scene.camera);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Clipping")) {
            engine_viewport.clear_clipping();
        }
    }
    ImGui::End();
}

void invalidate_hierarchy_snapshot(ViewerScene &scene) noexcept {
    scene.hierarchy_snapshot_valid = false;
}

[[nodiscard]] bool refresh_hierarchy_snapshot(ViewerState &state, ViewerScene &scene) {
    if (scene.hierarchy_snapshot_valid &&
        scene.hierarchy_snapshot.hierarchy_revision() == scene.scene->hierarchy_revision() &&
        scene.hierarchy_snapshot.visibility_revision() == scene.scene->visibility_revision()) {
        return true;
    }

    elf3d::Result<elf3d::SceneHierarchySnapshot> snapshot = scene.scene->hierarchy_snapshot();
    if (!snapshot) {
        set_viewport_error(state, snapshot.error());
        return false;
    }
    scene.hierarchy_snapshot = std::move(snapshot).value();
    scene.hierarchy_snapshot_valid = true;
    return true;
}

[[nodiscard]] std::vector<bool>
selected_hierarchy_ancestors(const std::vector<elf3d::SceneHierarchyItem> &items,
                             std::optional<elf3d::EntityId> selected,
                             std::optional<std::size_t> &selected_index) {
    std::vector<bool> ancestors(items.size(), false);
    selected_index.reset();
    if (!selected.has_value()) {
        return ancestors;
    }

    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].entity == selected.value()) {
            selected_index = index;
            break;
        }
    }
    if (!selected_index.has_value()) {
        return ancestors;
    }

    std::uint32_t wanted_depth = items[selected_index.value()].depth;
    for (std::size_t index = selected_index.value(); index > 0 && wanted_depth > 0;) {
        --index;
        if (items[index].depth + 1U == wanted_depth) {
            ancestors[index] = true;
            --wanted_depth;
        }
    }
    return ancestors;
}

void apply_hierarchy_error(ViewerState &state, const elf3d::Result<void> &result) {
    if (!result) {
        set_viewport_error(state, result.error());
    }
}

void build_hierarchy_row_context(ViewerState &state, ViewerScene &scene,
                                 elf3d::Viewport &engine_viewport,
                                 const elf3d::SceneHierarchyItem &item) {
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }
    if (ImGui::MenuItem("Select")) {
        apply_hierarchy_error(state,
                              engine_viewport.set_selected_entity(*scene.scene, item.entity));
    }
    if (item.local_visible && ImGui::MenuItem("Hide")) {
        const elf3d::Result<void> result = scene.scene->set_entity_visible(item.entity, false);
        if (!result) {
            set_viewport_error(state, result.error());
        }
        invalidate_hierarchy_snapshot(scene);
    }
    if ((!item.local_visible || !item.effective_visible) && ImGui::MenuItem("Show")) {
        const elf3d::Result<void> result = scene.scene->show_entity_and_ancestors(item.entity);
        if (!result) {
            set_viewport_error(state, result.error());
        }
        invalidate_hierarchy_snapshot(scene);
    }
    if (ImGui::MenuItem("Isolate")) {
        apply_hierarchy_error(state, engine_viewport.isolate_entity(*scene.scene, item.entity));
    }
    if (engine_viewport.is_isolating() && ImGui::MenuItem("Exit Isolation")) {
        engine_viewport.clear_isolation();
    }
    ImGui::EndPopup();
}

void build_scene_hierarchy_panel(ImGuiID dockspace_id, ViewerState &state, ViewerScene &scene,
                                 elf3d::Viewport &engine_viewport) {
    if (!state.show_scene_hierarchy) {
        return;
    }
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene Hierarchy", &state.show_scene_hierarchy)) {
        ImGui::End();
        return;
    }
    if (!refresh_hierarchy_snapshot(state, scene)) {
        ImGui::TextWrapped("Hierarchy unavailable: %s", state.viewport_error.c_str());
        ImGui::End();
        return;
    }

    const elf3d::SceneHierarchyStatistics hierarchy = scene.scene->hierarchy_statistics();
    ImGui::Text("Entities: %llu  Roots: %llu  Hidden: %llu / %llu",
                static_cast<unsigned long long>(hierarchy.entities),
                static_cast<unsigned long long>(hierarchy.root_entities),
                static_cast<unsigned long long>(hierarchy.effectively_hidden_entities),
                static_cast<unsigned long long>(hierarchy.entities));

    const std::optional<elf3d::EntityId> isolated = engine_viewport.isolated_entity();
    if (isolated.has_value()) {
        const std::string label = entity_label(scene, isolated.value());
        ImGui::Text("Isolation: %s", label.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Exit Isolation")) {
            engine_viewport.clear_isolation();
        }
    } else {
        ImGui::TextUnformatted("Isolation: none");
    }

    const bool has_selection = engine_viewport.has_selection();
    ImGui::BeginDisabled(!has_selection);
    if (ImGui::SmallButton("Hide Selected")) {
        apply_hierarchy_error(state, engine_viewport.hide_selected(*scene.scene));
        invalidate_hierarchy_snapshot(scene);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Show Selected")) {
        apply_hierarchy_error(state, engine_viewport.show_selected(*scene.scene));
        invalidate_hierarchy_snapshot(scene);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Isolate Selected")) {
        apply_hierarchy_error(state, engine_viewport.isolate_selected(*scene.scene));
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton("Show All")) {
        const elf3d::Result<void> result = scene.scene->show_all_entities();
        if (!result) {
            set_viewport_error(state, result.error());
        }
        invalidate_hierarchy_snapshot(scene);
    }

    std::vector<elf3d::SceneHierarchyItem> items;
    std::vector<std::string> names;
    items.reserve(scene.hierarchy_snapshot.size());
    names.reserve(scene.hierarchy_snapshot.size());
    for (std::size_t index = 0; index < scene.hierarchy_snapshot.size(); ++index) {
        const elf3d::Result<elf3d::SceneHierarchyItem> item = scene.hierarchy_snapshot.item(index);
        const elf3d::Result<std::string_view> name = scene.hierarchy_snapshot.name(index);
        if (!item || !name) {
            continue;
        }
        items.push_back(item.value());
        names.emplace_back(name.value());
    }

    std::optional<std::size_t> selected_index;
    const std::vector<bool> ancestors =
        selected_hierarchy_ancestors(items, engine_viewport.selected_entity(), selected_index);
    if (!engine_viewport.selected_entity().has_value()) {
        state.last_revealed_hierarchy_selection.reset();
    }
    const bool should_reveal =
        engine_viewport.selected_entity().has_value() &&
        state.last_revealed_hierarchy_selection != engine_viewport.selected_entity();

    ImGui::Separator();
    int open_depth = 0;
    for (std::size_t index = 0; index < items.size();) {
        const elf3d::SceneHierarchyItem &item = items[index];
        while (open_depth > static_cast<int>(item.depth)) {
            ImGui::TreePop();
            --open_depth;
        }

        std::string label = names[index].empty() ? entity_label(scene, item.entity) : names[index];
        if (item.has_camera) {
            label += " [camera]";
        } else if (item.renderable) {
            label += " [model]";
        }

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (item.child_count == 0) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (engine_viewport.selected_entity() == item.entity) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (ancestors[index]) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        const std::string id = std::to_string(item.entity.debug_value());
        ImGui::PushID(id.c_str());
        const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            apply_hierarchy_error(state,
                                  engine_viewport.set_selected_entity(*scene.scene, item.entity));
        }
        if (should_reveal && selected_index.has_value() && selected_index.value() == index) {
            ImGui::SetScrollHereY(0.5F);
            state.last_revealed_hierarchy_selection = item.entity;
        }
        build_hierarchy_row_context(state, scene, engine_viewport, item);

        ImGui::SameLine();
        if (ImGui::SmallButton(item.local_visible ? "Hide##visible" : "Show##visible")) {
            const elf3d::Result<void> result =
                scene.scene->set_entity_visible(item.entity, !item.local_visible);
            if (!result) {
                set_viewport_error(state, result.error());
            }
            invalidate_hierarchy_snapshot(scene);
        }
        if (!item.local_visible) {
            ImGui::SameLine();
            ImGui::TextDisabled("local hidden");
        } else if (!item.effective_visible) {
            ImGui::SameLine();
            ImGui::TextDisabled("inherited hidden");
        }
        if (isolated.has_value() && isolated.value() == item.entity) {
            ImGui::SameLine();
            ImGui::TextUnformatted("isolated");
        }
        ImGui::PopID();

        if (item.child_count != 0 && open) {
            ++open_depth;
            ++index;
            continue;
        }
        if (item.child_count != 0 && !open) {
            const std::uint32_t closed_depth = item.depth;
            ++index;
            while (index < items.size() && items[index].depth > closed_depth) {
                ++index;
            }
            continue;
        }
        ++index;
    }
    while (open_depth > 0) {
        ImGui::TreePop();
        --open_depth;
    }
    ImGui::End();
}

[[nodiscard]] const char *error_category(elf3d::ErrorCode code) noexcept {
    switch (code) {
    case elf3d::ErrorCode::source_file_not_found:
    case elf3d::ErrorCode::source_file_read_failed:
    case elf3d::ErrorCode::missing_external_buffer:
    case elf3d::ErrorCode::missing_external_image:
    case elf3d::ErrorCode::external_image_read_failed:
        return "File I/O";
    case elf3d::ErrorCode::unsupported_scene_format:
    case elf3d::ErrorCode::unsupported_required_extension:
    case elf3d::ErrorCode::unsupported_remote_uri:
    case elf3d::ErrorCode::unsupported_primitive_mode:
    case elf3d::ErrorCode::unsupported_index_type:
    case elf3d::ErrorCode::unsupported_image_mime_type:
    case elf3d::ErrorCode::unsupported_image_extension:
        return "Unsupported glTF feature";
    case elf3d::ErrorCode::malformed_gltf:
    case elf3d::ErrorCode::malformed_glb:
    case elf3d::ErrorCode::gltf_validation_failed:
    case elf3d::ErrorCode::invalid_buffer_range:
    case elf3d::ErrorCode::invalid_buffer_view:
    case elf3d::ErrorCode::invalid_accessor:
    case elf3d::ErrorCode::imported_index_out_of_range:
    case elf3d::ErrorCode::invalid_texcoord:
    case elf3d::ErrorCode::mismatched_texcoord_count:
    case elf3d::ErrorCode::malformed_data_uri:
    case elf3d::ErrorCode::invalid_base64_payload:
    case elf3d::ErrorCode::invalid_image_buffer_view:
    case elf3d::ErrorCode::image_range_out_of_bounds:
    case elf3d::ErrorCode::image_decode_failed:
        return "Invalid glTF data";
    case elf3d::ErrorCode::resource_limit_exceeded:
    case elf3d::ErrorCode::size_overflow:
    case elf3d::ErrorCode::zero_image_dimensions:
    case elf3d::ErrorCode::excessive_image_dimensions:
    case elf3d::ErrorCode::decoded_image_size_overflow:
    case elf3d::ErrorCode::image_resource_limit_exceeded:
        return "Resource limit";
    default:
        return "Scene import";
    }
}

[[nodiscard]] std::optional<std::string> build_open_modal(ViewerState &state) {
    if (state.request_open_modal) {
        ImGui::OpenPopup("Open glTF Model");
        state.request_open_modal = false;
    }
    std::optional<std::string> requested_path;
    if (ImGui::BeginPopupModal("Open glTF Model", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Enter a UTF-8 path to a .gltf or .glb file.");
        ImGui::SetNextItemWidth(620.0F);
        ImGui::InputText("##ModelPath", state.open_path.data(), state.open_path.size());
        if (ImGui::Button("Load") && state.open_path[0] != '\0') {
            requested_path = std::string{state.open_path.data()};
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return requested_path;
}

void build_error_modal(ViewerState &state) {
    if (state.request_error_modal) {
        ImGui::OpenPopup("Model Load Error");
        state.request_error_modal = false;
    }
    if (ImGui::BeginPopupModal("Model Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (state.load_failure.has_value()) {
            ImGui::TextWrapped("Source: %s", state.load_failure->source_path.c_str());
            ImGui::Text("Category: %s", error_category(state.load_failure->error.code()));
            ImGui::Separator();
            ImGui::TextWrapped("%s", state.load_failure->error.message());
        }
        if (ImGui::Button("Close")) {
            state.load_failure.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

const char *graphics_backend_name(elf3d::GraphicsBackend backend) noexcept {
    return backend == elf3d::GraphicsBackend::opengl ? "OpenGL 4.1 core" : "Unknown";
}

void build_status_bar(const ViewerState &state, const elf3d::Engine &engine,
                      const ViewerScene &scene, const elf3d::Viewport &engine_viewport) {
    if (!state.show_status_bar) {
        return;
    }
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const float status_height = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(
        ImVec2{viewport->Pos.x, viewport->Pos.y + viewport->Size.y - status_height});
    ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, status_height});
    ImGui::SetNextWindowViewport(viewport->ID);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8.0F, 3.0F});
    if (ImGui::Begin("##Elf3DStatusBar", nullptr, flags)) {
        const float fps = ImGui::GetIO().Framerate;
        const float frame_time_ms = fps > 0.0F ? 1000.0F / fps : 0.0F;
        const elf3d::SelectionSnapshot selection = engine_viewport.selection_snapshot();
        const std::string selection_status = selected_entity_label(scene, selection);
        std::string isolation_status = "none";
        const std::optional<elf3d::EntityId> isolated = engine_viewport.isolated_entity();
        if (isolated.has_value()) {
            isolation_status = entity_label(scene, isolated.value());
        }
        const elf3d::DistanceMeasurementSnapshot measurement =
            engine_viewport.distance_measurement_snapshot(*scene.scene);
        const elf3d::Result<elf3d::Bounds3> visible_bounds =
            engine_viewport.visible_bounds(*scene.scene);
        const std::string clipping =
            clipping_status(engine_viewport.clipping_snapshot(),
                            visible_bounds && visible_bounds.value().is_valid);
        std::string tool_status = std::string{"Tool: "} + tool_name(engine_viewport.active_tool());
        if (engine_viewport.active_tool() == elf3d::ViewportTool::distance_measurement) {
            tool_status += " | ";
            tool_status += measurement_state_name(measurement.state);
            if (measurement.preview_point.has_value()) {
                tool_status += " | ";
                tool_status += format_distance(measurement.preview_distance_meters,
                                               engine_viewport.measurement_settings().display_unit);
            }
        } else if (measurement.state == elf3d::DistanceMeasurementState::complete) {
            tool_status += " | Measurement: ";
            tool_status += format_distance(measurement.distance_meters,
                                           engine_viewport.measurement_settings().display_unit);
        }
        ImGui::Text(
            "Elf3D %s  |  %s  |  Viewport %u x %u  |  FBO %s  |  Draws %llu  |  "
            "Triangles %llu  |  Selected: %s  |  Isolation: %s  |  %s  |  %s  |  %.2f ms  |  %.1f FPS",
            elf3d::version_string(), graphics_backend_name(engine.graphics_backend()),
            state.view_dimensions.width, state.view_dimensions.height,
            state.framebuffer_valid ? "valid" : "inactive",
            static_cast<unsigned long long>(state.statistics.draw_calls),
            static_cast<unsigned long long>(state.statistics.triangles), selection_status.c_str(),
            isolation_status.c_str(), clipping.c_str(), tool_status.c_str(), frame_time_ms, fps);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void build_about_window(ViewerState &state) {
    if (!state.show_about) {
        return;
    }
    if (ImGui::Begin("About Elf3D", &state.show_about, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Elf3D");
        ImGui::Separator();
        ImGui::Text("Elf3D version: %s", elf3d::version_string());
        ImGui::Text("Dear ImGui version: %s", ImGui::GetVersion());
        ImGui::Text("Dear ImGui branch: %s", ELF3D_IMGUI_BRANCH);
        ImGui::Text("Dear ImGui commit: %s", ELF3D_IMGUI_COMMIT_SHA);
        ImGui::TextUnformatted("Graphics backend: OpenGL 4.1 core / Dear ImGui OpenGL3");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "elf3d_viewer is the reference and demonstration application for the Elf3D engine.");
    }
    ImGui::End();
}

void report_load_failure(ViewerState &state, const std::string &path, const elf3d::Error &error) {
    state.load_failure = LoadFailure{path, error};
    state.request_error_modal = true;
    std::cerr << "Failed to load '" << path << "' [" << error_category(error.code())
              << "]: " << error.message() << '\n';
}

void attempt_model_load(elf3d::Engine &engine, elf3d::Viewport &engine_viewport, ViewerState &state,
                        ViewerScene &active_scene, const std::string &source_path) {
    try {
        const std::filesystem::path path = path_from_utf8(source_path);
        elf3d::Result<ViewerScene> result = load_model_scene(engine, path);
        if (!result) {
            report_load_failure(state, source_path, result.error());
            return;
        }
        engine_viewport.cancel_interaction();
        engine_viewport.clear_selection();
        engine_viewport.clear_isolation();
        engine_viewport.clear_distance_measurement();
        engine_viewport.clear_clipping();
        active_scene = std::move(result).value();
        state.rotation_angle = 0.0F;
        state.statistics = {};
        state.last_revealed_hierarchy_selection.reset();
    } catch (...) {
        report_load_failure(state, source_path,
                            elf3d::Error{elf3d::ErrorCode::unexpected_exception,
                                         "The viewer could not convert the UTF-8 source path"});
    }
}

[[nodiscard]] bool camera_shortcuts_available(const ViewerState &state, const ViewerScene &scene,
                                              const elf3d::Viewport &engine_viewport) noexcept {
    const elf3d::Result<elf3d::Bounds3> visible_bounds =
        engine_viewport.visible_bounds(*scene.scene);
    return state.show_3d_view && visible_bounds && visible_bounds.value().is_valid &&
           has_nonzero_extent(state.view_dimensions) && !ImGui::GetIO().WantTextInput &&
           !navigation_blocked_by_modal();
}

int run_viewer(int argument_count, char **arguments) {
    glfwSetErrorCallback(glfw_error_callback);
    GlfwRuntime glfw;
    if (!glfw.initialize()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    Window window{glfwCreateWindow(1440, 900, "Elf3D Viewer", nullptr, nullptr)};
    if (!window) {
        std::cerr << "Failed to create the Elf3D GLFW window with an OpenGL 4.1 core context\n";
        return 1;
    }
    glfwMakeContextCurrent(window.get());
    if (glfwGetCurrentContext() != window.get() || glGetString(GL_VERSION) == nullptr) {
        std::cerr << "Failed to initialize the OpenGL context\n";
        return 1;
    }
    glfwSwapInterval(1);

    elf3d::EngineConfiguration engine_configuration;
    engine_configuration.opengl.load_procedure = load_opengl_procedure;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(engine_configuration);
    if (!engine_result) {
        std::cerr << "Failed to create the Elf3D engine: " << engine_result.error().message()
                  << '\n';
        return 1;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();

    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result = engine->create_viewport({});
    if (!viewport_result) {
        std::cerr << "Failed to create the Elf3D viewport: " << viewport_result.error().message()
                  << '\n';
        return 1;
    }
    std::unique_ptr<elf3d::Viewport> engine_viewport = std::move(viewport_result).value();

    elf3d::Result<ViewerScene> demo_result = create_demo_scene(*engine);
    if (!demo_result) {
        std::cerr << "Failed to create the Elf3D demonstration scene: "
                  << demo_result.error().message() << '\n';
        return 1;
    }
    ViewerScene active_scene = std::move(demo_result).value();

    std::string imgui_error;
    std::unique_ptr<elf3d::imgui::Context> imgui =
        elf3d::imgui::Context::create(window.get(), glsl_version, imgui_error);
    if (!imgui) {
        std::cerr << imgui_error << '\n';
        return 1;
    }

    ViewerState state;
    glfwSetWindowUserPointer(window.get(), &state);
    glfwSetDropCallback(window.get(), glfw_drop_callback);
    if (argument_count >= 2 && arguments[1] != nullptr) {
        attempt_model_load(*engine, *engine_viewport, state, active_scene, arguments[1]);
    }

    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
        glfwPollEvents();
        state.application_focused = glfwGetWindowAttrib(window.get(), GLFW_FOCUSED) == GLFW_TRUE;
        if (!state.application_focused) {
            engine_viewport->cancel_interaction();
        }
        imgui->begin_frame();

        ViewerCommands commands;
        build_main_menu(window.get(), state, active_scene, *engine_viewport, commands);
        const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();

        if (camera_shortcuts_available(state, active_scene, *engine_viewport)) {
            if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                commands.fit_to_scene = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                commands.reset_view = true;
            }
        }
        const bool viewport_shortcuts_available =
            state.show_3d_view && has_nonzero_extent(state.view_dimensions) &&
            !ImGui::GetIO().WantTextInput && !navigation_blocked_by_modal();
        if (viewport_shortcuts_available) {
            if (ImGui::IsKeyPressed(ImGuiKey_S)) {
                commands.select_tool = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_M)) {
                commands.measure_tool = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                commands.clear_measurement = true;
            }
        }
        if (state.show_3d_view && !ImGui::GetIO().WantTextInput && !navigation_blocked_by_modal() &&
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            const elf3d::DistanceMeasurementSnapshot measurement =
                engine_viewport->distance_measurement_snapshot(*active_scene.scene);
            if (measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point) {
                commands.cancel_measurement = true;
            } else {
                commands.clear_selection = true;
            }
        }

        if (commands.select_tool) {
            engine_viewport->set_active_tool(elf3d::ViewportTool::selection);
        }
        if (commands.measure_tool) {
            const elf3d::Result<void> result = engine_viewport->begin_distance_measurement();
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.show_clipping_panel) {
            state.show_clipping_panel = true;
        }
        if (commands.enable_section_plane) {
            elf3d::SectionPlane plane = engine_viewport->clipping_snapshot().section_plane;
            plane.enabled = !plane.enabled;
            if (plane.enabled) {
                const elf3d::Result<elf3d::Bounds3> bounds =
                    engine_viewport->visible_bounds(*active_scene.scene);
                if (bounds) {
                    plane.point = bounds_center(bounds.value());
                }
            }
            const elf3d::Result<void> result = engine_viewport->set_section_plane(plane);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.flip_section_side) {
            elf3d::SectionPlane plane = engine_viewport->clipping_snapshot().section_plane;
            plane.retained_half_space =
                plane.retained_half_space == elf3d::PlaneHalfSpace::positive
                    ? elf3d::PlaneHalfSpace::negative
                    : elf3d::PlaneHalfSpace::positive;
            const elf3d::Result<void> result = engine_viewport->set_section_plane(plane);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.add_clipping_box_from_bounds) {
            const elf3d::Result<std::uint32_t> result =
                engine_viewport->add_clipping_box_from_visible_bounds(*active_scene.scene);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.clear_clipping) {
            engine_viewport->clear_clipping();
        }
        if (commands.toggle_clipping_helpers) {
            const elf3d::ClippingSnapshot clipping = engine_viewport->clipping_snapshot();
            const elf3d::Result<void> result =
                engine_viewport->set_clipping_helpers_visible(!clipping.helpers.visible);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.fit_to_clipped_content) {
            const elf3d::Result<void> result =
                engine_viewport->fit_to_scene(*active_scene.scene, active_scene.camera);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.cancel_measurement) {
            engine_viewport->cancel_distance_measurement();
        }
        if (commands.clear_measurement) {
            engine_viewport->clear_distance_measurement();
        }

        if (commands.reload && active_scene.is_imported()) {
            attempt_model_load(*engine, *engine_viewport, state, active_scene,
                               path_to_utf8(active_scene.source_path));
        }
        if (commands.close_scene) {
            elf3d::Result<ViewerScene> replacement = create_demo_scene(*engine);
            if (replacement) {
                engine_viewport->cancel_interaction();
                engine_viewport->clear_selection();
                engine_viewport->clear_isolation();
                engine_viewport->clear_distance_measurement();
                engine_viewport->clear_clipping();
                active_scene = std::move(replacement).value();
                state.statistics = {};
                state.last_revealed_hierarchy_selection.reset();
            } else {
                report_load_failure(state, "Procedural cube demo", replacement.error());
            }
        }
        if (commands.fit_to_scene) {
            const elf3d::Result<void> result =
                engine_viewport->fit_to_scene(*active_scene.scene, active_scene.camera);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.reset_view) {
            const elf3d::Result<void> result =
                engine_viewport->reset_view(*active_scene.scene, active_scene.camera);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.clear_selection) {
            engine_viewport->clear_selection();
        }
        if (commands.hide_selected) {
            const elf3d::Result<void> result = engine_viewport->hide_selected(*active_scene.scene);
            if (!result) {
                set_viewport_error(state, result.error());
            }
            invalidate_hierarchy_snapshot(active_scene);
        }
        if (commands.show_selected) {
            const elf3d::Result<void> result = engine_viewport->show_selected(*active_scene.scene);
            if (!result) {
                set_viewport_error(state, result.error());
            }
            invalidate_hierarchy_snapshot(active_scene);
        }
        if (commands.show_all) {
            const elf3d::Result<void> result = active_scene.scene->show_all_entities();
            if (!result) {
                set_viewport_error(state, result.error());
            }
            invalidate_hierarchy_snapshot(active_scene);
        }
        if (commands.isolate_selected) {
            const elf3d::Result<void> result =
                engine_viewport->isolate_selected(*active_scene.scene);
            if (!result) {
                set_viewport_error(state, result.error());
            }
        }
        if (commands.exit_isolation) {
            engine_viewport->clear_isolation();
        }
        if (state.dropped_path.has_value()) {
            std::string path = std::move(state.dropped_path).value();
            state.dropped_path.reset();
            attempt_model_load(*engine, *engine_viewport, state, active_scene, path);
        }
        if (state.drop_copy_failed) {
            state.drop_copy_failed = false;
            report_load_failure(state, "Dropped file",
                                elf3d::Error{elf3d::ErrorCode::unexpected_exception,
                                             "The viewer could not copy the dropped UTF-8 path"});
        }
        const std::optional<std::string> modal_path = build_open_modal(state);
        if (modal_path.has_value()) {
            attempt_model_load(*engine, *engine_viewport, state, active_scene, modal_path.value());
        }

        build_3d_view(dockspace_id, state, *engine, *engine_viewport, active_scene);
        build_scene_hierarchy_panel(dockspace_id, state, active_scene, *engine_viewport);
        build_model_information(dockspace_id, state, active_scene);
        build_navigation_settings_window(state, *engine_viewport);
        build_selection_panel(dockspace_id, state, active_scene, *engine_viewport);
        build_measurement_panel(dockspace_id, state, active_scene, *engine_viewport);
        build_clipping_panel(dockspace_id, state, active_scene, *engine_viewport);
        build_status_bar(state, *engine, active_scene, *engine_viewport);
        build_about_window(state);
        build_error_modal(state);
        if (state.show_imgui_demo) {
            ImGui::ShowDemoWindow(&state.show_imgui_demo);
        }

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window.get(), &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.035F, 0.04F, 0.05F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        imgui->render();
        glfwSwapBuffers(window.get());
    }
    return 0;
}

} // namespace

int main(int argument_count, char **arguments) {
    try {
        return run_viewer(argument_count, arguments);
    } catch (const std::exception &exception) {
        std::cerr << "Elf3D viewer terminated after an unexpected exception: " << exception.what()
                  << '\n';
    } catch (...) {
        std::cerr << "Elf3D viewer terminated after an unknown exception\n";
    }
    return 1;
}
