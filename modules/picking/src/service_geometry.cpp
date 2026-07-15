module;

#include <elf3d/clipping.h>
#include <elf3d/core/assert.h>
#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

module elf.picking;

import elf.clipping;
import elf.math;
import elf.scene;

namespace elf3d::picking {
namespace {

constexpr double ray_epsilon = 1.0e-9;
constexpr double triangle_epsilon = 1.0e-8;

struct BoundsD {
    glm::dvec3 minimum;
    glm::dvec3 maximum;
};

} // namespace

namespace geometry_detail {

bool finite(double value) noexcept {
    return std::isfinite(value);
}

bool finite_vec3(const glm::dvec3& value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 to_dvec3(Float3 value) noexcept {
    return glm::dvec3{static_cast<double>(value.x), static_cast<double>(value.y),
                      static_cast<double>(value.z)};
}

Float3 to_float3_checked(const glm::dvec3& value) noexcept {
    return Float3{static_cast<float>(value.x), static_cast<float>(value.y),
                  static_cast<float>(value.z)};
}

bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool valid_bounds(Bounds3 bounds) noexcept {
    return finite_float3(bounds.minimum) && finite_float3(bounds.maximum) &&
           bounds.minimum.x <= bounds.maximum.x && bounds.minimum.y <= bounds.maximum.y &&
           bounds.minimum.z <= bounds.maximum.z;
}

[[nodiscard]] BoundsD to_bounds_d(Bounds3 bounds) noexcept {
    ELF3D_ASSERT(valid_bounds(bounds));
    return BoundsD{to_dvec3(bounds.minimum), to_dvec3(bounds.maximum)};
}

void expand(std::optional<BoundsD>& bounds, const glm::dvec3& point) noexcept {
    ELF3D_ASSERT(finite_vec3(point));
    if (!bounds.has_value()) {
        bounds = BoundsD{point, point};
        return;
    }
    bounds->minimum = glm::min(bounds->minimum, point);
    bounds->maximum = glm::max(bounds->maximum, point);
}

void expand(std::optional<BoundsD>& bounds, Bounds3 other) noexcept {
    const BoundsD converted = to_bounds_d(other);
    expand(bounds, converted.minimum);
    expand(bounds, converted.maximum);
}

[[nodiscard]] Bounds3 to_bounds3(const BoundsD& bounds) noexcept {
    ELF3D_ASSERT(finite_vec3(bounds.minimum) && finite_vec3(bounds.maximum));
    return Bounds3{to_float3_checked(bounds.minimum), to_float3_checked(bounds.maximum)};
}

Bounds3 triangle_bounds(Float3 a, Float3 b, Float3 c) noexcept {
    std::optional<BoundsD> bounds;
    expand(bounds, to_dvec3(a));
    expand(bounds, to_dvec3(b));
    expand(bounds, to_dvec3(c));
    ELF3D_ASSERT(bounds.has_value());
    return to_bounds3(*bounds);
}

Float3 triangle_centroid(Float3 a, Float3 b, Float3 c) noexcept {
    const glm::dvec3 centroid = (to_dvec3(a) + to_dvec3(b) + to_dvec3(c)) / 3.0;
    return to_float3_checked(centroid);
}

Bounds3 bounds_around_point(Float3 point) noexcept {
    const glm::dvec3 converted = to_dvec3(point);
    return to_bounds3(BoundsD{converted, converted});
}

Bounds3 merge_bounds(Bounds3 bounds, Bounds3 other) noexcept {
    std::optional<BoundsD> merged;
    expand(merged, bounds);
    expand(merged, other);
    ELF3D_ASSERT(merged.has_value());
    return to_bounds3(*merged);
}

Bounds3 merge_bounds(Bounds3 bounds, Float3 point) noexcept {
    std::optional<BoundsD> merged;
    expand(merged, bounds);
    expand(merged, to_dvec3(point));
    ELF3D_ASSERT(merged.has_value());
    return to_bounds3(*merged);
}

double axis_value(Float3 value, int axis) noexcept {
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

int longest_axis(Bounds3 bounds) noexcept {
    const Float3 extent{bounds.maximum.x - bounds.minimum.x, bounds.maximum.y - bounds.minimum.y,
                        bounds.maximum.z - bounds.minimum.z};
    if (extent.x >= extent.y && extent.x >= extent.z) {
        return 0;
    }
    if (extent.y >= extent.z) {
        return 1;
    }
    return 2;
}

bool finite_matrix(const math::Matrix4& matrix) noexcept {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4& world) noexcept {
    ELF3D_ASSERT(valid_bounds(local_bounds) && finite_matrix(world));
    const std::array<Float3, 8> corners{{
        {local_bounds.minimum.x, local_bounds.minimum.y, local_bounds.minimum.z},
        {local_bounds.maximum.x, local_bounds.minimum.y, local_bounds.minimum.z},
        {local_bounds.minimum.x, local_bounds.maximum.y, local_bounds.minimum.z},
        {local_bounds.maximum.x, local_bounds.maximum.y, local_bounds.minimum.z},
        {local_bounds.minimum.x, local_bounds.minimum.y, local_bounds.maximum.z},
        {local_bounds.maximum.x, local_bounds.minimum.y, local_bounds.maximum.z},
        {local_bounds.minimum.x, local_bounds.maximum.y, local_bounds.maximum.z},
        {local_bounds.maximum.x, local_bounds.maximum.y, local_bounds.maximum.z},
    }};
    std::optional<BoundsD> result;
    for (const Float3 corner : corners) {
        const math::Vector4 transformed = world * math::Vector4{corner.x, corner.y, corner.z, 1.0F};
        expand(result, glm::dvec3{transformed.x, transformed.y, transformed.z});
    }
    ELF3D_ASSERT(result.has_value());
    return to_bounds3(*result);
}

Result<Ray3> transform_ray_to_local(const Ray3& world_ray, const math::Matrix4& inverse_world) {
    const math::Vector4 origin =
        inverse_world *
        math::Vector4{world_ray.origin.x, world_ray.origin.y, world_ray.origin.z, 1.0F};
    const math::Vector4 direction =
        inverse_world *
        math::Vector4{world_ray.direction.x, world_ray.direction.y, world_ray.direction.z, 0.0F};
    const math::Vector3 local_direction{direction.x, direction.y, direction.z};
    const float length = glm::length(local_direction);
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z) ||
        !std::isfinite(length) || length <= static_cast<float>(ray_epsilon)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "The model transform produced an invalid local picking ray"};
    }
    return Ray3{Float3{origin.x, origin.y, origin.z}, math::to_float3(local_direction / length)};
}

[[nodiscard]] bool valid_hit_identity_and_position(const PickHit& hit) noexcept {
    return hit.entity.is_valid() && hit.mesh.is_valid() && finite_float3(hit.world_position) &&
           std::isfinite(hit.world_distance) && hit.world_distance >= 0.0F;
}

[[nodiscard]] bool valid_hit_normal(const PickHit& hit) noexcept {
    const float normal_length = std::sqrt(hit.world_normal.x * hit.world_normal.x +
                                          hit.world_normal.y * hit.world_normal.y +
                                          hit.world_normal.z * hit.world_normal.z);
    return finite_float3(hit.world_normal) && std::isfinite(normal_length) &&
           normal_length > 0.999F && normal_length < 1.001F;
}

[[nodiscard]] bool valid_hit_barycentric(const PickHit& hit) noexcept {
    const float barycentric_sum = hit.barycentric_coordinates.x + hit.barycentric_coordinates.y +
                                  hit.barycentric_coordinates.z;
    return finite_float3(hit.barycentric_coordinates) && std::isfinite(barycentric_sum) &&
           std::abs(barycentric_sum - 1.0F) < 0.001F;
}

bool validate_pick_hit(const PickHit& hit) noexcept {
    return valid_hit_identity_and_position(hit) && valid_hit_normal(hit) &&
           valid_hit_barycentric(hit);
}

Result<math::Matrix4> world_matrix(const scene::Storage& scene, EntityId entity) noexcept {
    const Result<Float4x4> world = scene.world_matrix(entity);
    if (!world) {
        return world.error();
    }
    return math::to_matrix(world.value());
}

Result<void> validate_refinement_request(const Ray3& ray, const PickCandidate& candidate) noexcept {
    if (!is_valid_ray(ray)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "Picking requires a finite normalized world-space ray"};
    }
    if (!candidate.entity.is_valid() || !candidate.mesh.is_valid()) {
        return Error{ErrorCode::invalid_argument,
                     "Picking refinement requires a valid entity and mesh candidate"};
    }
    return {};
}

