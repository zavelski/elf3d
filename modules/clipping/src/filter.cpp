#include <elf3d/clipping/filter.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace elf3d::clipping {
namespace {

constexpr std::array<std::array<int, 2>, 12> box_edges{{
    {{0, 1}},
    {{0, 2}},
    {{1, 3}},
    {{2, 3}},
    {{4, 5}},
    {{4, 6}},
    {{5, 7}},
    {{6, 7}},
    {{0, 4}},
    {{1, 5}},
    {{2, 6}},
    {{3, 7}},
}};

[[nodiscard]] bool finite_float(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return finite_float(value.x) && finite_float(value.y) && finite_float(value.z);
}

[[nodiscard]] float dot(Float3 left, Float3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
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

void expand(Bounds3 &bounds, Float3 point) noexcept {
    if (!finite_float3(point)) {
        bounds = {};
        return;
    }
    if (!bounds.is_valid) {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.is_valid = true;
        return;
    }
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

void expand(Bounds3 &bounds, Bounds3 other) noexcept {
    if (!is_valid_bounds(other)) {
        return;
    }
    expand(bounds, other.minimum);
    expand(bounds, other.maximum);
}

[[nodiscard]] std::array<Float3, 8> corners(Bounds3 bounds) noexcept {
    return {{
        {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
    }};
}

[[nodiscard]] float plane_signed_distance(const ClippingFilter &filter, Float3 point) noexcept {
    const float signed_distance = dot(filter.section_plane_normal, point) +
                                  filter.section_plane_offset;
    return filter.retain_positive_half_space ? signed_distance : -signed_distance;
}

[[nodiscard]] bool plane_contains_point(const ClippingFilter &filter, Float3 point) noexcept {
    if (!filter.section_plane_enabled) {
        return true;
    }
    return plane_signed_distance(filter, point) >= -clipping_boundary_epsilon;
}

[[nodiscard]] bool box_contains_point(Bounds3 box, Float3 point) noexcept {
    return point.x >= box.minimum.x - clipping_boundary_epsilon &&
           point.y >= box.minimum.y - clipping_boundary_epsilon &&
           point.z >= box.minimum.z - clipping_boundary_epsilon &&
           point.x <= box.maximum.x + clipping_boundary_epsilon &&
           point.y <= box.maximum.y + clipping_boundary_epsilon &&
           point.z <= box.maximum.z + clipping_boundary_epsilon;
}

[[nodiscard]] bool boxes_contain_point(const ClippingFilter &filter, Float3 point) noexcept {
    if (filter.enabled_box_count == 0) {
        return true;
    }
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        if (box_contains_point(filter.boxes[index], point)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool bounds_overlap(Bounds3 left, Bounds3 right) noexcept {
    return left.minimum.x <= right.maximum.x + clipping_boundary_epsilon &&
           left.maximum.x + clipping_boundary_epsilon >= right.minimum.x &&
           left.minimum.y <= right.maximum.y + clipping_boundary_epsilon &&
           left.maximum.y + clipping_boundary_epsilon >= right.minimum.y &&
           left.minimum.z <= right.maximum.z + clipping_boundary_epsilon &&
           left.maximum.z + clipping_boundary_epsilon >= right.minimum.z;
}

[[nodiscard]] bool bounds_inside_box(Bounds3 bounds, Bounds3 box) noexcept {
    for (const Float3 corner : corners(bounds)) {
        if (!box_contains_point(box, corner)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Bounds3 intersect_bounds(Bounds3 left, Bounds3 right) noexcept {
    if (!is_valid_bounds(left) || !is_valid_bounds(right) || !bounds_overlap(left, right)) {
        return {};
    }
    return Bounds3{
        Float3{std::max(left.minimum.x, right.minimum.x), std::max(left.minimum.y, right.minimum.y),
               std::max(left.minimum.z, right.minimum.z)},
        Float3{std::min(left.maximum.x, right.maximum.x), std::min(left.maximum.y, right.maximum.y),
               std::min(left.maximum.z, right.maximum.z)},
        true};
}

[[nodiscard]] bool plane_may_leave_bounds(const ClippingFilter &filter, Bounds3 bounds) noexcept {
    if (!filter.section_plane_enabled) {
        return true;
    }
    for (const Float3 corner : corners(bounds)) {
        if (plane_contains_point(filter, corner)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool plane_contains_bounds(const ClippingFilter &filter, Bounds3 bounds) noexcept {
    if (!filter.section_plane_enabled) {
        return true;
    }
    for (const Float3 corner : corners(bounds)) {
        if (!plane_contains_point(filter, corner)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] Bounds3 clip_bounds_against_plane(const ClippingFilter &filter,
                                                Bounds3 bounds) noexcept {
    if (!is_valid_bounds(bounds)) {
        return {};
    }
    if (!filter.section_plane_enabled) {
        return bounds;
    }

    const std::array<Float3, 8> box_corners = corners(bounds);
    std::array<float, 8> distances{};
    Bounds3 result;
    for (std::size_t index = 0; index < box_corners.size(); ++index) {
        distances[index] = plane_signed_distance(filter, box_corners[index]);
        if (distances[index] >= -clipping_boundary_epsilon) {
            expand(result, box_corners[index]);
        }
    }

    for (const std::array<int, 2> &edge : box_edges) {
        const int first = edge[0];
        const int second = edge[1];
        const float first_distance = distances[static_cast<std::size_t>(first)];
        const float second_distance = distances[static_cast<std::size_t>(second)];
        if ((first_distance < -clipping_boundary_epsilon &&
             second_distance < -clipping_boundary_epsilon) ||
            (first_distance > clipping_boundary_epsilon &&
             second_distance > clipping_boundary_epsilon)) {
            continue;
        }
        const float denominator = first_distance - second_distance;
        if (!finite_float(denominator) || std::abs(denominator) <= clipping_boundary_epsilon) {
            continue;
        }
        const float t = std::clamp(first_distance / denominator, 0.0F, 1.0F);
        const Float3 point =
            add(box_corners[static_cast<std::size_t>(first)],
                scale(subtract(box_corners[static_cast<std::size_t>(second)],
                               box_corners[static_cast<std::size_t>(first)]),
                      t));
        expand(result, point);
    }
    return result;
}

} // namespace

Result<SectionPlane> normalized_section_plane(const SectionPlane &plane) noexcept {
    const bool valid_side = plane.retained_half_space == PlaneHalfSpace::positive ||
                            plane.retained_half_space == PlaneHalfSpace::negative;
    if (!valid_side) {
        return Error{ErrorCode::invalid_section_plane,
                     "Section plane retained half-space is unsupported"};
    }
    if (!finite_float3(plane.point)) {
        return Error{ErrorCode::invalid_section_plane,
                     "Section plane point must contain only finite values"};
    }
    if (!finite_float3(plane.normal)) {
        return Error{ErrorCode::invalid_section_plane,
                     "Section plane normal must contain only finite values"};
    }
    const float length_squared = dot(plane.normal, plane.normal);
    if (!finite_float(length_squared) ||
        length_squared <= clipping_boundary_epsilon * clipping_boundary_epsilon) {
        return Error{ErrorCode::invalid_section_plane,
                     "Section plane normal must have nonzero length"};
    }
    const float length = std::sqrt(length_squared);
    SectionPlane normalized = plane;
    normalized.normal = scale(plane.normal, 1.0F / length);
    return normalized;
}

Result<ClippingBox> validated_clipping_box(const ClippingBox &box) noexcept {
    if (!finite_float3(box.minimum) || !finite_float3(box.maximum)) {
        return Error{ErrorCode::invalid_clipping_box,
                     "Clipping box bounds must contain only finite values"};
    }
    if (box.minimum.x > box.maximum.x || box.minimum.y > box.maximum.y ||
        box.minimum.z > box.maximum.z) {
        return Error{ErrorCode::invalid_clipping_box,
                     "Clipping box minimum coordinates must not exceed maximum coordinates"};
    }
    if (box.maximum.x - box.minimum.x <= minimum_clipping_box_extent ||
        box.maximum.y - box.minimum.y <= minimum_clipping_box_extent ||
        box.maximum.z - box.minimum.z <= minimum_clipping_box_extent) {
        return Error{ErrorCode::invalid_clipping_box,
                     "Clipping box extents must be greater than the clipping epsilon"};
    }
    return box;
}

Result<ClippingFilter> make_filter(const SectionPlane &section_plane,
                                   std::span<const ClippingBox> boxes,
                                   std::uint64_t revision) {
    if (boxes.size() > maximum_clipping_boxes) {
        return Error{ErrorCode::clipping_box_limit_exceeded,
                     "A viewport supports at most three clipping boxes"};
    }
    ClippingFilter filter;
    filter.revision = revision;
    const Result<SectionPlane> normalized = normalized_section_plane(section_plane);
    if (!normalized) {
        return normalized.error();
    }
    filter.section_plane_enabled = normalized.value().enabled;
    filter.section_plane_normal = normalized.value().normal;
    filter.section_plane_offset = -dot(normalized.value().normal, normalized.value().point);
    filter.retain_positive_half_space =
        normalized.value().retained_half_space == PlaneHalfSpace::positive;

    for (const ClippingBox &box : boxes) {
        const Result<ClippingBox> validated = validated_clipping_box(box);
        if (!validated) {
            return validated.error();
        }
        if (!validated.value().enabled) {
            continue;
        }
        filter.boxes[filter.enabled_box_count++] =
            Bounds3{validated.value().minimum, validated.value().maximum, true};
    }
    return filter;
}

ClippingFilter disabled_filter() noexcept {
    return {};
}

bool is_valid_bounds(Bounds3 bounds) noexcept {
    return bounds.is_valid && finite_float3(bounds.minimum) && finite_float3(bounds.maximum) &&
           bounds.minimum.x <= bounds.maximum.x && bounds.minimum.y <= bounds.maximum.y &&
           bounds.minimum.z <= bounds.maximum.z;
}

Bounds3 transform_bounds(Bounds3 local_bounds, const math::Matrix4 &world) noexcept {
    if (!is_valid_bounds(local_bounds)) {
        return {};
    }
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(world[column][row])) {
                return {};
            }
        }
    }

    Bounds3 result;
    for (const Float3 corner : corners(local_bounds)) {
        const math::Vector4 transformed =
            world * math::Vector4{corner.x, corner.y, corner.z, 1.0F};
        expand(result, Float3{transformed.x, transformed.y, transformed.z});
    }
    return result;
}

bool contains_point(const ClippingFilter &filter, Float3 world_position) noexcept {
    if (!finite_float3(world_position)) {
        return false;
    }
    return plane_contains_point(filter, world_position) &&
           boxes_contain_point(filter, world_position);
}

BoundsClassification classify_bounds(const ClippingFilter &filter,
                                     Bounds3 world_bounds) noexcept {
    if (!is_valid_bounds(world_bounds)) {
        return BoundsClassification::outside;
    }
    if (!filter.has_clipping()) {
        return BoundsClassification::inside;
    }

    if (filter.enabled_box_count == 0) {
        if (!plane_may_leave_bounds(filter, world_bounds)) {
            return BoundsClassification::outside;
        }
        return plane_contains_bounds(filter, world_bounds) ? BoundsClassification::inside
                                                           : BoundsClassification::intersecting;
    }

    bool any_box_overlap = false;
    bool any_region_survives_plane = false;
    bool inside_one_box = false;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        const Bounds3 box = filter.boxes[index];
        if (!bounds_overlap(world_bounds, box)) {
            continue;
        }
        any_box_overlap = true;
        const Bounds3 overlap = intersect_bounds(world_bounds, box);
        if (plane_may_leave_bounds(filter, overlap)) {
            any_region_survives_plane = true;
        }
        if (bounds_inside_box(world_bounds, box)) {
            inside_one_box = true;
        }
    }
    if (!any_box_overlap || !any_region_survives_plane) {
        return BoundsClassification::outside;
    }
    if (inside_one_box && plane_contains_bounds(filter, world_bounds)) {
        return BoundsClassification::inside;
    }
    return BoundsClassification::intersecting;
}

Bounds3 clipped_bounds(const ClippingFilter &filter, Bounds3 world_bounds) noexcept {
    if (!is_valid_bounds(world_bounds)) {
        return {};
    }
    if (!filter.has_clipping()) {
        return world_bounds;
    }
    if (classify_bounds(filter, world_bounds) == BoundsClassification::outside) {
        return {};
    }

    if (filter.enabled_box_count == 0) {
        return clip_bounds_against_plane(filter, world_bounds);
    }

    Bounds3 result;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        const Bounds3 overlapped = intersect_bounds(world_bounds, filter.boxes[index]);
        const Bounds3 clipped = clip_bounds_against_plane(filter, overlapped);
        expand(result, clipped);
    }
    return result;
}

} // namespace elf3d::clipping
