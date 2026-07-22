#include <elf3d/elf3d.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

enum class Scenario {
    first_frame,
    steady,
    orbit,
    pan,
    wheel,
    orbit_anchor,
    pick,
};

struct Options final {
    std::filesystem::path model;
    elf3d::Extent2D extent;
    Scenario scenario = Scenario::steady;
    std::string scenario_name;
    std::uint32_t warmup_frames = 0;
    std::uint32_t measured_frames = 0;
    std::filesystem::path report;
};

struct FrameResult final {
    double milliseconds = 0.0;
    elf3d::RenderStatistics render;
    elf3d::PickingStatistics picking;
};

struct ContextDiagnostics final {
    int window_width = 0;
    int window_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    GLint context_flags = 0;
    GLint profile_mask = 0;
    GLint red_bits = 0;
    GLint green_bits = 0;
    GLint blue_bits = 0;
    GLint alpha_bits = 0;
    GLint depth_bits = 0;
    GLint stencil_bits = 0;
    GLint samples = 0;
    GLint maximum_texture_size = 0;
    bool framebuffer_srgb_enabled = false;
};

[[nodiscard]] ContextDiagnostics capture_context_diagnostics() noexcept {
    ContextDiagnostics context;
    GLFWwindow* window = glfwGetCurrentContext();
    if (window != nullptr) {
        glfwGetWindowSize(window, &context.window_width, &context.window_height);
        glfwGetFramebufferSize(window, &context.framebuffer_width, &context.framebuffer_height);
    }
    glGetIntegerv(GL_CONTEXT_FLAGS, &context.context_flags);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &context.profile_mask);
    GLint draw_framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);
    if (draw_framebuffer == 0) {
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                              GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
                                              &context.red_bits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                              GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                              &context.green_bits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                              GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
                                              &context.blue_bits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                              GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
                                              &context.alpha_bits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH,
                                              GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
                                              &context.depth_bits);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_STENCIL,
                                              GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
                                              &context.stencil_bits);
    }
    glGetIntegerv(GL_SAMPLES, &context.samples);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &context.maximum_texture_size);
    context.framebuffer_srgb_enabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;
    return context;
}

class GlfwRuntime final {
  public:
    ~GlfwRuntime() {
        if (initialized_) {
            glfwTerminate();
        }
    }

    [[nodiscard]] bool initialize() noexcept {
        initialized_ = glfwInit() == GLFW_TRUE;
        return initialized_;
    }

  private:
    bool initialized_ = false;
};

class Window final {
  public:
    explicit Window(GLFWwindow* value) noexcept : value_(value) {}
    ~Window() {
        if (value_ != nullptr) {
            glfwDestroyWindow(value_);
        }
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] GLFWwindow* get() const noexcept {
        return value_;
    }

  private:
    GLFWwindow* value_ = nullptr;
};

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

elf3d::GraphicsProcedure load_opengl_procedure(const char* name) noexcept {
    return glfwGetProcAddress(name);
}

