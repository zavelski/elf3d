#include <elf3d/embed/runtime.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
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

constexpr std::string_view procedural_material_model = "procedural-material";

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

struct OptionPresence final {
    bool model = false;
    bool extent = false;
    bool scenario = false;
    bool warmup = false;
    bool frames = false;
    bool report = false;
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

struct ReportEnvironment final {
    const char* vendor = "unavailable";
    const char* renderer = "unavailable";
    const char* version = "unavailable";
    const char* shading_language = "unavailable";
    ContextDiagnostics context;
};

struct FrameReport final {
    const Options& options;
    double load_milliseconds = 0.0;
    std::size_t index = 0;
    const FrameResult& frame;
    const ReportEnvironment& environment;
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

[[nodiscard]] const char* graphics_string(GLenum name) noexcept {
    const auto* value = glGetString(name);
    return value == nullptr ? "unavailable" : reinterpret_cast<const char*>(value);
}

[[nodiscard]] ReportEnvironment capture_report_environment() noexcept {
    return ReportEnvironment{
        graphics_string(GL_VENDOR), graphics_string(GL_RENDERER), graphics_string(GL_VERSION),
        graphics_string(GL_SHADING_LANGUAGE_VERSION), capture_context_diagnostics()};
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

elf3d::EmbeddedGraphicsProcedure load_opengl_procedure(const char* name) noexcept {
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

template <typename Value>
[[nodiscard]] bool set_once(Value& destination, bool& present, Value value) {
    if (present) {
        return false;
    }
    destination = std::move(value);
    present = true;
    return true;
}

[[nodiscard]] bool parse_path_option(std::string_view option, std::string_view value,
                                     Options& options, OptionPresence& presence) {
    if (option == "--model") {
        return set_once(options.model, presence.model, path_from_utf8(value));
    }
    if (option == "--report") {
        return set_once(options.report, presence.report, path_from_utf8(value));
    }
    return false;
}

[[nodiscard]] bool parse_frame_option(std::string_view option, std::string_view value,
                                      Options& options, OptionPresence& presence) {
    const std::optional<std::uint32_t> frames = unsigned_value(value);
    if (!frames.has_value()) {
        return false;
    }
    if (option == "--warmup") {
        return set_once(options.warmup_frames, presence.warmup, *frames);
    }
    if (option == "--frames" && *frames != 0) {
        return set_once(options.measured_frames, presence.frames, *frames);
    }
    return false;
}

[[nodiscard]] bool parse_value_option(std::string_view option, std::string_view value,
                                      Options& options, OptionPresence& presence) {
    if (option == "--extent") {
        const std::optional<elf3d::Extent2D> extent = extent_value(value);
        return extent.has_value() && set_once(options.extent, presence.extent, *extent);
    }
    if (option == "--scenario") {
        const std::optional<Scenario> scenario = scenario_value(value);
        if (!scenario.has_value() || presence.scenario) {
            return false;
        }
        options.scenario = *scenario;
        options.scenario_name = value;
        presence.scenario = true;
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<Options> parse_options(int count, char** values) {
    Options options;
    OptionPresence presence;
    for (int index = 1; index < count; index += 2) {
        if (index + 1 >= count) {
            return std::nullopt;
        }
        ELF3D_ASSERT(values[index] != nullptr);
        ELF3D_ASSERT(values[index + 1] != nullptr);
        const std::string_view option{values[index]};
        const std::string_view value{values[index + 1]};
        if (!parse_path_option(option, value, options, presence) &&
            !parse_frame_option(option, value, options, presence) &&
            !parse_value_option(option, value, options, presence)) {
            return std::nullopt;
        }
    }
    const std::array required{presence.model,  presence.extent, presence.scenario,
                              presence.warmup, presence.frames, presence.report};
    if (!std::all_of(required.begin(), required.end(), [](bool value) { return value; })) {
        return std::nullopt;
    }
    if (options.scenario == Scenario::first_frame && options.measured_frames != 1) {
        return std::nullopt;
    }
    return options;
}

void print_usage() {
    std::cerr << "Usage: elf3d_render_benchmark --model <path|procedural-material> "
                 "--extent <width>x<height> "
                 "--scenario first-frame|steady|orbit|pan|wheel|orbit-anchor|pick "
                 "--warmup <frames> --frames <frames> --report <csv-path>\n";
}

[[nodiscard]] elf3d::NavigationInput scripted_input(Scenario scenario, std::uint32_t frame,
                                                    elf3d::Extent2D extent) noexcept {
    elf3d::NavigationInput input;
    input.pointer_position_pixels = {static_cast<float>(extent.width) * 0.5F,
                                     static_cast<float>(extent.height) * 0.5F};
    input.pointer_hovered = true;
    input.region_focused = true;
    input.frame_delta_seconds = 1.0F / 60.0F;
    if (scenario == Scenario::orbit || scenario == Scenario::orbit_anchor) {
        input.orbit_down = true;
        input.pointer_delta_pixels = frame == 0 ? elf3d::Float2{} : elf3d::Float2{2.0F, -1.0F};
    } else if (scenario == Scenario::pan) {
        input.pan_down = true;
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
        elf3d::NavigationInput pressed = scripted_input(options.scenario, 0, options.extent);
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

void write_report_header(std::ofstream& stream) {
    stream << "scenario,frame,load_ms,frame_ms,draw_calls,triangles,candidate_primitives,"
              "visible_primitives,culled_primitives,buffer_uploads,buffer_uploaded_bytes,"
              "draw_packet_rebuilds,"
              "resident_geometry_bytes,resident_texture_bytes,resident_environment_bytes,"
              "environment_preparations,cpu_list_ms,cpu_resources_ms,"
              "cpu_gl_ms,cpu_render_total_ms,gpu_main_available,gpu_main_ms,"
              "gpu_resolve_available,gpu_resolve_ms,pick_draw_calls,"
              "pick_pixels_read,pick_target_allocations,pick_pass_ms,pick_readback_ms,"
              "pick_allocation_ms,pick_cpu_ms,pick_gpu_available,pick_gpu_ms,gl_vendor,"
              "gl_renderer,gl_version,glsl_version,window_width,window_height,framebuffer_width,"
              "framebuffer_height,target_width,target_height,vsync,context_flags,profile_mask,"
              "red_bits,green_bits,blue_bits,alpha_bits,depth_bits,stencil_bits,samples,"
              "framebuffer_srgb_enabled,max_texture_size\n";
}

void write_frame_report(std::ofstream& stream, const FrameReport& report) {
    const ContextDiagnostics& context = report.environment.context;
    const FrameResult& frame = report.frame;
    stream << report.options.scenario_name << ',' << report.index << ',' << report.load_milliseconds
           << ',' << frame.milliseconds << ',' << frame.render.draw_calls << ','
           << frame.render.triangles << ',' << frame.render.candidate_primitives << ','
           << frame.render.visible_primitives << ',' << frame.render.frustum_culled_primitives
           << ',' << frame.render.gpu_buffer_uploads << ','
           << frame.render.gpu_buffer_uploaded_bytes << ',' << frame.render.draw_packet_rebuilds
           << ',' << frame.render.estimated_resident_geometry_bytes << ','
           << frame.render.estimated_resident_texture_bytes << ','
           << frame.render.estimated_resident_environment_bytes << ','
           << frame.render.environment_preparations << ','
           << frame.render.cpu_render_list_milliseconds << ','
           << frame.render.cpu_resource_preparation_milliseconds << ','
           << frame.render.cpu_gl_submission_milliseconds << ','
           << frame.render.cpu_total_milliseconds << ','
           << (frame.render.gpu_main_pass_timing_available ? 1 : 0) << ','
           << frame.render.gpu_main_pass_milliseconds << ','
           << (frame.render.gpu_resolve_timing_available ? 1 : 0) << ','
           << frame.render.gpu_resolve_milliseconds << ',' << frame.picking.latest_gpu_draw_calls
           << ',' << frame.picking.latest_gpu_pixels_read << ','
           << frame.picking.latest_target_allocations << ','
           << frame.picking.latest_pass_milliseconds << ','
           << frame.picking.latest_readback_milliseconds << ','
           << frame.picking.latest_allocation_milliseconds << ','
           << frame.picking.latest_cpu_milliseconds << ','
           << (frame.picking.latest_gpu_timing_available ? 1 : 0) << ','
           << frame.picking.latest_gpu_milliseconds << ',' << '"' << report.environment.vendor
           << "\",\"" << report.environment.renderer << "\",\"" << report.environment.version
           << "\",\"" << report.environment.shading_language << "\"," << context.window_width << ','
           << context.window_height << ',' << context.framebuffer_width << ','
           << context.framebuffer_height << ',' << report.options.extent.width << ','
           << report.options.extent.height << ",0," << context.context_flags << ','
           << context.profile_mask << ',' << context.red_bits << ',' << context.green_bits << ','
           << context.blue_bits << ',' << context.alpha_bits << ',' << context.depth_bits << ','
           << context.stencil_bits << ',' << context.samples << ','
           << (context.framebuffer_srgb_enabled ? 1 : 0) << ',' << context.maximum_texture_size
           << '\n';
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
    write_report_header(stream);
    stream << std::fixed << std::setprecision(6);
    const ReportEnvironment environment = capture_report_environment();
    for (std::size_t index = 0; index < frames.size(); ++index) {
        write_frame_report(
            stream, FrameReport{options, load_milliseconds, index, frames[index], environment});
    }
    return static_cast<bool>(stream);
}

struct BenchmarkScene final {
    std::unique_ptr<elf3d::EmbeddedRuntime> runtime;
    std::unique_ptr<elf3d::Scene> scene;
    std::unique_ptr<elf3d::Viewport> viewport;
    elf3d::EntityId camera;
    double load_milliseconds = 0.0;
};

struct SphereMesh final {
    std::vector<elf3d::VertexPositionNormal> vertices;
    std::vector<std::uint32_t> indices;
};

[[nodiscard]] SphereMesh make_sphere() {
    constexpr std::uint32_t longitude_count = 48;
    constexpr std::uint32_t latitude_count = 24;
    constexpr float pi = 3.14159265359F;
    SphereMesh mesh;
    constexpr std::uint32_t longitude_vertex_count = longitude_count + 1U;
    constexpr std::uint32_t latitude_vertex_count = latitude_count + 1U;
    mesh.vertices.reserve(longitude_vertex_count * latitude_vertex_count);
    for (std::uint32_t latitude = 0; latitude <= latitude_count; ++latitude) {
        const float polar = pi * static_cast<float>(latitude) / static_cast<float>(latitude_count);
        const float y = std::cos(polar);
        const float radius = std::sin(polar);
        for (std::uint32_t longitude = 0; longitude <= longitude_count; ++longitude) {
            const float azimuth =
                2.0F * pi * static_cast<float>(longitude) / static_cast<float>(longitude_count);
            const elf3d::Float3 normal{radius * std::cos(azimuth), y, radius * std::sin(azimuth)};
            mesh.vertices.push_back({normal, normal});
        }
    }
    for (std::uint32_t latitude = 0; latitude < latitude_count; ++latitude) {
        for (std::uint32_t longitude = 0; longitude < longitude_count; ++longitude) {
            const std::uint32_t row = longitude_count + 1;
            const std::uint32_t top_left = latitude * row + longitude;
            const std::uint32_t bottom_left = top_left + row;
            mesh.indices.insert(mesh.indices.end(), {top_left, bottom_left, top_left + 1,
                                                     top_left + 1, bottom_left, bottom_left + 1});
        }
    }
    return mesh;
}

[[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::Scene>>
create_material_scene(elf3d::Engine& engine, elf3d::EntityId& camera) {
    auto scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();
    const SphereMesh sphere = make_sphere();
    const auto mesh = scene->create_mesh({sphere.vertices, sphere.indices});
    if (!mesh) {
        return mesh.error();
    }
    constexpr std::array<float, 4> x_positions{{-3.0F, -1.0F, 1.0F, 3.0F}};
    constexpr std::array<float, 4> metallic{{0.0F, 0.0F, 1.0F, 1.0F}};
    constexpr std::array<float, 4> roughness{{0.8F, 0.25F, 0.08F, 0.65F}};
    constexpr std::array<elf3d::Color4, 4> colors{{
        {1.0F, 1.0F, 1.0F, 1.0F},
        {0.55F, 0.55F, 0.55F, 1.0F},
        {0.85F, 0.58F, 0.22F, 1.0F},
        {0.72F, 0.76F, 0.8F, 1.0F},
    }};
    for (std::size_t index = 0; index < x_positions.size(); ++index) {
        elf3d::MaterialDescription description;
        description.base_color = colors[index];
        description.metallic_factor = metallic[index];
        description.roughness_factor = roughness[index];
        const auto material = scene->create_material(description);
        if (!material) {
            return material.error();
        }
        const auto model = scene->create_model_entity(mesh.value(), material.value());
        if (!model) {
            return model.error();
        }
        elf3d::Transform transform;
        transform.translation = {x_positions[index], 0.0F, 0.0F};
        const auto positioned = scene->set_local_transform(model.value(), transform);
        if (!positioned) {
            return positioned.error();
        }
    }
    const auto camera_result = scene->create_perspective_camera_entity({});
    if (!camera_result) {
        return camera_result.error();
    }
    camera = camera_result.value();
    return scene;
}

[[nodiscard]] int initialize_graphics(const Options& options, GlfwRuntime& glfw,
                                      std::unique_ptr<Window>& window) {
    if (!glfw.initialize()) {
        std::cerr << "GLFW initialization failed\n";
        return 4;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = std::make_unique<Window>(glfwCreateWindow(static_cast<int>(options.extent.width),
                                                       static_cast<int>(options.extent.height),
                                                       "Elf3D render benchmark", nullptr, nullptr));
    if (window->get() == nullptr) {
        std::cerr << "Hidden OpenGL 4.1 context creation failed\n";
        return 5;
    }
    glfwMakeContextCurrent(window->get());
    glfwSwapInterval(0);
    if (gladLoadGL(load_opengl_procedure) == 0 || GLAD_GL_VERSION_4_1 == 0) {
        std::cerr << "OpenGL 4.1 is unavailable\n";
        return 6;
    }
    return 0;
}

[[nodiscard]] elf3d::Result<BenchmarkScene> create_benchmark_scene(const Options& options) {
    const elf3d::EmbeddedRuntimeOptions runtime_options{load_opengl_procedure};
    elf3d::Result<std::unique_ptr<elf3d::EmbeddedRuntime>> runtime_result =
        elf3d::EmbeddedRuntime::create(runtime_options);
    if (!runtime_result) {
        return runtime_result.error();
    }
    BenchmarkScene benchmark;
    benchmark.runtime = std::move(runtime_result).value();
    elf3d::Engine& engine = benchmark.runtime->engine();
    const auto load_begin = std::chrono::steady_clock::now();
    if (path_to_utf8(options.model) == procedural_material_model) {
        auto scene_result = create_material_scene(engine, benchmark.camera);
        if (!scene_result) {
            return scene_result.error();
        }
        benchmark.scene = std::move(scene_result).value();
    } else {
        elf3d::Result<elf3d::LoadedScene> loaded_result =
            engine.load_scene(path_to_utf8(options.model));
        if (!loaded_result) {
            return loaded_result.error();
        }
        benchmark.scene = std::move(loaded_result).value().scene;
        const elf3d::Result<elf3d::EntityId> camera =
            benchmark.scene->create_perspective_camera_entity({});
        if (!camera) {
            return camera.error();
        }
        benchmark.camera = camera.value();
    }
    const auto load_end = std::chrono::steady_clock::now();
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine.create_viewport(options.extent);
    if (!viewport_result) {
        return viewport_result.error();
    }
    benchmark.viewport = std::move(viewport_result).value();
    const elf3d::Result<void> framed =
        benchmark.viewport->reset_view(*benchmark.scene, benchmark.camera);
    if (!framed) {
        return framed.error();
    }
    benchmark.load_milliseconds =
        std::chrono::duration<double, std::milli>(load_end - load_begin).count();
    return benchmark;
}

[[nodiscard]] elf3d::Result<void> warm_up(const Options& options, BenchmarkScene& benchmark,
                                          GLFWwindow& window) {
    for (std::uint32_t frame = 0; frame < options.warmup_frames; ++frame) {
        const bool event_scenario =
            options.scenario == Scenario::orbit_anchor || options.scenario == Scenario::pick;
        const elf3d::Result<void> result =
            event_scenario ? benchmark.viewport->render(*benchmark.scene, benchmark.camera)
                           : execute_frame(options, frame, *benchmark.viewport, *benchmark.scene,
                                           benchmark.camera);
        if (!result) {
            return result.error();
        }
        const elf3d::Result<elf3d::NativeTextureView> resolved =
            benchmark.runtime->native_texture_view(benchmark.viewport->color_texture());
        if (!resolved) {
            return resolved.error();
        }
        glfwSwapBuffers(&window);
    }
    return {};
}

[[nodiscard]] elf3d::Result<std::vector<FrameResult>>
measure(const Options& options, BenchmarkScene& benchmark, GLFWwindow& window) {
    std::vector<FrameResult> frames;
    frames.reserve(options.measured_frames);
    for (std::uint32_t frame = 0; frame < options.measured_frames; ++frame) {
        const auto begin = std::chrono::steady_clock::now();
        const elf3d::Result<void> result =
            execute_frame(options, options.warmup_frames + frame, *benchmark.viewport,
                          *benchmark.scene, benchmark.camera);
        if (!result) {
            return result.error();
        }
        const elf3d::Result<elf3d::NativeTextureView> resolved =
            benchmark.runtime->native_texture_view(benchmark.viewport->color_texture());
        glfwSwapBuffers(&window);
        const auto end = std::chrono::steady_clock::now();
        if (!resolved) {
            return resolved.error();
        }
        elf3d::PickingStatistics picking;
        const elf3d::Result<elf3d::PickingStatistics> picking_result =
            benchmark.viewport->picking_statistics();
        if (picking_result) {
            picking = picking_result.value();
        }
        frames.push_back(FrameResult{std::chrono::duration<double, std::milli>(end - begin).count(),
                                     benchmark.viewport->render_statistics(), picking});
    }
    return frames;
}

[[nodiscard]] int run(const Options& options) {
    if (path_to_utf8(options.model) != procedural_material_model &&
        !std::filesystem::is_regular_file(options.model)) {
        std::cerr << "Model does not exist: " << path_to_utf8(options.model) << '\n';
        return 3;
    }
    GlfwRuntime glfw;
    std::unique_ptr<Window> window;
    const int graphics_status = initialize_graphics(options, glfw, window);
    if (graphics_status != 0) {
        return graphics_status;
    }
    elf3d::Result<BenchmarkScene> benchmark_result = create_benchmark_scene(options);
    if (!benchmark_result) {
        std::cerr << benchmark_result.error().message() << '\n';
        return 7;
    }
    BenchmarkScene benchmark = std::move(benchmark_result).value();
    const elf3d::Result<void> warmed = warm_up(options, benchmark, *window->get());
    if (!warmed) {
        std::cerr << warmed.error().message() << '\n';
        return 11;
    }
    elf3d::Result<std::vector<FrameResult>> frames_result =
        measure(options, benchmark, *window->get());
    if (!frames_result) {
        std::cerr << frames_result.error().message() << '\n';
        return 12;
    }
    const std::vector<FrameResult>& frames = frames_result.value();
    if (!write_report(options, benchmark.load_milliseconds, frames)) {
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
