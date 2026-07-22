#include "viewer_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace elf3d::viewer {
namespace {

inline constexpr unsigned int gl_shading_language_version = 0x8B8CU;
inline constexpr unsigned int gl_context_flags = 0x821EU;
inline constexpr unsigned int gl_context_profile_mask = 0x9126U;
inline constexpr unsigned int gl_samples = 0x80A9U;
inline constexpr unsigned int gl_framebuffer_srgb = 0x8DB9U;

[[nodiscard]] std::string gl_string(unsigned int name) {
    const unsigned char* value = glGetString(name);
    if (value == nullptr) {
        return "unavailable";
    }
    std::string result;
    for (std::size_t index = 0; value[index] != 0; ++index) {
        result.push_back(static_cast<char>(value[index]));
    }
    return result;
}

void capture_context_diagnostics(GLFWwindow* window, ViewerState& state) {
    (void)window;
    state.gl_vendor = gl_string(GL_VENDOR);
    state.gl_renderer = gl_string(GL_RENDERER);
    state.gl_version = gl_string(GL_VERSION);
    state.glsl_version_report = gl_string(gl_shading_language_version);
    glGetIntegerv(gl_context_flags, &state.gl_context_flags);
    glGetIntegerv(gl_context_profile_mask, &state.gl_context_profile_mask);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &state.maximum_texture_size);
    glGetIntegerv(GL_RED_BITS, &state.default_red_bits);
    glGetIntegerv(GL_GREEN_BITS, &state.default_green_bits);
    glGetIntegerv(GL_BLUE_BITS, &state.default_blue_bits);
    glGetIntegerv(GL_ALPHA_BITS, &state.default_alpha_bits);
    glGetIntegerv(GL_DEPTH_BITS, &state.default_depth_bits);
    glGetIntegerv(GL_STENCIL_BITS, &state.default_stencil_bits);
    glGetIntegerv(gl_samples, &state.default_samples);
    state.default_srgb_capable = glIsEnabled(gl_framebuffer_srgb) == GL_TRUE ? 1 : 0;
}

[[nodiscard]] double elapsed_milliseconds(std::chrono::steady_clock::time_point begin,
                                          std::chrono::steady_clock::time_point end) noexcept {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

void retain_frame_sample(ViewerState& state, const ViewerState::FrameSample& sample) {
    ++state.captured_frame_count;
    state.frame_samples.push_back(sample);
    const std::size_t maximum_samples = state.capture_performance_csv ? 10000U : 600U;
    if (state.frame_samples.size() > maximum_samples) {
        state.frame_samples.erase(
            state.frame_samples.begin(),
            state.frame_samples.begin() +
                static_cast<std::ptrdiff_t>(state.frame_samples.size() - maximum_samples));
    }
}

} // namespace

[[noreturn]] void fatal_viewer_allocation_failure() noexcept {
    fatal_error("Elf3D viewer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_viewer_exception() noexcept {
    fatal_error("Elf3D viewer encountered an unexpected exception");
}

void report_load_failure(ViewerState& state, const std::string& path, const elf3d::Error& error) {
    state.load_failure = LoadFailure{path, error};
    state.request_error_modal = true;
    std::cerr << "Failed to load '" << path << "' [" << error_category(error.code())
              << "]: " << error.message() << '\n';
}

void attempt_model_save(ViewerState& state, ViewerScene& scene, const std::string& target_path) {
    const elf3d::Result<void> saved = scene.scene->export_loaded_document(target_path);
    if (!saved) {
        state.save_failure = LoadFailure{target_path, saved.error()};
        state.request_save_error_modal = true;
        return;
    }
    scene.source_path = path_from_utf8(target_path);
    remember_model_directory(state, scene.source_path);
}

void attempt_model_load(elf3d::Engine& engine, elf3d::Viewport& engine_viewport, ViewerState& state,
                        ViewerScene& active_scene, const std::string& source_path) {
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
        remember_model_directory(state, active_scene.source_path);
        state.rotation_angle = 0.0F;
        state.statistics = {};
        state.last_revealed_hierarchy_selection.reset();
    } catch (const std::bad_alloc&) {
        fatal_viewer_allocation_failure();
    } catch (const std::filesystem::filesystem_error&) {
        report_load_failure(state, source_path,
                            elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                         "The viewer could not convert the UTF-8 source path"});
    } catch (...) {
        fatal_unexpected_viewer_exception();
    }
}

