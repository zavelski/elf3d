#ifndef ELF3D_APP_INPUT_H
#define ELF3D_APP_INPUT_H

#include <elf3d/math/value_types.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace elf3d {

enum class InputButton : std::uint8_t {
    left,
    middle,
    right,
    count,
};

enum class InputKey : std::uint8_t {
    a,
    b,
    c,
    d,
    e,
    f,
    g,
    h,
    i,
    j,
    k,
    l,
    m,
    n,
    o,
    p,
    q,
    r,
    s,
    t,
    u,
    v,
    w,
    x,
    y,
    z,
    space,
    escape,
    enter,
    tab,
    home,
    delete_key,
    count,
};

struct InputTransition {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint8_t click_count = 0;

    bool operator==(const InputTransition&) const = default;
};

struct InputModifiers {
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool super = false;

    bool operator==(const InputModifiers&) const = default;
};

struct InputSnapshot {
    // Pointer coordinates are in logical window coordinates. Mapping to a
    // viewport render target occurs only through an interaction region.
    Float2 pointer_position_window;
    Float2 pointer_delta_window;
    Float2 wheel_delta;

    std::array<InputTransition, static_cast<std::size_t>(InputButton::count)> buttons;
    std::array<InputTransition, static_cast<std::size_t>(InputKey::count)> keys;
    InputModifiers modifiers;

    bool window_focused = false;
    bool pointer_inside_window = false;
    bool text_input_owned = false;
    bool capture_lost = false;
    float dpi_scale = 1.0F;

    [[nodiscard]] const InputTransition& button(InputButton value) const noexcept {
        return buttons[static_cast<std::size_t>(value)];
    }

    [[nodiscard]] const InputTransition& key(InputKey value) const noexcept {
        return keys[static_cast<std::size_t>(value)];
    }

    bool operator==(const InputSnapshot&) const = default;
};

} // namespace elf3d

#endif
