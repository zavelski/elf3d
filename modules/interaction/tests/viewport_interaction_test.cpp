import elf.interaction;

int elf3d_interaction_test() {
    constexpr float threshold = 4.0F;
    elf3d::interaction::ViewportInteractionState state;
    const elf3d::interaction::PointerInputSnapshot inactive;
    const elf3d::interaction::ViewportInteractionFrame inactive_frame =
        state.update(inactive, threshold);
    if (inactive_frame.drag_active || inactive_frame.pointer_captured || state.pointer_captured()) {
        return 1;
    }

    elf3d::interaction::PointerInputSnapshot input;
    input.focused = true;
    input.left_button_down = true;
    if (state.update(input, threshold).drag_active) {
        return 2;
    }

    input.hovered = true;
    input.left_button_down = false;
    static_cast<void>(state.update(input, threshold));
    input.left_button_down = true;
    input.position_pixels = {10.0F, 10.0F};
    elf3d::interaction::ViewportInteractionFrame frame = state.update(input, threshold);
    if (!frame.pending_click || frame.drag_active || !frame.pointer_captured) {
        return 3;
    }

    input.position_pixels = {12.0F, 11.0F};
    input.delta_pixels = {2.0F, 1.0F};
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
    input.position_pixels = {20.0F, 20.0F};
    input.delta_pixels = {};
    frame = state.update(input, threshold);
    if (!frame.pending_click) {
        return 6;
    }
    input.position_pixels = {30.0F, 20.0F};
    input.delta_pixels = {10.0F, 0.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::interaction::InteractionMode::orbit ||
        frame.pointer_delta_pixels != elf3d::Float2{}) {
        return 7;
    }

    input.delta_pixels = {12.0F, 5.0F};
    input.position_pixels = {42.0F, 25.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_active || frame.mode != elf3d::interaction::InteractionMode::orbit ||
        frame.pointer_delta_pixels != elf3d::Float2{12.0F, 5.0F}) {
        return 8;
    }

    input.hovered = false;
    frame = state.update(input, threshold);
    if (!frame.drag_active || !frame.pointer_captured) {
        return 9;
    }

    input.left_button_down = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || frame.drag_active || state.pointer_captured()) {
        return 10;
    }

    input.hovered = true;
    input.left_button_down = true;
    input.pan_modifier_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::interaction::InteractionMode::pan) {
        return 11;
    }
    state.cancel();
    if (state.pointer_captured()) {
        return 12;
    }

    input.left_button_down = false;
    input.middle_button_down = false;
    input.pan_modifier_down = false;
    static_cast<void>(state.update(input, threshold));
    input.middle_button_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::interaction::InteractionMode::pan) {
        return 13;
    }
    input.focused = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || state.pointer_captured()) {
        return 14;
    }

    input.focused = true;
    input.hovered = true;
    input.middle_button_down = false;
    input.right_button_down = false;
    static_cast<void>(state.update(input, threshold));
    input.right_button_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::interaction::InteractionMode::pan) {
        return 15;
    }
    input.right_button_down = false;
    static_cast<void>(state.update(input, threshold));

    input.left_button_down = false;
    input.zoom_modifier_down = true;
    static_cast<void>(state.update(input, threshold));
    input.left_button_down = true;
    frame = state.update(input, threshold);
    if (!frame.drag_started || frame.mode != elf3d::interaction::InteractionMode::zoom) {
        return 16;
    }
    state.cancel();

    input = {};
    input.focused = true;
    input.hovered = true;
    static_cast<void>(state.update(input, threshold));
    input.left_button_down = true;
    input.position_pixels = {10.0F, 10.0F};
    static_cast<void>(state.update(input, threshold));
    input.position_pixels = {20.0F, 10.0F};
    input.delta_pixels = {10.0F, 0.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::interaction::InteractionMode::orbit || !frame.pointer_captured) {
        return 17;
    }
    input.right_button_down = true;
    input.delta_pixels = {1.0F, 0.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_active || frame.mode != elf3d::interaction::InteractionMode::orbit ||
        !frame.pointer_captured) {
        return 18;
    }
    input.left_button_down = false;
    frame = state.update(input, threshold);
    if (frame.drag_ended || !frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::interaction::InteractionMode::pan || !frame.pointer_captured ||
        frame.pointer_delta_pixels != elf3d::Float2{}) {
        return 19;
    }
    input.right_button_down = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || frame.pointer_captured || state.pointer_captured()) {
        return 20;
    }

    input = {};
    input.focused = true;
    input.hovered = true;
    input.right_button_down = true;
    input.position_pixels = {10.0F, 10.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::interaction::InteractionMode::pan || !frame.pointer_captured) {
        return 21;
    }
    input.left_button_down = true;
    input.delta_pixels = {0.0F, 1.0F};
    frame = state.update(input, threshold);
    if (!frame.drag_active || frame.mode != elf3d::interaction::InteractionMode::pan ||
        !frame.pointer_captured) {
        return 22;
    }
    input.right_button_down = false;
    frame = state.update(input, threshold);
    if (frame.drag_ended || !frame.drag_started || !frame.drag_active ||
        frame.mode != elf3d::interaction::InteractionMode::orbit || !frame.pointer_captured ||
        frame.pointer_delta_pixels != elf3d::Float2{}) {
        return 23;
    }
    input.left_button_down = false;
    frame = state.update(input, threshold);
    if (!frame.drag_ended || frame.pointer_captured || state.pointer_captured()) {
        return 24;
    }

    return 0;
}
