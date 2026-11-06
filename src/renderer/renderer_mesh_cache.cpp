#include "renderer_cache.hpp"
#include "renderer_mesh_preparation.h"

#include <elf3d/core/assert.h>
#include <limits>

namespace elf3d::renderer {
namespace {
[[nodiscard]] Result<std::size_t> geometry_index(const scene::RuntimePrimitiveView& primitive)
{
    const auto value = primitive.document_primitive.is_valid() ? primitive.geometry_identity
                                                               : primitive.mesh.debug_value();
    if (value > std::numeric_limits<std::size_t>::max() - 1U)
        return Error{ErrorCode::resource_limit_exceeded,
                     "Mesh cache identity exceeds addressable storage"};
    return static_cast<std::size_t>(value); // checked representability
}
void add_geometry_bytes(std::uint64_t& total, std::uint64_t bytes)
{
    if (bytes > std::numeric_limits<std::uint64_t>::max() - total)
        fatal_error("Renderer geometry residency counter exhausted");
    total += bytes;
}
} // namespace
Result<MeshWorkStep> Renderer::prepare_mesh_step(SceneId scene_id,
                                                 const scene::RuntimePrimitiveView& primitive,
                                                 const MeshWorkBudget& budget)
{
    const auto index = geometry_index(primitive);
    if (!index)
        return index.error();
    auto& scene = cache_->scene(scene_id);
    auto& entries = primitive.document_primitive.is_valid() ? scene.document_meshes : scene.meshes;
    const auto found = entries.find(index.value());
    if (found != entries.end() && found->second.mesh) {
        MeshWorkStep step{MeshWorkPhase::ready, graphics::MeshPreparationPhase::ready};
        step.ready = true;
        return step;
    }
    if (!retirement_status().empty())
        return MeshWorkStep{};
    auto& slot = entries[index.value()];
    if (!slot.preparation)
        slot.preparation = std::make_unique<MeshGeometryPreparation>();
    const auto staging_before = slot.preparation->staging.capacity() * sizeof(float);
    auto step = advance_mesh_geometry(*device_, primitive, *slot.preparation, budget);
    scene.staging_bytes -= staging_before;
    scene.staging_bytes += slot.preparation->staging.capacity() * sizeof(float);
    if (!step) {
        cache_->preparation_attempt = slot.preparation->attempt;
        return step.error();
    }
    slot.resident_bytes += step.value().allocated_bytes;
    add_geometry_bytes(scene.estimated_geometry_bytes, step.value().allocated_bytes);
    if (!step.value().ready)
        return step;
    const auto description = slot.preparation->description;
    const auto complete_bytes =
        static_cast<std::uint64_t>(description.vertex_bytes) + description.index_bytes;
    add_geometry_bytes(scene.estimated_geometry_bytes, complete_bytes - slot.resident_bytes);
    slot.resident_bytes = complete_bytes;
    slot.mesh = std::move(slot.preparation->mesh);
    slot.preparation.reset(); // GPU owner was adopted; staging already released.
    add_geometry_bytes(scene.resident_geometry_bytes, slot.resident_bytes);
    add_geometry_bytes(cache_->resident_geometry_bytes, slot.resident_bytes);
    step.value().completed_meshes = 1;
    step.value().completed_bytes = slot.resident_bytes;
    return step;
}
Result<std::size_t> Renderer::cached_mesh(SceneId scene_id,
                                          const scene::RuntimePrimitiveView& primitive,
                                          RenderStatistics& statistics)
{
    const auto index = geometry_index(primitive);
    if (!index)
        return index.error();
    for (;;) {
        const auto step = prepare_mesh_step(scene_id, primitive, {});
        if (!step)
            return step.error();
        statistics.gpu_buffer_uploads += step.value().completed_meshes;
        statistics.gpu_buffer_uploaded_bytes += step.value().uploaded_bytes;
        if (step.value().ready)
            return index.value();
        if (!retirement_status().empty()) {
            if (const auto retired = complete_retirement(); !retired)
                return retired.error();
            continue;
        }
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous mesh preparation could not advance"};
    }
}
graphics::StaticMesh& Renderer::mesh(SceneId scene_id, bool document_primitive,
                                     std::size_t index) const noexcept
{
    const auto& scene = cache_->scene(scene_id);
    const auto& entries = document_primitive ? scene.document_meshes : scene.meshes;
    const auto found = entries.find(index);
    ELF3D_ASSERT(found != entries.end() && found->second.mesh);
    return *found->second.mesh;
}
} // namespace elf3d::renderer
