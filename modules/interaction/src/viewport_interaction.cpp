module;

module elf.interaction;

namespace elf3d::interaction {
namespace {

[[nodiscard]] bool active_button_released(PointerButton button,
                                          const PointerInputSnapshot& input) noexcept {
    switch (button) {
    case PointerButton::left:
        return !input.left_button_down;
    case PointerButton::middle:
        return !input.middle_button_down;
    case PointerButton::right:
        return !input.right_button_down;
    case PointerButton::none:
        return false;
    }
    return false;
}

[[nodiscard]] InteractionMode left_button_mode(const PointerInputSnapshot& input) noexcept {
    if (input.pan_modifier_down) {
        return InteractionMode::pan;
    }
    if (input.zoom_modifier_down) {
        return InteractionMode::zoom;
    }
    return InteractionMode::orbit;
}

[[nodiscard]] InteractionMode button_mode(PointerButton button,
                                          const PointerInputSnapshot& input) noexcept {
    switch (button) {
    case PointerButton::left:
        return left_button_mode(input);
    case PointerButton::middle:
        return InteractionMode::pan;
    case PointerButton::right:
        return InteractionMode::pan;
    case PointerButton::none:
        return InteractionMode::none;
    }
    return InteractionMode::none;
}

[[nodiscard]] PointerButton held_button_after_release(PointerButton released,
                                                      const PointerInputSnapshot& input) noexcept {
    switch (released) {
    case PointerButton::left:
        if (input.right_button_down) {
            return PointerButton::right;
        }
        if (input.middle_button_down) {
            return PointerButton::middle;
        }
        break;
    case PointerButton::right:
        if (input.left_button_down) {
            return PointerButton::left;
        }
        if (input.middle_button_down) {
            return PointerButton::middle;
        }
        break;
    case PointerButton::middle:
        if (input.left_button_down) {
            return PointerButton::left;
        }
        if (input.right_button_down) {
            return PointerButton::right;
        }
        break;
    case PointerButton::none:
        break;
    }
    return PointerButton::none;
}

void start_drag(PointerButton button, const PointerInputSnapshot& input,
                ViewportInteractionFrame& frame, PointerButton& active_button,
                InteractionMode& mode) noexcept {
    active_button = button;
    mode = button_mode(button, input);
    frame.drag_started = true;
}

[[nodiscard]] float squared_distance(Float2 left, Float2 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

} // namespace

ViewportInteractionFrame
ViewportInteractionState::update(const PointerInputSnapshot& input,
                                 float click_drag_threshold_pixels) noexcept {
    ViewportInteractionFrame frame;
    frame.left_pressed = input.left_button_down && !previous_left_down_;
    frame.left_released = !input.left_button_down && previous_left_down_;
    frame.middle_pressed = input.middle_button_down && !previous_middle_down_;
    frame.middle_released = !input.middle_button_down && previous_middle_down_;
    frame.right_pressed = input.right_button_down && !previous_right_down_;
    frame.right_released = !input.right_button_down && previous_right_down_;

    if (!input.focused) {
        frame.drag_ended = active_button_ != PointerButton::none;
        frame.click_cancelled = pending_left_click_;
        cancel();
        previous_left_down_ = input.left_button_down;
        previous_middle_down_ = input.middle_button_down;
        previous_right_down_ = input.right_button_down;
        return frame;
    }

    const float threshold =
        click_drag_threshold_pixels >= 0.0F ? click_drag_threshold_pixels : 0.0F;
    const float threshold_squared = threshold * threshold;

    if (pending_left_click_) {
        const float movement_squared =
            squared_distance(input.position_pixels, pending_left_press_position_pixels_);
        if (input.left_button_down && input.right_button_down) {
            pending_left_click_ = false;
            start_drag(PointerButton::right, input, frame, active_button_, mode_);
        } else if (input.left_button_down && input.middle_button_down) {
            pending_left_click_ = false;
            start_drag(PointerButton::middle, input, frame, active_button_, mode_);
        } else if (input.left_button_down && movement_squared > threshold_squared) {
            pending_left_click_ = false;
            start_drag(PointerButton::left, input, frame, active_button_, mode_);
        } else if (frame.left_released) {
            frame.click_released = input.hovered && movement_squared <= threshold_squared;
            frame.click_cancelled = !frame.click_released;
            frame.click_position_pixels = input.position_pixels;
            pending_left_click_ = false;
        }
    }

    if (!pending_left_click_ && active_button_ == PointerButton::none && input.hovered) {
        if (frame.left_pressed) {
            if (input.pan_modifier_down || input.zoom_modifier_down) {
                start_drag(PointerButton::left, input, frame, active_button_, mode_);
            } else {
                pending_left_click_ = true;
                pending_left_press_position_pixels_ = input.position_pixels;
            }
        } else if (frame.middle_pressed) {
            start_drag(PointerButton::middle, input, frame, active_button_, mode_);
        } else if (frame.right_pressed) {
            start_drag(PointerButton::right, input, frame, active_button_, mode_);
        }
    }

    if (active_button_ != PointerButton::none && active_button_released(active_button_, input)) {
        const PointerButton next_button = held_button_after_release(active_button_, input);
        if (next_button != PointerButton::none) {
            start_drag(next_button, input, frame, active_button_, mode_);
        } else {
            frame.drag_ended = true;
            active_button_ = PointerButton::none;
            mode_ = InteractionMode::none;
        }
    }

    frame.drag_active = active_button_ != PointerButton::none;
    frame.pending_click = pending_left_click_;
    frame.mode = mode_;
    frame.active_button = active_button_;
    frame.pointer_captured = frame.drag_active || frame.pending_click;
    frame.pointer_delta_pixels = frame.drag_started ? Float2{} : input.delta_pixels;

    previous_left_down_ = input.left_button_down;
    previous_middle_down_ = input.middle_button_down;
    previous_right_down_ = input.right_button_down;
    return frame;
}

void ViewportInteractionState::cancel() noexcept {
    pending_left_click_ = false;
    active_button_ = PointerButton::none;
    mode_ = InteractionMode::none;
}

bool ViewportInteractionState::pointer_captured() const noexcept {
    return active_button_ != PointerButton::none || pending_left_click_;
}

InteractionMode ViewportInteractionState::mode() const noexcept {
    return mode_;
}

} // namespace elf3d::interaction
