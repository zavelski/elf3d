#ifndef ELF3D_APP_APPLICATION_H
#define ELF3D_APP_APPLICATION_H

#include <elf3d/app/input.h>
#include <elf3d/app/interaction.h>
#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/rendering.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace elf3d {

class Engine;
class Scene;
class Viewport;

namespace app::detail {
class ApplicationRunner;
struct ApplicationFrame;
struct ApplicationServices;
} // namespace app::detail

enum class PresentationMode : std::uint8_t {
    synchronized,
    immediate,
};

enum class ApplicationWindowVisibility : std::uint8_t {
    visible,
    hidden,
};

struct ApplicationOptions {
    // Borrowed strings must remain valid for the complete run_application call.
    std::string_view title = "Elf3D Application";
    Extent2D initial_window_extent{1600, 900};
    PresentationMode presentation_mode = PresentationMode::synchronized;
    ApplicationWindowVisibility initial_visibility = ApplicationWindowVisibility::visible;
};

struct GraphicsContextSnapshot {
    // All strings are borrowed for the complete run_application call.
    std::string_view backend_name;
    std::string_view vendor_name;
    std::string_view device_name;
    std::string_view api_version;
    std::string_view shading_language_version;
    std::int32_t red_bits = 0;
    std::int32_t green_bits = 0;
    std::int32_t blue_bits = 0;
    std::int32_t alpha_bits = 0;
    std::int32_t depth_bits = 0;
    std::int32_t stencil_bits = 0;
    std::int32_t samples = 0;
    std::int32_t maximum_texture_extent = 0;
    bool default_framebuffer_srgb = false;
};

class ApplicationContext final {
  public:
    [[nodiscard]] Engine& engine() const noexcept;
    [[nodiscard]] Extent2D window_extent() const noexcept;
    [[nodiscard]] Extent2D framebuffer_extent() const noexcept;
    [[nodiscard]] float dpi_scale() const noexcept;
    [[nodiscard]] InteractionArbiter& interaction_arbiter() const noexcept;
    [[nodiscard]] const GraphicsContextSnapshot& graphics_context() const noexcept;

  private:
    friend class app::detail::ApplicationRunner;

    ApplicationContext(const app::detail::ApplicationFrame& frame,
                       const app::detail::ApplicationServices& services) noexcept;

    Engine* engine_ = nullptr;
    Extent2D window_extent_;
    Extent2D framebuffer_extent_;
    float dpi_scale_ = 1.0F;
    InteractionArbiter* interaction_arbiter_ = nullptr;
    const GraphicsContextSnapshot* graphics_context_ = nullptr;
};

class ApplicationUpdateContext final {
  public:
    [[nodiscard]] Engine& engine() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;
    [[nodiscard]] Extent2D window_extent() const noexcept;
    [[nodiscard]] Extent2D framebuffer_extent() const noexcept;
    [[nodiscard]] float dpi_scale() const noexcept;
    [[nodiscard]] bool focused() const noexcept;
    [[nodiscard]] const InputSnapshot& input() const noexcept;
    [[nodiscard]] InteractionArbiter& interaction_arbiter() const noexcept;
    [[nodiscard]] std::size_t dropped_file_count() const noexcept;
    [[nodiscard]] std::string_view dropped_file(std::size_t index) const noexcept;

    void request_exit() noexcept;
    void set_presentation_mode(PresentationMode mode) noexcept;

  private:
    friend class app::detail::ApplicationRunner;

    ApplicationUpdateContext(const app::detail::ApplicationFrame& frame,
                             const app::detail::ApplicationServices& services) noexcept;

    [[nodiscard]] bool exit_requested() const noexcept;
    [[nodiscard]] std::optional<PresentationMode> requested_presentation_mode() const noexcept;

    Engine* engine_ = nullptr;
    double elapsed_seconds_ = 0.0;
    Extent2D window_extent_;
    Extent2D framebuffer_extent_;
    float dpi_scale_ = 1.0F;
    bool focused_ = false;
    bool exit_requested_ = false;
    std::optional<PresentationMode> requested_presentation_mode_;
    const InputSnapshot* input_ = nullptr;
    InteractionArbiter* interaction_arbiter_ = nullptr;
    const std::string_view* dropped_files_ = nullptr;
    std::size_t dropped_file_count_ = 0;
};

class ApplicationUiContext final {
  public:
    [[nodiscard]] Engine& engine() const noexcept;
    [[nodiscard]] Extent2D window_extent() const noexcept;
    [[nodiscard]] Extent2D framebuffer_extent() const noexcept;
    [[nodiscard]] float dpi_scale() const noexcept;
    [[nodiscard]] const InputSnapshot& input() const noexcept;
    [[nodiscard]] InteractionArbiter& interaction_arbiter() const noexcept;
    [[nodiscard]] std::size_t dropped_file_count() const noexcept;
    [[nodiscard]] std::string_view dropped_file(std::size_t index) const noexcept;

    // The render request is operation-scoped. Overlay spans are copied before
    // this call returns and are never retained from application memory.
    [[nodiscard]] Result<void>
    queue_viewport_render(Viewport& viewport, const Scene& scene, EntityId camera,
                          const ViewportRenderOptions& options = {}) noexcept;

    // Queues the viewport texture in the current Dear ImGui draw list without
    // exposing a native graphics handle to application source.
    [[nodiscard]] Result<void> draw_viewport_image(const Viewport& viewport,
                                                   Float2 top_left_screen_position,
                                                   Float2 display_size) noexcept;

  private:
    friend class app::detail::ApplicationRunner;

    class Impl;
    ApplicationUiContext(const app::detail::ApplicationFrame& frame,
                         const app::detail::ApplicationServices& services, Impl& impl) noexcept;

    Engine* engine_ = nullptr;
    Extent2D window_extent_;
    Extent2D framebuffer_extent_;
    float dpi_scale_ = 1.0F;
    const InputSnapshot* input_ = nullptr;
    InteractionArbiter* interaction_arbiter_ = nullptr;
    const std::string_view* dropped_files_ = nullptr;
    std::size_t dropped_file_count_ = 0;
    Impl* impl_ = nullptr;
};

class Application {
  public:
    virtual ~Application() noexcept = default;

    [[nodiscard]] virtual Result<void> start(ApplicationContext& context) noexcept = 0;
    [[nodiscard]] virtual Result<void> update(ApplicationUpdateContext& context) noexcept = 0;
    [[nodiscard]] virtual Result<void> build_ui(ApplicationUiContext& context) noexcept = 0;
    virtual void stop(ApplicationContext& context) noexcept = 0;
};

// Canonical standard lifecycle entry point. It owns the desktop platform,
// window, graphics context, UI integration, Engine, frame order, presentation,
// shutdown, and teardown for the duration of this call.
[[nodiscard]] Result<int> run_application(const ApplicationOptions& options,
                                          Application& application) noexcept;

} // namespace elf3d

#endif
