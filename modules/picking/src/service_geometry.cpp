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
#include <optional>
#include <utility>

module elf.picking;

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

bool finite_vec3(const glm::dvec3 &value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 to_dvec3(Float3 value) noexcept {
    return glm::dvec3{static_cast<double>(value.x), static_cast<double>(value.y),
                      static_cast<double>(value.z)};
}

Float3 to_float3_checked(const glm::dvec3 &value) noexcept {
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

void expand(std::optional<BoundsD> &bounds, const glm::dvec3 &point) noexcept {
    ELF3D_ASSERT(finite_vec3(point));
    if (!bounds.has_value()) {
        bounds = BoundsD{point, point};
        return;
    }
    bounds->minimum = glm::min(bounds->minimum, point);
    bounds->maximum = glm::max(bounds->maximum, point);
}

void expand(std::optional<BoundsD> &bounds, Bounds3 other) noexcept {
    const BoundsD converted = to_bounds_d(other);
    expand(bounds, converted.minimum);
    expand(bounds, converted.maximum);
}

[[nodiscard]] Bounds3 to_bounds3(const BoundsD &bounds) noexcept {
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

bool finite_matrix(const math::Matrix4 &matrix) noexcept {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4 &world) noexcept {
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

Result<Ray3> transform_ray_to_local(const Ray3 &world_ray,
                                    const math::Matrix4 &inverse_world) {
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

bool validate_pick_hit(const PickHit &hit) noexcept {
    if (!hit.entity.is_valid() || !hit.mesh.is_valid() || !finite_float3(hit.world_position) ||
        !finite_float3(hit.world_normal) || !finite_float3(hit.barycentric_coordinates) ||
        !std::isfinite(hit.world_distance) || hit.world_distance < 0.0F) {
        return false;
    }
    const float normal_length = std::sqrt(hit.world_normal.x * hit.world_normal.x +
                                          hit.world_normal.y * hit.world_normal.y +
                                          hit.world_normal.z * hit.world_normal.z);
    const float barycentric_sum = hit.barycentric_coordinates.x + hit.barycentric_coordinates.y +
                                  hit.barycentric_coordinates.z;
    return std::isfinite(normal_length) && normal_length > 0.999F && normal_length < 1.001F &&
           std::isfinite(barycentric_sum) && std::abs(barycentric_sum - 1.0F) < 0.001F;
}

Result<math::Matrix4> world_matrix(const scene::Storage &scene, EntityId entity) noexcept {
    const Result<Float4x4> world = scene.world_matrix(entity);
    if (!world) {
        return world.error();
    }
    return math::to_matrix(world.value());
}

Result<Ray3> make_picking_ray(const scene::Storage &scene, EntityId camera, Extent2D extent,
                              Float2 position_pixels) {
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
    const Result<Float4x4> view = math::camera_view_matrix(math::to_float4x4(camera_world.value()));
    if (!view) {
        return view.error();
    }
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const Result<Float4x4> projection = math::perspective_matrix(
        camera_description.value().vertical_field_of_view_radians, aspect,
        camera_description.value().near_plane, camera_description.value().far_plane);
    if (!projection) {
        return projection.error();
    }
    const math::Matrix4 view_projection =
        math::to_matrix(projection.value()) * math::to_matrix(view.value());
    const float determinant = glm::determinant(view_projection);
    if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Picking requires an invertible view-projection matrix"};
    }
    const math::Matrix4 inverse_view_projection = glm::inverse(view_projection);

    const float ndc_x =
        2.0F * (position_pixels.x + 0.5F) / static_cast<float>(extent.width) - 1.0F;
    const float ndc_y =
        1.0F - 2.0F * (position_pixels.y + 0.5F) / static_cast<float>(extent.height);
    const auto unproject = [&](float clip_z) -> Result<glm::dvec3> {
        const math::Vector4 world =
            inverse_view_projection * math::Vector4{ndc_x, ndc_y, clip_z, 1.0F};
        if (!std::isfinite(world.w) || std::abs(world.w) <= 0.000001F) {
            return Error{ErrorCode::invalid_picking_ray,
                         "Picking ray unprojection produced an invalid homogeneous point"};
        }
        const glm::dvec3 point{static_cast<double>(world.x) / world.w,
                               static_cast<double>(world.y) / world.w,
                               static_cast<double>(world.z) / world.w};
        if (!finite_vec3(point)) {
            return Error{ErrorCode::invalid_picking_ray,
                         "Picking ray unprojection produced a non-finite point"};
        }
        return point;
    };
    const Result<glm::dvec3> near_point = unproject(-1.0F);
    if (!near_point) {
        return near_point.error();
    }
    const Result<glm::dvec3> far_point = unproject(1.0F);
    if (!far_point) {
        return far_point.error();
    }
    (void)near_point;

    const glm::dvec3 origin{camera_world.value()[3].x, camera_world.value()[3].y,
                            camera_world.value()[3].z};
    (void)far_point;

    math::Vector3 right{camera_world.value()[0]};
    math::Vector3 up{camera_world.value()[1]};
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
    const float tan_half_fov =
        std::tan(camera_description.value().vertical_field_of_view_radians * 0.5F);
    const math::Vector3 world_direction =
        right * (ndc_x * aspect * tan_half_fov) + up * (ndc_y * tan_half_fov) - backward;
    const float direction_length = glm::length(world_direction);
    if (!finite_vec3(origin) || !std::isfinite(direction_length) ||
        direction_length <= static_cast<float>(ray_epsilon)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "Picking ray construction produced an invalid direction"};
    }

    const math::Vector3 direction = world_direction / direction_length;
    const Ray3 ray{to_float3_checked(origin), math::to_float3(direction)};
    if (!is_valid_ray(ray)) {
        return Error{ErrorCode::invalid_picking_ray,
                     "Picking ray construction produced non-finite values"};
    }
    return ray;
}

} // namespace geometry_detail

bool is_valid_ray(const Ray3 &ray) noexcept {
    if (!geometry_detail::finite_float3(ray.origin) || !geometry_detail::finite_float3(ray.direction)) {
        return false;
    }
    const float length =
        std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y +
                  ray.direction.z * ray.direction.z);
    return std::isfinite(length) && length > 0.999F && length < 1.001F;
}

bool intersect_ray_bounds(const Ray3 &ray, Bounds3 bounds, RayBoundsHit &hit) noexcept {
    if (!is_valid_ray(ray) || !geometry_detail::valid_bounds(bounds)) {
        return false;
    }

    const glm::dvec3 origin = geometry_detail::to_dvec3(ray.origin);
    const glm::dvec3 direction = geometry_detail::to_dvec3(ray.direction);
    const BoundsD box = geometry_detail::to_bounds_d(bounds);
    double entry = 0.0;
    double exit = static_cast<double>(std::numeric_limits<float>::max());

    for (int axis = 0; axis < 3; ++axis) {
        const double origin_value = origin[axis];
        const double direction_value = direction[axis];
        const double minimum = box.minimum[axis];
        const double maximum = box.maximum[axis];
        if (std::abs(direction_value) <= ray_epsilon) {
            if (origin_value < minimum || origin_value > maximum) {
                return false;
            }
            continue;
        }

        double t0 = (minimum - origin_value) / direction_value;
        double t1 = (maximum - origin_value) / direction_value;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        entry = std::max(entry, t0);
        exit = std::min(exit, t1);
        if (entry > exit) {
            return false;
        }
    }

    if (exit < 0.0 || !geometry_detail::finite(entry) || !geometry_detail::finite(exit)) {
        return false;
    }
    hit.entry_distance = static_cast<float>(std::max(entry, 0.0));
    hit.exit_distance = static_cast<float>(exit);
    return true;
}

std::optional<TriangleHit> intersect_ray_triangle(const Ray3 &ray, Float3 a, Float3 b, Float3 c,
                                                  bool cull_back_face) noexcept {
    if (!is_valid_ray(ray) || !geometry_detail::finite_float3(a) || !geometry_detail::finite_float3(b) ||
        !geometry_detail::finite_float3(c)) {
        return std::nullopt;
    }

    const glm::dvec3 origin = geometry_detail::to_dvec3(ray.origin);
    const glm::dvec3 direction = geometry_detail::to_dvec3(ray.direction);
    const glm::dvec3 v0 = geometry_detail::to_dvec3(a);
    const glm::dvec3 v1 = geometry_detail::to_dvec3(b);
    const glm::dvec3 v2 = geometry_detail::to_dvec3(c);
    const glm::dvec3 edge1 = v1 - v0;
    const glm::dvec3 edge2 = v2 - v0;
    const glm::dvec3 normal = glm::cross(edge1, edge2);
    const double normal_length = glm::length(normal);
    if (!geometry_detail::finite(normal_length) || normal_length <= triangle_epsilon) {
        return std::nullopt;
    }

    const glm::dvec3 h = glm::cross(direction, edge2);
    const double determinant = glm::dot(edge1, h);
    if (cull_back_face) {
        if (determinant <= triangle_epsilon) {
            return std::nullopt;
        }
    } else if (std::abs(determinant) <= triangle_epsilon) {
        return std::nullopt;
    }

    const double inverse_determinant = 1.0 / determinant;
    const glm::dvec3 s = origin - v0;
    const double u = inverse_determinant * glm::dot(s, h);
    if (u < -triangle_epsilon || u > 1.0 + triangle_epsilon) {
        return std::nullopt;
    }

    const glm::dvec3 q = glm::cross(s, edge1);
    const double v = inverse_determinant * glm::dot(direction, q);
    if (v < -triangle_epsilon || u + v > 1.0 + triangle_epsilon) {
        return std::nullopt;
    }

    const double distance = inverse_determinant * glm::dot(edge2, q);
    if (distance < 0.0 || !geometry_detail::finite(distance)) {
        return std::nullopt;
    }

    const double w = 1.0 - u - v;
    if (!geometry_detail::finite(u) || !geometry_detail::finite(v) || !geometry_detail::finite(w)) {
        return std::nullopt;
    }
    const glm::dvec3 normalized_normal = normal / normal_length;
    return TriangleHit{static_cast<float>(distance), 0,
                       Float3{static_cast<float>(w), static_cast<float>(u), static_cast<float>(v)},
                       geometry_detail::to_float3_checked(normalized_normal)};
}

} // namespace elf3d::picking

