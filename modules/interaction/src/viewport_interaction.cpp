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
    const auto first_held = [&input](PointerButton first,
                                     PointerButton second) noexcept -> PointerButton {
        if (!active_button_released(first, input)) {
            return first;
        }
        return !active_button_released(second, input) ? second : PointerButton::none;
    };
    switch (released) {
    case PointerButton::left:
        return first_held(PointerButton::right, PointerButton::middle);
    case PointerButton::right:
        return first_held(PointerButton::left, PointerButton::middle);
    case PointerButton::middle:
        return first_held(PointerButton::left, PointerButton::right);
    case PointerButton::none:
        return PointerButton::none;
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

[[nodiscard]] PointerButton pending_click_drag_button(const PointerInputSnapshot& input,
                                                      float movement_squared,
                                                      float threshold_squared) noexcept {
    if (input.left_button_down && input.right_button_down) {
        return PointerButton::right;
    }
    if (input.left_button_down && input.middle_button_down) {
        return PointerButton::middle;
    }
    if (input.left_button_down && movement_squared > threshold_squared) {
        return PointerButton::left;
    }
    return PointerButton::none;
}

[[nodiscard]] PointerButton pressed_button(const ViewportInteractionFrame& frame) noexcept {
    if (frame.left_pressed) {
        return PointerButton::left;
    }
    if (frame.middle_pressed) {
        return PointerButton::middle;
    }
    return frame.right_pressed ? PointerButton::right : PointerButton::none;
}

} // namespace

ViewportInteractionFrame ViewportInteractionState::button_transition_frame(
    const PointerInputSnapshot& input) const noexcept {
    ViewportInteractionFrame frame;
    frame.left_pressed = input.left_button_down && !previous_left_down_;
    frame.left_released = !input.left_button_down && previous_left_down_;
    frame.middle_pressed = input.middle_button_down && !previous_middle_down_;
    frame.middle_released = !input.middle_button_down && previous_middle_down_;
    frame.right_pressed = input.right_button_down && !previous_right_down_;
    frame.right_released = !input.right_button_down && previous_right_down_;
    return frame;
}

ViewportInteractionFrame
ViewportInteractionState::focused_out_frame(const PointerInputSnapshot& input,
                                            ViewportInteractionFrame frame) noexcept {
    frame.drag_ended = active_button_ != PointerButton::none;
    frame.click_cancelled = pending_left_click_;
    cancel();
    remember_buttons(input);
    return frame;
}

void ViewportInteractionState::update_pending_click(const PointerInputSnapshot& input,
                                                    float threshold_squared,
                                                    ViewportInteractionFrame& frame) noexcept {
    if (!pending_left_click_) {
        return;
    }
    const float movement_squared =
        squared_distance(input.position_pixels, pending_left_press_position_pixels_);
    const PointerButton drag_button =
        pending_click_drag_button(input, movement_squared, threshold_squared);
    if (drag_button != PointerButton::none) {
        pending_left_click_ = false;
        start_drag(drag_button, input, frame, active_button_, mode_);
        return;
    }
    if (frame.left_released) {
        frame.click_released = input.hovered && movement_squared <= threshold_squared;
        frame.click_cancelled = !frame.click_released;
        frame.click_position_pixels = input.position_pixels;
        pending_left_click_ = false;
    }
}

void ViewportInteractionState::start_pressed_interaction(const PointerInputSnapshot& input,
                                                         ViewportInteractionFrame& frame) noexcept {
    if (pending_left_click_ || active_button_ != PointerButton::none || !input.hovered) {
        return;
    }
    const PointerButton button = pressed_button(frame);
    if (button == PointerButton::none) {
        return;
    }
    if (button == PointerButton::left && !input.pan_modifier_down && !input.zoom_modifier_down) {
        pending_left_click_ = true;
        pending_left_press_position_pixels_ = input.position_pixels;
        return;
    }
    start_drag(button, input, frame, active_button_, mode_);
}

void ViewportInteractionState::finish_released_drag(const PointerInputSnapshot& input,
                                                    ViewportInteractionFrame& frame) noexcept {
    if (active_button_ == PointerButton::none || !active_button_released(active_button_, input)) {
        return;
    }
    const PointerButton next_button = held_button_after_release(active_button_, input);
    if (next_button != PointerButton::none) {
        start_drag(next_button, input, frame, active_button_, mode_);
        return;
    }
    frame.drag_ended = true;
    active_button_ = PointerButton::none;
    mode_ = InteractionMode::none;
}

void ViewportInteractionState::finish_frame(const PointerInputSnapshot& input,
                                            ViewportInteractionFrame& frame) noexcept {
    frame.drag_active = active_button_ != PointerButton::none;
    frame.pending_click = pending_left_click_;
    frame.mode = mode_;
    frame.active_button = active_button_;
    frame.pointer_captured = frame.drag_active || frame.pending_click;
    frame.pointer_delta_pixels = frame.drag_started ? Float2{} : input.delta_pixels;
    remember_buttons(input);
}

void ViewportInteractionState::remember_buttons(const PointerInputSnapshot& input) noexcept {
    previous_left_down_ = input.left_button_down;
    previous_middle_down_ = input.middle_button_down;
    previous_right_down_ = input.right_button_down;
}

ViewportInteractionFrame
ViewportInteractionState::update(const PointerInputSnapshot& input,
                                 float click_drag_threshold_pixels) noexcept {
    ViewportInteractionFrame frame = button_transition_frame(input);

    if (!input.focused) {
        return focused_out_frame(input, frame);
    }

    const float threshold =
        click_drag_threshold_pixels >= 0.0F ? click_drag_threshold_pixels : 0.0F;
    const float threshold_squared = threshold * threshold;

    update_pending_click(input, threshold_squared, frame);
    start_pressed_interaction(input, frame);
    finish_released_drag(input, frame);
    finish_frame(input, frame);
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
