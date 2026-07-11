module;

#include <elf3d/core/assert.h>
#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

module elf.picking;

import elf.clipping;
import elf.math;
import elf.scene;

namespace elf3d::picking::geometry_detail {

[[nodiscard]] bool finite(double value) noexcept;
[[nodiscard]] bool finite_vec3(const glm::dvec3& value) noexcept;
[[nodiscard]] glm::dvec3 to_dvec3(Float3 value) noexcept;
[[nodiscard]] Float3 to_float3_checked(const glm::dvec3& value) noexcept;
[[nodiscard]] bool valid_bounds(Bounds3 bounds) noexcept;
[[nodiscard]] Bounds3 triangle_bounds(Float3 a, Float3 b, Float3 c) noexcept;
[[nodiscard]] Float3 triangle_centroid(Float3 a, Float3 b, Float3 c) noexcept;
[[nodiscard]] Bounds3 bounds_around_point(Float3 point) noexcept;
[[nodiscard]] Bounds3 merge_bounds(Bounds3 bounds, Bounds3 other) noexcept;
[[nodiscard]] Bounds3 merge_bounds(Bounds3 bounds, Float3 point) noexcept;
[[nodiscard]] double axis_value(Float3 value, int axis) noexcept;
[[nodiscard]] int longest_axis(Bounds3 bounds) noexcept;
[[nodiscard]] bool finite_matrix(const math::Matrix4& matrix) noexcept;
[[nodiscard]] Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4& world) noexcept;
[[nodiscard]] Result<Ray3> transform_ray_to_local(const Ray3& world_ray,
                                                  const math::Matrix4& inverse_world);
[[nodiscard]] bool validate_pick_hit(const PickHit& hit) noexcept;
[[nodiscard]] Result<math::Matrix4> world_matrix(const scene::Storage& scene,
                                                 EntityId entity) noexcept;
[[nodiscard]] Result<Ray3> make_picking_ray(const scene::Storage& scene, EntityId camera,
                                            Extent2D extent, Float2 position_pixels);

} // namespace elf3d::picking::geometry_detail

namespace elf3d::picking {
namespace {

constexpr std::uint32_t bvh_leaf_size = 8;

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

} // namespace

using geometry_detail::axis_value;
using geometry_detail::bounds_around_point;
using geometry_detail::finite;
using geometry_detail::finite_matrix;
using geometry_detail::finite_vec3;
using geometry_detail::longest_axis;
using geometry_detail::merge_bounds;
using geometry_detail::to_dvec3;
using geometry_detail::to_float3_checked;
using geometry_detail::transform_bounds;
using geometry_detail::transform_ray_to_local;
using geometry_detail::triangle_bounds;
using geometry_detail::triangle_centroid;
using geometry_detail::valid_bounds;
using geometry_detail::validate_pick_hit;
using geometry_detail::world_matrix;

class PickingService::Impl final {
  public:
    struct MeshCacheKey {
        std::uintptr_t engine = 0;
        std::uint64_t scene = 0;
        std::uint64_t geometry = 0;
        bool document_backed = false;

        bool operator==(const MeshCacheKey&) const = default;
    };

    struct MeshAcceleration {
        std::vector<TriangleReference> triangles;
        std::vector<std::uint32_t> triangle_order;
        std::vector<BvhNode> nodes;
    };

    struct MeshCacheEntry {
        MeshCacheKey key;
        MeshAcceleration acceleration;
    };

    [[nodiscard]] Result<Ray3> make_picking_ray(const scene::Storage& scene, EntityId camera,
                                                Extent2D extent, Float2 position_pixels) const {
        return geometry_detail::make_picking_ray(scene, camera, extent, position_pixels);
    }

    [[nodiscard]] Result<std::optional<PickHit>> pick(const scene::Storage& scene, EntityId camera,
                                                      Extent2D extent, Float2 position_pixels,
                                                      const PickOptions& options) {
        const Result<scene::VisibilityFilter> visibility =
            scene::make_visibility_filter(scene, std::nullopt);
        if (!visibility) {
            return visibility.error();
        }
        return pick(scene, camera, extent, position_pixels, options, visibility.value());
    }

