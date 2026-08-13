#include <GLFW/glfw3.h>

#include <elf3d/app/application.h>
#include <elf3d/core/assert.h>
#include <elf3d/elf3d.h>
#include <elf3d/imgui/context.h>
#include <elf3d/imgui/texture.h>

#include "context_owner.h"
#include "engine_access.h"
#include "input_collector.h"
#include "viewport_texture.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace elf3d {
namespace {

using app::detail::InputCollector;
using app::detail::WindowSnapshot;

[[noreturn]] void fatal_app_allocation_failure() noexcept {
    fatal_error("Elf3D application framework memory allocation failed");
}

[[noreturn]] void fatal_unexpected_app_boundary_exception() noexcept {
    fatal_error("Elf3D application framework encountered an unexpected exception");
}

std::atomic_flag active_application_run = ATOMIC_FLAG_INIT;

class ActiveRunGuard final {
  public:
    [[nodiscard]] bool acquire() noexcept {
        acquired_ = !active_application_run.test_and_set(std::memory_order_acq_rel);
        return acquired_;
    }

    ~ActiveRunGuard() noexcept {
        if (acquired_) {
            active_application_run.clear(std::memory_order_release);
        }
    }

    ActiveRunGuard() = default;
    ActiveRunGuard(const ActiveRunGuard&) = delete;
    ActiveRunGuard& operator=(const ActiveRunGuard&) = delete;

  private:
    bool acquired_ = false;
};

class PlatformRuntime final {
  public:
    [[nodiscard]] Result<void> initialize() noexcept {
        if (glfwInit() != GLFW_TRUE) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "The Elf3D desktop platform could not initialize GLFW"};
        }
        initialized_ = true;
        return {};
    }

    ~PlatformRuntime() noexcept {
        if (initialized_) {
            glfwTerminate();
        }
    }

    PlatformRuntime() = default;
    PlatformRuntime(const PlatformRuntime&) = delete;
    PlatformRuntime& operator=(const PlatformRuntime&) = delete;

  private:
    bool initialized_ = false;
};

struct WindowDeleter final {
    void operator()(GLFWwindow* window) const noexcept {
        if (window != nullptr) {
            glfwDestroyWindow(window);
        }
    }
};

using Window = std::unique_ptr<GLFWwindow, WindowDeleter>;

[[nodiscard]] detail::GraphicsProcedure load_opengl_procedure(const char* name) noexcept {
    GLFWglproc procedure = glfwGetProcAddress(name);
    return reinterpret_cast<detail::GraphicsProcedure>(procedure);
}

[[nodiscard]] Extent2D extent_from_glfw(int width, int height) noexcept {
    return Extent2D{static_cast<std::uint32_t>(std::max(width, 0)),
                    static_cast<std::uint32_t>(std::max(height, 0))};
}

struct OwnedGraphicsContextSnapshot final {
    std::string backend_name = "OpenGL";
    std::string vendor_name;
    std::string device_name;
    std::string api_version;
    std::string shading_language_version;
    GraphicsContextSnapshot view;
};

[[nodiscard]] std::string graphics_string(unsigned int name) {
    const unsigned char* value = glGetString(name);
    if (value == nullptr) {
        return "unavailable";
    }
    std::string result;
    for (std::size_t index = 0; value[index] != 0; ++index) {
        result.push_back(static_cast<char>(value[index]));
    }
    return result;
}

