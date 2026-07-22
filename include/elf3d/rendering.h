#ifndef ELF3D_RENDERING_H
#define ELF3D_RENDERING_H

#include <elf3d/assets.h>
#include <elf3d/graphics.h>

#include <cstdint>
#include <optional>
#include <span>

namespace elf3d {

struct BasicLighting {
    // Direction in which light travels in world space.
    Float3 direction{-0.5F, -1.0F, -0.3F};
    Color4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float ambient_intensity = 0.08F;
    float diffuse_intensity = 3.0F;
};

enum class RenderShadingMode : std::uint8_t {
    standard,
    unlit,
};

struct RenderStatistics {
    std::uint64_t draw_calls = 0;
    std::uint64_t triangles = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    // Activity caused by the latest render call.
    std::uint64_t texture_bindings = 0;
    std::uint64_t gpu_texture_uploads = 0;
    // Current renderer cache contents after the latest render call.
    std::uint64_t unique_gpu_textures = 0;
    std::uint64_t overlay_lines = 0;
    std::uint64_t overlay_markers = 0;
    std::uint64_t clipping_bounds_tested = 0;
    std::uint64_t clipping_bounds_rejected = 0;
    std::uint64_t clipping_bounds_intersecting = 0;

    // Latest render-call workload and resource activity.
    std::uint64_t candidate_primitives = 0;
    std::uint64_t visible_primitives = 0;
    std::uint64_t frustum_culled_primitives = 0;
    std::uint64_t material_switches = 0;
    std::uint64_t shader_switches = 0;
    std::uint64_t instanced_draw_calls = 0;
    std::uint64_t gpu_buffer_uploads = 0;
    std::uint64_t gpu_buffer_uploaded_bytes = 0;
    std::uint64_t render_passes = 0;
    std::uint64_t draw_packet_rebuilds = 0;

    // Current renderer-owned residency estimates after the latest render call.
    std::uint64_t estimated_resident_geometry_bytes = 0;
    std::uint64_t estimated_resident_texture_bytes = 0;

    // CPU timings cover the latest render call. GPU timings are delayed and are
    // valid only when the matching availability flag is true.
    double cpu_render_list_milliseconds = 0.0;
    double cpu_resource_preparation_milliseconds = 0.0;
    double cpu_gl_submission_milliseconds = 0.0;
    double cpu_total_milliseconds = 0.0;
    double gpu_main_pass_milliseconds = 0.0;
    double gpu_resolve_milliseconds = 0.0;
    bool gpu_main_pass_timing_available = false;
    bool gpu_resolve_timing_available = false;

    bool operator==(const RenderStatistics&) const = default;
};

struct EntityHighlight {
    EntityId entity;
    Color4 color{1.0F, 0.55F, 0.05F, 1.0F};
    float strength = 0.0F;

    bool operator==(const EntityHighlight&) const = default;
};

struct ViewportRenderOptions {
    std::optional<EntityHighlight> highlight;
    std::span<const OverlayLineSegment> overlay_lines;
    std::span<const OverlayPointMarker> overlay_markers;
    RenderShadingMode shading_mode = RenderShadingMode::standard;

    [[nodiscard]] bool operator==(const ViewportRenderOptions& other) const noexcept {
        return highlight == other.highlight && overlay_lines.data() == other.overlay_lines.data() &&
               overlay_lines.size() == other.overlay_lines.size() &&
               overlay_markers.data() == other.overlay_markers.data() &&
               overlay_markers.size() == other.overlay_markers.size() &&
               shading_mode == other.shading_mode;
    }
};

} // namespace elf3d

#endif
