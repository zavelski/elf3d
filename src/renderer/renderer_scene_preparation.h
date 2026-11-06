#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics_preparation.h>
#include <optional>

namespace elf3d::renderer {
enum class SceneResourcePhase : std::uint8_t {
    clearing_metadata,
    scanning,
    mesh,
    textures,
    draw_metadata,
    environment,
    pipelines,
    auxiliary,
    ready
};
struct SceneResourcePreparation {
    SceneGraphicsPreparationStatus status;
    SceneResourcePhase phase = SceneResourcePhase::clearing_metadata;
    std::size_t entity_index = 0;
    std::uint32_t primitive_index = 0;
    std::uint32_t texture_slot = 0;
    std::optional<Error> failure;
};
struct SceneResourceWork {
    GraphicsPreparationPhase phase = GraphicsPreparationPhase::scanning;
    std::size_t packed_bytes = 0;
    std::size_t uploaded_bytes = 0;
    std::size_t allocated_bytes = 0;
    std::uint32_t driver_calls = 0;
    std::uint32_t completed_meshes = 0;
    std::uint32_t completed_textures = 0;
    bool progressed = false;
    bool shared_resource = false;
    std::optional<EnvironmentTextureKind> environment_texture;
    std::optional<ShaderPipelineKind> shader_pipeline;
    std::optional<ShaderPreparationPhase> shader_step;
    std::optional<std::uint32_t> mip_level;
    std::optional<std::uint32_t> cube_face;
};
} // namespace elf3d::renderer