    [[nodiscard]] Result<std::optional<PickHit>> pick(const scene::Storage& scene, EntityId camera,
                                                      Extent2D extent, Float2 position_pixels,
                                                      const PickOptions& options,
                                                      const scene::VisibilityFilter& visibility) {
        return pick(scene, camera, extent, position_pixels, options, visibility,
                    clipping::disabled_filter());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick(const scene::Storage& scene, EntityId camera, Extent2D extent, Float2 position_pixels,
         const PickOptions& options, const scene::VisibilityFilter& visibility,
         const clipping::ClippingFilter& clipping_filter) {
        const Result<Ray3> ray = make_picking_ray(scene, camera, extent, position_pixels);
        if (!ray) {
            return ray.error();
        }
        return pick_ray(scene, ray.value(), options, visibility, clipping_filter);
    }

    [[nodiscard]] Result<std::optional<PickHit>> refine_candidate(
        const scene::Storage& scene, EntityId camera, Extent2D extent, Float2 position_pixels,
        const PickOptions& options, const scene::VisibilityFilter& visibility,
        const clipping::ClippingFilter& clipping_filter, const PickCandidate& candidate) {
        const Result<Ray3> ray = make_picking_ray(scene, camera, extent, position_pixels);
        if (!ray) {
            return ray.error();
        }

        statistics_.latest_instance_bounds_tests = 0;
        statistics_.latest_mesh_bounds_tests = 0;
        statistics_.latest_bvh_node_tests = 0;
        statistics_.latest_triangle_tests = 0;
        statistics_.latest_bvh_builds = 0;
        statistics_.latest_clipping_bounds_rejected = 0;
        statistics_.latest_clipping_hits_rejected = 0;
        statistics_.latest_clipping_hits_accepted = 0;
        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());

        if (!is_valid_ray(ray.value())) {
            return Error{ErrorCode::invalid_picking_ray,
                         "Picking requires a finite normalized world-space ray"};
        }
        if (!candidate.entity.is_valid() || !candidate.mesh.is_valid()) {
            return Error{ErrorCode::invalid_argument,
                         "Picking refinement requires a valid entity and mesh candidate"};
        }

        const Result<const scene::EntityRecord*> record_result = scene.entity(candidate.entity);
        if (!record_result) {
            return record_result.error();
        }
        const scene::EntityRecord& record = *record_result.value();
        if (!record.model.has_value() ||
            !scene::entity_visible_in_filter(scene, visibility, candidate.entity)) {
            return std::optional<PickHit>{};
        }
        if (static_cast<std::size_t>(candidate.primitive_index) >=
            record.model->primitives.size()) {
            return std::optional<PickHit>{};
        }
        const Result<scene::RuntimePrimitiveView> primitive =
            scene.runtime_primitive(candidate.entity, candidate.primitive_index);
        if (!primitive) {
            return primitive.error();
        }
        if (primitive.value().mesh != candidate.mesh) {
            return std::optional<PickHit>{};
        }

        const Result<math::Matrix4> world = world_matrix(scene, record.id);
        if (!world || !finite_matrix(world.value())) {
            return std::optional<PickHit>{};
        }
        const float determinant = glm::determinant(math::Matrix3{world.value()});
        if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
            return std::optional<PickHit>{};
        }
        const math::Matrix4 inverse_world = glm::inverse(world.value());
        const Result<Ray3> local_ray = transform_ray_to_local(ray.value(), inverse_world);
        if (!local_ray) {
            return std::optional<PickHit>{};
        }

        if (!valid_bounds(primitive.value().bounds)) {
            return Error{ErrorCode::invalid_mesh_data,
                         "Picking encountered a mesh with invalid local bounds"};
        }

        const std::size_t base = static_cast<std::size_t>(candidate.triangle_index) * 3U;
        const std::span<const std::uint32_t> indices = primitive.value().indices();
        if (base + 2U >= indices.size()) {
            return std::optional<PickHit>{};
        }
        const std::uint32_t i0 = indices[base];
        const std::uint32_t i1 = indices[base + 1U];
        const std::uint32_t i2 = indices[base + 2U];
        if (static_cast<std::size_t>(i0) >= primitive.value().vertex_count() ||
            static_cast<std::size_t>(i1) >= primitive.value().vertex_count() ||
            static_cast<std::size_t>(i2) >= primitive.value().vertex_count()) {
            return std::optional<PickHit>{};
        }

        ++statistics_.latest_triangle_tests;
        const bool cull_back_face =
            options.respect_material_sidedness && !primitive.value().material_view.double_sided;
        std::optional<TriangleHit> triangle_hit = intersect_ray_triangle(
            local_ray.value(), primitive.value().position(i0), primitive.value().position(i1),
            primitive.value().position(i2), cull_back_face);
        if (!triangle_hit.has_value()) {
            return std::optional<PickHit>{};
        }
        triangle_hit->triangle_index = candidate.triangle_index;

