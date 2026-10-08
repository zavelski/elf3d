#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>

namespace {

class StudyApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override
    {
        started_ = true;
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        const auto camera = scene_->create_perspective_camera_entity({});
        if (!camera) {
            return camera.error();
        }
        camera_ = camera.value();
        auto viewport = context.engine().create_viewport({16, 16});
        if (!viewport) {
            return viewport.error();
        }
        viewport_ = std::move(viewport).value();
        viewport_->set_clear_color({0.25F, 0.5F, 0.75F, 1.0F});
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        ++update_count_;
        const auto previous = context.previous_frame_statistics();
        if (!previous) {
            return {};
        }
        if (previous->frame_index != 0 || previous->wall_milliseconds < 0.0) {
            return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Unexpected frame statistics"};
        }
        std::array<std::uint8_t, 16 * 16 * 4> pixels{};
        const auto readback = viewport_->read_color_pixels(pixels);
        if (!readback) {
            return readback.error();
        }
        captured_ = pixels[0] > 0 && pixels[3] == 255;
        context.request_exit();
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    build_ui(elf3d::ApplicationUiContext& context) noexcept override
    {
        ++ui_count_;
        return context.queue_viewport_render(*viewport_, *scene_, camera_);
    }

    void stop(elf3d::ApplicationContext&) noexcept override
    {
        stopped_with_scene_ = scene_ != nullptr;
        // All engine-owned objects must be released before run_application destroys the engine.
        viewport_.reset();
        scene_.reset();
    }

    [[nodiscard]] bool started() const noexcept
    {
        return started_;
    }

    [[nodiscard]] bool completed() const noexcept
    {
        return started_ && update_count_ == 2 && ui_count_ == 1 && captured_ &&
               stopped_with_scene_ && !scene_ && !viewport_;
    }

  private:
    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId camera_;
    int update_count_ = 0;
    int ui_count_ = 0;
    bool started_ = false;
    bool stopped_with_scene_ = false;
    bool captured_ = false;
};

} // namespace

int main()
{
    elf3d::ApplicationOptions options;
    options.title = "Elf3D external application";
    options.initial_window_extent = {320, 240};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;

    StudyApplication application;
    const elf3d::Result<int> result = elf3d::run_application(options, application);
    if (!result) {
        const elf3d::ErrorCode code = result.error().code();
        if (!application.started() && (code == elf3d::ErrorCode::graphics_initialization_failed ||
                                       code == elf3d::ErrorCode::graphics_context_unavailable ||
                                       code == elf3d::ErrorCode::unsupported_graphics_version)) {
            std::fprintf(stderr, "Skipped: %s\n", result.error().message());
            return 77;
        }
        std::fprintf(stderr, "run_application failed: %s\n", result.error().message());
        return 1;
    }
    if (result.value() != 0 || !application.completed()) {
        std::fprintf(stderr, "The application did not capture one frame and release its objects\n");
        return 1;
    }
    return 0;
}
