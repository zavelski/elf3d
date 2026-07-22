#include <elf3d/elf3d.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr int skipped = 77;

class GlfwRuntime final {
  public:
    GlfwRuntime() = default;
    ~GlfwRuntime() {
        if (initialized_) {
            glfwTerminate();
        }
    }

    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;

    [[nodiscard]] bool initialize() noexcept {
        initialized_ = glfwInit() == GLFW_TRUE;
        return initialized_;
    }

  private:
    bool initialized_ = false;
};

class Window final {
  public:
    explicit Window(GLFWwindow* window) noexcept : window_(window) {}
    ~Window() {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
        }
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    [[nodiscard]] GLFWwindow* get() const noexcept {
        return window_;
    }

  private:
    GLFWwindow* window_ = nullptr;
};

elf3d::GraphicsProcedure load_opengl_procedure(const char* name) noexcept {
    return glfwGetProcAddress(name);
}

[[nodiscard]] bool in_range(std::uint8_t value, std::uint8_t minimum,
                            std::uint8_t maximum) noexcept {
    return value >= minimum && value <= maximum;
}

[[nodiscard]] int fail(int code, const char* message) {
    std::cerr << message << '\n';
    return code;
}

void configure_hidden_context() noexcept {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
}

[[nodiscard]] bool is_skippable_graphics_error(elf3d::ErrorCode code) noexcept {
    return code == elf3d::ErrorCode::unsupported_graphics_version ||
           code == elf3d::ErrorCode::graphics_context_unavailable ||
           code == elf3d::ErrorCode::graphics_initialization_failed;
}

struct SmokeFixture {
    std::unique_ptr<elf3d::Scene> scene;
    std::unique_ptr<elf3d::Viewport> viewport;
    elf3d::EntityId non_camera_entity;
    elf3d::EntityId camera;
};

struct SmokeAssets {
    elf3d::MeshHandle mesh;
    elf3d::MaterialHandle red;
    elf3d::MaterialHandle green;
};

struct ForeignGlState {
    GLint draw_framebuffer = 0;
    GLint read_framebuffer = 0;
    std::array<GLint, 4> viewport{};
    GLint program = 0;
    GLint vertex_array = 0;
    GLint active_texture = 0;
    GLint texture = 0;
    GLboolean blend = GL_FALSE;
    GLboolean depth_test = GL_FALSE;
    GLboolean cull_face = GL_FALSE;
    std::array<GLboolean, 4> color_mask{};
    GLboolean depth_mask = GL_FALSE;

    bool operator==(const ForeignGlState&) const = default;
};

class ForeignGlObjects final {
  public:
    ~ForeignGlObjects() {
        glUseProgram(0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &texture);
        glDeleteVertexArrays(1, &vertex_array);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteProgram(program);
    }

    ForeignGlObjects(const ForeignGlObjects&) = delete;
    ForeignGlObjects& operator=(const ForeignGlObjects&) = delete;

    GLuint framebuffer = 0;
    GLuint vertex_array = 0;
    GLuint texture = 0;
    GLuint program = 0;

    ForeignGlObjects() = default;
};

[[nodiscard]] GLuint compile_foreign_shader(GLenum type, const char* source) noexcept {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint create_foreign_program() noexcept {
    constexpr const char* vertex_source =
        "#version 410 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);}";
    constexpr const char* fragment_source =
        "#version 410 core\nout vec4 color;void main(){color=vec4(1.0);}";
    const GLuint vertex = compile_foreign_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = compile_foreign_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex == 0 || fragment == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

[[nodiscard]] ForeignGlState capture_foreign_state() noexcept {
    ForeignGlState state;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state.draw_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state.read_framebuffer);
    glGetIntegerv(GL_VIEWPORT, state.viewport.data());
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertex_array);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.active_texture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture);
    state.blend = glIsEnabled(GL_BLEND);
    state.depth_test = glIsEnabled(GL_DEPTH_TEST);
    state.cull_face = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_COLOR_WRITEMASK, state.color_mask.data());
    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depth_mask);
    return state;
}

