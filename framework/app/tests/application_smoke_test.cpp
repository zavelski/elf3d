#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cstdio>
#include <memory>
#include <span>

namespace {

enum class CallbackPhase {
    start,
    update,
    build_ui,
    stop,
};

class CallbackTrace final {
  public:
    void record(CallbackPhase phase) noexcept {
        if (size_ < phases_.size()) {
            phases_[size_] = phase;
            ++size_;
        } else {
            overflowed_ = true;
        }
    }

    [[nodiscard]] bool matches(std::span<const CallbackPhase> expected) const noexcept {
        if (overflowed_ || size_ != expected.size()) {
            return false;
        }
        for (std::size_t index = 0; index < expected.size(); ++index) {
            if (phases_[index] != expected[index]) {
                return false;
            }
        }
        return true;
    }

  private:
    std::array<CallbackPhase, 8> phases_{};
    std::size_t size_ = 0;
    bool overflowed_ = false;
};

class LifecycleApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        trace_.record(CallbackPhase::start);
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        const elf3d::Result<elf3d::EntityId> camera = scene_->create_perspective_camera_entity({});
        if (!camera) {
            return camera.error();
        }
        camera_ = camera.value();

        elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport =
            context.engine().create_viewport({320, 240});
        if (!viewport) {
            return viewport.error();
        }
        viewport_ = std::move(viewport).value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override {
        trace_.record(CallbackPhase::update);
        ++update_count_;
        if (update_count_ >= 2) {
            context.request_exit();
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    build_ui(elf3d::ApplicationUiContext& context) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return context.queue_viewport_render(*viewport_, *scene_, camera_);
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::stop);
        viewport_.reset();
        scene_.reset();
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::update,
                                      CallbackPhase::build_ui, CallbackPhase::update,
                                      CallbackPhase::stop};
        return trace_.matches(expected) && scene_ == nullptr && viewport_ == nullptr;
    }

  private:
    CallbackTrace trace_;
    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId camera_;
    int update_count_ = 0;
};

class NoCallbackApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext&) noexcept override {
        called_ = true;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        called_ = true;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        called_ = true;
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        called_ = true;
    }

    [[nodiscard]] bool passed() const noexcept {
        return !called_;
    }

  private:
    bool called_ = false;
};

class StartupFailureApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        trace_.record(CallbackPhase::start);
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        return elf3d::Error{elf3d::ErrorCode::scene_import_failed,
                            "Injected application startup failure"};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        trace_.record(CallbackPhase::update);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::stop);
        released_partial_startup_ = scene_ != nullptr;
        scene_.reset();
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::stop};
        return trace_.matches(expected) && released_partial_startup_ && scene_ == nullptr;
    }

  private:
    CallbackTrace trace_;
    std::unique_ptr<elf3d::Scene> scene_;
    bool released_partial_startup_ = false;
};

class UpdateFailureApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::start);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        trace_.record(CallbackPhase::update);
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Injected application update failure"};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::stop);
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::update,
                                      CallbackPhase::stop};
        return trace_.matches(expected);
    }

  private:
    CallbackTrace trace_;
};

class UiFailureApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::start);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        trace_.record(CallbackPhase::update);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Injected application UI failure"};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::stop);
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::update,
                                      CallbackPhase::build_ui, CallbackPhase::stop};
        return trace_.matches(expected);
    }

  private:
    CallbackTrace trace_;
};

class QueuedRenderFailureApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        trace_.record(CallbackPhase::start);
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        if (!scene) {
            return scene.error();
        }
        scene_ = std::move(scene).value();
        elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport =
            context.engine().create_viewport({64, 64});
        if (!viewport) {
            return viewport.error();
        }
        viewport_ = std::move(viewport).value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        trace_.record(CallbackPhase::update);
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    build_ui(elf3d::ApplicationUiContext& context) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return context.queue_viewport_render(*viewport_, *scene_, {});
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        trace_.record(CallbackPhase::stop);
        viewport_.reset();
        scene_.reset();
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::update,
                                      CallbackPhase::build_ui, CallbackPhase::stop};
        return trace_.matches(expected) && scene_ == nullptr && viewport_ == nullptr;
    }

  private:
    CallbackTrace trace_;
    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
};

class CapturedStartupFailureApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        trace_.record(CallbackPhase::start);
        const elf3d::Result<elf3d::InteractionOwnerId> owner =
            context.interaction_arbiter().create_owner();
        if (!owner) {
            return owner.error();
        }
        owner_ = owner.value();
        const elf3d::InteractionRegionDescription description{
            {0.0F, 0.0F}, {64.0F, 64.0F}, {64, 64}, true};
        const elf3d::Result<elf3d::InteractionRegionId> region =
            context.interaction_arbiter().register_region(owner_, {}, description);
        if (!region) {
            return region.error();
        }
        const elf3d::Result<void> requested = context.interaction_arbiter().request(
            owner_, region.value(), elf3d::InteractionRequest::navigation);
        if (!requested) {
            return requested.error();
        }
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Injected captured startup failure"};
    }

    [[nodiscard]] elf3d::Result<void> update(elf3d::ApplicationUpdateContext&) noexcept override {
        trace_.record(CallbackPhase::update);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        trace_.record(CallbackPhase::build_ui);
        return {};
    }

    void stop(elf3d::ApplicationContext& context) noexcept override {
        trace_.record(CallbackPhase::stop);
        const elf3d::InteractionSnapshot snapshot = context.interaction_arbiter().snapshot(owner_);
        capture_released_ =
            !snapshot.pointer_captured &&
            snapshot.cancellation_reason == elf3d::InteractionCancellationReason::shutdown;
        context.interaction_arbiter().destroy_owner(owner_);
    }

    [[nodiscard]] bool passed() const noexcept {
        constexpr std::array expected{CallbackPhase::start, CallbackPhase::stop};
        return trace_.matches(expected) && capture_released_;
    }

  private:
    CallbackTrace trace_;
    elf3d::InteractionOwnerId owner_;
    bool capture_released_ = false;
};

