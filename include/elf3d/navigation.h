#ifndef ELF3D_NAVIGATION_H
#define ELF3D_NAVIGATION_H

#include <elf3d/math/value_types.h>

#include <optional>

namespace elf3d {

struct ViewportInput {
    Float2 pointer_position_pixels;
    Float2 pointer_delta_pixels;

    // Elapsed host-frame time. The default preserves a 60 Hz integration step.
    float frame_delta_seconds = 1.0F / 60.0F;
    // One logical wheel unit corresponds to one host integration wheel notch.
    float wheel_delta = 0.0F;

    bool is_hovered = false;
    bool is_focused = false;

    bool left_button_down = false;
    bool middle_button_down = false;
    bool right_button_down = false;

    bool shift_down = false;
    bool control_down = false;
    bool alt_down = false;
    bool x_down = false;
    bool z_down = false;
    bool space_down = false;

    // Current-frame keyboard navigation state. Host integrations should set
    // these from physical key-down polling, not queued key-repeat events.
    bool w_pressed = false;
    bool s_pressed = false;
    bool a_pressed = false;
    bool d_pressed = false;
    bool q_pressed = false;
    bool e_pressed = false;
};

struct OrbitNavigationSettings {
    // Radians applied per pointer pixel during orbit drags.
    float orbit_sensitivity = 0.005F;
    // Multiplier applied to screen-space pan world units per pixel.
    float pan_sensitivity = 1.0F;
    // Exponential wheel multiplier used as distance *= exp(-wheel * sensitivity).
    float zoom_sensitivity = 0.12F;

    float minimum_distance = 0.001F;
    float maximum_distance = 1.0e9F;
    // Minimum continuous dolly/pan scale as a fraction of the local working depth.
    float minimum_motion_scale = 0.5F;

    float minimum_pitch_radians = -1.55334306F;
    float maximum_pitch_radians = 1.55334306F;

    // When false, orbit drags make the visible model follow vertical mouse movement.
    bool invert_vertical_orbit = false;
    // When enabled, orbit initiation performs one reduced depth query so the
    // surface under the pointer remains stable during the drag.
    bool focus_depth_anchor_enabled = true;
};

enum class NavigationInteractionMode {
    none,
    orbit,
    pan,
    zoom,
};

struct NavigationSnapshot {
    Float3 pivot;
    float distance = 0.0F;
    float yaw_radians = 0.0F;
    float pitch_radians = 0.0F;

    bool is_orbiting = false;
    bool is_panning = false;
    bool is_pointer_captured = false;
    NavigationInteractionMode interaction_mode = NavigationInteractionMode::none;
};

} // namespace elf3d

#endif
