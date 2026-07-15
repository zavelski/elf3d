#pragma once

#include <elf3d/elf3d.h>
#include <elf3d/imgui/context.h>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::viewer {

inline constexpr char glsl_version[] = "#version 410 core";
inline constexpr float viewer_ui_font_size_pixels = 20.0F;
inline constexpr float panel_content_font_size_pixels = viewer_ui_font_size_pixels * 0.70F;
inline constexpr float panel_title_font_size_pixels = viewer_ui_font_size_pixels * 0.875F;
inline constexpr float side_dock_width_fraction = 0.27968F;

[[noreturn]] void fatal_viewer_allocation_failure() noexcept;
[[noreturn]] void fatal_unexpected_viewer_exception() noexcept;

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
    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;

  private:
    bool initialized_ = false;
};

struct WindowDeleter {
    void operator()(GLFWwindow* window) const noexcept {
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
    }
};

using Window = std::unique_ptr<GLFWwindow, WindowDeleter>;

struct LoadFailure {
    std::string source_path;
    Error error;
};

struct OpenBrowserEntry {
    std::filesystem::path path;
    std::string label;
    std::string modified_time;
    std::uintmax_t size_bytes = 0;
    bool directory = false;
    bool has_size = false;
};

struct ExternalEditorPaths {
    std::optional<std::filesystem::path> emeditor;
    std::optional<std::filesystem::path> notepad;
    std::optional<std::filesystem::path> notepad_plus_plus;
};

enum class FileDialogAction {
    open,
    save,
};

struct FileDialogResult {
    FileDialogAction action;
    std::string path;
};

class ScopedFont final {
  public:
    explicit ScopedFont(ImFont* font) noexcept : font_(font) {
        if (font_ != nullptr) {
            ImGui::PushFont(font_);
        }
    }

    ~ScopedFont() {
        if (font_ != nullptr) {
            ImGui::PopFont();
        }
    }

    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;

  private:
    ImFont* font_ = nullptr;
};

[[nodiscard]] inline bool begin_panel_window(const char* name, bool* open, ImFont* title_font,
                                             ImGuiWindowFlags flags = ImGuiWindowFlags_None) {
    const ScopedFont title_scope{title_font};
    return ImGui::Begin(name, open, flags);
}

struct ViewerState {
    bool show_3d_view = true;
    bool show_scene_hierarchy = true;
    bool show_model_information = true;
    bool show_rendering_panel = true;
    bool show_selection_panel = true;
    bool show_measurement_panel = true;
    bool show_clipping_panel = true;
    bool show_navigation_settings = false;
    bool show_imgui_demo = false;
    bool show_status_bar = true;
    bool show_about = false;
    bool reset_dock_layout = false;
    bool dock_layout_initialized = false;
    bool apply_dock_layout = false;
    ImGuiID dock_center_id = 0;
    ImGuiID dock_right_id = 0;
    ImGuiID dock_right_bottom_id = 0;
    bool request_open_modal = false;
    bool request_save_modal = false;
    bool request_error_modal = false;
    bool request_save_error_modal = false;
    std::array<char, 2048> open_folder_path{};
    std::array<char, 2048> open_file_path{};
    std::array<char, 256> open_search{};
    std::filesystem::path open_browser_directory;
    std::filesystem::path open_browser_selected_path;
    std::filesystem::path preferences_path;
    std::filesystem::path last_model_directory;
    std::vector<OpenBrowserEntry> open_browser_entries;
    std::vector<std::filesystem::path> open_browser_bookmarks;
    std::vector<std::filesystem::path> open_browser_recents;
    std::vector<std::filesystem::path> open_browser_history;
    std::size_t open_browser_history_index = 0;
    std::string open_browser_error;
    ExternalEditorPaths external_editors;
    std::optional<OpenBrowserEntry> open_browser_properties;
    std::optional<std::filesystem::path> pending_save_path;
    bool open_browser_initialized = false;
    bool open_browser_needs_refresh = false;
    bool request_open_browser_properties = false;
    std::optional<std::string> dropped_path;
    bool drop_copy_failed = false;
    Extent2D view_dimensions;
    bool framebuffer_valid = false;
    std::array<float, 4> clear_color{1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> cube_color{0.72F, 0.32F, 0.12F, 1.0F};
    BasicLighting lighting;
    bool rotate_cube = true;
    float rotation_speed = 0.8F;
    float rotation_angle = 0.0F;
    RenderStatistics statistics;
    std::string viewport_error;
    std::optional<LoadFailure> load_failure;
    std::optional<LoadFailure> save_failure;
    bool application_focused = true;
    bool cursor_captured = false;
    bool raw_mouse_motion_enabled = false;
    bool imgui_cursor_change_disabled_by_capture = false;
    bool imgui_cursor_change_was_disabled = false;
    std::optional<ImVec2> navigation_cursor_position;
    float navigation_wheel_delta = 0.0F;
    std::optional<ImVec2> navigation_wheel_position;
    std::optional<EntityId> last_revealed_hierarchy_selection;
    float main_menu_height = 0.0F;
    float toolbar_height = 0.0F;
    ImFont* panel_title_font = nullptr;
    ImFont* panel_content_font = nullptr;
};

struct ViewerScene {
    std::unique_ptr<Scene> scene;
    EntityId camera;
    std::optional<EntityId> cube;
    std::optional<MaterialHandle> cube_material;
    std::filesystem::path source_path;
    SceneStatistics source_statistics;
    std::optional<Bounds3> source_bounds;
    bool camera_needs_reset = true;
    bool hierarchy_snapshot_valid = false;
    SceneHierarchySnapshot hierarchy_snapshot;
    SceneLoadReport load_report;

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

struct DecodedImage {
    std::vector<unsigned char> rgba;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class ToolbarTexture final {
  public:
    ToolbarTexture() noexcept = default;
    ~ToolbarTexture();
    ToolbarTexture(const ToolbarTexture&) = delete;
    ToolbarTexture& operator=(const ToolbarTexture&) = delete;
    ToolbarTexture(ToolbarTexture&& other) noexcept;
    ToolbarTexture& operator=(ToolbarTexture&& other) noexcept;
    [[nodiscard]] bool upload(const DecodedImage& image) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] ImTextureRef texture_ref() const noexcept;