[[nodiscard]] OwnedGraphicsContextSnapshot capture_graphics_context() {
    constexpr unsigned int shading_language_version_name = 0x8B8CU;
    constexpr unsigned int samples_name = 0x80A9U;
    constexpr unsigned int framebuffer_srgb_name = 0x8DB9U;
    OwnedGraphicsContextSnapshot snapshot;
    snapshot.vendor_name = graphics_string(GL_VENDOR);
    snapshot.device_name = graphics_string(GL_RENDERER);
    snapshot.api_version = graphics_string(GL_VERSION);
    snapshot.shading_language_version = graphics_string(shading_language_version_name);
    snapshot.view =
        GraphicsContextSnapshot{snapshot.backend_name, snapshot.vendor_name, snapshot.device_name,
                                snapshot.api_version, snapshot.shading_language_version};
    glGetIntegerv(GL_RED_BITS, &snapshot.view.red_bits);
    glGetIntegerv(GL_GREEN_BITS, &snapshot.view.green_bits);
    glGetIntegerv(GL_BLUE_BITS, &snapshot.view.blue_bits);
    glGetIntegerv(GL_ALPHA_BITS, &snapshot.view.alpha_bits);
    glGetIntegerv(GL_DEPTH_BITS, &snapshot.view.depth_bits);
    glGetIntegerv(GL_STENCIL_BITS, &snapshot.view.stencil_bits);
    glGetIntegerv(samples_name, &snapshot.view.samples);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &snapshot.view.maximum_texture_extent);
    snapshot.view.default_framebuffer_srgb = glIsEnabled(framebuffer_srgb_name) == GL_TRUE;
    return snapshot;
}

[[nodiscard]] WindowSnapshot snapshot_window(GLFWwindow* window) noexcept {
    int window_width = 0;
    int window_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    float x_scale = 1.0F;
    float y_scale = 1.0F;
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    glfwGetWindowContentScale(window, &x_scale, &y_scale);
    const float maximum_scale = std::max(x_scale, y_scale);
    const float dpi_scale =
        std::isfinite(maximum_scale) && maximum_scale > 0.0F ? maximum_scale : 1.0F;
    return WindowSnapshot{extent_from_glfw(window_width, window_height),
                          extent_from_glfw(framebuffer_width, framebuffer_height), dpi_scale,
                          glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE};
}

[[nodiscard]] Result<void> validate_window_options(const ApplicationOptions& options) noexcept {
    constexpr std::uint32_t maximum_glfw_extent =
        static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    if (options.title.empty()) {
        return Error{ErrorCode::invalid_argument, "ApplicationOptions title must not be empty"};
    }
    if (options.initial_window_extent.width == 0 || options.initial_window_extent.height == 0 ||
        options.initial_window_extent.width > maximum_glfw_extent ||
        options.initial_window_extent.height > maximum_glfw_extent) {
        return Error{ErrorCode::invalid_viewport_dimensions,
                     "ApplicationOptions initial window extent is invalid"};
    }
    return {};
}

void configure_window(const ApplicationOptions& options) noexcept {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, options.initial_visibility == ApplicationWindowVisibility::visible
                                     ? GLFW_TRUE
                                     : GLFW_FALSE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
}

[[nodiscard]] Result<Window> create_window(const ApplicationOptions& options) {
    const Result<void> valid = validate_window_options(options);
    if (!valid) {
        return valid.error();
    }
    configure_window(options);

    const std::string owned_title{options.title};
    Window window{glfwCreateWindow(static_cast<int>(options.initial_window_extent.width),
                                   static_cast<int>(options.initial_window_extent.height),
                                   owned_title.c_str(), nullptr, nullptr)};
    if (window == nullptr) {
        return Error{ErrorCode::graphics_context_unavailable,
                     "The Elf3D application framework could not create an OpenGL 4.1 window"};
    }
    glfwMakeContextCurrent(window.get());
    if (glfwGetCurrentContext() != window.get()) {
        return Error{ErrorCode::graphics_context_unavailable,
                     "The Elf3D application framework could not make its context current"};
    }
    glfwSwapInterval(options.presentation_mode == PresentationMode::synchronized ? 1 : 0);
    return window;
}

} // namespace

namespace app::detail {

struct ApplicationServices final {
    Engine& engine;
    InteractionArbiter& interaction_arbiter;
    const GraphicsContextSnapshot& graphics_context;
};

struct ApplicationFrame final {
    WindowSnapshot window;
    double elapsed_seconds = 0.0;
    const InputSnapshot* input = nullptr;
    std::span<const std::string_view> dropped_files;
};

} // namespace app::detail

class ApplicationUiContext::Impl final {
  public:
    struct RenderRequest final {
        Viewport* viewport = nullptr;
        const Scene* scene = nullptr;
        EntityId camera;
        std::optional<EntityHighlight> highlight;
        std::vector<OverlayLineSegment> overlay_lines;
        std::vector<OverlayPointMarker> overlay_markers;
        RenderShadingMode shading_mode = RenderShadingMode::standard;
    };

