#ifndef ELF3D_PROJECTION_H
#define ELF3D_PROJECTION_H

#include <elf3d/graphics.h>

namespace elf3d {

struct ProjectedViewportPoint {
    Float2 position_pixels;
    float depth = 0.0F;

    bool is_in_front = false;
    bool is_inside_viewport = false;

    bool operator==(const ProjectedViewportPoint&) const = default;
};

} // namespace elf3d

#endif
