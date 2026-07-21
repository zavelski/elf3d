#ifndef ELF3D_MEASUREMENT_H
#define ELF3D_MEASUREMENT_H

#include <elf3d/assets.h>
#include <elf3d/core/error.h>
#include <elf3d/graphics.h>

#include <cstdint>
#include <optional>

namespace elf3d {

enum class ViewportTool {
    selection,
    distance_measurement,
};

enum class DistanceMeasurementState {
    empty,
    awaiting_first_point,
    awaiting_second_point,
    complete,
};

enum class LengthDisplayUnit {
    automatic_metric,
    meters,
    centimeters,
    millimeters,
    feet,
    inches,
};

struct MeasurementPoint {
    EntityId entity;
    MeshHandle mesh;

    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;

    Float3 barycentric_coordinates;

    Float3 world_position;
    Float3 world_normal;

    bool operator==(const MeasurementPoint&) const = default;
};

struct DistanceMeasurementSettings {
    Color4 line_color{1.0F, 0.75F, 0.1F, 1.0F};
    Color4 first_point_color{0.2F, 0.9F, 0.3F, 1.0F};
    Color4 second_point_color{1.0F, 0.35F, 0.2F, 1.0F};

    float line_thickness_pixels = 2.0F;
    float marker_radius_pixels = 6.0F;

    OverlayDepthMode depth_mode = OverlayDepthMode::always_visible;
    LengthDisplayUnit display_unit = LengthDisplayUnit::automatic_metric;

    bool operator==(const DistanceMeasurementSettings&) const = default;
};

struct DistanceMeasurementSnapshot {
    DistanceMeasurementState state = DistanceMeasurementState::empty;

    std::optional<MeasurementPoint> first_point;
    std::optional<MeasurementPoint> second_point;
    std::optional<MeasurementPoint> preview_point;
    std::optional<Float3> midpoint_world_position;

    double distance_meters = 0.0;
    double preview_distance_meters = 0.0;

    bool overlay_visible = false;
    bool anchors_currently_visible = false;

    std::optional<Error> diagnostic;
};

struct MeasurementStatistics {
    std::uint64_t committed_points = 0;
    std::uint64_t preview_picks = 0;
    std::uint64_t anchor_resolutions = 0;
    std::uint64_t overlay_lines = 0;
    std::uint64_t overlay_markers = 0;

    bool operator==(const MeasurementStatistics&) const = default;
};

struct ProjectedViewportPoint {
    Float2 position_pixels;
    float depth = 0.0F;

    bool is_in_front = false;
    bool is_inside_viewport = false;

    bool operator==(const ProjectedViewportPoint&) const = default;
};

} // namespace elf3d

#endif