    [[nodiscard]] Result<void> queue(Viewport& viewport, const Scene& scene, EntityId camera,
                                     const ViewportRenderOptions& options) {
        for (const RenderRequest& request : requests_) {
            if (request.viewport == &viewport) {
                return Error{ErrorCode::invalid_argument,
                             "A Viewport may be queued only once in an application frame"};
            }
        }
        if (options.overlay_lines.size() > maximum_viewport_overlay_lines ||
            options.overlay_markers.size() > maximum_viewport_overlay_markers) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "A queued Viewport render exceeds the overlay element limit"};
        }

        RenderRequest request;
        request.viewport = &viewport;
        request.scene = &scene;
        request.camera = camera;
        request.highlight = options.highlight;
        request.overlay_lines.assign(options.overlay_lines.begin(), options.overlay_lines.end());
        request.overlay_markers.assign(options.overlay_markers.begin(),
                                       options.overlay_markers.end());
        request.shading_mode = options.shading_mode;
        requests_.push_back(std::move(request));
        return {};
    }

    [[nodiscard]] Result<void> render_all() noexcept {
        for (RenderRequest& request : requests_) {
            if (request.viewport == nullptr || request.scene == nullptr) {
                return Error{ErrorCode::invalid_argument,
                             "A queued Viewport render references an invalid application object"};
            }
            ViewportRenderOptions options;
            options.highlight = request.highlight;
            options.overlay_lines = request.overlay_lines;
            options.overlay_markers = request.overlay_markers;
            options.shading_mode = request.shading_mode;
            const Result<void> rendered =
                request.viewport->render(*request.scene, request.camera, options);
            if (!rendered) {
                return rendered.error();
            }
        }
        return {};
    }

    void clear() noexcept {
        requests_.clear();
    }

  private:
    std::vector<RenderRequest> requests_;
};

ApplicationContext::ApplicationContext(const app::detail::ApplicationFrame& frame,
                                       const app::detail::ApplicationServices& services) noexcept
    : engine_(&services.engine), window_extent_(frame.window.window_extent),
      framebuffer_extent_(frame.window.framebuffer_extent), dpi_scale_(frame.window.dpi_scale),
      interaction_arbiter_(&services.interaction_arbiter),
      graphics_context_(&services.graphics_context) {}

