#include <elf3d/elf3d.h>
#include <elf3d/imgui/context.h>
#include <elf3d/imgui/texture.h>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

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

struct ViewerState {
    bool show_3d_view = true;
    bool show_imgui_demo = false;
    bool show_status_bar = true;
    bool show_about = false;
    elf3d::Extent2D view_dimensions;
    bool framebuffer_valid = false;
    std::array<float, 4> clear_color{0.08F, 0.16F, 0.28F, 1.0F};
    std::string viewport_error;
};

void glfw_error_callback(int error_code, const char *description) {
    std::cerr << "GLFW error " << error_code << ": "
              << (description != nullptr ? description : "No description") << '\n';
}

void build_main_menu(GLFWwindow *window, ViewerState &state) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        ImGui::BeginDisabled();
        ImGui::MenuItem("Open...");
        ImGui::EndDisabled();

        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("3D View", nullptr, &state.show_3d_view);
        ImGui::MenuItem("Dear ImGui Demo", nullptr, &state.show_imgui_demo);
        ImGui::MenuItem("Status Bar", nullptr, &state.show_status_bar);
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

void set_viewport_error(ViewerState &state, const elf3d::Error &error) {
    state.viewport_error = error.message();
    state.framebuffer_valid = false;
}

void build_3d_view(ImGuiID dockspace_id, ViewerState &state, elf3d::Engine &engine,
                   elf3d::Viewport &engine_viewport) {
    if (!state.show_3d_view) {
        state.view_dimensions = {};
        state.framebuffer_valid = false;
        return;
    }

    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{0.055F, 0.06F, 0.07F, 1.0F});
    const bool is_visible = ImGui::Begin("3D View", &state.show_3d_view);
    ImGui::PopStyleColor();

    if (is_visible) {
        ImGui::SetNextItemWidth(220.0F);
        ImGui::ColorEdit4("Clear color", state.clear_color.data(), ImGuiColorEditFlags_NoInputs);

        const ImVec2 area_size = ImGui::GetContentRegionAvail();
        state.view_dimensions = content_extent_in_pixels(area_size);
        state.viewport_error.clear();

        const elf3d::Result<void> resize_result = engine_viewport.resize(state.view_dimensions);
        if (!resize_result) {
            set_viewport_error(state, resize_result.error());
        } else {
            engine_viewport.set_clear_color(
                elf3d::Color4{state.clear_color[0], state.clear_color[1], state.clear_color[2],
                              state.clear_color[3]});

            const elf3d::Result<void> render_result = engine_viewport.render();
            if (!render_result) {
                set_viewport_error(state, render_result.error());
            } else {
                state.framebuffer_valid = engine_viewport.framebuffer_valid();

                if (state.framebuffer_valid) {
                    const elf3d::Result<elf3d::NativeTextureView> texture_result =
                        engine.native_texture_view(engine_viewport.color_texture());
                    if (!texture_result) {
                        set_viewport_error(state, texture_result.error());
                    } else {
                        const elf3d::Result<void> image_result = elf3d::imgui::image(
                            texture_result.value(), elf3d::Float2{area_size.x, area_size.y});
                        if (!image_result) {
                            set_viewport_error(state, image_result.error());
                        }
                    }
                } else {
                    ImGui::Dummy(area_size);
                }
            }
        }

        if (!state.viewport_error.empty()) {
            ImGui::TextWrapped("Viewport error: %s", state.viewport_error.c_str());
        }
    }

    ImGui::End();
}

const char *graphics_backend_name(elf3d::GraphicsBackend backend) noexcept {
    switch (backend) {
    case elf3d::GraphicsBackend::opengl:
        return "OpenGL 4.1 core";
    }
    return "Unknown";
}

void build_status_bar(const ViewerState &state, const elf3d::Engine &engine) {
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
        ImGui::Text("Elf3D %s  |  %s  |  Viewport %u x %u  |  FBO %s  |  %.2f ms  |  %.1f FPS",
                    elf3d::version_string(), graphics_backend_name(engine.graphics_backend()),
                    state.view_dimensions.width, state.view_dimensions.height,
                    state.framebuffer_valid ? "valid" : "inactive", frame_time_ms, fps);
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

int run_viewer() {
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
    engine_configuration.graphics_backend = elf3d::GraphicsBackend::opengl;
    engine_configuration.opengl.load_procedure = load_opengl_procedure;

    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(engine_configuration);
    if (!engine_result) {
        std::cerr << "Failed to create the Elf3D engine: " << engine_result.error().message()
                  << '\n';
        return 1;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();

    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine->create_viewport(elf3d::Extent2D{});
    if (!viewport_result) {
        std::cerr << "Failed to create the Elf3D viewport: " << viewport_result.error().message()
                  << '\n';
        return 1;
    }
    std::unique_ptr<elf3d::Viewport> engine_viewport = std::move(viewport_result).value();

    std::string imgui_error;
    std::unique_ptr<elf3d::imgui::Context> imgui =
        elf3d::imgui::Context::create(window.get(), glsl_version, imgui_error);
    if (!imgui) {
        std::cerr << imgui_error << '\n';
        return 1;
    }

    ViewerState state;
    while (glfwWindowShouldClose(window.get()) == GLFW_FALSE) {
        glfwPollEvents();
        imgui->begin_frame();

        build_main_menu(window.get(), state);
        const ImGuiID dockspace_id = ImGui::DockSpaceOverViewport();
        build_3d_view(dockspace_id, state, *engine, *engine_viewport);
        build_status_bar(state, *engine);
        build_about_window(state);

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

int main() {
    try {
        return run_viewer();
    } catch (const std::exception &exception) {
        std::cerr << "Elf3D viewer terminated after an unexpected exception: " << exception.what()
                  << '\n';
    } catch (...) {
        std::cerr << "Elf3D viewer terminated after an unknown exception\n";
    }

    return 1;
}
