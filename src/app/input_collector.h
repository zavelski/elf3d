#ifndef ELF3D_APP_INPUT_COLLECTOR_H
#define ELF3D_APP_INPUT_COLLECTOR_H

#include <elf3d/app/input.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct GLFWwindow;

namespace elf3d::app::detail {

struct WindowSnapshot final {
    Extent2D window_extent;
    Extent2D framebuffer_extent;
    float dpi_scale = 1.0F;
    bool focused = false;
};

[[nodiscard]] InputTransition normalized_input_transition(bool down, bool previous_down) noexcept;
void normalize_window_input(InputSnapshot& snapshot, const WindowSnapshot& window_snapshot,
                            bool text_input_owned, bool previous_focused) noexcept;
void accumulate_wheel_delta(Float2& accumulated, double x_offset, double y_offset) noexcept;
[[nodiscard]] Float2 take_wheel_delta(Float2& accumulated) noexcept;

class InputCollector final {
  public:
    void install(GLFWwindow& window) noexcept;
    [[nodiscard]] InputSnapshot capture(const WindowSnapshot& window_snapshot,
                                        bool text_input_owned) noexcept;
    [[nodiscard]] std::span<const std::string_view> dropped_files() const noexcept;

    InputCollector() = default;
    InputCollector(const InputCollector&) = delete;
    InputCollector& operator=(const InputCollector&) = delete;

  private:
    [[nodiscard]] bool glfw_key_down(int key) const noexcept;
    void prepare_dropped_files();
    void capture_pointer(InputSnapshot& snapshot, const WindowSnapshot& window_snapshot) noexcept;
    void capture_buttons(InputSnapshot& snapshot) noexcept;
    void capture_keys(InputSnapshot& snapshot) noexcept;
    void capture_modifiers(InputSnapshot& snapshot) noexcept;
    void add_wheel(double x_offset, double y_offset) noexcept;
    void add_dropped_paths(int path_count, const char** paths);

    static void scroll_callback(GLFWwindow* window, double x_offset, double y_offset) noexcept;
    static void drop_callback(GLFWwindow* window, int path_count, const char** paths) noexcept;

    GLFWwindow* window_ = nullptr;
    std::array<bool, static_cast<std::size_t>(InputButton::count)> previous_buttons_{};
    std::array<std::chrono::steady_clock::time_point, static_cast<std::size_t>(InputButton::count)>
        last_click_time_{};
    std::array<Float2, static_cast<std::size_t>(InputButton::count)> last_click_position_{};
    std::array<bool, static_cast<std::size_t>(InputButton::count)> has_click_sample_{};
    std::array<bool, static_cast<std::size_t>(InputKey::count)> previous_keys_{};
    Float2 previous_pointer_position_;
    Float2 wheel_delta_;
    bool has_pointer_sample_ = false;
    bool previous_focused_ = false;
    std::vector<std::string> pending_dropped_files_;
    std::vector<std::string> current_dropped_files_;
    std::vector<std::string_view> dropped_file_views_;
};

} // namespace elf3d::app::detail

#endif
