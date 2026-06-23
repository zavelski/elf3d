#include <elf3d/interaction/viewport_interaction.h>

int main() {
    constexpr float threshold = 4.0F;
    elf3d::interaction::ViewportInteractionState state;
    const elf3d::ViewportInput inactive;
    const elf3d::interaction::ViewportInteractionFrame inactive_frame =
        state.update(inactive, threshold);
    if (inactive_frame.drag_active || inactive_frame.pointer_captured || state.pointer_captured()) {
        return 1;
    }

    elf3d::ViewportInput input;
    input.is_focused = true;
    input.left_button_down = true;
    if (state.update(input, threshold).drag_active) {
        return 2;
    }

    input.is_hovered = true;
    input.left_button_down = false;
    static_cast<void>(state.update(input, threshold));
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    elf3d::interaction::ViewportInteractionFrame frame = state.update(input, threshold);
    if (!frame.pending_click || frame.drag_active || !frame.pointer_captured) {
        return 3;
    }

    input.pointer_position_pixels = {12.0F, 11.0F};
    input.pointer_delta_pixels = {2.0F, 1.0F};
    frame = state.update(input, threshold);
    if (!frame.pending_click || frame.drag_active) {
        return 4;
    }

    input.left_button_down = false;
    frame = state.update(input, threshold);
    if (!frame.click_released || frame.pointer_captured || state.pointer_captured()) {
        return 5;
    }

    input.left_button_down = true;
    input.pointer_position_pixels = {20.0F, 20.0F};
    input.pointer_delta_pixels = {};
    frame = state.update(input, threshold);
    if (!frame.pending_click) {
        return 6;
    }
    input.pointer_position_pixels = {30.0F, 20.0F};
    input.pointer_delta_pixels = {10.0F, 0.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::NavigationInteractionMode::orbit ||
        frame.pointer_delta_pixels != elf3d::Float2{}) {
        return 7;
    }

    input.shift_down = true;
    input.pointer_delta_pixels = {12.0F, 5.0F};
    input.pointer_position_pixels = {42.0F, 25.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_active || frame.mode != elf3d::NavigationInteractionMode::orbit ||
        frame.pointer_delta_pixels != elf3d::Float2{12.0F, 5.0F}) {
        return 8;
    }

    input.is_hovered = false;
    frame = state.update(input, threshold);
    if (!frame.drag_active || !frame.pointer_captured) {
        return 9;
    }

    input.left_button_down = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || frame.drag_active || state.pointer_captured()) {
        return 10;
    }

    input.is_hovered = true;
    input.left_button_down = true;
    input.shift_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::NavigationInteractionMode::pan) {
        return 11;
    }
    state.cancel();
    if (state.pointer_captured()) {
        return 12;
    }

    input.left_button_down = false;
    input.middle_button_down = false;
    static_cast<void>(state.update(input, threshold));
    input.middle_button_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::NavigationInteractionMode::pan) {
        return 13;
    }
    input.is_focused = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || state.pointer_captured()) {
        return 14;
    }

    return 0;
}
