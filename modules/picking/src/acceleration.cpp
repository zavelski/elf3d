#include "acceleration_detail.h"

#include <elf3d/internal/picking.h>

#include <elf3d/core/assert.h>
#include <elf3d/core/result.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>

#include "geometry_detail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <elf3d/internal/clipping.h>
#include <elf3d/internal/math.h>
#include <elf3d/internal/scene.h>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::picking::geometry_detail {

[[nodiscard]] bool valid_bounds(Bounds3 bounds) noexcept;
[[nodiscard]] Bounds3 triangle_bounds(Float3 a, Float3 b, Float3 c) noexcept;
[[nodiscard]] Float3 triangle_centroid(Float3 a, Float3 b, Float3 c) noexcept;
[[nodiscard]] Bounds3 bounds_around_point(Float3 point) noexcept;
[[nodiscard]] Bounds3 merge_bounds(Bounds3 bounds, Bounds3 other) noexcept;
[[nodiscard]] Bounds3 merge_bounds(Bounds3 bounds, Float3 point) noexcept;
[[nodiscard]] double axis_value(Float3 value, int axis) noexcept;
[[nodiscard]] int longest_axis(Bounds3 bounds) noexcept;
[[nodiscard]] Bounds3 transform_bounds(Bounds3 local_bounds, const Float4x4& world) noexcept;
[[nodiscard]] bool validate_pick_hit(const PickHit& hit) noexcept;
[[nodiscard]] Result<Ray3> make_picking_ray(const scene::Storage& scene, EntityId camera,
                                            Extent2D extent, Float2 position_pixels);
[[nodiscard]] Result<void> validate_refinement_request(const Ray3& ray,
                                                       const PickCandidate& candidate) noexcept;
[[nodiscard]] Result<std::optional<scene::RuntimePrimitiveView>>
refinement_primitive(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                     const PickCandidate& candidate);
[[nodiscard]] Result<std::optional<std::pair<Float4x4, Ray3>>>
refinement_transform(const scene::Storage& scene, EntityId entity, const Ray3& world_ray);
void reset_latest_statistics(PickingStatistics& statistics,
                             std::uint64_t cached_mesh_bvhs) noexcept;
[[nodiscard]] bool accept_refined_position(const clipping::ClippingFilter& filter,
                                           Float3 world_position,
                                           PickingStatistics& statistics) noexcept;

} // namespace elf3d::picking::geometry_detail

