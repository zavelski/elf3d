#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

class StudyApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        started_ = true;
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override {
        ++update_count_;
        // The first update proceeds through UI and presentation. Exit on the next update.
        if (update_count_ == 2) {
            context.request_exit();
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        ++ui_count_;
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        stopped_with_scene_ = scene_ != nullptr;
        // All engine-owned objects must be released before run_application destroys the engine.
        scene_.reset();
    }

    [[nodiscard]] bool started() const noexcept {
        return started_;
    }

    [[nodiscard]] bool completed() const noexcept {
        return started_ && update_count_ == 2 && ui_count_ == 1 && stopped_with_scene_ && !scene_;
    }

  private:
    std::unique_ptr<elf3d::Scene> scene_;
    int update_count_ = 0;
    int ui_count_ = 0;
    bool started_ = false;
    bool stopped_with_scene_ = false;
};

} // namespace

int main() {
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
        std::fprintf(stderr, "The application did not complete one frame and release its Scene\n");
        return 1;
    }
    return 0;
}