  private:
    unsigned int texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};

enum class ToolbarIcon : std::size_t {
    open,
    save_as,
    fit_view,
    reset_camera,
    select,
    measure,
    clipping_panel,
    section_plane,
    add_clipping_box,
    clear_clipping,
    hide_selected,
    show_selected,
    isolate_selected,
    show_all,
    reset_layout,
    count,
};

struct ToolbarIcons {
    std::array<ToolbarTexture, static_cast<std::size_t>(ToolbarIcon::count)> textures;
    [[nodiscard]] const ToolbarTexture& texture(ToolbarIcon icon) const noexcept {
        return textures[static_cast<std::size_t>(icon)];
    }
};

struct ViewPanelContext {
    GLFWwindow* window = nullptr;
    ImGuiID dockspace_id = 0;
    ViewerState* state = nullptr;
    Engine* engine = nullptr;
    Viewport* viewport = nullptr;
    ViewerScene* scene = nullptr;
};

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
[[nodiscard]] ImFont* load_viewer_font(GLFWwindow* window, const char* font_path_utf8,
                                       float logical_size_pixels);
[[nodiscard]] std::string lowercase_ascii(std::string value);
[[nodiscard]] std::string file_name_label(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path absolute_path_no_throw(const std::filesystem::path& path);
[[nodiscard]] bool same_path_key(const std::filesystem::path& left,
                                 const std::filesystem::path& right);
void push_unique_directory(std::vector<std::filesystem::path>& directories,
                           const std::filesystem::path& candidate, std::size_t maximum_count);
[[nodiscard]] std::optional<std::filesystem::path> environment_directory(const char* name);
void load_viewer_preferences(ViewerState& state);
void remember_model_directory(ViewerState& state, const std::filesystem::path& model_path);
void initialize_open_browser(ViewerState& state, const ViewerScene& scene);
void initialize_save_browser(ViewerState& state, const ViewerScene& scene);
void set_open_browser_directory(ViewerState& state, const std::filesystem::path& directory,
                                bool add_to_history = true);
void navigate_open_browser_history(ViewerState& state, int offset);
void refresh_open_browser_entries(ViewerState& state);
void set_open_browser_selected_file(ViewerState& state, const std::filesystem::path& path);
void clear_open_browser_selected_file(ViewerState& state);
[[nodiscard]] bool supported_model_path(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path fallback_open_directory();
[[nodiscard]] std::string format_file_size(std::uintmax_t bytes);
[[nodiscard]] ExternalEditorPaths find_external_editors();
[[nodiscard]] bool launch_external_editor(const std::filesystem::path& editor,
                                          const std::filesystem::path& file);
void build_file_context_menu(const OpenBrowserEntry& entry, ViewerState& state,
                             std::optional<std::filesystem::path>& open_request);
void build_file_properties_popup(ViewerState& state);

[[nodiscard]] std::filesystem::path viewer_asset_root(int argument_count, char** arguments);
[[nodiscard]] ToolbarIcons load_toolbar_icons(const std::filesystem::path& asset_root);
[[nodiscard]] Result<ViewerScene> create_demo_scene(Engine& engine);
[[nodiscard]] Result<ViewerScene> load_model_scene(Engine& engine,
                                                   const std::filesystem::path& path);
void glfw_error_callback(int error_code, const char* description);
void glfw_drop_callback(GLFWwindow* window, int path_count, const char** paths);
void glfw_navigation_scroll_callback(GLFWwindow* window, double x_offset, double y_offset);
[[nodiscard]] Quaternion axis_angle(Float3 axis, float radians) noexcept;

void build_main_menu(GLFWwindow* window, ViewerState& state, const ViewerScene& scene,
                     Viewport& viewport, ViewerCommands& commands);
void build_toolbar(ViewerState& state, const ToolbarIcons& icons, const ViewerScene& scene,
                   Viewport& viewport, ViewerCommands& commands);
[[nodiscard]] ImGuiID build_main_dockspace(ViewerState& state);
void set_default_dock(ImGuiID dock_id, bool force);
void tooltip(const char* text);

[[nodiscard]] GraphicsProcedure load_opengl_procedure(const char* name) noexcept;
[[nodiscard]] bool has_nonzero_extent(Extent2D extent) noexcept;
[[nodiscard]] const char* tool_name(ViewportTool tool) noexcept;
[[nodiscard]] const char* measurement_state_name(DistanceMeasurementState state) noexcept;
[[nodiscard]] Float3 bounds_center(const Bounds3& bounds) noexcept;
[[nodiscard]] bool valid_box_for_commit(const ClippingBox& box) noexcept;
[[nodiscard]] bool navigation_blocked_by_modal() noexcept;
void release_navigation_cursor(GLFWwindow* window, ViewerState& state) noexcept;
[[nodiscard]] bool glfw_mouse_button_down(GLFWwindow* window, int button) noexcept;
void set_viewport_error(ViewerState& state, const Error& error);
void reset_demo_cube_transform(ViewerState& state, ViewerScene& scene);
void update_demo_cube_animation(ViewerState& state, ViewerScene& scene);
void apply_demo_cube_color(ViewerState& state, ViewerScene& scene);
bool color_control(const char* label, std::array<float, 4>& rgba);
[[nodiscard]] std::string clipping_status(const ClippingSnapshot& snapshot,
                                          bool has_visible_bounds);
[[nodiscard]] std::string format_distance(double meters, LengthDisplayUnit unit);
void build_3d_view(const ViewPanelContext& context);

[[nodiscard]] std::string entity_label(const ViewerScene& scene, EntityId entity);
[[nodiscard]] std::string selected_entity_label(const ViewerScene& scene,
                                                const SelectionSnapshot& selection);
void build_model_information(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene);
void build_rendering_panel(ImGuiID dockspace_id, ViewerState& state, ViewerScene& scene);
void build_navigation_settings_window(ImGuiID dockspace_id, ViewerState& state, Viewport& viewport);
void build_selection_panel(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene,
                           Viewport& viewport);
void build_measurement_panel(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene,
                             Viewport& viewport);
void build_clipping_panel(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene,
                          Viewport& viewport);
void build_scene_hierarchy_panel(ImGuiID dockspace_id, ViewerState& state, ViewerScene& scene,
                                 Viewport& viewport);
void invalidate_hierarchy_snapshot(ViewerScene& scene) noexcept;
void apply_hierarchy_error(ViewerState& state, const Result<void>& result);

[[nodiscard]] const char* error_category(ErrorCode code) noexcept;
void push_professional_dialog_style();
void pop_professional_dialog_style();
[[nodiscard]] std::optional<FileDialogResult> build_open_modal(ViewerState& state,
                                                               const ViewerScene& scene);
[[nodiscard]] std::optional<FileDialogResult> build_save_modal(ViewerState& state,
                                                               const ViewerScene& scene);
void build_error_modal(ViewerState& state);
void build_save_error_modal(ViewerState& state);
void build_status_bar(const ViewerState& state, const Engine& engine, const ViewerScene& scene,
                      const Viewport& viewport);
void build_about_window(ViewerState& state);

int run_viewer_entry(int argument_count, char** arguments);

} // namespace elf3d::viewer