[[nodiscard]] bool configure_foreign_state(ForeignGlObjects& objects) noexcept {
    glGenFramebuffers(1, &objects.framebuffer);
    glGenVertexArrays(1, &objects.vertex_array);
    glGenTextures(1, &objects.texture);
    objects.program = create_foreign_program();
    if (objects.framebuffer == 0 || objects.vertex_array == 0 || objects.texture == 0 ||
        objects.program == 0) {
        return false;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, objects.framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, objects.framebuffer);
    glViewport(3, 4, 17, 19);
    glUseProgram(objects.program);
    glBindVertexArray(objects.vertex_array);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, objects.texture);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glDepthMask(GL_FALSE);
    return glGetError() == GL_NO_ERROR;
}

[[nodiscard]] int create_public_objects(elf3d::Engine& engine, SmokeFixture& fixture) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine.create_viewport({64, 64});
    if (!scene_result || !viewport_result) {
        return fail(3, "OpenGL smoke test failed to create public scene or viewport");
    }
    fixture.scene = std::move(scene_result).value();
    fixture.viewport = std::move(viewport_result).value();
    return 0;
}

[[nodiscard]] int create_scene_assets(elf3d::Scene& scene, SmokeAssets& assets) {
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-2.0F, -2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{2.0F, -2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = scene.create_mesh({vertices, indices});

    elf3d::MaterialDescription red_material;
    red_material.base_color = {1.0F, 0.0F, 0.0F, 0.5F};
    red_material.alpha_mode = elf3d::AlphaMode::blend;
    red_material.unlit = true;
    const auto red = scene.create_material(red_material);

    elf3d::MaterialDescription green_material;
    green_material.base_color = {0.0F, 1.0F, 0.0F, 0.5F};
    green_material.alpha_mode = elf3d::AlphaMode::blend;
    green_material.unlit = true;
    const auto green = scene.create_material(green_material);
    if (!mesh || !red || !green) {
        return fail(4, "OpenGL smoke test failed to create scene assets");
    }
    assets = SmokeAssets{mesh.value(), red.value(), green.value()};
    return 0;
}

[[nodiscard]] int create_scene_entities(SmokeFixture& fixture, const SmokeAssets& assets) {
    const auto far_model = fixture.scene->create_model_entity(assets.mesh, assets.red);
    const auto near_model = fixture.scene->create_model_entity(assets.mesh, assets.green);
    const auto camera = fixture.scene->create_perspective_camera_entity({});
    if (!far_model || !near_model || !camera) {
        return fail(4, "OpenGL smoke test failed to create scene entities");
    }
    fixture.non_camera_entity = far_model.value();
    fixture.camera = camera.value();
    elf3d::Transform far_transform;
    far_transform.translation = {0.0F, 0.0F, -3.0F};
    elf3d::Transform near_transform;
    near_transform.translation = {0.0F, 0.0F, -2.0F};
    if (!fixture.scene->set_local_transform(far_model.value(), far_transform) ||
        !fixture.scene->set_local_transform(near_model.value(), near_transform)) {
        return fail(5, "OpenGL smoke test failed to position transparent models");
    }
    return 0;
}

[[nodiscard]] int verify_camera_role_errors(SmokeFixture& fixture) {
    const elf3d::Result<void> navigation = fixture.viewport->update_navigation(
        *fixture.scene, fixture.non_camera_entity, elf3d::ViewportInput{});
    if (navigation || navigation.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(20, "Viewport navigation accepted a non-camera entity");
    }

    const elf3d::Result<elf3d::Ray3> picking_ray = fixture.viewport->make_picking_ray(
        *fixture.scene, fixture.non_camera_entity, {32.0F, 32.0F});
    if (picking_ray || picking_ray.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(21, "Viewport picking accepted a non-camera entity");
    }

    const elf3d::Result<elf3d::ProjectedViewportPoint> projection =
        fixture.viewport->project_world_to_viewport(*fixture.scene, fixture.non_camera_entity, {});
    if (projection || projection.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(22, "Viewport projection accepted a non-camera entity");
    }

    const elf3d::Result<void> rendered =
        fixture.viewport->render(*fixture.scene, fixture.non_camera_entity);
    if (rendered || rendered.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(23, "Viewport rendering accepted a non-camera entity");
    }
    return 0;
}

[[nodiscard]] int render_scene(SmokeFixture& fixture) {
    fixture.viewport->set_clear_color({0.0F, 0.0F, 0.0F, 1.0F});
    const elf3d::Result<void> render_result =
        fixture.viewport->render(*fixture.scene, fixture.camera);
    if (!render_result || fixture.viewport->render_statistics().draw_calls != 2) {
        return fail(6, "OpenGL smoke test failed to render the transparent scene");
    }
    return 0;
}

[[nodiscard]] int verify_foreign_state_preserved(elf3d::Engine& engine, SmokeFixture& fixture) {
    ForeignGlObjects objects;
    if (!configure_foreign_state(objects)) {
        return fail(24, "OpenGL smoke test failed to configure foreign host state");
    }
    const ForeignGlState expected = capture_foreign_state();

    const elf3d::Result<void> render = fixture.viewport->render(*fixture.scene, fixture.camera);
    if (!render || capture_foreign_state() != expected) {
        return fail(25, "Viewport rendering did not preserve foreign OpenGL state");
    }

    const elf3d::Result<std::optional<elf3d::PickHit>> pick =
        fixture.viewport->pick(*fixture.scene, fixture.camera, {32.0F, 32.0F});
    if (!pick || capture_foreign_state() != expected) {
        return fail(26, "Viewport picking did not preserve foreign OpenGL state");
    }

    const elf3d::Result<elf3d::NativeTextureView> texture =
        engine.native_texture_view(fixture.viewport->color_texture());
    if (!texture || capture_foreign_state() != expected) {
        return fail(27, "Viewport display resolve did not preserve foreign OpenGL state");
    }
    return 0;
}

[[nodiscard]] int verify_rendered_pixel(elf3d::Engine& engine, const SmokeFixture& fixture) {
    const elf3d::Result<elf3d::NativeTextureView> texture_result =
        engine.native_texture_view(fixture.viewport->color_texture());
    if (!texture_result) {
        std::cerr << texture_result.error().message() << '\n';
        return 7;
    }
    if (texture_result.value().extent != elf3d::Extent2D{64U, 64U}) {
        return fail(8, "OpenGL smoke test returned an unexpected texture extent");
    }
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_result.value().value));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        return fail(9, "OpenGL smoke test failed to read the rendered texture");
    }
    const std::size_t center = ((32U * 64U) + 32U) * 4U;
    const std::uint8_t red_channel = pixels[center];
    const std::uint8_t green_channel = pixels[center + 1U];
    const std::uint8_t blue_channel = pixels[center + 2U];
    const std::uint8_t alpha_channel = pixels[center + 3U];
    if (!in_range(red_channel, 130U, 145U) || !in_range(green_channel, 180U, 196U) ||
        blue_channel > 8U || alpha_channel < 250U) {
        std::cerr << "Unexpected linear-blend sRGB pixel: R=" << static_cast<unsigned>(red_channel)
                  << " G=" << static_cast<unsigned>(green_channel)
                  << " B=" << static_cast<unsigned>(blue_channel)
                  << " A=" << static_cast<unsigned>(alpha_channel) << '\n';
        return 10;
    }
    return 0;
}