        const math::Vector3 local_position =
            math::to_vector(local_ray.value().origin) +
            math::to_vector(local_ray.value().direction) * triangle_hit->distance;
        const math::Vector4 world_position4 = world.value() * math::Vector4{local_position, 1.0F};
        const glm::dvec3 world_position{world_position4.x, world_position4.y, world_position4.z};
        const glm::dvec3 world_distance_vector = world_position - to_dvec3(ray.value().origin);
        const double world_distance = glm::length(world_distance_vector);
        if (!finite_vec3(world_position) || !finite(world_distance) || world_distance < 0.0) {
            return std::optional<PickHit>{};
        }

        PickHit hit;
        hit.entity = record.id;
        hit.mesh = primitive.value().mesh;
        hit.primitive_index = candidate.primitive_index;
        hit.triangle_index = triangle_hit->triangle_index;
        hit.world_position = to_float3_checked(world_position);
        if (clipping_filter.has_clipping()) {
            if (!clipping::contains_point(clipping_filter, hit.world_position)) {
                ++statistics_.latest_clipping_hits_rejected;
                return std::optional<PickHit>{};
            }
            ++statistics_.latest_clipping_hits_accepted;
        }

        const math::Matrix3 normal_transform =
            glm::transpose(glm::inverse(math::Matrix3{world.value()}));
        math::Vector3 world_normal =
            normal_transform * math::to_vector(triangle_hit->geometric_normal);
        const float world_normal_length = glm::length(world_normal);
        if (!std::isfinite(world_normal_length) || world_normal_length <= 0.000001F) {
            return std::optional<PickHit>{};
        }
        world_normal /= world_normal_length;
        hit.world_normal = math::to_float3(world_normal);
        hit.barycentric_coordinates = triangle_hit->barycentric_coordinates;
        hit.world_distance = static_cast<float>(world_distance);
        if (!validate_pick_hit(hit)) {
            return Error{ErrorCode::invalid_picking_ray,
                         "Picking refinement produced a non-finite or invalid hit result"};
        }
        return std::optional<PickHit>{hit};
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options) {
        const Result<scene::VisibilityFilter> visibility =
            scene::make_visibility_filter(scene, std::nullopt);
        if (!visibility) {
            return visibility.error();
        }
        return pick_ray(scene, ray, options, visibility.value());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options,
             const scene::VisibilityFilter& visibility) {
        return pick_ray(scene, ray, options, visibility, clipping::disabled_filter());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options,
             const scene::VisibilityFilter& visibility,
             const clipping::ClippingFilter& clipping_filter) {
        statistics_.latest_instance_bounds_tests = 0;
        statistics_.latest_mesh_bounds_tests = 0;
        statistics_.latest_bvh_node_tests = 0;
        statistics_.latest_triangle_tests = 0;
        statistics_.latest_bvh_builds = 0;
        statistics_.latest_clipping_bounds_rejected = 0;
        statistics_.latest_clipping_hits_rejected = 0;
        statistics_.latest_clipping_hits_accepted = 0;
        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());

        if (!is_valid_ray(ray)) {
            return Error{ErrorCode::invalid_picking_ray,
                         "Picking requires a finite normalized world-space ray"};
        }

        std::optional<PickHit> nearest;
        float nearest_world_distance = std::numeric_limits<float>::max();

        for (const std::optional<scene::EntityRecord>& record : scene.entities()) {
            if (!record.has_value() || !record->model.has_value() ||
                !scene::entity_visible_in_filter(scene, visibility, record->id)) {
                continue;
            }
            const Result<math::Matrix4> world = world_matrix(scene, record->id);
            if (!world || !finite_matrix(world.value())) {
                continue;
            }
            const float determinant = glm::determinant(math::Matrix3{world.value()});
            if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
                continue;
            }
            const math::Matrix4 inverse_world = glm::inverse(world.value());
            const Result<Ray3> local_ray = transform_ray_to_local(ray, inverse_world);
            if (!local_ray) {
                continue;
            }
            const math::Matrix3 normal_transform =
                glm::transpose(glm::inverse(math::Matrix3{world.value()}));