[[nodiscard]] bool camera_shortcuts_available(const ViewerState& state, const ViewerScene& scene,
                                              const elf3d::Viewport& engine_viewport) noexcept {
    const elf3d::Result<std::optional<elf3d::Bounds3>> visible_bounds =
        engine_viewport.visible_bounds(*scene.scene);
    return state.show_3d_view && visible_bounds && visible_bounds.value().has_value() &&
           has_nonzero_extent(state.view_dimensions) && !ImGui::GetIO().WantTextInput &&
           !navigation_blocked_by_modal();
}

struct ViewerRuntime {
    GlfwRuntime glfw;
    Window window;
    std::unique_ptr<elf3d::Engine> engine;
    std::unique_ptr<elf3d::Viewport> viewport;
    ViewerScene scene;
    ViewerState state;
    std::unique_ptr<elf3d::imgui::Context> imgui;
    ToolbarIcons toolbar_icons;
};

[[nodiscard]] bool initialize_viewer_window(ViewerRuntime& runtime) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!runtime.glfw.initialize()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    runtime.window.reset(glfwCreateWindow(1600, 900, "Elf3D Viewer", nullptr, nullptr));
    if (!runtime.window) {
        std::cerr << "Failed to create the Elf3D GLFW window with an OpenGL 4.1 core context\n";
        return false;
    }
    glfwMakeContextCurrent(runtime.window.get());
    if (glfwGetCurrentContext() != runtime.window.get() || glGetString(GL_VERSION) == nullptr) {
        std::cerr << "Failed to initialize the OpenGL context\n";
        return false;
    }
    glfwSwapInterval(1);
    capture_context_diagnostics(runtime.window.get(), runtime.state);
    return true;
}

[[nodiscard]] bool initialize_viewer_engine(ViewerRuntime& runtime) {
    elf3d::EngineConfiguration configuration;
    configuration.opengl.load_procedure = load_opengl_procedure;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine = elf3d::Engine::create(configuration);
    if (!engine) {
        std::cerr << "Failed to create the Elf3D engine: " << engine.error().message() << '\n';
        return false;
    }
    runtime.engine = std::move(engine).value();
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport = runtime.engine->create_viewport({});
    if (!viewport) {
        std::cerr << "Failed to create the Elf3D viewport: " << viewport.error().message() << '\n';
        return false;
    }
    runtime.viewport = std::move(viewport).value();
    elf3d::Result<ViewerScene> scene = create_demo_scene(*runtime.engine);
    if (!scene) {
        std::cerr << "Failed to create the Elf3D demonstration scene: " << scene.error().message()
                  << '\n';
        return false;
    }
    runtime.scene = std::move(scene).value();
    return true;
}

[[nodiscard]] bool initialize_viewer_imgui(ViewerRuntime& runtime,
                                           const std::filesystem::path& asset_root) {
    const std::string font_path = path_to_utf8(asset_root / "font" / "DroidSans.ttf");
    elf3d::imgui::ContextOptions options;
    options.font_path_utf8 = font_path;
    options.font_size_pixels = viewer_ui_font_size_pixels;
    elf3d::Result<std::unique_ptr<elf3d::imgui::Context>> imgui =
        elf3d::imgui::Context::create(runtime.window.get(), glsl_version, options);
    if (!imgui) {
        std::cerr << imgui.error().message() << '\n';
        return false;
    }
    runtime.imgui = std::move(imgui).value();
    runtime.toolbar_icons = load_toolbar_icons(asset_root);
    runtime.state.panel_title_font =
        load_viewer_font(runtime.window.get(), font_path.c_str(), panel_title_font_size_pixels);
    runtime.state.panel_content_font =
        load_viewer_font(runtime.window.get(), font_path.c_str(), panel_content_font_size_pixels);
    return true;
}

