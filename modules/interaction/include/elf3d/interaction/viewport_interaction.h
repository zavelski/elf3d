#ifndef ELF3D_INTERACTION_VIEWPORT_INTERACTION_H
#define ELF3D_INTERACTION_VIEWPORT_INTERACTION_H

#include <elf3d/navigation.h>

namespace elf3d::interaction {

enum class PointerButton {
    none,
    left,
    middle,
};

struct ViewportInteractionFrame {
    bool left_pressed = false;
    bool left_released = false;
    bool middle_pressed = false;
    bool middle_released = false;

    bool drag_started = false;
    bool drag_active = false;
    bool drag_ended = false;
    bool pending_click = false;
    bool click_released = false;
    bool click_cancelled = false;

    NavigationInteractionMode mode = NavigationInteractionMode::none;
    PointerButton active_button = PointerButton::none;
    bool pointer_captured = false;
    Float2 pointer_delta_pixels;
    Float2 click_position_pixels;
};

class ViewportInteractionState final {
  public:
    [[nodiscard]] ViewportInteractionFrame update(const ViewportInput &input,
                                                  float click_drag_threshold_pixels) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool pointer_captured() const noexcept;
    [[nodiscard]] NavigationInteractionMode mode() const noexcept;

  private:
    bool previous_left_down_ = false;
    bool previous_middle_down_ = false;
    bool pending_left_click_ = false;
    Float2 pending_left_press_position_pixels_;
    PointerButton active_button_ = PointerButton::none;
    NavigationInteractionMode mode_ = NavigationInteractionMode::none;
};

} // namespace elf3d::interaction

#endif