            for (std::uint32_t primitive_index = 0;
                 primitive_index < record->model->primitives.size(); ++primitive_index) {
                const Result<scene::RuntimePrimitiveView> primitive =
                    scene.runtime_primitive(record->id, primitive_index);
                if (!primitive) {
                    return primitive.error();
                }
                if (!valid_bounds(primitive.value().bounds)) {
                    return Error{ErrorCode::invalid_mesh_data,
                                 "Picking encountered a mesh with invalid local bounds"};
                }

                ++statistics_.latest_instance_bounds_tests;
                const Bounds3 world_bounds =
                    transform_bounds(primitive.value().bounds, world.value());
                if (clipping_filter.has_clipping()) {
                    const clipping::BoundsClassification classification =
                        clipping::classify_bounds(clipping_filter, world_bounds);
                    if (classification == clipping::BoundsClassification::outside) {
                        ++statistics_.latest_clipping_bounds_rejected;
                        continue;
                    }
                }
                RayBoundsHit world_bounds_hit;
                if (!intersect_ray_bounds(ray, world_bounds, world_bounds_hit)) {
                    continue;
                }

                ++statistics_.latest_mesh_bounds_tests;
                RayBoundsHit local_bounds_hit;
                if (!intersect_ray_bounds(local_ray.value(), primitive.value().bounds,
                                          local_bounds_hit)) {
                    continue;
                }

                const Result<const MeshAcceleration*> acceleration_result =
                    acceleration(scene.id(), primitive.value());
                if (!acceleration_result) {
                    return acceleration_result.error();
                }

                const bool cull_back_face = options.respect_material_sidedness &&
                                            !primitive.value().material_view.double_sided;
                const auto accept_hit = [&](const TriangleHit& hit) noexcept {
                    if (!clipping_filter.has_clipping()) {
                        return true;
                    }
                    const math::Vector3 local_position =
                        math::to_vector(local_ray.value().origin) +
                        math::to_vector(local_ray.value().direction) * hit.distance;
                    const math::Vector4 world_position4 =
                        world.value() * math::Vector4{local_position, 1.0F};
                    const Float3 world_position{world_position4.x, world_position4.y,
                                                world_position4.z};
                    const bool accepted = clipping::contains_point(clipping_filter, world_position);
                    if (accepted) {
                        ++statistics_.latest_clipping_hits_accepted;
                    } else {
                        ++statistics_.latest_clipping_hits_rejected;
                    }
                    return accepted;
                };
                const std::optional<TriangleHit> triangle_hit =
                    traverse_mesh(*acceleration_result.value(), primitive.value(),
                                  local_ray.value(), cull_back_face, accept_hit);
                if (!triangle_hit.has_value()) {
                    continue;
                }

                const math::Vector3 local_position =
                    math::to_vector(local_ray.value().origin) +
                    math::to_vector(local_ray.value().direction) * triangle_hit->distance;
                const math::Vector4 world_position4 =
                    world.value() * math::Vector4{local_position, 1.0F};
                const glm::dvec3 world_position{world_position4.x, world_position4.y,
                                                world_position4.z};
                const glm::dvec3 world_distance_vector = world_position - to_dvec3(ray.origin);
                const double world_distance = glm::length(world_distance_vector);
                if (!finite_vec3(world_position) || !finite(world_distance) ||
                    world_distance < 0.0 ||
                    world_distance >= static_cast<double>(nearest_world_distance)) {
                    continue;
                }

                math::Vector3 world_normal =
                    normal_transform * math::to_vector(triangle_hit->geometric_normal);
                const float world_normal_length = glm::length(world_normal);
                if (!std::isfinite(world_normal_length) || world_normal_length <= 0.000001F) {
                    continue;
                }
                world_normal /= world_normal_length;

                PickHit candidate;
                candidate.entity = record->id;
                candidate.mesh = primitive.value().mesh;
                candidate.primitive_index = primitive_index;
                candidate.triangle_index = triangle_hit->triangle_index;
                candidate.world_position = to_float3_checked(world_position);
                if (!clipping::contains_point(clipping_filter, candidate.world_position)) {
                    ++statistics_.latest_clipping_hits_rejected;
                    continue;
                }
                candidate.world_normal = math::to_float3(world_normal);
                candidate.barycentric_coordinates = triangle_hit->barycentric_coordinates;
                candidate.world_distance = static_cast<float>(world_distance);
                if (!validate_pick_hit(candidate)) {
                    return Error{ErrorCode::invalid_picking_ray,
                                 "Picking produced a non-finite or invalid hit result"};
                }
                nearest_world_distance = candidate.world_distance;
                nearest = candidate;
            }
        }

        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
        return nearest;
    }

    void release_scene(SceneId scene) noexcept {
        const std::uintptr_t engine_token = detail::SceneHandleAccess::engine_token(scene);
        const std::uint64_t scene_value = detail::SceneHandleAccess::value(scene);
        mesh_cache_.erase(
            std::remove_if(mesh_cache_.begin(), mesh_cache_.end(),
                           [engine_token, scene_value](const MeshCacheEntry& entry) noexcept {
                               return entry.key.engine == engine_token &&
                                      entry.key.scene == scene_value;
                           }),
            mesh_cache_.end());
        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
    }

    [[nodiscard]] PickingStatistics statistics() const noexcept {
        PickingStatistics result = statistics_;
        result.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
        return result;
    }

  private:
    [[nodiscard]] Result<const MeshAcceleration*>
    acceleration(SceneId scene_id, const scene::RuntimePrimitiveView& primitive) {
        const bool document_backed = primitive.document_primitive.is_valid();
        const std::uint64_t geometry = document_backed ? primitive.document_primitive.debug_value()
                                                       : primitive.mesh.debug_value();
        const MeshCacheKey key{detail::SceneHandleAccess::engine_token(scene_id),
                               detail::SceneHandleAccess::value(scene_id), geometry,
                               document_backed};
        const auto existing =
            std::find_if(mesh_cache_.begin(), mesh_cache_.end(),
                         [&key](const MeshCacheEntry& entry) noexcept { return entry.key == key; });
        if (existing != mesh_cache_.end()) {
            return &existing->acceleration;
        }

        Result<MeshAcceleration> build_result = build_acceleration(primitive);
        if (!build_result) {
            return build_result.error();
        }
        mesh_cache_.push_back(MeshCacheEntry{key, std::move(build_result).value()});
        ++statistics_.latest_bvh_builds;
        ++statistics_.lifetime_bvh_builds;
        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
        return &mesh_cache_.back().acceleration;
    }

    [[nodiscard]] Result<MeshAcceleration>
    build_acceleration(const scene::RuntimePrimitiveView& primitive) const {
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
            const std::uint32_t i0 = indices[triangle_index * 3];
            const std::uint32_t i1 = indices[triangle_index * 3 + 1];
            const std::uint32_t i2 = indices[triangle_index * 3 + 2];
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
            acceleration.triangles.push_back(TriangleReference{
                static_cast<std::uint32_t>(triangle_index), bounds, triangle_centroid(a, b, c)});
            acceleration.triangle_order.push_back(static_cast<std::uint32_t>(triangle_index));
        }
        if (acceleration.triangles.empty()) {
            return Error{ErrorCode::invalid_mesh_data,
                         "Picking BVH construction requires at least one triangle"};
        }
        const std::uint32_t root = build_node(
            acceleration, 0, static_cast<std::uint32_t>(acceleration.triangle_order.size()));
        (void)root;
        return acceleration;
    }

    [[nodiscard]] std::uint32_t build_node(MeshAcceleration& acceleration, std::uint32_t first,
                                           std::uint32_t count) const {
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

    template <typename AcceptHit>
    [[nodiscard]] std::optional<TriangleHit>
    traverse_mesh(const MeshAcceleration& acceleration,
                  const scene::RuntimePrimitiveView& primitive, const Ray3& local_ray,
                  bool cull_back_face, AcceptHit accept_hit) {
        if (acceleration.nodes.empty()) {
            return std::nullopt;
        }

        std::array<std::uint32_t, 128> stack{};
        std::uint32_t stack_size = 0;
        stack[stack_size++] = 0;
        std::optional<TriangleHit> nearest;
        float nearest_distance = std::numeric_limits<float>::max();

        while (stack_size != 0) {
            const BvhNode& node = acceleration.nodes[stack[--stack_size]];
            ++statistics_.latest_bvh_node_tests;
            RayBoundsHit bounds_hit;
            if (!intersect_ray_bounds(local_ray, node.bounds, bounds_hit) ||
                bounds_hit.entry_distance > nearest_distance) {
                continue;
            }
            if (node.is_leaf) {
                for (std::uint32_t index = node.first_triangle;
                     index < node.first_triangle + node.triangle_count; ++index) {
                    const TriangleReference& triangle =
                        acceleration.triangles[acceleration.triangle_order[index]];
                    const std::size_t base = static_cast<std::size_t>(triangle.triangle_index) * 3;
                    const std::span<const std::uint32_t> indices = primitive.indices();
                    if (base + 2 >= indices.size()) {
                        continue;
                    }
                    const std::uint32_t i0 = indices[base];
                    const std::uint32_t i1 = indices[base + 1];
                    const std::uint32_t i2 = indices[base + 2];
                    if (static_cast<std::size_t>(i0) >= primitive.vertex_count() ||
                        static_cast<std::size_t>(i1) >= primitive.vertex_count() ||
                        static_cast<std::size_t>(i2) >= primitive.vertex_count()) {
                        continue;
                    }
                    ++statistics_.latest_triangle_tests;
                    std::optional<TriangleHit> hit = intersect_ray_triangle(
                        local_ray, primitive.position(i0), primitive.position(i1),
                        primitive.position(i2), cull_back_face);
                    if (hit.has_value() && hit->distance < nearest_distance) {
                        TriangleHit candidate = *hit;
                        candidate.triangle_index = triangle.triangle_index;
                        if (!accept_hit(candidate)) {
                            continue;
                        }
                        nearest_distance = candidate.distance;
                        nearest = candidate;
                    }
                }
                continue;
            }
            if (stack_size + 2 > stack.size()) {
                return nearest;
            }
            stack[stack_size++] = node.right;
            stack[stack_size++] = node.left;
        }
        return nearest;
    }

    PickingStatistics statistics_;
    std::vector<MeshCacheEntry> mesh_cache_;
};

