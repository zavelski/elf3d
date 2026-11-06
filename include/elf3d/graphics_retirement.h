#ifndef ELF3D_GRAPHICS_RETIREMENT_H
#define ELF3D_GRAPHICS_RETIREMENT_H

#include <cstdint>
#include <elf3d/assets.h>
#include <optional>

namespace elf3d {
struct GraphicsRetirementBudget {
    double milliseconds = 20;
    std::uint32_t work_units = 65536;
    std::uint32_t driver_calls = 65536;
    std::uint32_t objects = 4096;
};
enum class GraphicsRetirementPhase : std::uint8_t {
    cache_metadata,
    draw_metadata,
    mesh_staging,
    mesh_object,
    texture_object,
    picking_triangles,
    picking_order,
    picking_nodes,
    environment_object,
    shader_object
};
struct GraphicsRetirementFailure {
    GraphicsRetirementPhase phase = GraphicsRetirementPhase::cache_metadata;
    SceneId scene;
    double milliseconds = 0;
    std::uint32_t driver_call_limit = 0;
};
struct GraphicsRetirementStatus {
    std::uint64_t renderer_scenes = 0;
    std::uint64_t picking_scenes = 0;
    std::uint64_t mesh_entries = 0;
    std::uint64_t texture_entries = 0;
    std::uint64_t picking_meshes = 0;
    std::uint64_t estimated_geometry_bytes = 0;
    std::uint64_t estimated_texture_bytes = 0;
    std::uint64_t staging_bytes = 0;
    std::uint64_t picking_bytes = 0;
    std::uint64_t shared_preparations = 0;
    std::uint64_t shared_texture_bytes = 0;
    // Retained diagnostic context; it does not make an otherwise empty queue busy.
    std::optional<GraphicsRetirementFailure> last_failure;
    [[nodiscard]] bool empty() const noexcept
    {
        return renderer_scenes == 0 && picking_scenes == 0 && shared_preparations == 0;
    }
};
struct GraphicsRetirementStep {
    GraphicsRetirementPhase phase = GraphicsRetirementPhase::cache_metadata;
    SceneId scene;
    std::uint32_t deleted_objects = 0;
    std::uint32_t driver_calls = 0;
    std::uint64_t freed_cpu_bytes = 0;
    double milliseconds = 0;
    bool progressed = false;
};
struct GraphicsRetirementActivity {
    std::uint32_t work_units = 0;
    std::uint32_t driver_calls = 0;
    std::uint32_t deleted_objects = 0;
    std::uint64_t freed_cpu_bytes = 0;
    double milliseconds = 0;
    GraphicsRetirementStep last_step;
    GraphicsRetirementStep longest_step;
    bool budget_exhausted = false;
};
struct GraphicsRetirementReport {
    GraphicsRetirementStatus status;
    GraphicsRetirementActivity activity;
};
} // namespace elf3d
#endif
