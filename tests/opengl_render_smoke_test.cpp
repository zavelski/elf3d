#include <elf3d/elf3d.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
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
    elf3d::EntityId camera;
};

struct SmokeAssets {
    elf3d::MeshHandle mesh;
    elf3d::MaterialHandle red;
    elf3d::MaterialHandle green;
};

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
    const auto far_model = fixture.scene->create_model(assets.mesh, assets.red);
    const auto near_model = fixture.scene->create_model(assets.mesh, assets.green);
    const auto camera = fixture.scene->create_perspective_camera({});
    if (!far_model || !near_model || !camera) {
        return fail(4, "OpenGL smoke test failed to create scene entities");
    }
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

[[nodiscard]] int render_scene(SmokeFixture& fixture) {
    fixture.viewport->set_clear_color({0.0F, 0.0F, 0.0F, 1.0F});
    const elf3d::Result<void> render_result =
        fixture.viewport->render(*fixture.scene, fixture.camera);
    if (!render_result || fixture.viewport->statistics().draw_calls != 2) {
        return fail(6, "OpenGL smoke test failed to render the transparent scene");
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
    const int rendered = render_scene(fixture);
    if (rendered != 0) {
        return rendered;
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

    const int loaded_version = gladLoadGL(load_opengl_procedure);
    if (loaded_version == 0 || GLAD_GL_VERSION_4_1 == 0) {
        std::cout << "Skipping OpenGL render smoke test: OpenGL 4.1 is unavailable\n";
        return skipped;
    }
    return run_render_smoke_with_engine();
}