[[nodiscard]] bool has_render_gpu_timings(const elf3d::RenderStatistics& statistics) noexcept {
    return statistics.gpu_main_pass_timing_available && statistics.gpu_resolve_timing_available &&
           statistics.gpu_main_pass_milliseconds >= 0.0 &&
           statistics.gpu_resolve_milliseconds >= 0.0;
}

[[nodiscard]] int verify_delayed_render_gpu_timings(elf3d::Engine& engine, SmokeFixture& fixture) {
    bool render_timings_available = false;
    for (int attempt = 0; attempt < 16 && !render_timings_available; ++attempt) {
        if (!fixture.viewport->render(*fixture.scene, fixture.camera) ||
            !engine.native_texture_view(fixture.viewport->color_texture())) {
            return fail(28, "GPU timing test could not render and resolve a frame");
        }
        glfwSwapBuffers(glfwGetCurrentContext());
        const elf3d::RenderStatistics statistics = fixture.viewport->render_statistics();
        render_timings_available = has_render_gpu_timings(statistics);
    }
    if (!render_timings_available) {
        return fail(29, "Nonblocking GPU render timings did not become available");
    }

    return 0;
}

[[nodiscard]] int verify_delayed_picking_gpu_timing(SmokeFixture& fixture) {
    bool timing_available = false;
    for (int attempt = 0; attempt < 8 && !timing_available; ++attempt) {
        if (!fixture.viewport->pick(*fixture.scene, fixture.camera, {32.0F, 32.0F})) {
            return fail(30, "GPU timing test could not perform a pick");
        }
        const elf3d::Result<elf3d::PickingStatistics> statistics =
            fixture.viewport->picking_statistics();
        timing_available = statistics && statistics.value().latest_gpu_timing_available &&
                           statistics.value().latest_gpu_milliseconds >= 0.0;
    }
    if (!timing_available) {
        return fail(31, "Nonblocking GPU picking timing did not become available");
    }
    return 0;
}

