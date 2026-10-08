#include "viewer_tools.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "viewer_tool_style.hpp"

namespace elf3d::viewer {
using namespace tool_detail;
namespace {

[[nodiscard]] bool valid_clipping_settings(const ClippingToolSettings& settings) noexcept
{
    return finite_color(settings.section_plane_color) && finite_color(settings.box_color) &&
           std::isfinite(settings.line_thickness_pixels) && settings.line_thickness_pixels > 0.0F &&
           settings.line_thickness_pixels <= 32.0F && valid_depth_mode(settings.depth_mode);
}

[[nodiscard]] Float3 subtract(Float3 left, Float3 right) noexcept
{
    return Float3{left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] Float3 add(Float3 left, Float3 right) noexcept
{
    return Float3{left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] Float3 scale(Float3 value, float multiplier) noexcept
{
    return Float3{value.x * multiplier, value.y * multiplier, value.z * multiplier};
}

[[nodiscard]] float dot(Float3 left, Float3 right) noexcept
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] float length(Float3 value) noexcept
{
    return std::sqrt(dot(value, value));
}

[[nodiscard]] Float3 cross(Float3 left, Float3 right) noexcept
{
    return Float3{left.y * right.z - left.z * right.y, left.z * right.x - left.x * right.z,
                  left.x * right.y - left.y * right.x};
}

[[nodiscard]] Float3 normalized_or(Float3 value, Float3 fallback) noexcept
{
    const float value_length = length(value);
    if (!std::isfinite(value_length) || value_length <= 0.000001F) {
        return fallback;
    }
    return scale(value, 1.0F / value_length);
}

[[nodiscard]] Float3 center(Bounds3 bounds) noexcept
{
    return Float3{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                  (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                  (bounds.minimum.z + bounds.maximum.z) * 0.5F};
}

[[nodiscard]] float radius(Bounds3 bounds) noexcept
{
    return std::max(0.5F * length(subtract(bounds.maximum, bounds.minimum)), 0.5F);
}

void expand_degenerate_bounds(Bounds3& bounds) noexcept
{
    const Float3 extent = subtract(bounds.maximum, bounds.minimum);
    const float largest_extent = std::max({extent.x, extent.y, extent.z, 0.0F});
    const float minimum_half_extent = std::max(largest_extent * 0.005F, 0.0005F);
    const auto expand_axis = [minimum_half_extent](float& minimum, float& maximum) noexcept {
        if (maximum > minimum) {
            return;
        }

        const float axis_center = (minimum + maximum) * 0.5F;
        minimum = axis_center - minimum_half_extent;
        maximum = axis_center + minimum_half_extent;
    };
    expand_axis(bounds.minimum.x, bounds.maximum.x);
    expand_axis(bounds.minimum.y, bounds.maximum.y);
    expand_axis(bounds.minimum.z, bounds.maximum.z);
}

void append_clipping_line(ClippingToolOverlay& overlay, Float3 start, Float3 end,
                          const ClippingToolSettings& settings, Color4 color) noexcept
{
    if (overlay.line_count >= overlay.lines.size()) {
        return;
    }
    overlay.lines[overlay.line_count++] =
        OverlayLineSegment{start, end, color, settings.line_thickness_pixels, settings.depth_mode};
}

void append_clipping_box(ClippingToolOverlay& overlay, const ClippingBox& box,
                         const ClippingToolSettings& settings) noexcept
{
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
    constexpr std::array<std::array<std::size_t, 2>, 12> edges{{
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
    for (const std::array<std::size_t, 2>& edge : edges) {
        append_clipping_line(overlay, corners[edge[0]], corners[edge[1]], settings,
                             settings.box_color);
    }
}

void append_section_plane(ClippingToolOverlay& overlay, const SectionPlane& plane, Bounds3 bounds,
                          const ClippingToolSettings& settings) noexcept
{
    if (!plane.enabled) {
        return;
    }

    const Float3 normal = normalized_or(plane.normal, {0.0F, 1.0F, 0.0F});
    Float3 plane_center = center(bounds);
    const float signed_distance = dot(normal, subtract(plane_center, plane.point));
    plane_center = subtract(plane_center, scale(normal, signed_distance));
    const float plane_radius = radius(bounds);
    const Float3 reference =
        std::abs(normal.y) < 0.9F ? Float3{0.0F, 1.0F, 0.0F} : Float3{1.0F, 0.0F, 0.0F};
    const Float3 first_axis =
        scale(normalized_or(cross(normal, reference), {1.0F, 0.0F, 0.0F}), plane_radius);
    const Float3 second_axis =
        scale(normalized_or(cross(normal, first_axis), {0.0F, 0.0F, 1.0F}), plane_radius);
    const std::array<Float3, 4> corners{{
        subtract(subtract(plane_center, first_axis), second_axis),
        subtract(add(plane_center, first_axis), second_axis),
        add(add(plane_center, first_axis), second_axis),
        add(subtract(plane_center, first_axis), second_axis),
    }};
    append_clipping_line(overlay, corners[0], corners[1], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[1], corners[2], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[2], corners[3], settings, settings.section_plane_color);
    append_clipping_line(overlay, corners[3], corners[0], settings, settings.section_plane_color);
}

} // namespace

Result<void> ClippingTool::set_settings(const ClippingToolSettings& settings) noexcept
{
    if (!valid_clipping_settings(settings)) {
        return Error{ErrorCode::invalid_argument,
                     "Clipping Tool settings require finite colors, line thickness in (0, 32], "
                     "and a supported depth mode"};
    }
    settings_ = settings;
    settings_.section_plane_color = sanitized_color(settings.section_plane_color);
    settings_.box_color = sanitized_color(settings.box_color);
    return {};
}

ClippingToolSettings ClippingTool::settings() const noexcept
{
    return settings_;
}

void ClippingTool::set_helpers_visible(bool visible) noexcept
{
    settings_.helpers_visible = visible;
}

bool ClippingTool::helpers_visible() const noexcept
{
    return settings_.helpers_visible;
}

Result<ClippingBox> ClippingTool::box_from_visible_bounds(const Scene& scene,
                                                          const Viewport& viewport) const noexcept
{
    const Result<std::optional<Bounds3>> bounds = viewport.unclipped_visible_bounds(scene);
    if (!bounds) {
        return bounds.error();
    }
    if (!bounds.value().has_value()) {
        return Error{ErrorCode::scene_has_no_bounds,
                     "Clipping box creation requires visible renderable scene bounds"};
    }
    Bounds3 expanded = *bounds.value();
    expand_degenerate_bounds(expanded);
    return ClippingBox{expanded.minimum, expanded.maximum, true};
}

Result<std::uint32_t> ClippingTool::add_box_from_visible_bounds(const Scene& scene,
                                                                Viewport& viewport) noexcept
{
    const Result<ClippingBox> box = box_from_visible_bounds(scene, viewport);
    if (!box) {
        return box.error();
    }
    return viewport.add_clipping_box(box.value());
}

Result<void> ClippingTool::reset_box_to_visible_bounds(const Scene& scene, Viewport& viewport,
                                                       std::uint32_t index) noexcept
{
    const Result<ClippingBox> box = box_from_visible_bounds(scene, viewport);
    if (!box) {
        return box.error();
    }
    return viewport.set_clipping_box(index, box.value());
}

Result<ClippingToolOverlay> ClippingTool::overlay(const Scene& scene,
                                                  const Viewport& viewport) const noexcept
{
    ClippingToolOverlay result;
    if (!settings_.helpers_visible) {
        return result;
    }

    const ClippingSnapshot clipping = viewport.clipping_snapshot();
    if (clipping.section_plane.enabled) {
        const Result<std::optional<Bounds3>> bounds = viewport.unclipped_visible_bounds(scene);
        if (!bounds) {
            return bounds.error();
        }
        if (bounds.value().has_value()) {
            append_section_plane(result, clipping.section_plane, *bounds.value(), settings_);
        }
    }
    for (std::uint32_t index = 0; index < clipping.box_count; ++index) {
        if (clipping.boxes[index].enabled) {
            append_clipping_box(result, clipping.boxes[index], settings_);
        }
    }
    return result;
}

} // namespace elf3d::viewer