[[nodiscard]] bool initialize_viewer_runtime(ViewerRuntime& runtime, int argument_count,
                                             char** arguments) {
    if (!initialize_viewer_window(runtime) || !initialize_viewer_engine(runtime)) {
        return false;
    }
    load_viewer_preferences(runtime.state);
    glfwSetWindowUserPointer(runtime.window.get(), &runtime.state);
    glfwSetScrollCallback(runtime.window.get(), glfw_navigation_scroll_callback);
    const auto asset_root = viewer_asset_root(argument_count, arguments);
    if (!initialize_viewer_imgui(runtime, asset_root)) {
        return false;
    }
    glfwSetDropCallback(runtime.window.get(), glfw_drop_callback);
    if (argument_count >= 2 && arguments[1] != nullptr) {
        attempt_model_load(*runtime.engine, *runtime.viewport, runtime.state, runtime.scene,
                           arguments[1]);
    }
    return true;
}

void begin_viewer_frame(ViewerRuntime& runtime) {
    runtime.state.viewport_rendered_this_frame = false;
    runtime.state.navigation_wheel_delta = 0.0F;
    runtime.state.navigation_wheel_position.reset();
    glfwPollEvents();
    runtime.state.application_focused =
        glfwGetWindowAttrib(runtime.window.get(), GLFW_FOCUSED) == GLFW_TRUE;
    if (!runtime.state.application_focused) {
        runtime.viewport->cancel_interaction();
        release_navigation_cursor(runtime.window.get(), runtime.state);
    }
    runtime.imgui->begin_frame();
    runtime.state.viewport_error.clear();
}

void collect_save_shortcut(ViewerRuntime& runtime) {
    if (runtime.scene.is_imported() && !navigation_blocked_by_modal() && ImGui::GetIO().KeyCtrl &&
        ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_S)) {
        runtime.state.request_save_modal = true;
    }
}

void collect_camera_shortcuts(ViewerRuntime& runtime, ViewerCommands& commands) {
    if (!camera_shortcuts_available(runtime.state, runtime.scene, *runtime.viewport)) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        commands.fit_to_scene = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        commands.reset_view = true;
    }
}

[[nodiscard]] bool tool_shortcuts_available(const ViewerRuntime& runtime) noexcept {
    return runtime.state.show_3d_view && has_nonzero_extent(runtime.state.view_dimensions) &&
           !ImGui::GetIO().WantTextInput && !navigation_blocked_by_modal();
}

[[nodiscard]] bool selection_shortcut_pressed(const ViewerRuntime& runtime) noexcept {
    return !glfw_mouse_button_down(runtime.window.get(), GLFW_MOUSE_BUTTON_LEFT) &&
           !glfw_mouse_button_down(runtime.window.get(), GLFW_MOUSE_BUTTON_RIGHT) &&
           !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_S);
}

void collect_tool_shortcuts(ViewerRuntime& runtime, ViewerCommands& commands) {
    if (!tool_shortcuts_available(runtime)) {
        return;
    }
    if (selection_shortcut_pressed(runtime)) {
        commands.select_tool = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_M)) {
        commands.measure_tool = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        commands.clear_measurement = true;
    }
}

void collect_escape_shortcut(ViewerRuntime& runtime, ViewerCommands& commands) {
    if (!runtime.state.show_3d_view || ImGui::GetIO().WantTextInput ||
        navigation_blocked_by_modal() || !ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        return;
    }
    const elf3d::DistanceMeasurementSnapshot measurement =
        runtime.viewport->distance_measurement_snapshot(*runtime.scene.scene);
    if (measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point) {
        commands.cancel_measurement = true;
    } else {
        commands.clear_selection = true;
    }
}

void collect_viewer_shortcuts(ViewerRuntime& runtime, ViewerCommands& commands) {
    collect_save_shortcut(runtime);
    collect_camera_shortcuts(runtime, commands);
    collect_tool_shortcuts(runtime, commands);
    collect_escape_shortcut(runtime, commands);
}

