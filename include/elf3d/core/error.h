#ifndef ELF3D_CORE_ERROR_H
#define ELF3D_CORE_ERROR_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace elf3d {

enum class ErrorCode {
    none,
    invalid_argument,
    missing_graphics_procedure_loader,
    graphics_initialization_failed,
    unsupported_graphics_version,
    graphics_context_unavailable,
    graphics_thread_violation,
    graphics_shutdown,
    invalid_viewport_dimensions,
    framebuffer_creation_failed,
    framebuffer_incomplete,
    texture_unavailable,
    backend_mismatch,
    unexpected_exception,
};

class Error final {
  public:
    static constexpr std::size_t message_capacity = 255;

    constexpr Error() noexcept = default;

    constexpr Error(ErrorCode code, std::string_view message) noexcept : code_(code) {
        const std::size_t count = std::min(message.size(), message_capacity);
        for (std::size_t index = 0; index < count; ++index) {
            message_[index] = message[index];
        }
        message_[count] = '\0';
    }

    [[nodiscard]] constexpr ErrorCode code() const noexcept {
        return code_;
    }

    [[nodiscard]] constexpr const char *message() const noexcept {
        return message_.data();
    }

    [[nodiscard]] constexpr bool has_error() const noexcept {
        return code_ != ErrorCode::none;
    }

  private:
    ErrorCode code_ = ErrorCode::none;
    std::array<char, message_capacity + 1> message_{};
};

} // namespace elf3d

#endif
