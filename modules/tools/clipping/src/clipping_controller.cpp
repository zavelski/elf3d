module;

#include <elf3d/clipping.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>

module elf.tool.clipping;

import elf.assets;
import elf.clipping;
import elf.scene;

namespace elf3d::tools::clipping {
namespace {

[[nodiscard]] bool finite_color(Color4 color) noexcept {
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
}

[[nodiscard]] Color4 sanitized_color(Color4 color) noexcept {
    const auto channel = [](float value, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 1.0F),
                  channel(color.blue, 1.0F), channel(color.alpha, 1.0F)};
}

[[nodiscard]] bool valid_helper_settings(const ClippingHelperSettings &settings) noexcept {
    return finite_color(settings.section_plane_color) && finite_color(settings.box_color) &&
           std::isfinite(settings.line_thickness_pixels) &&
           settings.line_thickness_pixels > 0.0F && settings.line_thickness_pixels <= 32.0F;
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

[[nodiscard]] Float3 normalized_or(Float3 value, Float3 fallback) noexcept {
    const float value_length = length(value);
    if (!std::isfinite(value_length) || value_length <= 0.000001F) {
        return fallback;
    }
    return scale(value, 1.0F / value_length);
}

[[nodiscard]] Float3 bounds_center(Bounds3 bounds) noexcept {
    return Float3{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                  (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                  (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

[[nodiscard]] float bounds_radius(Bounds3 bounds) noexcept {
    const Float3 extent = subtract(bounds.maximum, bounds.minimum);
    return std::max(0.5F * length(extent), 0.5F);
}

void append_line(ClippingOverlay &overlay, Float3 start, Float3 end, Color4 color,
                 float thickness) noexcept {
    if (overlay.line_count >= overlay.lines.size()) {
        return;
    }
    overlay.lines[overlay.line_count++] =
        OverlayLineSegment{start, end, color, thickness, OverlayDepthMode::always_visible};
}

void append_box(ClippingOverlay &overlay, const ClippingBox &box, Color4 color,
                float thickness) noexcept {
    const Float3 min = box.minimum;
    const Float3 max = box.maximum;
    const std::array<Float3, 8> corners{{
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z},
    }};
    constexpr std::array<std::array<int, 2>, 12> edges{{
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
    for (const std::array<int, 2> &edge : edges) {
        append_line(overlay, corners[static_cast<std::size_t>(edge[0])],
                    corners[static_cast<std::size_t>(edge[1])], color, thickness);
    }
}

void append_section_plane(ClippingOverlay &overlay, const SectionPlane &plane, Bounds3 helper_bounds,
                          Color4 color, float thickness) noexcept {
    const Result<SectionPlane> normalized = elf3d::clipping::normalized_section_plane(plane);
    if (!normalized || !normalized.value().enabled) {
        return;
    }
    const Float3 normal = normalized.value().normal;
    Float3 center = elf3d::clipping::is_valid_bounds(helper_bounds) ? bounds_center(helper_bounds)
                                                                    : normalized.value().point;
    const float signed_distance = dot(normal, subtract(center, normalized.value().point));
    center = subtract(center, scale(normal, signed_distance));
    const float radius = elf3d::clipping::is_valid_bounds(helper_bounds) ? bounds_radius(helper_bounds)
                                                                         : 1.0F;
    const Float3 reference =
        std::abs(normal.y) < 0.9F ? Float3{0.0F, 1.0F, 0.0F} : Float3{1.0F, 0.0F, 0.0F};
    const Float3 first_axis = scale(normalized_or(cross(normal, reference), {1.0F, 0.0F, 0.0F}),
                                    radius);
    const Float3 second_axis =
        scale(normalized_or(cross(normal, first_axis), {0.0F, 0.0F, 1.0F}), radius);
    const std::array<Float3, 4> corners{{
        subtract(subtract(center, first_axis), second_axis),
        subtract(add(center, first_axis), second_axis),
        add(add(center, first_axis), second_axis),
        add(subtract(center, first_axis), second_axis),
    }};
    append_line(overlay, corners[0], corners[1], color, thickness);
    append_line(overlay, corners[1], corners[2], color, thickness);
    append_line(overlay, corners[2], corners[3], color, thickness);
    append_line(overlay, corners[3], corners[0], color, thickness);
}

} // namespace

Result<void> ClippingController::set_section_plane(const SectionPlane &plane) noexcept {
    const Result<SectionPlane> normalized = elf3d::clipping::normalized_section_plane(plane);
    if (!normalized) {
        return normalized.error();
    }
    if (section_plane_ == plane) {
        return {};
    }
    section_plane_ = plane;
    increment_revision();
    return {};
}

void ClippingController::clear_section_plane() noexcept {
    if (!section_plane_.enabled) {
        return;
    }
    section_plane_.enabled = false;
    increment_revision();
}

Result<std::uint32_t> ClippingController::add_box(const ClippingBox &box) {
    if (box_count_ >= maximum_clipping_boxes) {
        return Error{ErrorCode::clipping_box_limit_exceeded,
                     "A viewport supports at most three clipping boxes"};
    }
    const Result<ClippingBox> validated = elf3d::clipping::validated_clipping_box(box);
    if (!validated) {
        return validated.error();
    }
    boxes_[box_count_] = box;
    const std::uint32_t index = box_count_;
    ++box_count_;
    increment_revision();
    return index;
}

Result<void> ClippingController::set_box(std::uint32_t index,
                                         const ClippingBox &box) noexcept {
    if (index >= box_count_) {
        return Error{ErrorCode::invalid_clipping_box_index,
                     "The clipping box index is outside the viewport box range"};
    }
    const Result<ClippingBox> validated = elf3d::clipping::validated_clipping_box(box);
    if (!validated) {
        return validated.error();
    }
    if (boxes_[index] == box) {
        return {};
    }
    boxes_[index] = box;
    increment_revision();
    return {};
}

Result<void> ClippingController::remove_box(std::uint32_t index) noexcept {
    if (index >= box_count_) {
        return Error{ErrorCode::invalid_clipping_box_index,
                     "The clipping box index is outside the viewport box range"};
    }
    for (std::uint32_t current = index; current + 1U < box_count_; ++current) {
        boxes_[current] = boxes_[current + 1U];
    }
    boxes_[box_count_ - 1U] = {};
    --box_count_;
    increment_revision();
    return {};
}

void ClippingController::clear_boxes() noexcept {
    if (box_count_ == 0) {
        return;
    }
    boxes_ = {};
    box_count_ = 0;
    increment_revision();
}

void ClippingController::clear() noexcept {
    const bool had_plane = section_plane_.enabled;
    const bool had_boxes = box_count_ != 0;
    if (!had_plane && !had_boxes) {
        return;
    }
    section_plane_.enabled = false;
    boxes_ = {};
    box_count_ = 0;
    increment_revision();
}

Result<void> ClippingController::set_helpers_visible(bool visible) noexcept {
    if (helper_settings_.visible == visible) {
        return {};
    }
    helper_settings_.visible = visible;
    increment_revision();
    return {};
}

Result<void>
ClippingController::set_helper_settings(const ClippingHelperSettings &settings) noexcept {
    if (!valid_helper_settings(settings)) {
        return Error{ErrorCode::invalid_clipping_settings,
                     "Clipping helper settings require finite colors and positive line thickness"};
    }
    ClippingHelperSettings sanitized = settings;
    sanitized.section_plane_color = sanitized_color(settings.section_plane_color);
    sanitized.box_color = sanitized_color(settings.box_color);
    if (helper_settings_ == sanitized) {
        return {};
    }
    helper_settings_ = sanitized;
    increment_revision();
    return {};
}

Result<void> ClippingController::reset_box_to_visible_bounds(
    const scene::Storage &scene, const scene::VisibilityFilter &visibility,
    std::uint32_t index) noexcept {
    if (index >= box_count_) {
        return Error{ErrorCode::invalid_clipping_box_index,
                     "The clipping box index is outside the viewport box range"};
    }
    const Result<ClippingBox> box = box_from_visible_bounds(scene, visibility);
    if (!box) {
        return box.error();
    }
    return set_box(index, box.value());
}

Result<std::uint32_t> ClippingController::add_box_from_visible_bounds(
    const scene::Storage &scene, const scene::VisibilityFilter &visibility) {
    const Result<ClippingBox> box = box_from_visible_bounds(scene, visibility);
    if (!box) {
        return box.error();
    }
    return add_box(box.value());
}

ClippingSnapshot ClippingController::snapshot() const noexcept {
    ClippingSnapshot result;
    result.section_plane = section_plane_;
    result.box_count = box_count_;
    for (std::uint32_t index = 0; index < box_count_; ++index) {
        result.boxes[index] = boxes_[index];
    }
    result.helpers = helper_settings_;
    result.revision = revision_;
    return result;
}

Result<elf3d::clipping::ClippingFilter> ClippingController::filter() const {
    return elf3d::clipping::make_filter(section_plane_,
                                        std::span<const ClippingBox>{boxes_.data(), box_count_},
                                        revision_);
}

std::uint64_t ClippingController::revision() const noexcept {
    return revision_;
}

Result<ClippingOverlay> ClippingController::overlay(Bounds3 helper_bounds) const noexcept {
    ClippingOverlay result;
    if (!helper_settings_.visible) {
        return result;
    }
    const Result<elf3d::clipping::ClippingFilter> current_filter = filter();
    if (!current_filter) {
        return current_filter.error();
    }
    if (!current_filter.value().has_clipping()) {
        return result;
    }
    append_section_plane(result, section_plane_, helper_bounds, helper_settings_.section_plane_color,
                         helper_settings_.line_thickness_pixels);
    for (std::uint32_t index = 0; index < box_count_; ++index) {
        if (boxes_[index].enabled) {
            append_box(result, boxes_[index], helper_settings_.box_color,
                       helper_settings_.line_thickness_pixels);
        }
    }
    return result;
}

void ClippingController::increment_revision() noexcept {
    ++revision_;
    if (revision_ == 0) {
        ++revision_;
    }
}

Result<ClippingBox>
ClippingController::box_from_visible_bounds(const scene::Storage &scene,
                                            const scene::VisibilityFilter &visibility) const
    noexcept {
    const Bounds3 bounds = scene.visible_world_bounds(visibility);
    if (!elf3d::clipping::is_valid_bounds(bounds)) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Clipping box reset requires visible renderable scene bounds"};
    }
    ClippingBox box;
    box.minimum = bounds.minimum;
    box.maximum = bounds.maximum;
    box.enabled = true;
    return box;
}

Bounds3 visible_bounds(const scene::Storage &scene, const scene::VisibilityFilter &visibility,
                       const elf3d::clipping::ClippingFilter &filter) noexcept {
    Bounds3 result;
    for (const std::optional<scene::EntityRecord> &record : scene.entities()) {
        if (!record.has_value() || !record->model.has_value() ||
            !scene::entity_visible_in_filter(scene, visibility, record->id)) {
            continue;
        }
        const Result<Float4x4> world = scene.world_matrix(record->id);
        if (!world) {
            continue;
        }
        for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
            const Result<const assets::MeshAsset *> mesh = scene.assets().mesh(primitive.mesh);
            if (!mesh) {
                continue;
            }
            const Bounds3 world_bounds =
                elf3d::clipping::transform_bounds(mesh.value()->bounds, world.value());
            const Bounds3 clipped = elf3d::clipping::clipped_bounds(filter, world_bounds);
            if (elf3d::clipping::is_valid_bounds(clipped)) {
                if (!result.is_valid) {
                    result = clipped;
                } else {
                    result.minimum.x = std::min(result.minimum.x, clipped.minimum.x);
                    result.minimum.y = std::min(result.minimum.y, clipped.minimum.y);
                    result.minimum.z = std::min(result.minimum.z, clipped.minimum.z);
                    result.maximum.x = std::max(result.maximum.x, clipped.maximum.x);
                    result.maximum.y = std::max(result.maximum.y, clipped.maximum.y);
                    result.maximum.z = std::max(result.maximum.z, clipped.maximum.z);
                }
            }
        }
    }
    return result;
}

} // namespace elf3d::tools::clipping