[[nodiscard]] std::optional<std::uint32_t> unsigned_value(std::string_view value) noexcept {
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::optional<elf3d::Extent2D> extent_value(std::string_view value) noexcept {
    const std::size_t separator = value.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<std::uint32_t> width = unsigned_value(value.substr(0, separator));
    const std::optional<std::uint32_t> height = unsigned_value(value.substr(separator + 1));
    if (!width.has_value() || !height.has_value() || *width == 0 || *height == 0) {
        return std::nullopt;
    }
    return elf3d::Extent2D{*width, *height};
}

[[nodiscard]] std::optional<Scenario> scenario_value(std::string_view value) noexcept {
    if (value == "first-frame") {
        return Scenario::first_frame;
    }
    if (value == "steady") {
        return Scenario::steady;
    }
    if (value == "orbit") {
        return Scenario::orbit;
    }
    if (value == "pan") {
        return Scenario::pan;
    }
    if (value == "wheel") {
        return Scenario::wheel;
    }
    if (value == "orbit-anchor") {
        return Scenario::orbit_anchor;
    }
    if (value == "pick") {
        return Scenario::pick;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Options> parse_options(int count, char** values) {
    Options options;
    for (int index = 1; index < count; index += 2) {
        if (index + 1 >= count || values[index] == nullptr || values[index + 1] == nullptr) {
            return std::nullopt;
        }
        const std::string_view option{values[index]};
        const std::string_view value{values[index + 1]};
        if (option == "--model") {
            options.model = path_from_utf8(value);
        } else if (option == "--extent") {
            const std::optional<elf3d::Extent2D> extent = extent_value(value);
            if (!extent.has_value()) {
                return std::nullopt;
            }
            options.extent = *extent;
        } else if (option == "--scenario") {
            const std::optional<Scenario> scenario = scenario_value(value);
            if (!scenario.has_value()) {
                return std::nullopt;
            }
            options.scenario = *scenario;
            options.scenario_name = value;
        } else if (option == "--warmup") {
            const std::optional<std::uint32_t> frames = unsigned_value(value);
            if (!frames.has_value()) {
                return std::nullopt;
            }
            options.warmup_frames = *frames;
        } else if (option == "--frames") {
            const std::optional<std::uint32_t> frames = unsigned_value(value);
            if (!frames.has_value() || *frames == 0) {
                return std::nullopt;
            }
            options.measured_frames = *frames;
        } else if (option == "--report") {
            options.report = path_from_utf8(value);
        } else {
            return std::nullopt;
        }
    }
    if (options.model.empty() || options.extent.width == 0 || options.extent.height == 0 ||
        options.scenario_name.empty() || options.measured_frames == 0 || options.report.empty()) {
        return std::nullopt;
    }
    if (options.scenario == Scenario::first_frame && options.measured_frames != 1) {
        return std::nullopt;
    }
    return options;
}

void print_usage() {
    std::cerr << "Usage: elf3d_render_benchmark --model <path> --extent <width>x<height> "
                 "--scenario first-frame|steady|orbit|pan|wheel|orbit-anchor|pick "
                 "--warmup <frames> --frames <frames> --report <csv-path>\n";
}

[[nodiscard]] elf3d::ViewportInput scripted_input(Scenario scenario, std::uint32_t frame,
                                                  elf3d::Extent2D extent) noexcept {
    elf3d::ViewportInput input;
    input.pointer_position_pixels = {static_cast<float>(extent.width) * 0.5F,
                                     static_cast<float>(extent.height) * 0.5F};
    input.is_hovered = true;
    input.is_focused = true;
    input.frame_delta_seconds = 1.0F / 60.0F;
    if (scenario == Scenario::orbit || scenario == Scenario::orbit_anchor) {
        input.left_button_down = true;
        input.pointer_delta_pixels = frame == 0 ? elf3d::Float2{} : elf3d::Float2{2.0F, -1.0F};
    } else if (scenario == Scenario::pan) {
        input.middle_button_down = true;
        input.pointer_delta_pixels = frame == 0 ? elf3d::Float2{} : elf3d::Float2{2.0F, 1.0F};
    } else if (scenario == Scenario::wheel) {
        input.wheel_delta = 0.125F;
    }
    return input;
}

[[nodiscard]] elf3d::Result<void> execute_frame(const Options& options, std::uint32_t frame,
                                                elf3d::Viewport& viewport, elf3d::Scene& scene,
                                                elf3d::EntityId camera) {
    if (options.scenario == Scenario::orbit_anchor) {
        elf3d::ViewportInput pressed = scripted_input(options.scenario, 0, options.extent);
        const elf3d::Result<void> press = viewport.update_navigation(scene, camera, pressed);
        if (!press) {
            return press.error();
        }
        pressed.pointer_position_pixels.x += 16.0F;
        pressed.pointer_position_pixels.y -= 8.0F;
        pressed.pointer_delta_pixels = {16.0F, -8.0F};
        const elf3d::Result<void> moved = viewport.update_navigation(scene, camera, pressed);
        if (!moved) {
            return moved.error();
        }
        return viewport.render(scene, camera);
    }
    if (options.scenario == Scenario::orbit || options.scenario == Scenario::pan ||
        options.scenario == Scenario::wheel) {
        const elf3d::Result<void> navigation = viewport.update_navigation(
            scene, camera, scripted_input(options.scenario, frame, options.extent));
        if (!navigation) {
            return navigation.error();
        }
    }
    if (options.scenario == Scenario::pick) {
        const elf3d::Result<std::optional<elf3d::PickHit>> picked =
            viewport.pick(scene, camera,
                          {static_cast<float>(options.extent.width) * 0.5F,
                           static_cast<float>(options.extent.height) * 0.5F});
        if (!picked) {
            return picked.error();
        }
    }
    return viewport.render(scene, camera);
}

[[nodiscard]] bool write_report(const Options& options, double load_milliseconds,
                                const std::vector<FrameResult>& frames) {
    std::error_code error;
    std::filesystem::create_directories(options.report.parent_path(), error);
    if (error) {
        std::cerr << "Could not create report directory: " << error.message() << '\n';
        return false;
    }
    std::ofstream stream{options.report, std::ios::trunc};
    if (!stream) {
        std::cerr << "Could not create report: " << path_to_utf8(options.report) << '\n';
        return false;
    }
    stream << "scenario,frame,load_ms,frame_ms,draw_calls,triangles,candidate_primitives,"
              "visible_primitives,culled_primitives,buffer_uploads,buffer_uploaded_bytes,"
              "draw_packet_rebuilds,"
              "resident_geometry_bytes,resident_texture_bytes,cpu_list_ms,cpu_resources_ms,"
              "cpu_gl_ms,cpu_render_total_ms,gpu_main_available,gpu_main_ms,"
              "gpu_resolve_available,gpu_resolve_ms,pick_draw_calls,"
              "pick_pixels_read,pick_target_allocations,pick_pass_ms,pick_readback_ms,"
              "pick_allocation_ms,pick_cpu_ms,pick_gpu_available,pick_gpu_ms,gl_vendor,"
              "gl_renderer,gl_version,glsl_version,window_width,window_height,framebuffer_width,"
              "framebuffer_height,target_width,target_height,vsync,context_flags,profile_mask,"
              "red_bits,green_bits,blue_bits,alpha_bits,depth_bits,stencil_bits,samples,"
              "framebuffer_srgb_enabled,max_texture_size\n";
    stream << std::fixed << std::setprecision(6);
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    const ContextDiagnostics context = capture_context_diagnostics();
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const FrameResult& frame = frames[index];
        stream << options.scenario_name << ',' << index << ',' << load_milliseconds << ','
               << frame.milliseconds << ',' << frame.render.draw_calls << ','
               << frame.render.triangles << ',' << frame.render.candidate_primitives << ','
               << frame.render.visible_primitives << ',' << frame.render.frustum_culled_primitives
               << ',' << frame.render.gpu_buffer_uploads << ','
               << frame.render.gpu_buffer_uploaded_bytes << ',' << frame.render.draw_packet_rebuilds
               << ',' << frame.render.estimated_resident_geometry_bytes << ','
               << frame.render.estimated_resident_texture_bytes << ','
               << frame.render.cpu_render_list_milliseconds << ','
               << frame.render.cpu_resource_preparation_milliseconds << ','
               << frame.render.cpu_gl_submission_milliseconds << ','
               << frame.render.cpu_total_milliseconds << ','
               << (frame.render.gpu_main_pass_timing_available ? 1 : 0) << ','
               << frame.render.gpu_main_pass_milliseconds << ','
               << (frame.render.gpu_resolve_timing_available ? 1 : 0) << ','
               << frame.render.gpu_resolve_milliseconds << ','
               << frame.picking.latest_gpu_draw_calls << ',' << frame.picking.latest_gpu_pixels_read
               << ',' << frame.picking.latest_target_allocations << ','
               << frame.picking.latest_pass_milliseconds << ','
               << frame.picking.latest_readback_milliseconds << ','
               << frame.picking.latest_allocation_milliseconds << ','
               << frame.picking.latest_cpu_milliseconds << ','
               << (frame.picking.latest_gpu_timing_available ? 1 : 0) << ','
               << frame.picking.latest_gpu_milliseconds << ',' << '"'
               << (vendor != nullptr ? vendor : "unavailable") << "\",\""
               << (renderer != nullptr ? renderer : "unavailable") << "\",\""
               << (version != nullptr ? version : "unavailable") << "\",\""
               << (glsl != nullptr ? glsl : "unavailable") << "\"," << context.window_width << ','
               << context.window_height << ',' << context.framebuffer_width << ','
               << context.framebuffer_height << ',' << options.extent.width << ','
               << options.extent.height << ",0," << context.context_flags << ','
               << context.profile_mask << ',' << context.red_bits << ',' << context.green_bits
               << ',' << context.blue_bits << ',' << context.alpha_bits << ',' << context.depth_bits
               << ',' << context.stencil_bits << ',' << context.samples << ','
               << (context.framebuffer_srgb_enabled ? 1 : 0) << ',' << context.maximum_texture_size
               << '\n';
    }
    return static_cast<bool>(stream);
}

[[nodiscard]] int run(const Options& options) {
    if (!std::filesystem::is_regular_file(options.model)) {
        std::cerr << "Model does not exist: " << path_to_utf8(options.model) << '\n';
        return 3;
    }
    GlfwRuntime glfw;
    if (!glfw.initialize()) {
        std::cerr << "GLFW initialization failed\n";
        return 4;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    Window window{glfwCreateWindow(static_cast<int>(options.extent.width),
                                   static_cast<int>(options.extent.height),
                                   "Elf3D render benchmark", nullptr, nullptr)};
    if (window.get() == nullptr) {
        std::cerr << "Hidden OpenGL 4.1 context creation failed\n";
        return 5;
    }
    glfwMakeContextCurrent(window.get());
    glfwSwapInterval(0);
    if (gladLoadGL(load_opengl_procedure) == 0 || GLAD_GL_VERSION_4_1 == 0) {
        std::cerr << "OpenGL 4.1 is unavailable\n";
        return 6;
    }

    elf3d::EngineConfiguration configuration;
    configuration.opengl.load_procedure = load_opengl_procedure;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(configuration);
    if (!engine_result) {
        std::cerr << engine_result.error().message() << '\n';
        return 7;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();
    const auto load_begin = std::chrono::steady_clock::now();
    elf3d::Result<elf3d::LoadedScene> loaded_result =
        engine->load_scene(path_to_utf8(options.model));
    const auto load_end = std::chrono::steady_clock::now();
    if (!loaded_result) {
        std::cerr << loaded_result.error().message() << '\n';
        return 8;
    }
    elf3d::LoadedScene loaded = std::move(loaded_result).value();
    const elf3d::Result<elf3d::EntityId> camera =
        loaded.scene->create_perspective_camera_entity({});
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine->create_viewport(options.extent);
    if (!camera || !viewport_result) {
        std::cerr << "Camera or viewport creation failed\n";
        return 9;
    }
    std::unique_ptr<elf3d::Viewport> viewport = std::move(viewport_result).value();
    if (!viewport->reset_view(*loaded.scene, camera.value())) {
        std::cerr << "Could not frame the model\n";
        return 10;
    }

    for (std::uint32_t frame = 0; frame < options.warmup_frames; ++frame) {
        const bool event_scenario =
            options.scenario == Scenario::orbit_anchor || options.scenario == Scenario::pick;
        const elf3d::Result<void> result =
            event_scenario
                ? viewport->render(*loaded.scene, camera.value())
                : execute_frame(options, frame, *viewport, *loaded.scene, camera.value());
        if (!result) {
            std::cerr << result.error().message() << '\n';
            return 11;
        }
        const elf3d::Result<elf3d::NativeTextureView> resolved =
            engine->native_texture_view(viewport->color_texture());
        if (!resolved) {
            std::cerr << resolved.error().message() << '\n';
            return 11;
        }
        glfwSwapBuffers(window.get());
    }

    std::vector<FrameResult> frames;
    frames.reserve(options.measured_frames);
    for (std::uint32_t frame = 0; frame < options.measured_frames; ++frame) {
        const auto begin = std::chrono::steady_clock::now();
        const elf3d::Result<void> result = execute_frame(options, options.warmup_frames + frame,
                                                         *viewport, *loaded.scene, camera.value());
        if (!result) {
            std::cerr << result.error().message() << '\n';
            return 12;
        }
        const elf3d::Result<elf3d::NativeTextureView> resolved =
            engine->native_texture_view(viewport->color_texture());
        glfwSwapBuffers(window.get());
        const auto end = std::chrono::steady_clock::now();
        if (!resolved) {
            std::cerr << resolved.error().message() << '\n';
            return 12;
        }
        elf3d::PickingStatistics picking;
        const elf3d::Result<elf3d::PickingStatistics> picking_result =
            viewport->picking_statistics();
        if (picking_result) {
            picking = picking_result.value();
        }
        frames.push_back(FrameResult{std::chrono::duration<double, std::milli>(end - begin).count(),
                                     viewport->render_statistics(), picking});
    }
    const double load_milliseconds =
        std::chrono::duration<double, std::milli>(load_end - load_begin).count();
    if (!write_report(options, load_milliseconds, frames)) {
        return 13;
    }
    std::cout << "Wrote " << frames.size() << " frame(s) to " << path_to_utf8(options.report)
              << '\n';
    return 0;
}

} // namespace

int main(int count, char** values) {
    const std::optional<Options> options = parse_options(count, values);
    if (!options.has_value()) {
        print_usage();
        return 2;
    }
    try {
        return run(*options);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 14;
    }
}