namespace elf3d::picking::acceleration_detail {
using geometry_detail::axis_value;
using geometry_detail::bounds_around_point;
using geometry_detail::longest_axis;
using geometry_detail::merge_bounds;
using geometry_detail::triangle_bounds;
using geometry_detail::triangle_centroid;
using geometry_detail::valid_bounds;
namespace {

constexpr std::uint32_t bvh_leaf_size = 8;

[[nodiscard]] Result<TriangleReference>
make_triangle_reference(const scene::RuntimePrimitiveView& primitive, std::size_t triangle_index)
{
    const std::span<const std::uint32_t> indices = primitive.indices();
    const std::uint32_t i0 = indices[triangle_index * 3U];
    const std::uint32_t i1 = indices[triangle_index * 3U + 1U];
    const std::uint32_t i2 = indices[triangle_index * 3U + 2U];
    if (static_cast<std::size_t>(i0) >= primitive.vertex_count() ||
        static_cast<std::size_t>(i1) >= primitive.vertex_count() ||
        static_cast<std::size_t>(i2) >= primitive.vertex_count()) {
        return Error{ErrorCode::mesh_index_out_of_range,
                     "Picking BVH encountered an index outside the vertex range"};
    }

    const Float3 a = primitive.position(i0);
    const Float3 b = primitive.position(i1);
    const Float3 c = primitive.position(i2);
    const Bounds3 bounds = triangle_bounds(a, b, c);
    if (!valid_bounds(bounds)) {
        return Error{ErrorCode::invalid_mesh_data,
                     "Picking BVH encountered non-finite triangle bounds"};
    }
    return TriangleReference{static_cast<std::uint32_t>(triangle_index), bounds,
                             triangle_centroid(a, b, c)};
}

[[nodiscard]] std::uint32_t build_node(MeshAcceleration& acceleration, std::uint32_t first,
                                       std::uint32_t count)
{
    const std::uint32_t node_index = static_cast<std::uint32_t>(acceleration.nodes.size());
    acceleration.nodes.push_back(BvhNode{});

    Bounds3 node_bounds{};
    Bounds3 centroid_bounds{};
    bool has_bounds = false;
    for (std::uint32_t index = first; index < first + count; ++index) {
        const TriangleReference& triangle =
            acceleration.triangles[acceleration.triangle_order[index]];
        if (!has_bounds) {
            node_bounds = triangle.bounds;
            centroid_bounds = bounds_around_point(triangle.centroid);
            has_bounds = true;
            continue;
        }
        node_bounds = merge_bounds(node_bounds, triangle.bounds);
        centroid_bounds = merge_bounds(centroid_bounds, triangle.centroid);
    }

    BvhNode node;
    ELF3D_ASSERT(has_bounds);
    node.bounds = node_bounds;
    if (count <= bvh_leaf_size) {
        node.first_triangle = first;
        node.triangle_count = count;
        node.is_leaf = true;
        acceleration.nodes[node_index] = node;
        return node_index;
    }

    const int axis = longest_axis(centroid_bounds);
    std::stable_sort(acceleration.triangle_order.begin() + first,
                     acceleration.triangle_order.begin() + first + count,
                     [&](std::uint32_t left, std::uint32_t right) {
                         const double left_value =
                             axis_value(acceleration.triangles[left].centroid, axis);
                         const double right_value =
                             axis_value(acceleration.triangles[right].centroid, axis);
                         if (left_value == right_value) {
                             return left < right;
                         }
                         return left_value < right_value;
                     });
    const std::uint32_t left_count = count / 2;
    node.left = build_node(acceleration, first, left_count);
    node.right = build_node(acceleration, first + left_count, count - left_count);
    node.is_leaf = false;
    acceleration.nodes[node_index] = node;
    return node_index;
}

} // namespace

[[nodiscard]] Result<MeshAcceleration>
build_acceleration(const scene::RuntimePrimitiveView& primitive)
{
    const std::span<const std::uint32_t> indices = primitive.indices();
    if (indices.empty() || indices.size() % 3 != 0 || primitive.vertex_count() == 0) {
        return Error{ErrorCode::invalid_mesh_data,
                     "Picking BVH construction requires indexed triangle mesh data"};
    }
    MeshAcceleration acceleration;
    const std::size_t triangle_count = indices.size() / 3;
    if (triangle_count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return Error{ErrorCode::picking_acceleration_failed,
                     "Picking BVH triangle count exceeds internal limits"};
    }

    acceleration.triangles.reserve(triangle_count);
    acceleration.triangle_order.reserve(triangle_count);
    acceleration.nodes.reserve(triangle_count * 2);
    for (std::size_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index) {
        const Result<TriangleReference> triangle =
            make_triangle_reference(primitive, triangle_index);
        if (!triangle) {
            return triangle.error();
        }
        acceleration.triangles.push_back(triangle.value());
        acceleration.triangle_order.push_back(static_cast<std::uint32_t>(triangle_index));
    }
    if (acceleration.triangles.empty()) {
        return Error{ErrorCode::invalid_mesh_data,
                     "Picking BVH construction requires at least one triangle"};
    }

    const std::uint32_t root =
        build_node(acceleration, 0, static_cast<std::uint32_t>(acceleration.triangle_order.size()));
    (void)root;
    return acceleration;
}

} // namespace elf3d::picking::acceleration_detail
