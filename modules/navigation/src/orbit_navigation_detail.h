#pragma once

#include <elf3d/math/detail/glm_helpers.h>

namespace elf3d::navigation::navigation_detail {

inline constexpr float pi = 3.14159265358979323846F;
inline constexpr float half_pi = pi * 0.5F;
inline constexpr float minimum_axis_length = 0.000001F;
inline constexpr float matrix_comparison_epsilon = 0.0001F;
inline constexpr float fit_margin = 1.05F;
inline constexpr float maximum_pointer_delta_pixels = 10000.0F;
inline constexpr float wheel_dolly_speed_scale = 0.5F;
inline constexpr float right_button_pan_speed_scale = 0.5F;
inline constexpr float keyboard_forward_to_wheel_speed_scale = 0.025F;
inline constexpr float keyboard_pan_step_width_divisor = 400.0F;
inline constexpr float keyboard_reference_updates_per_second = 60.0F;
inline constexpr float maximum_keyboard_frame_delta_seconds = 0.25F;

struct BoundsInfo {
    bool has_bounds = false;
    math::Vector3 center{};
    float radius = 1.0F;
};

struct CameraBasis {
    math::Vector3 position{};
    math::Vector3 right{1.0F, 0.0F, 0.0F};
    math::Vector3 up{0.0F, 1.0F, 0.0F};
    math::Vector3 forward{0.0F, 0.0F, -1.0F};
};

struct PanOffset {
    bool has_value = false;
    math::Vector3 value{};
};

struct KeyboardPanDelta {
    float view_horizontal_pixels = 0.0F;
    float world_vertical_pixels = 0.0F;
};

struct DistanceLimits {
    float minimum = 0.001F;
    float maximum = 1.0e9F;
};

} // namespace elf3d::navigation::navigation_detail
