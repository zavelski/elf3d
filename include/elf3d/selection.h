#ifndef ELF3D_SELECTION_H
#define ELF3D_SELECTION_H

#include <elf3d/math/value_types.h>
#include <elf3d/picking.h>

namespace elf3d {

struct SelectionSettings {
    // Viewport render-target pixels from the initial left press to remain a click.
    float click_drag_threshold_pixels = 4.0F;
    Color4 highlight_color{1.0F, 0.55F, 0.05F, 1.0F};
    float highlight_strength = 0.45F;

    bool operator==(const SelectionSettings &) const = default;
};

struct SelectionSnapshot {
    bool has_selection = false;
    bool has_pick_hit = false;
    EntityId entity;
    PickHit pick_hit;

    bool operator==(const SelectionSnapshot &) const = default;
};

} // namespace elf3d

#endif
