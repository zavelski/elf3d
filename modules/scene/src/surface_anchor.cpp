module;

#include <elf3d/core/result.h>
#include <elf3d/picking.h>
#include <elf3d/surface_anchor.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

module elf.scene;

import elf.math;

namespace elf3d::scene {
namespace {

constexpr float barycentric_tolerance = 0.001F;
constexpr float minimum_normal_length = 0.000001F;

[[nodiscard]] bool valid_barycentric(Float3 barycentric) noexcept {
    if (!math::is_finite(barycentric)) {
        return false;
    }
    const float sum = barycentric.x + barycentric.y + barycentric.z;
    return std::isfinite(sum) && std::abs(sum - 1.0F) <= barycentric_tolerance &&
           barycentric.x >= -barycentric_tolerance && barycentric.y >= -barycentric_tolerance &&
           barycentric.z >= -barycentric_tolerance &&
           barycentric.x <= 1.0F + barycentric_tolerance &&
           barycentric.y <= 1.0F + barycentric_tolerance &&
           barycentric.z <= 1.0F + barycentric_tolerance;
}

[[nodiscard]] bool valid_hit(const PickHit& hit) noexcept {
    return hit.entity.is_valid() && hit.mesh.is_valid() && math::is_finite(hit.world_position) &&
           math::is_finite(hit.world_normal) && valid_barycentric(hit.barycentric_coordinates) &&
           std::isfinite(hit.world_distance) && hit.world_distance >= 0.0F;
}

[[nodiscard]] Float3 subtract(Float3 left, Float3 right) noexcept {
    return Float3{left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Float3 add(Float3 left, Float3 right) noexcept {
    return Float3{left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Float3 scale(Float3 value, float multiplier) noexcept {
    return Float3{value.x * multiplier, value.y * multiplier, value.z * multiplier};
}

[[nodiscard]] float dot(Float3 left, Float3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length(Float3 value) noexcept {
    return std::sqrt(dot(value, value));
}

[[nodiscard]] Float3 cross(Float3 left, Float3 right) noexcept {
    return Float3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                  left.x * right.y - left.y * right.x};
}

[[nodiscard]] Float3 barycentric_point(Float3 first, Float3 second, Float3 third,
                                       Float3 barycentric) noexcept {
    return add(add(scale(first, barycentric.x), scale(second, barycentric.y)),
               scale(third, barycentric.z));
}

[[nodiscard]] Float3 multiply_normal_matrix(const math::Matrix3x3& matrix, Float3 normal) noexcept {
    return Float3{
        matrix[0] * normal.x + matrix[3] * normal.y + matrix[6] * normal.z,
        matrix[1] * normal.x + matrix[4] * normal.y + matrix[7] * normal.z,
        matrix[2] * normal.x + matrix[5] * normal.y + matrix[8] * normal.z,
    };
}

struct LocalAnchorGeometry {
    Float3 position;
    Float3 normal;
};

[[nodiscard]] Result<RuntimePrimitiveView> anchor_primitive(const Storage& scene,
                                                            const SurfaceAnchor& anchor) noexcept {
    if (anchor.scene != scene.id()) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor belongs to a different scene"};
    }
    if (!valid_barycentric(anchor.barycentric_coordinates)) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor contains invalid barycentric coordinates"};
    }
    const Result<const EntityRecord*> record = scene.entity(anchor.entity);
    if (!record) {
        return record.error();
    }
    const std::optional<ModelComponent>& model = record.value()->model;
    if (!model.has_value()) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor entity no longer has model geometry"};
    }
    if (static_cast<std::size_t>(anchor.primitive_index) >= model->primitives.size()) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor primitive index is no longer valid"};
    }
    Result<RuntimePrimitiveView> primitive =
        scene.runtime_primitive(anchor.entity, anchor.primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    if (primitive.value().mesh != anchor.mesh) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor mesh no longer matches its model primitive"};
    }
    return primitive;
}

[[nodiscard]] Result<LocalAnchorGeometry>
local_anchor_geometry(const RuntimePrimitiveView& primitive, const SurfaceAnchor& anchor) noexcept {
    const std::size_t base = static_cast<std::size_t>(anchor.triangle_index) * 3U;
    const std::span<const std::uint32_t> indices = primitive.indices();
    if (base + 2U >= indices.size()) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor triangle index is no longer valid"};
    }
    const std::uint32_t first_index = indices[base];
    const std::uint32_t second_index = indices[base + 1U];
    const std::uint32_t third_index = indices[base + 2U];
    if (static_cast<std::size_t>(first_index) >= primitive.vertex_count() ||
        static_cast<std::size_t>(second_index) >= primitive.vertex_count() ||
        static_cast<std::size_t>(third_index) >= primitive.vertex_count()) {
        return Error{ErrorCode::mesh_index_out_of_range,
                     "A surface anchor triangle references a stale mesh index"};
    }

