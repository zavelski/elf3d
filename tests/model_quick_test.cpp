#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}
[[nodiscard]] int verify_model_import(const elf3d::LoadedScene& loaded)
{
    const elf3d::SceneStatistics expected_statistics{1, 1, 2, 2, 2, 8, 12, 4, 1, 2, 2, 16, 2, 1};
    if (loaded.report.diagnostic_count() != 0 ||
        loaded.scene->statistics() != expected_statistics) {
        return fail(4, "Embedded smoke model produced unexpected import facts");
    }
    return 0;
}

bool valid_render_statistics(const elf3d::RenderStatistics& statistics) noexcept
{
    return !(statistics.draw_calls != 2 || statistics.triangles != 4 || statistics.vertices != 8 ||
             statistics.indices != 12 || statistics.texture_bindings != 3 ||
             statistics.gpu_texture_uploads != 3 || statistics.unique_gpu_textures != 3);
}

class ModelApplication final : public elf3d::Application {
  public:
    elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override
    {
        constexpr std::string_view path =
            ELF3D_TEST_SOURCE_DIR "/tests/fixtures/elf3d_smoke/elf3d_smoke.gltf";
        auto loaded = context.engine().load_scene(path);
        if (!loaded) {
            return loaded.error();
        }
        if (verify_model_import(loaded.value()) != 0) {
            return elf3d::Error{elf3d::ErrorCode::scene_import_failed, "Unexpected model facts"};
        }
        scene_ = std::move(loaded).value().scene;
        auto camera = scene_->create_perspective_camera_entity({});
        auto viewport = context.engine().create_viewport({64, 64});
        if (!camera) {
            return camera.error();
        }
        if (!viewport) {
            return viewport.error();
        }
        camera_ = camera.value();
        viewport_ = std::move(viewport).value();
        viewport_->set_clear_color({0, 0, 0, 1});
        return viewport_->reset_view(*scene_, camera_);
    }
    elf3d::Result<void> update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        if (!context.previous_frame_statistics()) {
            return {};
        }
        if (!valid_render_statistics(viewport_->render_statistics())) {
            return elf3d::Error{elf3d::ErrorCode::draw_submission_failed,
                                "Unexpected model render statistics"};
        }
        std::array<std::uint8_t, 64U * 64U * 4U> pixels{};
        const auto readback = viewport_->read_color_pixels(pixels);
        if (!readback) {
            return readback.error();
        }
        std::size_t colored = 0;
        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4U) {
            if (pixels[pixel] > 8U || pixels[pixel + 1U] > 8U || pixels[pixel + 2U] > 8U) {
                ++colored;
            }
        }
        if (colored < 256U) {
            return elf3d::Error{elf3d::ErrorCode::draw_submission_failed,
                                "Model did not produce enough visible pixels"};
        }
        passed_ = true;
        context.request_exit();
        return {};
    }
    elf3d::Result<void> build_ui(elf3d::ApplicationUiContext& context) noexcept override
    {
        return context.queue_viewport_render(*viewport_, *scene_, camera_);
    }
    void stop(elf3d::ApplicationContext&) noexcept override
    {
        viewport_.reset();
        scene_.reset();
    }
    bool passed() const noexcept
    {
        return passed_;
    }

  private:
    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId camera_;
    bool passed_ = false;
};
} // namespace

int main()
{
    ModelApplication application;
    elf3d::ApplicationOptions options;
    options.initial_window_extent = {64, 64};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    options.presentation_mode = elf3d::PresentationMode::immediate;
    const auto result = elf3d::run_application(options, application);
    if (!result) {
        const auto code = result.error().code();
        if (code == elf3d::ErrorCode::graphics_initialization_failed ||
            code == elf3d::ErrorCode::graphics_context_unavailable ||
            code == elf3d::ErrorCode::unsupported_graphics_version) {
            std::cout << "SKIP: " << result.error().message() << '\n';
            return 77;
        }
        std::cerr << result.error().message() << '\n';
        return 1;
    }
    return application.passed() ? 0 : 1;
}