Result<std::optional<scene::RuntimePrimitiveView>>
refinement_primitive(const scene::Storage& scene, const scene::VisibilityFilter& visibility,
                     const PickCandidate& candidate) {
    const Result<const scene::EntityRecord*> record_result = scene.entity(candidate.entity);
    if (!record_result) {
        return record_result.error();
    }
    const scene::EntityRecord& record = *record_result.value();
    if (!record.model.has_value() ||
        !scene::entity_visible_in_filter(scene, visibility, candidate.entity)) {
        return std::optional<scene::RuntimePrimitiveView>{};
    }
    if (static_cast<std::size_t>(candidate.primitive_index) >= record.model->primitives.size()) {
        return std::optional<scene::RuntimePrimitiveView>{};
    }
    const Result<scene::RuntimePrimitiveView> primitive =
        scene.runtime_primitive(candidate.entity, candidate.primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    if (primitive.value().mesh != candidate.mesh) {
        return std::optional<scene::RuntimePrimitiveView>{};
    }
    return std::optional{primitive.value()};
}

Result<std::optional<std::pair<math::Matrix4, Ray3>>>
refinement_transform(const scene::Storage& scene, EntityId entity, const Ray3& world_ray) {
    const Result<math::Matrix4> world = world_matrix(scene, entity);
    if (!world || !finite_matrix(world.value())) {
        return std::optional<std::pair<math::Matrix4, Ray3>>{};
    }
    const float determinant = glm::determinant(math::Matrix3{world.value()});
    if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
        return std::optional<std::pair<math::Matrix4, Ray3>>{};
    }
    const Result<Ray3> local_ray = transform_ray_to_local(world_ray, glm::inverse(world.value()));
    if (!local_ray) {
        return std::optional<std::pair<math::Matrix4, Ray3>>{};
    }
    return std::optional{std::pair{world.value(), local_ray.value()}};
}