    const Float3 first = primitive.position(first_index);
    const Float3 second = primitive.position(second_index);
    const Float3 third = primitive.position(third_index);
    const Float3 local_normal = cross(subtract(second, first), subtract(third, first));
    const float normal_length = length(local_normal);
    if (!std::isfinite(normal_length) || normal_length <= minimum_normal_length) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor triangle has degenerate geometry"};
    }
    return LocalAnchorGeometry{
        barycentric_point(first, second, third, anchor.barycentric_coordinates),
        scale(local_normal, 1.0F / normal_length)};
}

} // namespace

Result<SurfaceAnchor> Storage::create_surface_anchor(const PickHit& hit) const noexcept {
    if (!valid_hit(hit)) {
        return Error{ErrorCode::invalid_surface_anchor_hit,
                     "A surface anchor requires a valid finite triangle PickHit"};
    }
    const Result<const EntityRecord*> record = entity(hit.entity);
    if (!record) {
        return record.error();
    }
    const std::optional<ModelComponent>& model = record.value()->model;
    if (!model.has_value()) {
        return Error{ErrorCode::invalid_surface_anchor_hit,
                     "A surface anchor hit must refer to a model entity"};
    }
    if (static_cast<std::size_t>(hit.primitive_index) >= model->primitives.size()) {
        return Error{ErrorCode::invalid_surface_anchor_hit,
                     "A surface anchor hit refers to an invalid model primitive"};
    }
    const Result<RuntimePrimitiveView> primitive =
        runtime_primitive(hit.entity, hit.primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    if (primitive.value().mesh != hit.mesh) {
        return Error{ErrorCode::invalid_surface_anchor_hit,
                     "A surface anchor hit mesh does not match its model primitive"};
    }
    const std::size_t base = static_cast<std::size_t>(hit.triangle_index) * 3U;
    if (base + 2U >= primitive.value().indices().size()) {
        return Error{ErrorCode::invalid_surface_anchor_hit,
                     "A surface anchor hit refers to an invalid mesh triangle"};
    }
    return SurfaceAnchor{id(),
                         hit.entity,
                         hit.mesh,
                         hit.primitive_index,
                         hit.triangle_index,
                         hit.barycentric_coordinates};
}

Result<ResolvedSurfaceAnchor>
Storage::resolve_surface_anchor(const SurfaceAnchor& anchor) const noexcept {
    const Result<RuntimePrimitiveView> primitive = anchor_primitive(*this, anchor);
    if (!primitive) {
        return primitive.error();
    }
    const Result<LocalAnchorGeometry> local = local_anchor_geometry(primitive.value(), anchor);
    if (!local) {
        return local.error();
    }

    const Result<Float4x4> world = world_matrix(anchor.entity);
    if (!world) {
        return world.error();
    }
    const Result<math::Matrix3x3> normal_transform = world_normal_matrix(anchor.entity);
    if (!normal_transform) {
        return normal_transform.error();
    }

    const Float3 world_position = math::transform_point(world.value(), local.value().position);
    Float3 world_normal = multiply_normal_matrix(normal_transform.value(), local.value().normal);
    const float world_normal_length = length(world_normal);
    if (!math::is_finite(world_position) || !std::isfinite(world_normal_length) ||
        world_normal_length <= minimum_normal_length) {
        return Error{ErrorCode::invalid_surface_anchor,
                     "A surface anchor resolved to non-finite world values"};
    }
    world_normal = scale(world_normal, 1.0F / world_normal_length);
    return ResolvedSurfaceAnchor{anchor, world_position, world_normal};
}

} // namespace elf3d::scene
