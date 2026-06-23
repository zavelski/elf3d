#include <elf3d/interaction/viewport_interaction.h>

namespace elf3d::interaction {
namespace {

[[nodiscard]] bool active_button_released(PointerButton button,
                                          const ViewportInput &input) noexcept {
    switch (button) {
    case PointerButton::left:
        return !input.left_button_down;
    case PointerButton::middle:
        return !input.middle_button_down;
    case PointerButton::none:
        return false;
    }
    return false;
}

[[nodiscard]] float squared_distance(Float2 left, Float2 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

} // namespace

ViewportInteractionFrame
ViewportInteractionState::update(const ViewportInput &input,
                                 float click_drag_threshold_pixels) noexcept {
    ViewportInteractionFrame frame;
    frame.left_pressed = input.left_button_down && !previous_left_down_;
    frame.left_released = !input.left_button_down && previous_left_down_;
    frame.middle_pressed = input.middle_button_down && !previous_middle_down_;
    frame.middle_released = !input.middle_button_down && previous_middle_down_;

    if (!input.is_focused) {
        frame.drag_ended = active_button_ != PointerButton::none;
        frame.click_cancelled = pending_left_click_;
        cancel();
        previous_left_down_ = input.left_button_down;
        previous_middle_down_ = input.middle_button_down;
        return frame;
    }

    const float threshold =
        click_drag_threshold_pixels >= 0.0F ? click_drag_threshold_pixels : 0.0F;
    const float threshold_squared = threshold * threshold;

    if (pending_left_click_) {
        const float movement_squared =
            squared_distance(input.pointer_position_pixels, pending_left_press_position_pixels_);
        if (input.left_button_down && movement_squared > threshold_squared) {
            pending_left_click_ = false;
            active_button_ = PointerButton::left;
            mode_ = NavigationInteractionMode::orbit;
            frame.drag_started = true;
        } else if (frame.left_released) {
            frame.click_released = input.is_hovered && movement_squared <= threshold_squared;
            frame.click_cancelled = !frame.click_released;
            frame.click_position_pixels = input.pointer_position_pixels;
            pending_left_click_ = false;
        }
    }

    if (!pending_left_click_ && active_button_ == PointerButton::none && input.is_hovered) {
        if (frame.left_pressed) {
            if (input.shift_down) {
                active_button_ = PointerButton::left;
                mode_ = NavigationInteractionMode::pan;
                frame.drag_started = true;
            } else {
                pending_left_click_ = true;
                pending_left_press_position_pixels_ = input.pointer_position_pixels;
            }
        } else if (frame.middle_pressed) {
            active_button_ = PointerButton::middle;
            mode_ = NavigationInteractionMode::pan;
            frame.drag_started = true;
        }
    }

    if (active_button_ != PointerButton::none && active_button_released(active_button_, input)) {
        frame.drag_ended = true;
        active_button_ = PointerButton::none;
        mode_ = NavigationInteractionMode::none;
    }

    frame.drag_active = active_button_ != PointerButton::none;
    frame.pending_click = pending_left_click_;
    frame.mode = mode_;
    frame.active_button = active_button_;
    frame.pointer_captured = frame.drag_active || frame.pending_click;
    frame.pointer_delta_pixels = frame.drag_started ? Float2{} : input.pointer_delta_pixels;

    previous_left_down_ = input.left_button_down;
    previous_middle_down_ = input.middle_button_down;
    return frame;
}

void ViewportInteractionState::cancel() noexcept {
    pending_left_click_ = false;
    active_button_ = PointerButton::none;
    mode_ = NavigationInteractionMode::none;
}

bool ViewportInteractionState::pointer_captured() const noexcept {
    return active_button_ != PointerButton::none || pending_left_click_;
}

NavigationInteractionMode ViewportInteractionState::mode() const noexcept {
    return mode_;
}

} // namespace elf3d::interaction