void reset_latest_statistics(PickingStatistics& statistics,
                             std::uint64_t cached_mesh_bvhs) noexcept {
    statistics.latest_instance_bounds_tests = 0;
    statistics.latest_mesh_bounds_tests = 0;
    statistics.latest_bvh_node_tests = 0;
    statistics.latest_triangle_tests = 0;
    statistics.latest_bvh_builds = 0;
    statistics.latest_clipping_bounds_rejected = 0;
    statistics.latest_clipping_hits_rejected = 0;
    statistics.latest_clipping_hits_accepted = 0;
    statistics.cached_mesh_bvhs = cached_mesh_bvhs;
}

bool accept_refined_position(const clipping::ClippingFilter& filter, Float3 world_position,
                             PickingStatistics& statistics) noexcept {
    if (!filter.has_clipping()) {
        return true;
    }
    if (!clipping::contains_point(filter, world_position)) {
        ++statistics.latest_clipping_hits_rejected;
        return false;
    }
    ++statistics.latest_clipping_hits_accepted;
    return true;
}

[[nodiscard]] Result<void> validate_viewport_request(Extent2D extent,
                                                     Float2 position_pixels) noexcept {
    if (extent.width == 0 || extent.height == 0) {
        return Error{ErrorCode::invalid_viewport_dimensions,
                     "Picking requires a nonzero viewport extent"};
    }
    if (!std::isfinite(position_pixels.x) || !std::isfinite(position_pixels.y)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Picking coordinates must be finite viewport pixels"};
    }
    if (position_pixels.x < 0.0F || position_pixels.y < 0.0F ||
        position_pixels.x >= static_cast<float>(extent.width) ||
        position_pixels.y >= static_cast<float>(extent.height)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Picking coordinates are outside the viewport extent"};
    }
    return {};
}

struct PickingCameraFrame final {
    PerspectiveCameraDescription description;
    math::Matrix4 world;
    float aspect = 1.0F;
    float ndc_x = 0.0F;
    float ndc_y = 0.0F;
};

[[nodiscard]] Result<PickingCameraFrame> camera_frame(const scene::Storage& scene, EntityId camera,
                                                      Extent2D extent, Float2 position_pixels) {
    const Result<void> viewport = validate_viewport_request(extent, position_pixels);
    if (!viewport) {
        return viewport.error();
    }

    const Result<PerspectiveCameraDescription> camera_description =
        scene.perspective_camera(camera);
    if (!camera_description) {
        return camera_description.error();
    }
    if (!scene::valid_camera_description(camera_description.value())) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires a valid perspective camera configuration"};
    }
    const Result<math::Matrix4> camera_world = world_matrix(scene, camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    if (!finite_matrix(camera_world.value()) || !std::isfinite(aspect)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires a finite camera transform and aspect ratio"};
    }
    const float ndc_x = 2.0F * (position_pixels.x + 0.5F) / static_cast<float>(extent.width) - 1.0F;
    const float ndc_y =
        1.0F - 2.0F * (position_pixels.y + 0.5F) / static_cast<float>(extent.height);
    return PickingCameraFrame{camera_description.value(), camera_world.value(), aspect, ndc_x,
                              ndc_y};
}

