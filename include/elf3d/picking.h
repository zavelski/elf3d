#ifndef ELF3D_PICKING_H
#define ELF3D_PICKING_H

#include <elf3d/assets.h>

#include <cstdint>

namespace elf3d {

// Picking positions are expressed in viewport render-target pixels: origin at
// the displayed 3D image's top-left, +X right, +Y down, valid range
// X in [0, width) and Y in [0, height).
struct Ray3 {
    Float3 origin;
    Float3 direction{0.0F, 0.0F, -1.0F};

    bool operator==(const Ray3 &) const = default;
};

struct PickOptions {
    bool respect_material_sidedness = true;

    bool operator==(const PickOptions &) const = default;
};

struct PickHit {
    EntityId entity;
    MeshHandle mesh;

    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;

    Float3 world_position;
    Float3 world_normal;
    Float3 barycentric_coordinates;

    float world_distance = 0.0F;

    bool operator==(const PickHit &) const = default;
};

struct PickingStatistics {
    std::uint64_t latest_instance_bounds_tests = 0;
    std::uint64_t latest_mesh_bounds_tests = 0;
    std::uint64_t latest_bvh_node_tests = 0;
    std::uint64_t latest_triangle_tests = 0;
    std::uint64_t latest_bvh_builds = 0;
    std::uint64_t latest_clipping_bounds_rejected = 0;
    std::uint64_t latest_clipping_hits_rejected = 0;
    std::uint64_t latest_clipping_hits_accepted = 0;
    std::uint64_t latest_gpu_requests = 0;
    std::uint64_t latest_gpu_hits = 0;
    std::uint64_t latest_gpu_misses = 0;
    std::uint64_t latest_gpu_draw_calls = 0;
    std::uint64_t latest_gpu_pixels_read = 0;
    std::uint64_t latest_gpu_pass_time_microseconds = 0;
    std::uint64_t latest_gpu_readback_time_microseconds = 0;
    std::uint64_t latest_cpu_refinements = 0;
    std::uint64_t latest_cpu_fallbacks = 0;

    std::uint64_t lifetime_bvh_builds = 0;
    std::uint64_t lifetime_gpu_requests = 0;
    std::uint64_t lifetime_gpu_hits = 0;
    std::uint64_t lifetime_gpu_misses = 0;
    std::uint64_t lifetime_cpu_refinements = 0;
    std::uint64_t lifetime_cpu_fallbacks = 0;
    std::uint64_t cached_mesh_bvhs = 0;

    bool operator==(const PickingStatistics &) const = default;
};

} // namespace elf3d

#endif