PickingService::PickingService() : impl_(std::make_unique<Impl>()) {}
PickingService::~PickingService() = default;
PickingService::PickingService(PickingService&&) noexcept = default;
PickingService& PickingService::operator=(PickingService&&) noexcept = default;

Result<Ray3> PickingService::make_picking_ray(const scene::Storage& scene, EntityId camera,
                                              Extent2D extent, Float2 position_pixels) const {
    return impl_->make_picking_ray(scene, camera, extent, position_pixels);
}

Result<std::optional<PickHit>> PickingService::pick(const scene::Storage& scene, EntityId camera,
                                                    Extent2D extent, Float2 position_pixels,
                                                    const PickOptions& options) {
    return impl_->pick(scene, camera, extent, position_pixels, options);
}

Result<std::optional<PickHit>> PickingService::pick(const scene::Storage& scene, EntityId camera,
                                                    Extent2D extent, Float2 position_pixels,
                                                    const PickOptions& options,
                                                    const scene::VisibilityFilter& visibility) {
    return impl_->pick(scene, camera, extent, position_pixels, options, visibility);
}

Result<std::optional<PickHit>>
PickingService::pick(const scene::Storage& scene, EntityId camera, Extent2D extent,
                     Float2 position_pixels, const PickOptions& options,
                     const scene::VisibilityFilter& visibility,
                     const clipping::ClippingFilter& clipping_filter) {
    return impl_->pick(scene, camera, extent, position_pixels, options, visibility,
                       clipping_filter);
}

