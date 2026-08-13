#include "input_collector.h"

#include <elf3d/app/input.h>

#include <cstdio>
#include <limits>

namespace {

[[nodiscard]] int fail(const char* message) noexcept {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

[[nodiscard]] bool transitions_are_edge_triggered() noexcept {
    using elf3d::app::detail::normalized_input_transition;
    const elf3d::InputTransition idle = normalized_input_transition(false, false);
    const elf3d::InputTransition pressed = normalized_input_transition(true, false);
    const elf3d::InputTransition held = normalized_input_transition(true, true);
    const elf3d::InputTransition released = normalized_input_transition(false, true);
    return idle == elf3d::InputTransition{} &&
           pressed == elf3d::InputTransition{true, true, false, 0} &&
           held == elf3d::InputTransition{true, false, false, 0} &&
           released == elf3d::InputTransition{false, false, true, 0};
}

[[nodiscard]] bool wheel_is_accumulated_once() noexcept {
    elf3d::Float2 accumulated;
    elf3d::app::detail::accumulate_wheel_delta(accumulated, 0.25, 1.5);
    elf3d::app::detail::accumulate_wheel_delta(accumulated, 0.5, -0.25);
    elf3d::app::detail::accumulate_wheel_delta(accumulated, std::numeric_limits<double>::infinity(),
                                               1.0);
    const elf3d::Float2 captured = elf3d::app::detail::take_wheel_delta(accumulated);
    const elf3d::Float2 next_frame = elf3d::app::detail::take_wheel_delta(accumulated);
    return captured == elf3d::Float2{0.75F, 1.25F} && next_frame == elf3d::Float2{};
}

[[nodiscard]] bool window_metadata_is_normalized() noexcept {
    elf3d::InputSnapshot focused;
    const elf3d::app::detail::WindowSnapshot fractional_dpi{{800, 600}, {1000, 750}, 1.25F, true};
    elf3d::app::detail::normalize_window_input(focused, fractional_dpi, true, false);
    if (!focused.window_focused || !focused.text_input_owned || focused.capture_lost ||
        focused.dpi_scale != 1.25F) {
        return false;
    }

    elf3d::InputSnapshot focus_lost;
    const elf3d::app::detail::WindowSnapshot unfocused{{800, 600}, {1400, 1050}, 1.75F, false};
    elf3d::app::detail::normalize_window_input(focus_lost, unfocused, false, true);
    if (focus_lost.window_focused || focus_lost.text_input_owned || !focus_lost.capture_lost ||
        focus_lost.dpi_scale != 1.75F) {
        return false;
    }

    elf3d::InputSnapshot remains_unfocused;
    elf3d::app::detail::normalize_window_input(remains_unfocused, unfocused, false, false);
    return !remains_unfocused.capture_lost;
}

} // namespace

int main() {
    if (!transitions_are_edge_triggered()) {
        return fail("Normalized input transitions were not edge-triggered");
    }
    if (!wheel_is_accumulated_once()) {
        return fail("Fractional wheel input was not accumulated and consumed exactly once");
    }
    if (!window_metadata_is_normalized()) {
        return fail("DPI, text ownership, or focus-loss normalization failed");
    }
    return 0;
}
