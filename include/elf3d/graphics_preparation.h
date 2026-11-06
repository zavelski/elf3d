#ifndef ELF3D_GRAPHICS_PREPARATION_H
#define ELF3D_GRAPHICS_PREPARATION_H

#include <cstddef>
#include <cstdint>
#include <elf3d/assets.h>
#include <elf3d/graphics_retirement.h>
#include <optional>

namespace elf3d {
// Explicit context-thread service allowance. The time target is checked between
// bounded work units, not inside one driver allocation/upload/mipmap call.
inline constexpr std::size_t default_vertex_packing_bytes = 8U * 1024U * 1024U;
inline constexpr std::size_t default_graphics_transfer_bytes = 32U * 1024U * 1024U;
struct GraphicsPreparationBudget {
    double milliseconds = 20.0;
    std::size_t upload_bytes = 256U * 1024U * 1024U;
    std::uint32_t driver_calls = 65536;
    std::uint32_t work_units = 65536;
    std::uint32_t retired_objects = 4096;
    std::size_t packing_bytes = default_vertex_packing_bytes;
    std::size_t transfer_bytes = default_graphics_transfer_bytes;
};
enum class GraphicsPreparationState : std::uint8_t { cpu_ready, uploading, ready, failed };
enum class GraphicsPreparationPhase : std::uint8_t {
    scanning,
    mesh_setup,
    vertex_packing,
    vertex_upload,
    index_upload,
    texture_setup,
    texture_upload,
    texture_mipmaps,
    draw_metadata,
    environment_checksum,
    environment_setup,
    environment_allocation,
    environment_upload,
    environment_ready,
    shader_setup,
    shader_vertex_compile,
    shader_fragment_compile,
    shader_link,
    shader_bindings,
    ready
};
enum class EnvironmentTextureKind : std::uint8_t { diffuse, specular, brdf_lut };
enum class ShaderPipelineKind : std::uint8_t { core, extended, picking, overlay, display_resolve };
enum class ShaderPreparationPhase : std::uint8_t {
    vertex_object,
    vertex_source,
    vertex_compile,
    vertex_status,
    fragment_object,
    fragment_source,
    fragment_compile,
    fragment_status,
    program_object,
    attach_vertex,
    attach_fragment,
    link,
    link_status,
    release_vertex,
    release_fragment,
    uniforms,
    samplers,
    validation,
    ready,
    failed,
    consumed,
    factory,
    vertex_array,
    vertex_buffer,
    vertex_attributes,
    interface_validation,
    object_retirement
};

// Diagnostic context for a failed unit. The original Error still comes from
// advance_scene_graphics. The admitted call limit is not an exact failure count.
struct GraphicsPreparationFailure {
    GraphicsPreparationPhase phase = GraphicsPreparationPhase::scanning;
    EntityId entity;
    std::uint32_t primitive_index = 0;
    std::optional<MaterialTextureSlot> texture_slot;
    std::optional<EnvironmentTextureKind> environment_texture;
    std::optional<ShaderPipelineKind> shader_pipeline;
    std::optional<ShaderPreparationPhase> shader_step;
    std::optional<std::uint32_t> mip_level;
    std::optional<std::uint32_t> cube_face;
    double milliseconds = 0;
    std::uint32_t driver_call_limit = 0;
};
struct SceneGraphicsPreparationStatus {
    GraphicsPreparationState state = GraphicsPreparationState::cpu_ready;
    std::uint64_t source_revision = 0;
    std::uint64_t completed_entity_slots = 0;
    std::uint64_t entity_slots = 0;
    std::uint64_t completed_primitives = 0;
    std::uint64_t completed_meshes = 0;
    std::uint64_t completed_textures = 0;
    std::uint64_t uploaded_bytes = 0;
    // Logical base storage allocated during this preparation, not physical VRAM.
    std::uint64_t allocated_bytes = 0;
    // Shared built-in environment storage, independently cached by the Engine.
    std::uint64_t shared_environment_bytes = 0;
    bool environment_ready = false;
    bool pipelines_ready = false;
    bool auxiliary_graphics_ready = false;
    std::optional<GraphicsPreparationFailure> failure;
};
struct GraphicsPreparationActivity {
    std::uint32_t work_units = 0;
    std::uint32_t driver_calls = 0;
    std::uint64_t packed_bytes = 0;
    std::uint64_t uploaded_bytes = 0;
    std::uint64_t allocated_bytes = 0;
    double milliseconds = 0;
    struct Step {
        GraphicsPreparationPhase phase = GraphicsPreparationPhase::scanning;
        EntityId entity;
        std::uint32_t primitive_index = 0;
        std::optional<MaterialTextureSlot> texture_slot;
        std::optional<EnvironmentTextureKind> environment_texture;
        std::optional<ShaderPipelineKind> shader_pipeline;
        std::optional<ShaderPreparationPhase> shader_step;
        std::optional<std::uint32_t> mip_level;
        std::optional<std::uint32_t> cube_face;
        double milliseconds = 0;
        std::uint32_t driver_calls = 0;
        std::uint64_t uploaded_bytes = 0;
        std::uint64_t allocated_bytes = 0;
    };
    Step last_step;
    Step longest_step;
    bool budget_exhausted = false;
};
struct SceneGraphicsPreparationReport {
    GraphicsRetirementReport retirement;
    SceneGraphicsPreparationStatus status;
    GraphicsPreparationActivity activity;
};
} // namespace elf3d
#endif
