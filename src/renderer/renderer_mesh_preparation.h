#pragma once
#include <elf3d/graphics_preparation.h>

#include "renderer_vertex_packing.h"
#include <optional>
#include <vector>

namespace elf3d::renderer {
struct MeshWorkBudget {
    std::size_t upload_bytes = default_graphics_transfer_bytes;
    std::uint32_t driver_calls = GraphicsPreparationBudget{}.driver_calls;
    std::size_t packing_bytes = default_vertex_packing_bytes;
    std::size_t transfer_bytes = default_graphics_transfer_bytes;
};
enum class MeshWorkPhase : std::uint8_t { begin, staging, packing, gpu, adopt, ready };
struct MeshWorkStep {
    MeshWorkPhase phase = MeshWorkPhase::begin;
    graphics::MeshPreparationPhase native_phase = graphics::MeshPreparationPhase::objects;
    std::size_t packed_bytes = 0;
    std::size_t uploaded_bytes = 0;
    std::size_t allocated_bytes = 0;
    std::uint32_t driver_calls = 0;
    bool progressed = false;
    bool ready = false;
    std::uint32_t completed_meshes = 0;
    std::uint64_t completed_bytes = 0;
};
// Cache-local ownership only. All source borrows expire inside advance. The
// caller drives one CPU/native unit at a time; no thread or scheduler is created.
struct MeshGeometryPreparation {
    MeshHandle source_mesh;
    PrimitiveId source_primitive;
    std::uint64_t source_geometry = 0;
    RuntimeVertexDescription description;
    std::unique_ptr<graphics::StaticMeshPreparation> pending;
    std::unique_ptr<graphics::StaticMesh> mesh;
    std::vector<float> staging;
    std::size_t staging_offset = 0;
    std::size_t filled_floats = 0;
    std::size_t sent_floats = 0;
    std::uint64_t allocated_bytes = 0;
    std::optional<Error> failure;
    GraphicsPreparationFailure attempt;
    bool initialized = false;
};
[[nodiscard]] Result<MeshWorkStep>
advance_mesh_geometry(graphics::Device& device, const scene::RuntimePrimitiveView& primitive,
                      MeshGeometryPreparation& state, const MeshWorkBudget& budget = {});
} // namespace elf3d::renderer