struct CameraBasis final {
    glm::dvec3 origin;
    math::Vector3 right;
    math::Vector3 up;
    math::Vector3 backward;
};

[[nodiscard]] Result<CameraBasis> camera_basis(const math::Matrix4& camera_world) {
    const glm::dvec3 origin{camera_world[3].x, camera_world[3].y, camera_world[3].z};
    math::Vector3 right{camera_world[0]};
    math::Vector3 up{camera_world[1]};
    const float right_length = glm::length(right);
    if (!std::isfinite(right_length) || right_length <= static_cast<float>(ray_epsilon)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires a non-degenerate camera right axis"};
    }
    right /= right_length;
    up -= right * glm::dot(right, up);
    const float up_length = glm::length(up);
    if (!std::isfinite(up_length) || up_length <= static_cast<float>(ray_epsilon)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires a non-degenerate camera up axis"};
    }
    up /= up_length;
    const math::Vector3 backward = glm::normalize(glm::cross(right, up));
    if (!finite_vec3(origin) || !std::isfinite(glm::length(backward))) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires a finite camera basis"};
    }
    return CameraBasis{origin, right, up, backward};
}

[[nodiscard]] Result<Ray3> ray_from_camera_frame(const PickingCameraFrame& frame) {
    const Result<CameraBasis> basis = camera_basis(frame.world);
    if (!basis) {
        return basis.error();
    }
    const float tan_half_fov = std::tan(frame.description.vertical_field_of_view_radians * 0.5F);
    const math::Vector3 world_direction =
        basis.value().right * (frame.ndc_x * frame.aspect * tan_half_fov) +
        basis.value().up * (frame.ndc_y * tan_half_fov) - basis.value().backward;
    const float direction_length = glm::length(world_direction);
    if (!std::isfinite(direction_length) || direction_length <= static_cast<float>(ray_epsilon)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "Picking ray construction produced an invalid direction"};
    }

    const math::Vector3 direction = world_direction / direction_length;
    const Ray3 ray{to_float3_checked(basis.value().origin), math::to_float3(direction)};
    if (!is_valid_ray(ray)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "Picking ray construction produced non-finite values"};
    }
    return ray;
}

Result<Ray3> make_picking_ray(const scene::Storage& scene, EntityId camera, Extent2D extent,
                              Float2 position_pixels) {
    const Result<PickingCameraFrame> frame = camera_frame(scene, camera, extent, position_pixels);
    if (!frame) {
        return frame.error();
    }
    return ray_from_camera_frame(frame.value());
}

} // namespace geometry_detail