void execute_tool_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.select_tool) {
        runtime.viewport->set_active_tool(elf3d::ViewportTool::selection);
    }
    if (commands.measure_tool) {
        const elf3d::Result<void> result = runtime.viewport->begin_distance_measurement();
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
    if (commands.show_clipping_panel) {
        runtime.state.show_clipping_panel = true;
    }
    if (commands.cancel_measurement) {
        runtime.viewport->cancel_distance_measurement();
    }
    if (commands.clear_measurement) {
        runtime.viewport->clear_distance_measurement();
    }
}

void toggle_section_plane(ViewerRuntime& runtime) {
    elf3d::SectionPlane plane = runtime.viewport->clipping_snapshot().section_plane;
    plane.enabled = !plane.enabled;
    if (plane.enabled) {
        const elf3d::Result<std::optional<elf3d::Bounds3>> bounds =
            runtime.viewport->visible_bounds(*runtime.scene.scene);
        if (bounds && bounds.value().has_value()) {
            plane.point = bounds_center(*bounds.value());
        }
    }
    const elf3d::Result<void> result = runtime.viewport->set_section_plane(plane);
    if (!result) {
        set_viewport_error(runtime.state, result.error());
    }
}

void flip_section_plane(ViewerRuntime& runtime) {
    elf3d::SectionPlane plane = runtime.viewport->clipping_snapshot().section_plane;
    plane.retained_half_space = plane.retained_half_space == elf3d::PlaneHalfSpace::positive
                                    ? elf3d::PlaneHalfSpace::negative
                                    : elf3d::PlaneHalfSpace::positive;
    const elf3d::Result<void> result = runtime.viewport->set_section_plane(plane);
    if (!result) {
        set_viewport_error(runtime.state, result.error());
    }
}

void execute_section_plane_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.enable_section_plane) {
        toggle_section_plane(runtime);
    }
    if (commands.flip_section_side) {
        flip_section_plane(runtime);
    }
}

void execute_clipping_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.add_clipping_box_from_bounds) {
        const elf3d::Result<std::uint32_t> result =
            runtime.viewport->add_clipping_box_from_visible_bounds(*runtime.scene.scene);
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
    if (commands.clear_clipping) {
        runtime.viewport->clear_clipping();
    }
    if (commands.toggle_clipping_helpers) {
        const elf3d::ClippingSnapshot clipping = runtime.viewport->clipping_snapshot();
        const elf3d::Result<void> result =
            runtime.viewport->set_clipping_helpers_visible(!clipping.helpers.visible);
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
    if (commands.fit_to_clipped_content) {
        const elf3d::Result<void> result =
            runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera);
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
}

void replace_with_demo_scene(ViewerRuntime& runtime) {
    elf3d::Result<ViewerScene> replacement = create_demo_scene(*runtime.engine);
    if (!replacement) {
        report_load_failure(runtime.state, "Procedural cube demo", replacement.error());
        return;
    }
    runtime.viewport->cancel_interaction();
    runtime.viewport->clear_selection();
    runtime.viewport->clear_isolation();
    runtime.viewport->clear_distance_measurement();
    runtime.viewport->clear_clipping();
    runtime.scene = std::move(replacement).value();
    runtime.state.statistics = {};
    runtime.state.last_revealed_hierarchy_selection.reset();
}

void execute_scene_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.reload && runtime.scene.is_imported()) {
        attempt_model_load(*runtime.engine, *runtime.viewport, runtime.state, runtime.scene,
                           path_to_utf8(runtime.scene.source_path));
    }
    if (commands.close_scene) {
        replace_with_demo_scene(runtime);
    }
    if (commands.fit_to_scene) {
        const elf3d::Result<void> result =
            runtime.viewport->fit_to_scene(*runtime.scene.scene, runtime.scene.camera);
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
    if (commands.reset_view) {
        const elf3d::Result<void> result =
            runtime.viewport->reset_view(*runtime.scene.scene, runtime.scene.camera);
        if (!result) {
            set_viewport_error(runtime.state, result.error());
        }
    }
}

