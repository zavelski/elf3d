#include <elf3d/elf3d.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
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

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
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

[[nodiscard]] int verify_model_import(const elf3d::LoadedScene& loaded) {
    const elf3d::SceneStatistics expected_statistics{1, 1, 2, 2, 2, 8, 12, 4, 1, 2, 2, 16, 2, 1};
    if (loaded.report.diagnostic_count() != 0 ||
        loaded.scene->statistics() != expected_statistics) {
        return fail(4, "Embedded smoke model produced unexpected import facts");
    }
    return 0;
}

[[nodiscard]] int verify_render_statistics(elf3d::Viewport& viewport, elf3d::Scene& scene,
                                           elf3d::EntityId camera) {
    const elf3d::Result<void> render_result = viewport.render(scene, camera);
    const elf3d::RenderStatistics statistics = viewport.statistics();
    if (!render_result || statistics.draw_calls != 2 || statistics.triangles != 4 ||
        statistics.vertices != 8 || statistics.indices != 12 || statistics.texture_bindings != 3 ||
        statistics.gpu_texture_uploads != 3 || statistics.unique_gpu_textures != 3) {
        return fail(7, "Embedded-model quick test produced unexpected render statistics");
    }
    return 0;
}

[[nodiscard]] int verify_rendered_pixels(elf3d::Engine& engine, elf3d::Viewport& viewport) {
    const elf3d::Result<elf3d::NativeTextureView> texture_result =
        engine.native_texture_view(viewport.color_texture());
    if (!texture_result || texture_result.value().extent != elf3d::Extent2D{64U, 64U}) {
        return fail(8, "Embedded-model quick test returned an unexpected texture view");
    }
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_result.value().value));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (glGetError() != GL_NO_ERROR) {
        return fail(9, "Embedded-model quick test failed to read the rendered texture");
    }
    std::size_t colored_pixels = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4U) {
        if (pixels[pixel] > 8U || pixels[pixel + 1U] > 8U || pixels[pixel + 2U] > 8U) {
            ++colored_pixels;
        }
    }
    if (colored_pixels < 256U) {
        return fail(10, "Embedded smoke model did not produce enough visible pixels");
    }
    return 0;
}

[[nodiscard]] int run_model_quick(elf3d::Engine& engine) {
    const std::filesystem::path model_path = std::filesystem::path{ELF3D_TEST_SOURCE_DIR} /
                                             "tests" / "fixtures" / "elf3d_smoke" /
                                             "elf3d_smoke.gltf";
    elf3d::Result<elf3d::LoadedScene> loaded_result =
        engine.load_scene_with_report(path_to_utf8(model_path));
    if (!loaded_result) {
        std::cerr << loaded_result.error().message() << '\n';
        return 3;
    }
    elf3d::LoadedScene loaded = std::move(loaded_result).value();
    const int imported = verify_model_import(loaded);
    if (imported != 0) {
        return imported;
    }
    const elf3d::Result<elf3d::EntityId> camera = loaded.scene->create_perspective_camera({});
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine.create_viewport({64, 64});
    if (!camera || !viewport_result) {
        return fail(5, "Embedded-model quick test failed to create a camera or viewport");
    }
    std::unique_ptr<elf3d::Viewport> viewport = std::move(viewport_result).value();
    viewport->set_clear_color({0.0F, 0.0F, 0.0F, 1.0F});
    if (!viewport->reset_view(*loaded.scene, camera.value())) {
        return fail(6, "Embedded-model quick test failed to frame the model");
    }
    const int rendered = verify_render_statistics(*viewport, *loaded.scene, camera.value());
    if (rendered != 0) {
        return rendered;
    }
    return verify_rendered_pixels(engine, *viewport);
}

[[nodiscard]] int run_model_quick_with_engine() {
    elf3d::EngineConfiguration configuration;
    configuration.opengl.load_procedure = load_opengl_procedure;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(configuration);
    if (!engine_result) {
        if (is_skippable_graphics_error(engine_result.error().code())) {
            std::cout << "Skipping embedded-model quick test: " << engine_result.error().message()
                      << '\n';
            return skipped;
        }
        std::cerr << engine_result.error().message() << '\n';
        return 2;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();
    return run_model_quick(*engine);
}

} // namespace

int main() {
    GlfwRuntime glfw;
    if (!glfw.initialize()) {
        std::cout << "Skipping embedded-model quick test: GLFW initialization failed\n";
        return skipped;
    }

    configure_hidden_context();

    Window window{glfwCreateWindow(64, 64, "Elf3D model quick test", nullptr, nullptr)};
    if (window.get() == nullptr) {
        std::cout << "Skipping embedded-model quick test: hidden context creation failed\n";
        return skipped;
    }
    glfwMakeContextCurrent(window.get());
    if (glfwGetCurrentContext() != window.get()) {
        return fail(1, "GLFW did not make the quick-test context current");
    }

    const int loaded_version = gladLoadGL(load_opengl_procedure);
    if (loaded_version == 0 || GLAD_GL_VERSION_4_1 == 0) {
        std::cout << "Skipping embedded-model quick test: OpenGL 4.1 is unavailable\n";
        return skipped;
    }
    return run_model_quick_with_engine();
}
