#include <elf3d/clipping.h>
#include <elf3d/core/error.h>
#include <elf3d/math/detail/glm_helpers.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>

import elf.clipping;

namespace {

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.0001F) noexcept {
    return nearly_equal(left.x, right.x, tolerance) && nearly_equal(left.y, right.y, tolerance) &&
           nearly_equal(left.z, right.z, tolerance);
}

} // namespace

int elf3d_clipping_test() {
    elf3d::SectionPlane plane;
    plane.enabled = true;
    plane.normal = {2.0F, 0.0F, 0.0F};
    const auto normalized = elf3d::clipping::normalized_section_plane(plane);
    if (!normalized || !nearly_equal(normalized.value().normal, {1.0F, 0.0F, 0.0F})) {
        return 1;
    }
    plane.normal = {};
    if (elf3d::clipping::normalized_section_plane(plane).error().code() !=
        elf3d::ErrorCode::invalid_section_plane) {
        return 2;
    }
    plane.normal = {1.0F, 0.0F, 0.0F};
    plane.point.x = std::numeric_limits<float>::quiet_NaN();
    if (elf3d::clipping::normalized_section_plane(plane).error().code() !=
        elf3d::ErrorCode::invalid_section_plane) {
        return 3;
    }

    const elf3d::ClippingBox box{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}, true};
    if (!elf3d::clipping::validated_clipping_box(box)) {
        return 4;
    }
    elf3d::ClippingBox invalid_box = box;
    invalid_box.minimum.x = 2.0F;
    if (elf3d::clipping::validated_clipping_box(invalid_box).error().code() !=
        elf3d::ErrorCode::invalid_clipping_box) {
        return 5;
    }
    invalid_box = box;
    invalid_box.maximum.z = invalid_box.minimum.z;
    if (elf3d::clipping::validated_clipping_box(invalid_box).error().code() !=
        elf3d::ErrorCode::invalid_clipping_box) {
        return 6;
    }

    const elf3d::clipping::ClippingFilter disabled = elf3d::clipping::disabled_filter();
    constexpr elf3d::Bounds3 default_bounds{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    if (!elf3d::clipping::contains_point(disabled, {100.0F, -20.0F, 7.0F}) ||
        elf3d::clipping::classify_bounds(disabled, default_bounds) !=
            elf3d::clipping::BoundsClassification::inside) {
        return 7;
    }

    plane = {};
    plane.enabled = true;
    plane.normal = {1.0F, 0.0F, 0.0F};
    const auto plane_filter = elf3d::clipping::make_filter(plane, {}, 1);
    if (!plane_filter || !elf3d::clipping::contains_point(plane_filter.value(), {0.0F, 0.0F, 0.0F}) ||
        !elf3d::clipping::contains_point(plane_filter.value(), {2.0F, 0.0F, 0.0F}) ||
        elf3d::clipping::contains_point(plane_filter.value(), {-0.1F, 0.0F, 0.0F})) {
        return 8;
    }
    plane.retained_half_space = elf3d::PlaneHalfSpace::negative;
    const auto flipped_filter = elf3d::clipping::make_filter(plane, {}, 2);
    if (!flipped_filter || !elf3d::clipping::contains_point(flipped_filter.value(), {-2.0F, 0.0F, 0.0F}) ||
        elf3d::clipping::contains_point(flipped_filter.value(), {0.1F, 0.0F, 0.0F})) {
        return 9;
    }

    const std::array<elf3d::ClippingBox, 2> disjoint_boxes{{
        {{-2.0F, -1.0F, -1.0F}, {-1.0F, 1.0F, 1.0F}, true},
        {{1.0F, -1.0F, -1.0F}, {2.0F, 1.0F, 1.0F}, true},
    }};
    plane.enabled = false;
    const auto boxes_filter = elf3d::clipping::make_filter(plane, disjoint_boxes, 3);
    if (!boxes_filter || !elf3d::clipping::contains_point(boxes_filter.value(), {-1.5F, 0.0F, 0.0F}) ||
        !elf3d::clipping::contains_point(boxes_filter.value(), {1.5F, 0.0F, 0.0F}) ||
        elf3d::clipping::contains_point(boxes_filter.value(), {0.0F, 0.0F, 0.0F})) {
        return 10;
    }

    plane.enabled = true;
    plane.retained_half_space = elf3d::PlaneHalfSpace::positive;
    const auto combined = elf3d::clipping::make_filter(plane, disjoint_boxes, 4);
    if (!combined || elf3d::clipping::contains_point(combined.value(), {-1.5F, 0.0F, 0.0F}) ||
        !elf3d::clipping::contains_point(combined.value(), {1.5F, 0.0F, 0.0F})) {
        return 11;
    }

    const elf3d::Bounds3 centered{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const elf3d::Bounds3 positive{{0.25F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}};
    const elf3d::Bounds3 negative{{-1.0F, -1.0F, -1.0F}, {-0.25F, 1.0F, 1.0F}};
    if (elf3d::clipping::classify_bounds(plane_filter.value(), positive) !=
            elf3d::clipping::BoundsClassification::inside ||
        elf3d::clipping::classify_bounds(plane_filter.value(), negative) !=
            elf3d::clipping::BoundsClassification::outside ||
        elf3d::clipping::classify_bounds(plane_filter.value(), centered) !=
            elf3d::clipping::BoundsClassification::intersecting) {
        return 12;
    }
    const std::optional<elf3d::Bounds3> clipped =
        elf3d::clipping::clipped_bounds(plane_filter.value(), centered);
    if (!clipped.has_value() || !nearly_equal(clipped->minimum.x, 0.0F) ||
        !nearly_equal(clipped->maximum.x, 1.0F)) {
        return 13;
    }
    const std::optional<elf3d::Bounds3> union_bounds =
        elf3d::clipping::clipped_bounds(boxes_filter.value(), centered);
    if (!union_bounds.has_value() || !nearly_equal(union_bounds->minimum.x, -1.0F) ||
        !nearly_equal(union_bounds->maximum.x, 1.0F)) {
        return 14;
    }
    const std::optional<elf3d::Bounds3> empty =
        elf3d::clipping::clipped_bounds(boxes_filter.value(),
                                        elf3d::Bounds3{{-0.25F, -0.25F, -0.25F},
                                                       {0.25F, 0.25F, 0.25F}});
    if (empty.has_value()) {
        return 15;
    }

    const elf3d::math::Matrix4 native_translated =
        glm::translate(elf3d::math::Matrix4{1.0F}, elf3d::math::Vector3{4.0F, 0.0F, 0.0F});
    const elf3d::Float4x4 translated = elf3d::math::to_float4x4(native_translated);
    const elf3d::Bounds3 transformed = elf3d::clipping::transform_bounds(centered, translated);
    if (!nearly_equal(transformed.minimum.x, 3.0F) ||
        !nearly_equal(transformed.maximum.x, 5.0F)) {
        return 16;
    }

    return 0;
}
