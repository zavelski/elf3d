#pragma once

#include <elf3d/internal/picking.h>

// Private BVH storage shared by the service and its builder implementation.
namespace elf3d::picking::acceleration_detail {

struct TriangleReference {
    std::uint32_t triangle_index = 0;
    Bounds3 bounds;
    Float3 centroid;
};

struct BvhNode {
    Bounds3 bounds;
    std::uint32_t left = 0;
    std::uint32_t right = 0;
    std::uint32_t first_triangle = 0;
    std::uint32_t triangle_count = 0;
    bool is_leaf = false;
};

struct MeshAcceleration {
    std::vector<TriangleReference> triangles;
    std::vector<std::uint32_t> triangle_order;
    std::vector<BvhNode> nodes;
};

[[nodiscard]] Result<MeshAcceleration>
build_acceleration(const scene::RuntimePrimitiveView& primitive);

} // namespace elf3d::picking::acceleration_detail
