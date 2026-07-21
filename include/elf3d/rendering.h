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

    [[nodiscard]] bool operator==(const ViewportRenderOptions& other) const noexcept {
        return highlight == other.highlight && overlay_lines.data() == other.overlay_lines.data() &&
               overlay_lines.size() == other.overlay_lines.size() &&
               overlay_markers.data() == other.overlay_markers.data() &&
               overlay_markers.size() == other.overlay_markers.size();
    }
};

} // namespace elf3d

#endif