Engine& ApplicationContext::engine() const noexcept {
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

Extent2D ApplicationContext::window_extent() const noexcept {
    return window_extent_;
}

Extent2D ApplicationContext::framebuffer_extent() const noexcept {
    return framebuffer_extent_;
}

float ApplicationContext::dpi_scale() const noexcept {
    return dpi_scale_;
}

InteractionArbiter& ApplicationContext::interaction_arbiter() const noexcept {
    ELF3D_ASSERT(interaction_arbiter_ != nullptr);
    return *interaction_arbiter_;
}

const GraphicsContextSnapshot& ApplicationContext::graphics_context() const noexcept {
    ELF3D_ASSERT(graphics_context_ != nullptr);
    return *graphics_context_;
}

ApplicationUpdateContext::ApplicationUpdateContext(
    const app::detail::ApplicationFrame& frame,
    const app::detail::ApplicationServices& services) noexcept
    : engine_(&services.engine), elapsed_seconds_(frame.elapsed_seconds),
      window_extent_(frame.window.window_extent),
      framebuffer_extent_(frame.window.framebuffer_extent), dpi_scale_(frame.window.dpi_scale),
      focused_(frame.window.focused), input_(frame.input),
      interaction_arbiter_(&services.interaction_arbiter),
      dropped_files_(frame.dropped_files.data()), dropped_file_count_(frame.dropped_files.size()) {}

Engine& ApplicationUpdateContext::engine() const noexcept {
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

double ApplicationUpdateContext::elapsed_seconds() const noexcept {
    return elapsed_seconds_;
}

Extent2D ApplicationUpdateContext::window_extent() const noexcept {
    return window_extent_;
}

Extent2D ApplicationUpdateContext::framebuffer_extent() const noexcept {
    return framebuffer_extent_;
}

float ApplicationUpdateContext::dpi_scale() const noexcept {
    return dpi_scale_;
}

bool ApplicationUpdateContext::focused() const noexcept {
    return focused_;
}

const InputSnapshot& ApplicationUpdateContext::input() const noexcept {
    ELF3D_ASSERT(input_ != nullptr);
    return *input_;
}

InteractionArbiter& ApplicationUpdateContext::interaction_arbiter() const noexcept {
    ELF3D_ASSERT(interaction_arbiter_ != nullptr);
    return *interaction_arbiter_;
}

std::size_t ApplicationUpdateContext::dropped_file_count() const noexcept {
    return dropped_file_count_;
}

std::string_view ApplicationUpdateContext::dropped_file(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < dropped_file_count_);
    return dropped_files_[index];
}

void ApplicationUpdateContext::request_exit() noexcept {
    exit_requested_ = true;
}

void ApplicationUpdateContext::set_presentation_mode(PresentationMode mode) noexcept {
    requested_presentation_mode_ = mode;
}

bool ApplicationUpdateContext::exit_requested() const noexcept {
    return exit_requested_;
}

std::optional<PresentationMode>
ApplicationUpdateContext::requested_presentation_mode() const noexcept {
    return requested_presentation_mode_;
}

ApplicationUiContext::ApplicationUiContext(const app::detail::ApplicationFrame& frame,
                                           const app::detail::ApplicationServices& services,
                                           Impl& impl) noexcept
    : engine_(&services.engine), window_extent_(frame.window.window_extent),
      framebuffer_extent_(frame.window.framebuffer_extent), dpi_scale_(frame.window.dpi_scale),
      input_(frame.input), interaction_arbiter_(&services.interaction_arbiter),
      dropped_files_(frame.dropped_files.data()), dropped_file_count_(frame.dropped_files.size()),
      impl_(&impl) {}

Engine& ApplicationUiContext::engine() const noexcept {
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

Extent2D ApplicationUiContext::window_extent() const noexcept {
    return window_extent_;
}

Extent2D ApplicationUiContext::framebuffer_extent() const noexcept {
    return framebuffer_extent_;
}

float ApplicationUiContext::dpi_scale() const noexcept {
    return dpi_scale_;
}

const InputSnapshot& ApplicationUiContext::input() const noexcept {
    ELF3D_ASSERT(input_ != nullptr);
    return *input_;
}

InteractionArbiter& ApplicationUiContext::interaction_arbiter() const noexcept {
    ELF3D_ASSERT(interaction_arbiter_ != nullptr);
    return *interaction_arbiter_;
}

std::size_t ApplicationUiContext::dropped_file_count() const noexcept {
    return dropped_file_count_;
}

std::string_view ApplicationUiContext::dropped_file(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < dropped_file_count_);
    return dropped_files_[index];
}

Result<void>
ApplicationUiContext::queue_viewport_render(Viewport& viewport, const Scene& scene, EntityId camera,
                                            const ViewportRenderOptions& options) noexcept {
    try {
        if (impl_ == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport rendering is unavailable outside the application UI phase"};
        }
        return impl_->queue(viewport, scene, camera, options);
    } catch (const std::bad_alloc&) {
        fatal_app_allocation_failure();
    } catch (...) {
        fatal_unexpected_app_boundary_exception();
    }
}

Result<void> ApplicationUiContext::draw_viewport_image(const Viewport& viewport,
                                                       Float2 top_left_screen_position,
                                                       Float2 display_size) noexcept {
    if (engine_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "Viewport presentation is unavailable outside the application UI phase"};
    }
    const Result<detail::NativeTextureView> texture =
        detail::EngineAccess::native_texture_view(*engine_, viewport.color_texture());
    if (!texture) {
        return texture.error();
    }
    return imgui::detail::draw_viewport_image(texture.value(), top_left_screen_position,
                                              display_size);
}

