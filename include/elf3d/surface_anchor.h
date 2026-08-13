#ifndef ELF3D_SURFACE_ANCHOR_H
#define ELF3D_SURFACE_ANCHOR_H

#include <elf3d/assets.h>

#include <cstdint>

namespace elf3d {

// A persistent, Scene-owned reference to a point on one triangle. The value
// contains identities and barycentric coordinates only; resolve it whenever
// current world-space geometry is required.
struct SurfaceAnchor {
    SceneId scene;
    EntityId entity;
    MeshHandle mesh;

    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
    Float3 barycentric_coordinates;

    bool operator==(const SurfaceAnchor&) const = default;
};

struct ResolvedSurfaceAnchor {
    SurfaceAnchor anchor;
    Float3 world_position;
    Float3 world_normal;

    bool operator==(const ResolvedSurfaceAnchor&) const = default;
};

} // namespace elf3d

#endif
