#include "application_frame.h"

#include <elf3d/core/assert.h>

namespace elf3d {

ApplicationContext::ApplicationContext(const app::detail::ApplicationFrame& frame,
                                       const app::detail::ApplicationServices& services) noexcept
    : engine_(&services.engine), window_extent_(frame.window.window_extent),
      framebuffer_extent_(frame.window.framebuffer_extent), dpi_scale_(frame.window.dpi_scale),
      interaction_arbiter_(&services.interaction_arbiter),
      graphics_context_(&services.graphics_context)
{
}

Engine& ApplicationContext::engine() const noexcept
{
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

Extent2D ApplicationContext::window_extent() const noexcept
{
    return window_extent_;
}

Extent2D ApplicationContext::framebuffer_extent() const noexcept
{
    return framebuffer_extent_;
}

float ApplicationContext::dpi_scale() const noexcept
{
    return dpi_scale_;
}

InteractionArbiter& ApplicationContext::interaction_arbiter() const noexcept
{
    ELF3D_ASSERT(interaction_arbiter_ != nullptr);
    return *interaction_arbiter_;
}

const GraphicsContextSnapshot& ApplicationContext::graphics_context() const noexcept
{
    ELF3D_ASSERT(graphics_context_ != nullptr);
    return *graphics_context_;
}

ApplicationUpdateContext::ApplicationUpdateContext(
    const app::detail::ApplicationFrame& frame,
    const app::detail::ApplicationServices& services) noexcept
    : engine_(&services.engine), elapsed_seconds_(frame.elapsed_seconds),
      previous_frame_statistics_(frame.previous_frame_statistics),
      window_extent_(frame.window.window_extent),
      framebuffer_extent_(frame.window.framebuffer_extent), dpi_scale_(frame.window.dpi_scale),
      focused_(frame.window.focused), input_(frame.input),
      interaction_arbiter_(&services.interaction_arbiter),
      dropped_files_(frame.dropped_files.data()), dropped_file_count_(frame.dropped_files.size())
{
}

Engine& ApplicationUpdateContext::engine() const noexcept
{
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

std::optional<ApplicationFrameStatistics>
ApplicationUpdateContext::previous_frame_statistics() const noexcept
{
    return previous_frame_statistics_;
}

double ApplicationUpdateContext::elapsed_seconds() const noexcept
{
    return elapsed_seconds_;
}

Extent2D ApplicationUpdateContext::window_extent() const noexcept
{
    return window_extent_;
}

Extent2D ApplicationUpdateContext::framebuffer_extent() const noexcept
{
    return framebuffer_extent_;
}

float ApplicationUpdateContext::dpi_scale() const noexcept
{
    return dpi_scale_;
}

bool ApplicationUpdateContext::focused() const noexcept
{
    return focused_;
}

const InputSnapshot& ApplicationUpdateContext::input() const noexcept
{
    ELF3D_ASSERT(input_ != nullptr);
    return *input_;
}

InteractionArbiter& ApplicationUpdateContext::interaction_arbiter() const noexcept
{
    ELF3D_ASSERT(interaction_arbiter_ != nullptr);
    return *interaction_arbiter_;
}

std::size_t ApplicationUpdateContext::dropped_file_count() const noexcept
{
    return dropped_file_count_;
}

std::string_view ApplicationUpdateContext::dropped_file(std::size_t index) const noexcept
{
    ELF3D_ASSERT(index < dropped_file_count_);
    return dropped_files_[index];
}

void ApplicationUpdateContext::request_exit() noexcept
{
    exit_requested_ = true;
}

void ApplicationUpdateContext::set_presentation_mode(PresentationMode mode) noexcept
{
    requested_presentation_mode_ = mode;
}

bool ApplicationUpdateContext::exit_requested() const noexcept
{
    return exit_requested_;
}

std::optional<PresentationMode>
ApplicationUpdateContext::requested_presentation_mode() const noexcept
{
    return requested_presentation_mode_;
}

} // namespace elf3d
