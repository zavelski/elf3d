#pragma once

#include <elf3d/assets.h>

#include <cstdint>
#include <optional>

namespace elf3d::renderer {

struct GpuPickHit {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
    Float3 world_position;
    float depth = 1.0F;
    float world_distance = 0.0F;
};

struct GpuPickResult {
    std::optional<GpuPickHit> hit;
    std::uint64_t draw_calls = 0;
    std::uint64_t pixels_read = 0;
    double pass_milliseconds = 0.0;
    double readback_milliseconds = 0.0;
};

struct GpuFocusDepthAnchorResult {
    std::optional<Float3> world_position;
    std::uint64_t draw_calls = 0;
    std::uint64_t pixels_read = 0;
    double pass_milliseconds = 0.0;
    double readback_milliseconds = 0.0;
};

struct PickingRasterOptions {
    bool respect_material_sidedness = true;
};

struct GpuPickRequest {
    EntityId camera;
    Float2 target_position_pixels;
    Extent2D viewport_extent;
    Float2 viewport_position_pixels;
    PickingRasterOptions raster;
};

struct GpuFocusDepthRequest {
    EntityId camera;
    Extent2D viewport_extent;
    PickingRasterOptions raster;
};

} // namespace elf3d::renderer