namespace app::detail {

struct ApplicationRunnerResources final {
    GLFWwindow& window;
    Engine& engine;
    imgui::detail::ContextOwner& imgui_context;
    InputCollector& input_collector;
    const GraphicsContextSnapshot& graphics_context;
    Application& application;
};

class ApplicationRunner final {
  public:
    ApplicationRunner(const ApplicationRunnerResources& resources,
                      PresentationMode presentation_mode) noexcept
        : window_(&resources.window), engine_(&resources.engine),
          imgui_context_(&resources.imgui_context), input_collector_(&resources.input_collector),
          graphics_context_(&resources.graphics_context), presentation_mode_(presentation_mode),
          application_(&resources.application) {}

    [[nodiscard]] Result<int> run() noexcept {
        ELF3D_ASSERT(window_ != nullptr);
        ELF3D_ASSERT(engine_ != nullptr);
        ELF3D_ASSERT(imgui_context_ != nullptr);
        ELF3D_ASSERT(input_collector_ != nullptr);
        ELF3D_ASSERT(application_ != nullptr);

        ApplicationFrame frame;
        frame.window = snapshot_window(window_);
        const ApplicationServices current_services = services();
        ApplicationContext startup_context{frame, current_services};
        const Result<void> started = application_->start(startup_context);
        if (!started) {
            interaction_arbiter_.cancel_all(InteractionCancellationReason::shutdown);
            application_->stop(startup_context);
            return started.error();
        }

        const Result<void> frames_result = run_frames(frame);
        stop(frame);
        if (!frames_result) {
            return frames_result.error();
        }
        return 0;
    }

  private:
    [[nodiscard]] ApplicationServices services() noexcept {
        return ApplicationServices{*engine_, interaction_arbiter_, *graphics_context_};
    }

    [[nodiscard]] ApplicationFrame
    collect_frame(std::chrono::steady_clock::time_point& previous_frame) noexcept {
        glfwPollEvents();
        const WindowSnapshot window = snapshot_window(window_);
        const auto current_frame = std::chrono::steady_clock::now();
        const double elapsed_seconds =
            std::chrono::duration<double>(current_frame - previous_frame).count();
        previous_frame = current_frame;
        current_input_ = input_collector_->capture(window, ImGui::GetIO().WantTextInput);
        interaction_arbiter_.begin_frame(current_input_);
        return ApplicationFrame{window, elapsed_seconds, &current_input_,
                                input_collector_->dropped_files()};
    }

    [[nodiscard]] Result<bool> update_application(const ApplicationFrame& frame) noexcept {
        const ApplicationServices current_services = services();
        ApplicationUpdateContext update_context{frame, current_services};
        const Result<void> updated = application_->update(update_context);
        if (!updated) {
            finish_interaction_frame();
            return updated.error();
        }
        apply_presentation_mode(update_context.requested_presentation_mode());
        if (update_context.exit_requested()) {
            finish_interaction_frame();
            return false;
        }
        return true;
    }

    void apply_presentation_mode(std::optional<PresentationMode> requested_mode) noexcept {
        if (!requested_mode.has_value() || *requested_mode == presentation_mode_) {
            return;
        }
        presentation_mode_ = *requested_mode;
        glfwSwapInterval(presentation_mode_ == PresentationMode::synchronized ? 1 : 0);
    }

    [[nodiscard]] Result<void> build_and_present(const ApplicationFrame& frame) noexcept {
        imgui_context_->begin_frame();
        render_queue_.clear();
        const ApplicationServices current_services = services();
        ApplicationUiContext ui_context{frame, current_services, render_queue_};
        const Result<void> ui_built = application_->build_ui(ui_context);
        finish_interaction_frame();
        if (!ui_built) {
            imgui_context_->discard_frame();
            return ui_built.error();
        }
        const Result<void> rendered = render_queue_.render_all();
        if (!rendered) {
            imgui_context_->discard_frame();
            return rendered.error();
        }
        present(frame.window.framebuffer_extent);
        return {};
    }

