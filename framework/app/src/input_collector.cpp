#include "input_collector.h"

#include <GLFW/glfw3.h>

#include <elf3d/core/assert.h>

#include <cmath>
#include <cstdint>
#include <new>

namespace elf3d::app::detail {
namespace {

[[noreturn]] void fatal_input_allocation_failure() noexcept {
    fatal_error("Elf3D application input collection memory allocation failed");
}

[[noreturn]] void fatal_unexpected_input_boundary_exception() noexcept {
    fatal_error("Elf3D application input boundary encountered an unexpected exception");
}

} // namespace

InputTransition normalized_input_transition(bool down, bool previous_down) noexcept {
    return InputTransition{down, down && !previous_down, !down && previous_down};
}

void normalize_window_input(InputSnapshot& snapshot, const WindowSnapshot& window_snapshot,
                            bool text_input_owned, bool previous_focused) noexcept {
    snapshot.window_focused = window_snapshot.focused;
    snapshot.text_input_owned = text_input_owned;
    snapshot.dpi_scale = window_snapshot.dpi_scale;
    snapshot.capture_lost = previous_focused && !window_snapshot.focused;
}

void accumulate_wheel_delta(Float2& accumulated, double x_offset, double y_offset) noexcept {
    if (!std::isfinite(x_offset) || !std::isfinite(y_offset)) {
        return;
    }
    const Float2 candidate{accumulated.x + static_cast<float>(x_offset),
                           accumulated.y + static_cast<float>(y_offset)};
    if (std::isfinite(candidate.x) && std::isfinite(candidate.y)) {
        accumulated = candidate;
    }
}

Float2 take_wheel_delta(Float2& accumulated) noexcept {
    const Float2 result = accumulated;
    accumulated = {};
    return result;
}

void InputCollector::install(GLFWwindow& window) noexcept {
    window_ = &window;
    glfwSetWindowUserPointer(window_, this);
    glfwSetScrollCallback(window_, &InputCollector::scroll_callback);
    glfwSetDropCallback(window_, &InputCollector::drop_callback);
}

void InputCollector::prepare_dropped_files() {
    current_dropped_files_.clear();
    current_dropped_files_.swap(pending_dropped_files_);
    dropped_file_views_.clear();
    dropped_file_views_.reserve(current_dropped_files_.size());
    for (const std::string& path : current_dropped_files_) {
        dropped_file_views_.emplace_back(path);
    }
}

void InputCollector::capture_pointer(InputSnapshot& snapshot,
                                     const WindowSnapshot& window_snapshot) noexcept {
    double pointer_x = 0.0;
    double pointer_y = 0.0;
    glfwGetCursorPos(window_, &pointer_x, &pointer_y);
    if (std::isfinite(pointer_x) && std::isfinite(pointer_y)) {
        snapshot.pointer_position_window =
            Float2{static_cast<float>(pointer_x), static_cast<float>(pointer_y)};
        if (has_pointer_sample_) {
            snapshot.pointer_delta_window =
                Float2{snapshot.pointer_position_window.x - previous_pointer_position_.x,
                       snapshot.pointer_position_window.y - previous_pointer_position_.y};
        }
        previous_pointer_position_ = snapshot.pointer_position_window;
        has_pointer_sample_ = true;
    } else {
        has_pointer_sample_ = false;
    }
    snapshot.pointer_inside_window = snapshot.pointer_position_window.x >= 0.0F &&
                                     snapshot.pointer_position_window.y >= 0.0F &&
                                     snapshot.pointer_position_window.x <
                                         static_cast<float>(window_snapshot.window_extent.width) &&
                                     snapshot.pointer_position_window.y <
                                         static_cast<float>(window_snapshot.window_extent.height);
    snapshot.wheel_delta = take_wheel_delta(wheel_delta_);
}

InputSnapshot InputCollector::capture(const WindowSnapshot& window_snapshot,
                                      bool text_input_owned) noexcept {
    try {
        prepare_dropped_files();
    } catch (const std::bad_alloc&) {
        fatal_input_allocation_failure();
    } catch (...) {
        fatal_unexpected_input_boundary_exception();
    }

    InputSnapshot snapshot;
    normalize_window_input(snapshot, window_snapshot, text_input_owned, previous_focused_);
    capture_pointer(snapshot, window_snapshot);
    capture_buttons(snapshot);
    capture_keys(snapshot);
    capture_modifiers(snapshot);
    previous_focused_ = window_snapshot.focused;
    return snapshot;
}

std::span<const std::string_view> InputCollector::dropped_files() const noexcept {
    return dropped_file_views_;
}

bool InputCollector::glfw_key_down(int key) const noexcept {
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

void InputCollector::capture_buttons(InputSnapshot& snapshot) noexcept {
    constexpr std::array glfw_buttons{GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_MIDDLE,
                                      GLFW_MOUSE_BUTTON_RIGHT};
    for (std::size_t index = 0; index < glfw_buttons.size(); ++index) {
        const bool down = glfwGetMouseButton(window_, glfw_buttons[index]) == GLFW_PRESS;
        snapshot.buttons[index] = normalized_input_transition(down, previous_buttons_[index]);
        if (snapshot.buttons[index].pressed) {
            const auto now = std::chrono::steady_clock::now();
            const Float2 delta{snapshot.pointer_position_window.x - last_click_position_[index].x,
                               snapshot.pointer_position_window.y - last_click_position_[index].y};
            const double elapsed =
                has_click_sample_[index]
                    ? std::chrono::duration<double>(now - last_click_time_[index]).count()
                    : 1.0;
            snapshot.buttons[index].click_count =
                has_click_sample_[index] && elapsed <= 0.35 &&
                        delta.x * delta.x + delta.y * delta.y <= 25.0F
                    ? std::uint8_t{2}
                    : std::uint8_t{1};
            last_click_time_[index] = now;
            last_click_position_[index] = snapshot.pointer_position_window;
            has_click_sample_[index] = true;
        }
        previous_buttons_[index] = down;
    }
}

void InputCollector::capture_keys(InputSnapshot& snapshot) noexcept {
    constexpr std::array glfw_keys{
        GLFW_KEY_A,    GLFW_KEY_B,      GLFW_KEY_C,      GLFW_KEY_D,     GLFW_KEY_E,
        GLFW_KEY_F,    GLFW_KEY_G,      GLFW_KEY_H,      GLFW_KEY_I,     GLFW_KEY_J,
        GLFW_KEY_K,    GLFW_KEY_L,      GLFW_KEY_M,      GLFW_KEY_N,     GLFW_KEY_O,
        GLFW_KEY_P,    GLFW_KEY_Q,      GLFW_KEY_R,      GLFW_KEY_S,     GLFW_KEY_T,
        GLFW_KEY_U,    GLFW_KEY_V,      GLFW_KEY_W,      GLFW_KEY_X,     GLFW_KEY_Y,
        GLFW_KEY_Z,    GLFW_KEY_SPACE,  GLFW_KEY_ESCAPE, GLFW_KEY_ENTER, GLFW_KEY_TAB,
        GLFW_KEY_HOME, GLFW_KEY_DELETE,
    };
    static_assert(glfw_keys.size() == static_cast<std::size_t>(InputKey::count));
    for (std::size_t index = 0; index < glfw_keys.size(); ++index) {
        const bool down = glfw_key_down(glfw_keys[index]);
        snapshot.keys[index] = normalized_input_transition(down, previous_keys_[index]);
        previous_keys_[index] = down;
    }
}

void InputCollector::capture_modifiers(InputSnapshot& snapshot) noexcept {
    snapshot.modifiers = InputModifiers{
        glfw_key_down(GLFW_KEY_LEFT_SHIFT) || glfw_key_down(GLFW_KEY_RIGHT_SHIFT),
        glfw_key_down(GLFW_KEY_LEFT_CONTROL) || glfw_key_down(GLFW_KEY_RIGHT_CONTROL),
        glfw_key_down(GLFW_KEY_LEFT_ALT) || glfw_key_down(GLFW_KEY_RIGHT_ALT),
        glfw_key_down(GLFW_KEY_LEFT_SUPER) || glfw_key_down(GLFW_KEY_RIGHT_SUPER)};
}

void InputCollector::add_wheel(double x_offset, double y_offset) noexcept {
    accumulate_wheel_delta(wheel_delta_, x_offset, y_offset);
}

void InputCollector::add_dropped_paths(int path_count, const char** paths) {
    constexpr std::size_t maximum_files_per_frame = 16;
    constexpr std::size_t maximum_path_bytes = 32768;
    for (int index = 0;
         index < path_count && pending_dropped_files_.size() < maximum_files_per_frame; ++index) {
        if (paths[index] == nullptr) {
            continue;
        }
        const std::string_view path{paths[index]};
        if (!path.empty() && path.size() <= maximum_path_bytes) {
            pending_dropped_files_.emplace_back(path);
        }
    }
}

void InputCollector::scroll_callback(GLFWwindow* window, double x_offset,
                                     double y_offset) noexcept {
    auto* collector = static_cast<InputCollector*>(glfwGetWindowUserPointer(window));
    if (collector != nullptr) {
        collector->add_wheel(x_offset, y_offset);
    }
}

void InputCollector::drop_callback(GLFWwindow* window, int path_count,
                                   const char** paths) noexcept {
    auto* collector = static_cast<InputCollector*>(glfwGetWindowUserPointer(window));
    if (collector == nullptr || path_count <= 0 || paths == nullptr) {
        return;
    }
    try {
        collector->add_dropped_paths(path_count, paths);
    } catch (const std::bad_alloc&) {
        fatal_input_allocation_failure();
    } catch (...) {
        fatal_unexpected_input_boundary_exception();
    }
}

} // namespace elf3d::app::detail