Result<std::optional<PickHit>> PickingService::refine_candidate(
    const scene::Storage& scene, EntityId camera, Extent2D extent, Float2 position_pixels,
    const PickOptions& options, const scene::VisibilityFilter& visibility,
    const clipping::ClippingFilter& clipping_filter, const PickCandidate& candidate) {
    return impl_->refine_candidate(scene, camera, extent, position_pixels, options, visibility,
                                   clipping_filter, candidate);
}

Result<std::optional<PickHit>>
PickingService::pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options) {
    return impl_->pick_ray(scene, ray, options);
}

Result<std::optional<PickHit>> PickingService::pick_ray(const scene::Storage& scene,
                                                        const Ray3& ray, const PickOptions& options,
                                                        const scene::VisibilityFilter& visibility) {
    return impl_->pick_ray(scene, ray, options, visibility);
}

Result<std::optional<PickHit>>
PickingService::pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options,
                         const scene::VisibilityFilter& visibility,
                         const clipping::ClippingFilter& clipping_filter) {
    return impl_->pick_ray(scene, ray, options, visibility, clipping_filter);
}

void PickingService::release_scene(SceneId scene) noexcept {
    impl_->release_scene(scene);
}

PickingStatistics PickingService::statistics() const noexcept {
    return impl_->statistics();
}

} // namespace elf3d::picking