void execute_selection_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.clear_selection) {
        runtime.viewport->clear_selection();
    }
    if (commands.hide_selected) {
        apply_hierarchy_error(runtime.state,
                              runtime.viewport->hide_selected_in_scene(*runtime.scene.scene));
        invalidate_hierarchy_snapshot(runtime.scene);
    }
    if (commands.show_selected) {
        apply_hierarchy_error(runtime.state,
                              runtime.viewport->show_selected_in_scene(*runtime.scene.scene));
        invalidate_hierarchy_snapshot(runtime.scene);
    }
}

void execute_visibility_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    if (commands.show_all) {
        apply_hierarchy_error(runtime.state, runtime.scene.scene->show_all_entities());
        invalidate_hierarchy_snapshot(runtime.scene);
    }
    if (commands.isolate_selected) {
        apply_hierarchy_error(runtime.state,
                              runtime.viewport->isolate_selected(*runtime.scene.scene));
    }
    if (commands.exit_isolation) {
        runtime.viewport->clear_isolation();
    }
}

void execute_viewer_commands(ViewerRuntime& runtime, const ViewerCommands& commands) {
    execute_tool_commands(runtime, commands);
    execute_section_plane_commands(runtime, commands);
    execute_clipping_commands(runtime, commands);
    execute_scene_commands(runtime, commands);
    execute_selection_commands(runtime, commands);
    execute_visibility_commands(runtime, commands);
}

void handle_pending_model_files(ViewerRuntime& runtime) {
    if (runtime.state.dropped_path.has_value()) {
        std::string path = std::move(*runtime.state.dropped_path);
        runtime.state.dropped_path.reset();
        attempt_model_load(*runtime.engine, *runtime.viewport, runtime.state, runtime.scene, path);
    }
    if (runtime.state.drop_copy_failed) {
        runtime.state.drop_copy_failed = false;
        report_load_failure(runtime.state, "Dropped file",
                            elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                         "The viewer could not copy the dropped UTF-8 path"});
    }
    const std::optional<FileDialogResult> open_result =
        build_open_modal(runtime.state, runtime.scene);
    if (open_result.has_value()) {
        attempt_model_load(*runtime.engine, *runtime.viewport, runtime.state, runtime.scene,
                           open_result->path);
    }
    const std::optional<FileDialogResult> save_result =
        build_save_modal(runtime.state, runtime.scene);
    if (!save_result.has_value()) {
        return;
    }
    if (save_result->action == FileDialogAction::open) {
        attempt_model_load(*runtime.engine, *runtime.viewport, runtime.state, runtime.scene,
                           save_result->path);
    } else {
        attempt_model_save(runtime.state, runtime.scene, save_result->path);
    }
}

void build_viewer_panels(ViewerRuntime& runtime, ImGuiID dockspace_id) {
    ViewerState& state = runtime.state;
    ViewerScene& scene = runtime.scene;
    build_rendering_panel(dockspace_id, state, scene);
    update_demo_cube_animation(state, scene);
    build_3d_view(ViewPanelContext{runtime.window.get(), dockspace_id, &state, runtime.engine.get(),
                                   runtime.viewport.get(), &scene});
    build_scene_hierarchy_panel(dockspace_id, state, scene, *runtime.viewport);
    build_model_information(dockspace_id, state, scene);
    build_navigation_settings_window(dockspace_id, state, *runtime.viewport);
    build_selection_panel(dockspace_id, state, scene, *runtime.viewport);
    build_measurement_panel(dockspace_id, state, scene, *runtime.viewport);
    build_clipping_panel(dockspace_id, state, scene, *runtime.viewport);
    build_status_bar(state, *runtime.engine, scene, *runtime.viewport);
    build_about_window(state);
    build_error_modal(state);
    build_save_error_modal(state);
    if (state.show_imgui_demo) {
        ImGui::ShowDemoWindow(&state.show_imgui_demo);
    }
    state.apply_dock_layout = false;
}