[[nodiscard]] elf3d::ApplicationOptions hidden_options() noexcept {
    elf3d::ApplicationOptions options;
    options.title = "Elf3D application lifecycle test";
    options.initial_window_extent = {320, 240};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    return options;
}

[[nodiscard]] bool environment_cannot_create_context(elf3d::ErrorCode code) noexcept {
    return code == elf3d::ErrorCode::graphics_initialization_failed ||
           code == elf3d::ErrorCode::graphics_context_unavailable ||
           code == elf3d::ErrorCode::unsupported_graphics_version;
}

[[nodiscard]] bool expected_failure(elf3d::Application& application,
                                    elf3d::ErrorCode expected_code) noexcept {
    const elf3d::Result<int> result = elf3d::run_application(hidden_options(), application);
    return !result && result.error().code() == expected_code;
}

[[nodiscard]] int fail(const char* message) noexcept {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

[[nodiscard]] int test_successful_lifecycle() noexcept {
    LifecycleApplication first_lifecycle;
    const elf3d::Result<int> first_result =
        elf3d::run_application(hidden_options(), first_lifecycle);
    if (!first_result) {
        if (environment_cannot_create_context(first_result.error().code())) {
            return 77;
        }
        std::fprintf(stderr, "run_application failed: %s\n", first_result.error().message());
        return 1;
    }
    if (first_result.value() != 0 || !first_lifecycle.passed()) {
        return fail("The successful application lifecycle sequence was incorrect");
    }
    return 0;
}

[[nodiscard]] bool startup_failure_invokes_no_callbacks() noexcept {
    NoCallbackApplication no_callback;
    elf3d::ApplicationOptions invalid_options = hidden_options();
    invalid_options.initial_window_extent = {};
    const elf3d::Result<int> invalid_result = elf3d::run_application(invalid_options, no_callback);
    return !invalid_result &&
           invalid_result.error().code() == elf3d::ErrorCode::invalid_viewport_dimensions &&
           no_callback.passed();
}

[[nodiscard]] bool partial_startup_rolls_back() noexcept {
    StartupFailureApplication startup_failure;
    return expected_failure(startup_failure, elf3d::ErrorCode::scene_import_failed) &&
           startup_failure.passed();
}

[[nodiscard]] bool update_failure_stops() noexcept {
    UpdateFailureApplication update_failure;
    return expected_failure(update_failure, elf3d::ErrorCode::invalid_argument) &&
           update_failure.passed();
}

[[nodiscard]] bool ui_failure_stops() noexcept {
    UiFailureApplication ui_failure;
    return expected_failure(ui_failure, elf3d::ErrorCode::invalid_argument) && ui_failure.passed();
}

[[nodiscard]] bool queued_render_failure_stops() noexcept {
    QueuedRenderFailureApplication render_failure;
    return expected_failure(render_failure, elf3d::ErrorCode::invalid_entity) &&
           render_failure.passed();
}

[[nodiscard]] bool captured_startup_releases_capture() noexcept {
    CapturedStartupFailureApplication captured_failure;
    return expected_failure(captured_failure, elf3d::ErrorCode::invalid_argument) &&
           captured_failure.passed();
}

[[nodiscard]] bool next_lifecycle_is_clean() noexcept {
    LifecycleApplication final_lifecycle;
    const elf3d::Result<int> final_result =
        elf3d::run_application(hidden_options(), final_lifecycle);
    return final_result && final_result.value() == 0 && final_lifecycle.passed();
}

} // namespace

int main() {
    if (const int result = test_successful_lifecycle(); result != 0) {
        return result;
    }
    if (!startup_failure_invokes_no_callbacks()) {
        return fail(
            "Window startup failure invoked application callbacks or returned the wrong error");
    }
    if (!partial_startup_rolls_back()) {
        return fail("Partial application startup did not roll back through stop");
    }
    if (!update_failure_stops()) {
        return fail("Application update failure propagation or teardown was incorrect");
    }
    if (!ui_failure_stops()) {
        return fail("Application UI failure propagation or teardown was incorrect");
    }
    if (!queued_render_failure_stops()) {
        return fail("Queued rendering failure propagation or teardown was incorrect");
    }
    if (!captured_startup_releases_capture()) {
        return fail("Shutdown did not release capture before the final application callback");
    }
    if (!next_lifecycle_is_clean()) {
        return fail("A completed or failed application run contaminated the next lifecycle");
    }
    return 0;
}