namespace {

struct RayInterval final {
    double entry = 0.0;
    double exit = static_cast<double>(std::numeric_limits<float>::max());
};

[[nodiscard]] bool update_ray_interval(double origin, double direction, double minimum,
                                       double maximum, RayInterval& interval) noexcept {
    if (std::abs(direction) <= ray_epsilon) {
        return origin >= minimum && origin <= maximum;
    }
    double near_distance = (minimum - origin) / direction;
    double far_distance = (maximum - origin) / direction;
    if (near_distance > far_distance) {
        std::swap(near_distance, far_distance);
    }
    interval.entry = std::max(interval.entry, near_distance);
    interval.exit = std::min(interval.exit, far_distance);
    return interval.entry <= interval.exit;
}

struct TriangleFrame final {
    glm::dvec3 origin;
    glm::dvec3 direction;
    glm::dvec3 vertex;
    glm::dvec3 edge1;
    glm::dvec3 edge2;
    glm::dvec3 normal;
    double normal_length = 0.0;
};

[[nodiscard]] std::optional<TriangleFrame> triangle_frame(const Ray3& ray, Float3 a, Float3 b,
                                                          Float3 c) noexcept {
    if (!is_valid_ray(ray) || !geometry_detail::finite_float3(a) ||
        !geometry_detail::finite_float3(b) || !geometry_detail::finite_float3(c)) {
        return std::nullopt;
    }
    const glm::dvec3 vertex = geometry_detail::to_dvec3(a);
    const glm::dvec3 edge1 = geometry_detail::to_dvec3(b) - vertex;
    const glm::dvec3 edge2 = geometry_detail::to_dvec3(c) - vertex;
    const glm::dvec3 normal = glm::cross(edge1, edge2);
    const double normal_length = glm::length(normal);
    if (!geometry_detail::finite(normal_length) || normal_length <= triangle_epsilon) {
        return std::nullopt;
    }
    return TriangleFrame{geometry_detail::to_dvec3(ray.origin),
                         geometry_detail::to_dvec3(ray.direction),
                         vertex,
                         edge1,
                         edge2,
                         normal,
                         normal_length};
}

[[nodiscard]] std::optional<double> triangle_determinant(const TriangleFrame& frame,
                                                         bool cull_back_face) noexcept {
    const double determinant = glm::dot(frame.edge1, glm::cross(frame.direction, frame.edge2));
    if (cull_back_face) {
        return determinant > triangle_epsilon ? std::optional{determinant} : std::nullopt;
    }
    return std::abs(determinant) > triangle_epsilon ? std::optional{determinant} : std::nullopt;
}

[[nodiscard]] bool inside_barycentric_triangle(double u, double v) noexcept {
    return u >= -triangle_epsilon && u <= 1.0 + triangle_epsilon && v >= -triangle_epsilon &&
           u + v <= 1.0 + triangle_epsilon;
}

struct TriangleCoordinates final {
    double w = 0.0;
    double u = 0.0;
    double v = 0.0;
    double distance = 0.0;
};

[[nodiscard]] std::optional<TriangleCoordinates> triangle_coordinates(const TriangleFrame& frame,
                                                                      double determinant) noexcept {
    const double inverse_determinant = 1.0 / determinant;
    const glm::dvec3 offset = frame.origin - frame.vertex;
    const glm::dvec3 cross_offset = glm::cross(offset, frame.edge1);
    const double u =
        inverse_determinant * glm::dot(offset, glm::cross(frame.direction, frame.edge2));
    const double v = inverse_determinant * glm::dot(frame.direction, cross_offset);
    if (!inside_barycentric_triangle(u, v)) {
        return std::nullopt;
    }
    const double distance = inverse_determinant * glm::dot(frame.edge2, cross_offset);
    if (distance < 0.0 || !geometry_detail::finite(distance)) {
        return std::nullopt;
    }
    const double w = 1.0 - u - v;
    if (!geometry_detail::finite(u) || !geometry_detail::finite(v) || !geometry_detail::finite(w)) {
        return std::nullopt;
    }
    return TriangleCoordinates{w, u, v, distance};
}

} // namespace

bool is_valid_ray(const Ray3& ray) noexcept {
    if (!geometry_detail::finite_float3(ray.origin) ||
        !geometry_detail::finite_float3(ray.direction)) {
        return false;
    }
    const float length =
        std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y +
                  ray.direction.z * ray.direction.z);
    return std::isfinite(length) && length > 0.999F && length < 1.001F;
}

bool intersect_ray_bounds(const Ray3& ray, Bounds3 bounds, RayBoundsHit& hit) noexcept {
    if (!is_valid_ray(ray) || !geometry_detail::valid_bounds(bounds)) {
        return false;
    }

    const glm::dvec3 origin = geometry_detail::to_dvec3(ray.origin);
    const glm::dvec3 direction = geometry_detail::to_dvec3(ray.direction);
    const BoundsD box = geometry_detail::to_bounds_d(bounds);
    RayInterval interval;

    for (int axis = 0; axis < 3; ++axis) {
        if (!update_ray_interval(origin[axis], direction[axis], box.minimum[axis],
                                 box.maximum[axis], interval)) {
            return false;
        }
    }

    if (interval.exit < 0.0 || !geometry_detail::finite(interval.entry) ||
        !geometry_detail::finite(interval.exit)) {
        return false;
    }
    hit.entry_distance = static_cast<float>(std::max(interval.entry, 0.0));
    hit.exit_distance = static_cast<float>(interval.exit);
    return true;
}

std::optional<TriangleHit> intersect_ray_triangle(const Ray3& ray, Float3 a, Float3 b, Float3 c,
                                                  bool cull_back_face) noexcept {
    const std::optional<TriangleFrame> frame = triangle_frame(ray, a, b, c);
    if (!frame.has_value()) {
        return std::nullopt;
    }
    const std::optional<double> determinant = triangle_determinant(*frame, cull_back_face);
    if (!determinant.has_value()) {
        return std::nullopt;
    }
    const std::optional<TriangleCoordinates> coordinates =
        triangle_coordinates(*frame, *determinant);
    if (!coordinates.has_value()) {
        return std::nullopt;
    }
    const glm::dvec3 normalized_normal = frame->normal / frame->normal_length;
    return TriangleHit{static_cast<float>(coordinates->distance), 0,
                       Float3{static_cast<float>(coordinates->w),
                              static_cast<float>(coordinates->u),
                              static_cast<float>(coordinates->v)},
                       geometry_detail::to_float3_checked(normalized_normal)};
}

} // namespace elf3d::picking