void build_viewer_frame_ui(ViewerRuntime& runtime) {
    ViewerCommands commands;
    build_main_menu(runtime.window.get(), runtime.state, runtime.scene, *runtime.viewport,
                    commands);
    build_toolbar(runtime.state, runtime.toolbar_icons, runtime.scene, *runtime.viewport, commands);
    const ImGuiID dockspace_id = build_main_dockspace(runtime.state);
    collect_viewer_shortcuts(runtime, commands);
    execute_viewer_commands(runtime, commands);
    handle_pending_model_files(runtime);
    build_viewer_panels(runtime, dockspace_id);
}

int run_viewer(int argument_count, char** arguments) {
    ViewerRuntime runtime;
    if (!initialize_viewer_runtime(runtime, argument_count, arguments)) {
        return 1;
    }
    ViewerState& state = runtime.state;
    Window& window = runtime.window;
    std::unique_ptr<elf3d::imgui::Context>& imgui = runtime.imgui;

    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
        const auto frame_begin = std::chrono::steady_clock::now();
        begin_viewer_frame(runtime);
        const auto event_input_end = std::chrono::steady_clock::now();
        build_viewer_frame_ui(runtime);

        if (state.vsync_enabled != state.vsync_applied) {
            glfwSwapInterval(state.vsync_enabled ? 1 : 0);
            state.vsync_applied = state.vsync_enabled;
        }

        const auto ui_build_end = std::chrono::steady_clock::now();
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window.get(), &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.035F, 0.04F, 0.05F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        imgui->render();
        const auto composition_end = std::chrono::steady_clock::now();
        glfwSwapBuffers(window.get());
        const auto frame_end = std::chrono::steady_clock::now();
        const double work_milliseconds = elapsed_milliseconds(event_input_end, ui_build_end);
        const double render_milliseconds =
            state.viewport_rendered_this_frame ? state.statistics.cpu_total_milliseconds : 0.0;
        ViewerState::FrameSample sample{elapsed_milliseconds(frame_begin, frame_end),
                                        elapsed_milliseconds(frame_begin, event_input_end),
                                        std::max(0.0, work_milliseconds - render_milliseconds),
                                        render_milliseconds,
                                        elapsed_milliseconds(ui_build_end, composition_end),
                                        elapsed_milliseconds(composition_end, frame_end),
                                        elapsed_milliseconds(event_input_end, frame_end)};
        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(window.get(), &window_width, &window_height);
        if (state.viewport_rendered_this_frame) {
            sample.render = state.statistics;
        }
        const Result<PickingStatistics> picking = runtime.viewport->picking_statistics();
        if (picking &&
            (picking.value().lifetime_gpu_requests != state.sampled_picking_gpu_requests ||
             picking.value().lifetime_cpu_fallbacks != state.sampled_picking_cpu_fallbacks)) {
            sample.picking = picking.value();
            state.sampled_picking_gpu_requests = picking.value().lifetime_gpu_requests;
            state.sampled_picking_cpu_fallbacks = picking.value().lifetime_cpu_fallbacks;
        }
        sample.window_dimensions = Extent2D{static_cast<std::uint32_t>(std::max(window_width, 0)),
                                            static_cast<std::uint32_t>(std::max(window_height, 0))};
        sample.framebuffer_dimensions =
            Extent2D{static_cast<std::uint32_t>(std::max(framebuffer_width, 0)),
                     static_cast<std::uint32_t>(std::max(framebuffer_height, 0))};
        sample.view_dimensions = state.view_dimensions;
        sample.target_dimensions = state.render_target_dimensions;
        sample.render_scale_percent = state.diagnostic_render_scale_percent;
        sample.vsync_enabled = state.vsync_enabled;
        sample.standard_shading = state.shading_mode == RenderShadingMode::standard;
        sample.rendered_3d = state.viewport_rendered_this_frame;
        retain_frame_sample(state, sample);
    }
    release_navigation_cursor(window.get(), state);
    return 0;
}

int run_viewer_entry(int argument_count, char** arguments) {
    try {
        return run_viewer(argument_count, arguments);
    } catch (const std::bad_alloc&) {
        fatal_viewer_allocation_failure();
    } catch (...) {
        fatal_unexpected_viewer_exception();
    }
}

} // namespace elf3d::viewer
