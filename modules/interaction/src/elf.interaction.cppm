module;

#include <elf3d/math/value_types.h>

export module elf.interaction;

import elf.core;
import elf.math;

export namespace elf3d::interaction {

enum class InteractionMode {
    none,
    orbit,
    pan,
    zoom,
};

enum class PointerButton {
    none,
    left,
    middle,
    right,
};

struct PointerInputSnapshot {
    Float2 position_pixels;
    Float2 delta_pixels;

    bool hovered = false;
    bool focused = false;

    bool left_button_down = false;
    bool middle_button_down = false;
    bool right_button_down = false;

    bool pan_modifier_down = false;
    bool zoom_modifier_down = false;
};

struct ViewportInteractionFrame {
    bool left_pressed = false;
    bool left_released = false;
    bool middle_pressed = false;
    bool middle_released = false;
    bool right_pressed = false;
    bool right_released = false;

    bool drag_started = false;
    bool drag_active = false;
    bool drag_ended = false;
    bool pending_click = false;
    bool click_released = false;
    bool click_cancelled = false;

    InteractionMode mode = InteractionMode::none;
    PointerButton active_button = PointerButton::none;
    bool pointer_captured = false;
    Float2 pointer_delta_pixels;
    Float2 click_position_pixels;
};

class ViewportInteractionState final {
  public:
    [[nodiscard]] ViewportInteractionFrame update(const PointerInputSnapshot &input,
                                                  float click_drag_threshold_pixels) noexcept;
    void cancel() noexcept;

    [[nodiscard]] bool pointer_captured() const noexcept;
    [[nodiscard]] InteractionMode mode() const noexcept;

  private:
    bool previous_left_down_ = false;
    bool previous_middle_down_ = false;
    bool previous_right_down_ = false;
    bool pending_left_click_ = false;
    Float2 pending_left_press_position_pixels_;
    PointerButton active_button_ = PointerButton::none;
    InteractionMode mode_ = InteractionMode::none;
};

} // namespace elf3d::interaction