    void present(Extent2D framebuffer_extent) noexcept {
        glViewport(0, 0, static_cast<GLsizei>(framebuffer_extent.width),
                   static_cast<GLsizei>(framebuffer_extent.height));
        glClearColor(0.035F, 0.04F, 0.05F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        imgui_context_->render();
        glfwSwapBuffers(window_);
    }

    [[nodiscard]] Result<void> run_frames(ApplicationFrame& frame) noexcept {
        auto previous_frame = std::chrono::steady_clock::now();
        while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
            frame = collect_frame(previous_frame);
            const Result<bool> updated = update_application(frame);
            if (!updated) {
                return updated.error();
            }
            if (!updated.value()) {
                return {};
            }
            const Result<void> presented = build_and_present(frame);
            if (!presented) {
                return presented.error();
            }
        }
        return {};
    }

    void stop(ApplicationFrame& frame) noexcept {
        render_queue_.clear();
        interaction_arbiter_.cancel_all(InteractionCancellationReason::shutdown);
        synchronize_pointer_capture();
        frame.window = snapshot_window(window_);
        const ApplicationServices current_services = services();
        ApplicationContext shutdown_context{frame, current_services};
        application_->stop(shutdown_context);
    }

    void finish_interaction_frame() noexcept {
        interaction_arbiter_.end_frame();
        synchronize_pointer_capture();
    }

    void synchronize_pointer_capture() noexcept {
        const int requested_mode = interaction_arbiter_.pointer_capture_requested()
                                       ? GLFW_CURSOR_DISABLED
                                       : GLFW_CURSOR_NORMAL;
        if (glfwGetInputMode(window_, GLFW_CURSOR) != requested_mode) {
            glfwSetInputMode(window_, GLFW_CURSOR, requested_mode);
        }
    }

    GLFWwindow* window_ = nullptr;
    Engine* engine_ = nullptr;
    imgui::detail::ContextOwner* imgui_context_ = nullptr;
    InputCollector* input_collector_ = nullptr;
    const GraphicsContextSnapshot* graphics_context_ = nullptr;
    PresentationMode presentation_mode_ = PresentationMode::synchronized;
    Application* application_ = nullptr;
    ApplicationUiContext::Impl render_queue_;
    InteractionArbiter interaction_arbiter_;
    InputSnapshot current_input_;
};

} // namespace app::detail

Result<int> run_application(const ApplicationOptions& options, Application& application) noexcept {
    try {
        ActiveRunGuard run_guard;
        if (!run_guard.acquire()) {
            return Error{ErrorCode::invalid_argument,
                         "Only one Elf3D standard application run may be active"};
        }

        PlatformRuntime platform;
        const Result<void> platform_initialized = platform.initialize();
        if (!platform_initialized) {
            return platform_initialized.error();
        }

        Result<Window> window_result = create_window(options);
        if (!window_result) {
            return window_result.error();
        }
        Window window = std::move(window_result).value();

        InputCollector input_collector;
        input_collector.install(*window);

        detail::EngineCreateOptions engine_options;
        engine_options.load_opengl_procedure = load_opengl_procedure;
        Result<std::unique_ptr<Engine>> engine_result =
            detail::EngineAccess::create(engine_options);
        if (!engine_result) {
            return engine_result.error();
        }
        std::unique_ptr<Engine> engine = std::move(engine_result).value();

        Result<std::unique_ptr<imgui::detail::ContextOwner>> imgui_result =
            imgui::detail::ContextOwner::create(window.get(), "#version 410 core");
        if (!imgui_result) {
            return imgui_result.error();
        }
        std::unique_ptr<imgui::detail::ContextOwner> imgui_context =
            std::move(imgui_result).value();

        OwnedGraphicsContextSnapshot graphics_context = capture_graphics_context();

        const app::detail::ApplicationRunnerResources resources{
            *window, *engine, *imgui_context, input_collector, graphics_context.view, application};
        app::detail::ApplicationRunner runner{resources, options.presentation_mode};
        Result<int> result = runner.run();

        // The application has released its objects in stop. The Engine is
        // deliberately destroyed before UI, context/window, and platform owners.
        engine.reset();
        imgui_context.reset();
        window.reset();
        return result;
    } catch (const std::bad_alloc&) {
        fatal_app_allocation_failure();
    } catch (...) {
        fatal_unexpected_app_boundary_exception();
    }
}

} // namespace elf3d
