#include <elf3d/picking/service.h>

#include <elf3d/assets/handle_access.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elf3d::picking {
namespace {

constexpr double ray_epsilon = 1.0e-9;
constexpr double triangle_epsilon = 1.0e-8;
constexpr std::uint32_t bvh_leaf_size = 8;

struct BoundsD {
    glm::dvec3 minimum;
    glm::dvec3 maximum;
    bool is_valid = false;
};

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

[[nodiscard]] bool finite(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite_vec3(const glm::dvec3 &value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] glm::dvec3 to_dvec3(Float3 value) noexcept {
    return glm::dvec3{static_cast<double>(value.x), static_cast<double>(value.y),
                      static_cast<double>(value.z)};
}

[[nodiscard]] Float3 to_float3_checked(const glm::dvec3 &value) noexcept {
    return Float3{static_cast<float>(value.x), static_cast<float>(value.y),
                  static_cast<float>(value.z)};
}

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] bool valid_bounds(Bounds3 bounds) noexcept {
    return bounds.is_valid && finite_float3(bounds.minimum) && finite_float3(bounds.maximum) &&
           bounds.minimum.x <= bounds.maximum.x && bounds.minimum.y <= bounds.maximum.y &&
           bounds.minimum.z <= bounds.maximum.z;
}

[[nodiscard]] BoundsD to_bounds_d(Bounds3 bounds) noexcept {
    if (!valid_bounds(bounds)) {
        return {};
    }
    return BoundsD{to_dvec3(bounds.minimum), to_dvec3(bounds.maximum), true};
}

void expand(BoundsD &bounds, const glm::dvec3 &point) noexcept {
    if (!finite_vec3(point)) {
        bounds.is_valid = false;
        return;
    }
    if (!bounds.is_valid) {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.is_valid = true;
        return;
    }
    bounds.minimum = glm::min(bounds.minimum, point);
    bounds.maximum = glm::max(bounds.maximum, point);
}

void expand(BoundsD &bounds, Bounds3 other) noexcept {
    const BoundsD converted = to_bounds_d(other);
    if (!converted.is_valid) {
        return;
    }
    expand(bounds, converted.minimum);
    expand(bounds, converted.maximum);
}

[[nodiscard]] Bounds3 to_bounds3(BoundsD bounds) noexcept {
    if (!bounds.is_valid || !finite_vec3(bounds.minimum) || !finite_vec3(bounds.maximum)) {
        return {};
    }
    return Bounds3{to_float3_checked(bounds.minimum), to_float3_checked(bounds.maximum), true};
}

[[nodiscard]] Bounds3 triangle_bounds(Float3 a, Float3 b, Float3 c) noexcept {
    BoundsD bounds;
    expand(bounds, to_dvec3(a));
    expand(bounds, to_dvec3(b));
    expand(bounds, to_dvec3(c));
    return to_bounds3(bounds);
}

[[nodiscard]] Float3 triangle_centroid(Float3 a, Float3 b, Float3 c) noexcept {
    const glm::dvec3 centroid = (to_dvec3(a) + to_dvec3(b) + to_dvec3(c)) / 3.0;
    return to_float3_checked(centroid);
}

[[nodiscard]] double axis_value(Float3 value, int axis) noexcept {
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

[[nodiscard]] int longest_axis(Bounds3 bounds) noexcept {
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

[[nodiscard]] bool finite_matrix(const math::Matrix4 &matrix) noexcept {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4 &world) noexcept {
    if (!valid_bounds(local_bounds) || !finite_matrix(world)) {
        return {};
    }
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
    BoundsD result;
    for (const Float3 corner : corners) {
        const math::Vector4 transformed = world * math::Vector4{corner.x, corner.y, corner.z, 1.0F};
        expand(result, glm::dvec3{transformed.x, transformed.y, transformed.z});
    }
    return to_bounds3(result);
}

[[nodiscard]] Result<Ray3> transform_ray_to_local(const Ray3 &world_ray,
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

[[nodiscard]] bool validate_pick_hit(const PickHit &hit) noexcept {
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

class ScenePickingView final {
  public:
    explicit ScenePickingView(const scene::Storage &scene) noexcept : scene_(scene) {}

    [[nodiscard]] SceneId id() const noexcept {
        return scene_.id();
    }

    [[nodiscard]] std::span<const std::optional<scene::EntityRecord>> entities() const noexcept {
        return scene_.entities();
    }

    [[nodiscard]] Result<PerspectiveCameraDescription>
    perspective_camera(EntityId camera) const noexcept {
        return scene_.perspective_camera(camera);
    }

    [[nodiscard]] Result<math::Matrix4> world_matrix(EntityId entity) const noexcept {
        return scene_.world_matrix(entity);
    }

    [[nodiscard]] Result<const assets::MeshAsset *> mesh(MeshHandle handle) const {
        return scene_.assets().mesh(handle);
    }

    [[nodiscard]] Result<const assets::MaterialAsset *> material(MaterialHandle handle) const {
        return scene_.assets().material(handle);
    }

  private:
    const scene::Storage &scene_;
};

} // namespace

bool is_valid_ray(const Ray3 &ray) noexcept {
    if (!finite_float3(ray.origin) || !finite_float3(ray.direction)) {
        return false;
    }
    const float length =
        std::sqrt(ray.direction.x * ray.direction.x + ray.direction.y * ray.direction.y +
                  ray.direction.z * ray.direction.z);
    return std::isfinite(length) && length > 0.999F && length < 1.001F;
}

bool intersect_ray_bounds(const Ray3 &ray, Bounds3 bounds, RayBoundsHit &hit) noexcept {
    if (!is_valid_ray(ray) || !valid_bounds(bounds)) {
        return false;
    }

    const glm::dvec3 origin = to_dvec3(ray.origin);
    const glm::dvec3 direction = to_dvec3(ray.direction);
    const BoundsD box = to_bounds_d(bounds);
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

    if (exit < 0.0 || !finite(entry) || !finite(exit)) {
        return false;
    }
    hit.entry_distance = static_cast<float>(std::max(entry, 0.0));
    hit.exit_distance = static_cast<float>(exit);
    return true;
}

std::optional<TriangleHit> intersect_ray_triangle(const Ray3 &ray, Float3 a, Float3 b, Float3 c,
                                                  bool cull_back_face) noexcept {
    if (!is_valid_ray(ray) || !finite_float3(a) || !finite_float3(b) || !finite_float3(c)) {
        return std::nullopt;
    }

    const glm::dvec3 origin = to_dvec3(ray.origin);
    const glm::dvec3 direction = to_dvec3(ray.direction);
    const glm::dvec3 v0 = to_dvec3(a);
    const glm::dvec3 v1 = to_dvec3(b);
    const glm::dvec3 v2 = to_dvec3(c);
    const glm::dvec3 edge1 = v1 - v0;
    const glm::dvec3 edge2 = v2 - v0;
    const glm::dvec3 normal = glm::cross(edge1, edge2);
    const double normal_length = glm::length(normal);
    if (!finite(normal_length) || normal_length <= triangle_epsilon) {
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
    if (distance < 0.0 || !finite(distance)) {
        return std::nullopt;
    }

    const double w = 1.0 - u - v;
    if (!finite(u) || !finite(v) || !finite(w)) {
        return std::nullopt;
    }
    const glm::dvec3 normalized_normal = normal / normal_length;
    return TriangleHit{static_cast<float>(distance), 0,
                       Float3{static_cast<float>(w), static_cast<float>(u), static_cast<float>(v)},
                       to_float3_checked(normalized_normal)};
}

class PickingService::Impl final {
  public:
    struct MeshCacheKey {
        std::uintptr_t engine = 0;
        std::uint64_t scene = 0;
        std::uint64_t mesh = 0;

        bool operator==(const MeshCacheKey &) const = default;
    };

    struct MeshCacheHash {
        [[nodiscard]] std::size_t operator()(const MeshCacheKey &key) const noexcept {
            std::size_t result = std::hash<std::uintptr_t>{}(key.engine);
            const auto combine = [&result](std::size_t value) {
                result ^=
                    value + static_cast<std::size_t>(0x9e3779b9U) + (result << 6U) + (result >> 2U);
            };
            combine(std::hash<std::uint64_t>{}(key.scene));
            combine(std::hash<std::uint64_t>{}(key.mesh));
            return result;
        }
    };

    struct MeshAcceleration {
        std::vector<TriangleReference> triangles;
        std::vector<std::uint32_t> triangle_order;
        std::vector<BvhNode> nodes;
    };

    [[nodiscard]] Result<Ray3> make_picking_ray(const scene::Storage &scene, EntityId camera,
                                                Extent2D extent, Float2 position_pixels) const {
        const ScenePickingView scene_view{scene};
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
            scene_view.perspective_camera(camera);
        if (!camera_description) {
            return camera_description.error();
        }
        if (!scene::valid_camera_description(camera_description.value())) {
            return Error{ErrorCode::invalid_camera_configuration,
                         "Picking requires a valid perspective camera configuration"};
        }
        const Result<math::Matrix4> camera_world = scene_view.world_matrix(camera);
        if (!camera_world) {
            return camera_world.error();
        }
        const Result<math::Matrix4> view = math::camera_view_matrix(camera_world.value());
        if (!view) {
            return view.error();
        }
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        const Result<math::Matrix4> projection = math::perspective_matrix(
            camera_description.value().vertical_field_of_view_radians, aspect,
            camera_description.value().near_plane, camera_description.value().far_plane);
        if (!projection) {
            return projection.error();
        }
        const math::Matrix4 view_projection = projection.value() * view.value();
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

    [[nodiscard]] Result<std::optional<PickHit>> pick(const scene::Storage &scene, EntityId camera,
                                                      Extent2D extent, Float2 position_pixels,
                                                      const PickOptions &options) {
        const Result<scene::VisibilityFilter> visibility =
            scene::make_visibility_filter(scene, std::nullopt);
        if (!visibility) {
            return visibility.error();
        }
        return pick(scene, camera, extent, position_pixels, options, visibility.value());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick(const scene::Storage &scene, EntityId camera, Extent2D extent, Float2 position_pixels,
         const PickOptions &options, const scene::VisibilityFilter &visibility) {
        return pick(scene, camera, extent, position_pixels, options, visibility,
                    clipping::disabled_filter());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick(const scene::Storage &scene, EntityId camera, Extent2D extent, Float2 position_pixels,
         const PickOptions &options, const scene::VisibilityFilter &visibility,
         const clipping::ClippingFilter &clipping_filter) {
        const Result<Ray3> ray = make_picking_ray(scene, camera, extent, position_pixels);
        if (!ray) {
            return ray.error();
        }
        return pick_ray(scene, ray.value(), options, visibility, clipping_filter);
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options) {
        const Result<scene::VisibilityFilter> visibility =
            scene::make_visibility_filter(scene, std::nullopt);
        if (!visibility) {
            return visibility.error();
        }
        return pick_ray(scene, ray, options, visibility.value());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options,
             const scene::VisibilityFilter &visibility) {
        return pick_ray(scene, ray, options, visibility, clipping::disabled_filter());
    }

    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options,
             const scene::VisibilityFilter &visibility,
             const clipping::ClippingFilter &clipping_filter) {
        const ScenePickingView view{scene};
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

        for (const std::optional<scene::EntityRecord> &record : view.entities()) {
            if (!record.has_value() || !record->model.has_value() ||
                !scene::entity_visible_in_filter(scene, visibility, record->id)) {
                continue;
            }
            const Result<math::Matrix4> world = view.world_matrix(record->id);
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
                const ModelPrimitiveBinding &primitive = record->model->primitives[primitive_index];
                const Result<const assets::MeshAsset *> mesh_result = view.mesh(primitive.mesh);
                if (!mesh_result) {
                    return mesh_result.error();
                }
                const Result<const assets::MaterialAsset *> material_result =
                    view.material(primitive.material);
                if (!material_result) {
                    return material_result.error();
                }
                const assets::MeshAsset &mesh = *mesh_result.value();
                if (!valid_bounds(mesh.bounds)) {
                    return Error{ErrorCode::invalid_mesh_data,
                                 "Picking encountered a mesh with invalid local bounds"};
                }

                ++statistics_.latest_instance_bounds_tests;
                const Bounds3 world_bounds = transform_bounds(mesh.bounds, world.value());
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
                if (!intersect_ray_bounds(local_ray.value(), mesh.bounds, local_bounds_hit)) {
                    continue;
                }

                const Result<const MeshAcceleration *> acceleration_result =
                    acceleration(view.id(), primitive.mesh, mesh);
                if (!acceleration_result) {
                    return acceleration_result.error();
                }

                const bool cull_back_face = options.respect_material_sidedness &&
                                            !material_result.value()->description.double_sided;
                const auto accept_hit = [&](const TriangleHit &hit) noexcept {
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
                    traverse_mesh(*acceleration_result.value(), mesh, local_ray.value(),
                                  cull_back_face, accept_hit);
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
                candidate.mesh = primitive.mesh;
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
        for (auto iterator = mesh_cache_.begin(); iterator != mesh_cache_.end();) {
            if (iterator->first.engine == engine_token && iterator->first.scene == scene_value) {
                iterator = mesh_cache_.erase(iterator);
            } else {
                ++iterator;
            }
        }
        statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
    }

    [[nodiscard]] PickingStatistics statistics() const noexcept {
        PickingStatistics result = statistics_;
        result.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
        return result;
    }

  private:
    [[nodiscard]] Result<const MeshAcceleration *> acceleration(SceneId scene, MeshHandle mesh,
                                                                const assets::MeshAsset &asset) {
        const MeshCacheKey key{detail::SceneHandleAccess::engine_token(scene),
                               detail::SceneHandleAccess::value(scene),
                               detail::SceneHandleAccess::value(mesh)};
        const auto existing = mesh_cache_.find(key);
        if (existing != mesh_cache_.end()) {
            return &existing->second;
        }

        Result<MeshAcceleration> build_result = build_acceleration(asset);
        if (!build_result) {
            return build_result.error();
        }
        try {
            auto [iterator, inserted] = mesh_cache_.emplace(key, std::move(build_result).value());
            if (!inserted) {
                return Error{ErrorCode::picking_acceleration_failed,
                             "The picking BVH cache rejected a unique mesh identity"};
            }
            ++statistics_.latest_bvh_builds;
            ++statistics_.lifetime_bvh_builds;
            statistics_.cached_mesh_bvhs = static_cast<std::uint64_t>(mesh_cache_.size());
            return &iterator->second;
        } catch (...) {
            return Error{ErrorCode::picking_acceleration_failed,
                         "Picking BVH caching failed while allocating storage"};
        }
    }

    [[nodiscard]] Result<MeshAcceleration> build_acceleration(const assets::MeshAsset &mesh) const {
        if (mesh.indices.empty() || mesh.indices.size() % 3 != 0 || mesh.vertices.empty()) {
            return Error{ErrorCode::invalid_mesh_data,
                         "Picking BVH construction requires indexed triangle mesh data"};
        }
        try {
            MeshAcceleration acceleration;
            const std::size_t triangle_count = mesh.indices.size() / 3;
            if (triangle_count >
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return Error{ErrorCode::picking_acceleration_failed,
                             "Picking BVH triangle count exceeds internal limits"};
            }
            acceleration.triangles.reserve(triangle_count);
            acceleration.triangle_order.reserve(triangle_count);
            acceleration.nodes.reserve(triangle_count * 2);
            for (std::size_t triangle_index = 0; triangle_index < triangle_count;
                 ++triangle_index) {
                const std::uint32_t i0 = mesh.indices[triangle_index * 3];
                const std::uint32_t i1 = mesh.indices[triangle_index * 3 + 1];
                const std::uint32_t i2 = mesh.indices[triangle_index * 3 + 2];
                if (static_cast<std::size_t>(i0) >= mesh.vertices.size() ||
                    static_cast<std::size_t>(i1) >= mesh.vertices.size() ||
                    static_cast<std::size_t>(i2) >= mesh.vertices.size()) {
                    return Error{ErrorCode::mesh_index_out_of_range,
                                 "Picking BVH encountered an index outside the vertex range"};
                }
                const Float3 a = mesh.vertices[i0].position;
                const Float3 b = mesh.vertices[i1].position;
                const Float3 c = mesh.vertices[i2].position;
                const Bounds3 bounds = triangle_bounds(a, b, c);
                if (!valid_bounds(bounds)) {
                    return Error{ErrorCode::invalid_mesh_data,
                                 "Picking BVH encountered non-finite triangle bounds"};
                }
                acceleration.triangles.push_back(
                    TriangleReference{static_cast<std::uint32_t>(triangle_index), bounds,
                                      triangle_centroid(a, b, c)});
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
        } catch (...) {
            return Error{ErrorCode::picking_acceleration_failed,
                         "Picking BVH construction failed while allocating storage"};
        }
    }

    [[nodiscard]] std::uint32_t build_node(MeshAcceleration &acceleration, std::uint32_t first,
                                           std::uint32_t count) const {
        const std::uint32_t node_index = static_cast<std::uint32_t>(acceleration.nodes.size());
        acceleration.nodes.push_back(BvhNode{});

        BoundsD node_bounds;
        BoundsD centroid_bounds;
        for (std::uint32_t index = first; index < first + count; ++index) {
            const TriangleReference &triangle =
                acceleration.triangles[acceleration.triangle_order[index]];
            expand(node_bounds, triangle.bounds);
            expand(centroid_bounds, to_dvec3(triangle.centroid));
        }

        BvhNode node;
        node.bounds = to_bounds3(node_bounds);
        if (count <= bvh_leaf_size || !centroid_bounds.is_valid) {
            node.first_triangle = first;
            node.triangle_count = count;
            node.is_leaf = true;
            acceleration.nodes[node_index] = node;
            return node_index;
        }

        const int axis = longest_axis(to_bounds3(centroid_bounds));
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
    [[nodiscard]] std::optional<TriangleHit> traverse_mesh(const MeshAcceleration &acceleration,
                                                           const assets::MeshAsset &mesh,
                                                           const Ray3 &local_ray,
                                                           bool cull_back_face,
                                                           AcceptHit accept_hit) {
        if (acceleration.nodes.empty()) {
            return std::nullopt;
        }

        std::array<std::uint32_t, 128> stack{};
        std::uint32_t stack_size = 0;
        stack[stack_size++] = 0;
        std::optional<TriangleHit> nearest;
        float nearest_distance = std::numeric_limits<float>::max();

        while (stack_size != 0) {
            const BvhNode &node = acceleration.nodes[stack[--stack_size]];
            ++statistics_.latest_bvh_node_tests;
            RayBoundsHit bounds_hit;
            if (!intersect_ray_bounds(local_ray, node.bounds, bounds_hit) ||
                bounds_hit.entry_distance > nearest_distance) {
                continue;
            }
            if (node.is_leaf) {
                for (std::uint32_t index = node.first_triangle;
                     index < node.first_triangle + node.triangle_count; ++index) {
                    const TriangleReference &triangle =
                        acceleration.triangles[acceleration.triangle_order[index]];
                    const std::size_t base = static_cast<std::size_t>(triangle.triangle_index) * 3;
                    if (base + 2 >= mesh.indices.size()) {
                        continue;
                    }
                    const std::uint32_t i0 = mesh.indices[base];
                    const std::uint32_t i1 = mesh.indices[base + 1];
                    const std::uint32_t i2 = mesh.indices[base + 2];
                    if (static_cast<std::size_t>(i0) >= mesh.vertices.size() ||
                        static_cast<std::size_t>(i1) >= mesh.vertices.size() ||
                        static_cast<std::size_t>(i2) >= mesh.vertices.size()) {
                        continue;
                    }
                    ++statistics_.latest_triangle_tests;
                    std::optional<TriangleHit> hit = intersect_ray_triangle(
                        local_ray, mesh.vertices[i0].position, mesh.vertices[i1].position,
                        mesh.vertices[i2].position, cull_back_face);
                    if (hit.has_value() && hit->distance < nearest_distance) {
                        TriangleHit candidate = hit.value();
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
    std::unordered_map<MeshCacheKey, MeshAcceleration, MeshCacheHash> mesh_cache_;
};

PickingService::PickingService() : impl_(std::make_unique<Impl>()) {}
PickingService::~PickingService() = default;
PickingService::PickingService(PickingService &&) noexcept = default;
PickingService &PickingService::operator=(PickingService &&) noexcept = default;

Result<Ray3> PickingService::make_picking_ray(const scene::Storage &scene, EntityId camera,
                                              Extent2D extent, Float2 position_pixels) const {
    return impl_->make_picking_ray(scene, camera, extent, position_pixels);
}

Result<std::optional<PickHit>> PickingService::pick(const scene::Storage &scene, EntityId camera,
                                                    Extent2D extent, Float2 position_pixels,
                                                    const PickOptions &options) {
    return impl_->pick(scene, camera, extent, position_pixels, options);
}

Result<std::optional<PickHit>>
PickingService::pick(const scene::Storage &scene, EntityId camera, Extent2D extent,
                     Float2 position_pixels, const PickOptions &options,
                     const scene::VisibilityFilter &visibility) {
    return impl_->pick(scene, camera, extent, position_pixels, options, visibility);
}

Result<std::optional<PickHit>>
PickingService::pick(const scene::Storage &scene, EntityId camera, Extent2D extent,
                     Float2 position_pixels, const PickOptions &options,
                     const scene::VisibilityFilter &visibility,
                     const clipping::ClippingFilter &clipping_filter) {
    return impl_->pick(scene, camera, extent, position_pixels, options, visibility,
                       clipping_filter);
}

Result<std::optional<PickHit>>
PickingService::pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options) {
    return impl_->pick_ray(scene, ray, options);
}

Result<std::optional<PickHit>>
PickingService::pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options,
                         const scene::VisibilityFilter &visibility) {
    return impl_->pick_ray(scene, ray, options, visibility);
}

Result<std::optional<PickHit>>
PickingService::pick_ray(const scene::Storage &scene, const Ray3 &ray, const PickOptions &options,
                         const scene::VisibilityFilter &visibility,
                         const clipping::ClippingFilter &clipping_filter) {
    return impl_->pick_ray(scene, ray, options, visibility, clipping_filter);
}

void PickingService::release_scene(SceneId scene) noexcept {
    impl_->release_scene(scene);
}

PickingStatistics PickingService::statistics() const noexcept {
    return impl_->statistics();
}

} // namespace elf3d::picking
