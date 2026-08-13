#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <memory>
#include <utility>

namespace elf3d_examples {

class MinimalApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        scene_.reset();
    }

  private:
    std::unique_ptr<elf3d::Scene> scene_;
};

[[nodiscard]] elf3d::Result<int> run_minimal_standard_application() noexcept {
    MinimalApplication application;
    elf3d::ApplicationOptions options;
    options.title = "Minimal Elf3D Application";
    return elf3d::run_application(options, application);
}

} // namespace elf3d_examples