[[nodiscard]] int verify_delayed_gpu_timings(elf3d::Engine& engine, SmokeFixture& fixture) {
    const int render = verify_delayed_render_gpu_timings(engine, fixture);
    return render != 0 ? render : verify_delayed_picking_gpu_timing(fixture);
}

[[nodiscard]] int verify_foreign_timer_query_preserved(SmokeFixture& fixture) {
    GLuint query = 0;
    glGenQueries(1, &query);
    glBeginQuery(GL_TIME_ELAPSED, query);
    const elf3d::Result<void> render = fixture.viewport->render(*fixture.scene, fixture.camera);
    GLint current_query = 0;
    glGetQueryiv(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &current_query);
    glEndQuery(GL_TIME_ELAPSED);
    glDeleteQueries(1, &query);
    return render && current_query == static_cast<GLint>(query)
               ? 0
               : fail(32, "Viewport rendering disturbed a foreign timer query");
}

[[nodiscard]] int run_render_smoke(elf3d::Engine& engine) {
    SmokeFixture fixture;
    const int objects = create_public_objects(engine, fixture);
    if (objects != 0) {
        return objects;
    }
    SmokeAssets assets;
    const int asset_status = create_scene_assets(*fixture.scene, assets);
    if (asset_status != 0) {
        return asset_status;
    }
    const int entities = create_scene_entities(fixture, assets);
    if (entities != 0) {
        return entities;
    }
    const int camera_roles = verify_camera_role_errors(fixture);
    if (camera_roles != 0) {
        return camera_roles;
    }
    const int rendered = render_scene(fixture);
    if (rendered != 0) {
        return rendered;
    }
    const int state_preservation = verify_foreign_state_preserved(engine, fixture);
    if (state_preservation != 0) {
        return state_preservation;
    }
    const int timings = verify_delayed_gpu_timings(engine, fixture);
    if (timings != 0) {
        return timings;
    }
    const int foreign_timer = verify_foreign_timer_query_preserved(fixture);
    if (foreign_timer != 0) {
        return foreign_timer;
    }
    return verify_rendered_pixel(engine, fixture);
}

[[nodiscard]] int run_render_smoke_with_engine() {
    elf3d::EngineConfiguration configuration;
    configuration.opengl.load_procedure = load_opengl_procedure;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(configuration);
    if (!engine_result) {
        if (is_skippable_graphics_error(engine_result.error().code())) {
            std::cout << "Skipping OpenGL render smoke test: " << engine_result.error().message()
                      << '\n';
            return skipped;
        }
        std::cerr << engine_result.error().message() << '\n';
        return 2;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();
    return run_render_smoke(*engine);
}

} // namespace

int main() {
    GlfwRuntime glfw;
    if (!glfw.initialize()) {
        std::cout << "Skipping OpenGL render smoke test: GLFW initialization failed\n";
        return skipped;
    }

    configure_hidden_context();

    Window window{glfwCreateWindow(64, 64, "Elf3D OpenGL smoke", nullptr, nullptr)};
    if (window.get() == nullptr) {
        std::cout << "Skipping OpenGL render smoke test: hidden context creation failed\n";
        return skipped;
    }
    glfwMakeContextCurrent(window.get());
    if (glfwGetCurrentContext() != window.get()) {
        return fail(1, "GLFW did not make the smoke-test context current");
    }
    glfwSwapInterval(0);

    const int loaded_version = gladLoadGL(load_opengl_procedure);
    if (loaded_version == 0 || GLAD_GL_VERSION_4_1 == 0) {
        std::cout << "Skipping OpenGL render smoke test: OpenGL 4.1 is unavailable\n";
        return skipped;
    }
    return run_render_smoke_with_engine();
}
