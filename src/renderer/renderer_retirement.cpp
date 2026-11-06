#include "renderer_cache.hpp"
#include "renderer_environment_preparation.h"
#include "renderer_pipeline_preparation.h"

namespace elf3d::renderer {
namespace {
[[nodiscard]] Result<graphics::ResourceRetirementStep>
retire_mesh_preparation(graphics::Device& device, MeshGeometryPreparation& preparation,
                        const graphics::ResourceRetirementBudget& budget)
{
    ELF3D_ASSERT(!preparation.pending || !preparation.mesh);
    if (preparation.pending)
        return preparation.pending->advance_retirement(budget);
    if (preparation.mesh)
        return device.retire_static_mesh(*preparation.mesh, budget);
    return graphics::ResourceRetirementStep{0, 0, true, false};
}
[[nodiscard]] Result<graphics::ResourceRetirementStep>
retire_texture_preparation(graphics::Device& device, TextureImagePreparation& preparation,
                           const graphics::ResourceRetirementBudget& budget)
{
    ELF3D_ASSERT(!preparation.pending || !preparation.texture);
    if (preparation.pending)
        return preparation.pending->advance_retirement(budget);
    if (preparation.texture)
        return device.retire_texture_2d(*preparation.texture, budget);
    return graphics::ResourceRetirementStep{0, 0, true, false};
}
[[nodiscard]] GraphicsRetirementStep resource_step(const graphics::ResourceRetirementStep& native,
                                                   GraphicsRetirementPhase phase) noexcept
{
    GraphicsRetirementStep step;
    step.phase = phase;
    step.driver_calls = native.driver_calls;
    step.deleted_objects = native.deleted_objects;
    step.progressed = native.progressed || native.complete;
    return step;
}
void subtract_retired_delta(GraphicsRetirementStatus& total, const GraphicsRetirementStatus& before,
                            const GraphicsRetirementStatus& after) noexcept
{
    total.mesh_entries -= before.mesh_entries - after.mesh_entries;
    total.texture_entries -= before.texture_entries - after.texture_entries;
    total.estimated_geometry_bytes -=
        before.estimated_geometry_bytes - after.estimated_geometry_bytes;
    total.estimated_texture_bytes -= before.estimated_texture_bytes - after.estimated_texture_bytes;
    total.staging_bytes -= before.staging_bytes - after.staging_bytes;
}
[[nodiscard]] Result<GraphicsRetirementStep>
retire_shared_preparations(graphics::Device& device, PipelineResources* pipelines,
                           EnvironmentPreparation* environment,
                           const graphics::ResourceRetirementBudget& budget)
{
    if (pipelines && pipelines->needs_retirement())
        return pipelines->retire_unit(budget);
    if (environment && environment->needs_retirement())
        return environment->retire_unit(budget);
    if (device.auxiliary_graphics_status().retirement_pending) {
        const auto retired = device.retire_auxiliary_graphics(budget);
        if (!retired)
            return retired.error();
        return resource_step(retired.value(), GraphicsRetirementPhase::shader_object);
    }
    return GraphicsRetirementStep{};
}
[[nodiscard]] GraphicsRetirementPhase
shared_retirement_phase(const PipelineResources* pipelines,
                        const EnvironmentPreparation* environment,
                        const graphics::Device& device) noexcept
{
    if (pipelines && pipelines->needs_retirement())
        return GraphicsRetirementPhase::shader_object;
    if (environment && environment->needs_retirement())
        return GraphicsRetirementPhase::environment_object;
    return device.auxiliary_graphics_status().retirement_pending
               ? GraphicsRetirementPhase::shader_object
               : GraphicsRetirementPhase::cache_metadata;
}
} // namespace
GraphicsRetirementPhase Renderer::CacheState::MeshEntry::retirement_phase() const noexcept
{
    return preparation && preparation->staging.capacity() != 0
               ? GraphicsRetirementPhase::mesh_staging
               : GraphicsRetirementPhase::mesh_object;
}
GraphicsRetirementPhase Renderer::CacheState::SceneEntry::retirement_phase() const noexcept
{
    if (!meshes.empty())
        return meshes.begin()->second.retirement_phase();
    if (!document_meshes.empty())
        return document_meshes.begin()->second.retirement_phase();
    if (!images.empty() || !document_images.empty())
        return GraphicsRetirementPhase::texture_object;
    return GraphicsRetirementPhase::draw_metadata;
}
Result<GraphicsRetirementStep>
Renderer::CacheState::MeshEntry::retire_unit(graphics::Device& device,
                                             const graphics::ResourceRetirementBudget& budget)
{
    if (preparation && preparation->staging.capacity() != 0) {
        GraphicsRetirementStep step;
        step.phase = GraphicsRetirementPhase::mesh_staging;
        step.freed_cpu_bytes = preparation->staging.capacity() * sizeof(float);
        std::vector<float>{}.swap(preparation->staging);
        step.progressed = true;
        return step;
    }
    auto retired = mesh          ? device.retire_static_mesh(*mesh, budget)
                   : preparation ? retire_mesh_preparation(device, *preparation, budget)
                                 : Result<graphics::ResourceRetirementStep>{
                                       graphics::ResourceRetirementStep{0, 0, true, false}};
    if (!retired)
        return retired.error();
    if (retired.value().complete) {
        mesh.reset();
        preparation.reset(); // native objects and staging were released explicitly
    }
    return resource_step(retired.value(), GraphicsRetirementPhase::mesh_object);
}
Result<GraphicsRetirementStep>
Renderer::CacheState::TextureVariant::retire_unit(graphics::Device& device,
                                                  const graphics::ResourceRetirementBudget& budget)
{
    auto retired = texture       ? device.retire_texture_2d(*texture, budget)
                   : preparation ? retire_texture_preparation(device, *preparation, budget)
                                 : Result<graphics::ResourceRetirementStep>{
                                       graphics::ResourceRetirementStep{0, 0, true, false}};
    if (!retired)
        return retired.error();
    if (retired.value().complete) {
        texture.reset();
        preparation.reset();
    }
    return resource_step(retired.value(), GraphicsRetirementPhase::texture_object);
}
GraphicsRetirementStatus Renderer::CacheState::SceneEntry::retirement_status() const noexcept
{
    GraphicsRetirementStatus status;
    status.renderer_scenes = 1;
    status.mesh_entries = meshes.size() + document_meshes.size();
    status.texture_entries = texture_entries;
    status.estimated_geometry_bytes = estimated_geometry_bytes;
    status.estimated_texture_bytes = estimated_texture_bytes;
    status.staging_bytes = staging_bytes;
    return status;
}
Result<GraphicsRetirementStep>
Renderer::CacheState::SceneEntry::retire_mesh_map(MeshEntries& entries, graphics::Device& device,
                                                  const graphics::ResourceRetirementBudget& budget)
{
    auto first = entries.begin();
    const auto step = first->second.retire_unit(device, budget);
    if (!step)
        return step.error();
    staging_bytes -= step.value().freed_cpu_bytes;
    if (!first->second.mesh && !first->second.preparation) {
        estimated_geometry_bytes -= first->second.resident_bytes;
        entries.erase(first);
    }
    return step.value();
}
Result<GraphicsRetirementStep>
Renderer::CacheState::SceneEntry::retire_image_map(ImageEntries& entries, graphics::Device& device,
                                                   const graphics::ResourceRetirementBudget& budget)
{
    auto first = entries.begin();
    GraphicsRetirementStep step;
    step.progressed = true;
    auto& variants = first->second.variants;
    if (variants.empty()) {
        entries.erase(first);
        return step;
    }
    auto result = variants.back().retire_unit(device, budget);
    if (!result)
        return result.error();
    if (!variants.back().texture && !variants.back().preparation) {
        estimated_texture_bytes -= variants.back().accounted_bytes;
        --texture_entries;
        variants.pop_back();
    }
    return result;
}
Result<GraphicsRetirementStep>
Renderer::CacheState::SceneEntry::retire_unit(graphics::Device& device,
                                              const graphics::ResourceRetirementBudget& budget)
{
    if (!meshes.empty())
        return retire_mesh_map(meshes, device, budget);
    if (!document_meshes.empty())
        return retire_mesh_map(document_meshes, device, budget);
    if (!images.empty())
        return retire_image_map(images, device, budget);
    if (!document_images.empty())
        return retire_image_map(document_images, device, budget);
    GraphicsRetirementStep step;
    step.progressed = true;
    step.phase = GraphicsRetirementPhase::draw_metadata;
    if (!draw_packets.empty()) {
        auto first = draw_packets.begin();
        if (first->second.primitives.empty())
            draw_packets.erase(first);
        else
            first->second.primitives.erase(first->second.primitives.begin());
    }
    return step;
}
void Renderer::release_scene(SceneId id) noexcept
{
    const auto found = cache_->scenes.find(id);
    if (found == cache_->scenes.end() || found->first != id)
        return;
    const auto& scene = *found->second;
    cache_->resident_geometry_bytes -= scene.resident_geometry_bytes;
    cache_->resident_texture_bytes -= scene.resident_texture_bytes;
    cache_->texture_count -= scene.texture_count;
    if (scene.meshes.empty() && scene.document_meshes.empty() && scene.images.empty() &&
        scene.document_images.empty() && scene.draw_packets.empty()) {
        cache_->scenes.erase(found); // fixed-size empty cache owner only
        return;
    }
    const auto status = scene.retirement_status();
    ++cache_->retired_status.renderer_scenes;
    cache_->retired_status.mesh_entries += status.mesh_entries;
    cache_->retired_status.texture_entries += status.texture_entries;
    cache_->retired_status.estimated_geometry_bytes += status.estimated_geometry_bytes;
    cache_->retired_status.estimated_texture_bytes += status.estimated_texture_bytes;
    cache_->retired_status.staging_bytes += status.staging_bytes;
    auto node = cache_->scenes.extract(found);
    const auto inserted = cache_->retired_scenes.insert(std::move(node));
    ELF3D_ASSERT(inserted.inserted); // Scene IDs are not reused before retirement.
}
GraphicsRetirementStatus Renderer::retirement_status() const noexcept
{
    auto status = cache_->retired_status;
    if (environment_preparation_ && environment_preparation_->needs_retirement()) {
        status.shared_preparations = 1;
        status.shared_texture_bytes = environment_preparation_->allocated_bytes();
    }
    if (pipelines_ && pipelines_->needs_retirement())
        ++status.shared_preparations;
    if (device_->auxiliary_graphics_status().retirement_pending)
        ++status.shared_preparations;
    return status;
}
Result<GraphicsRetirementStep>
Renderer::CacheState::retire_scene_unit(graphics::Device& device,
                                        const graphics::ResourceRetirementBudget& budget)
{
    auto first = retired_scenes.begin();
    auto& scene = *first->second;
    retirement_attempt.phase = scene.retirement_phase();
    retirement_attempt.scene = first->first;
    const auto before = scene.retirement_status();
    auto step = scene.retire_unit(device, budget);
    if (!step)
        return step.error();
    step.value().scene = first->first;
    subtract_retired_delta(retired_status, before, scene.retirement_status());
    if (scene.meshes.empty() && scene.document_meshes.empty() && scene.images.empty() &&
        scene.document_images.empty() && scene.draw_packets.empty()) {
        retired_scenes.erase(first);
        --retired_status.renderer_scenes;
    }
    return step;
}
Result<GraphicsRetirementStep>
Renderer::advance_retirement_unit(const graphics::ResourceRetirementBudget& budget)
{
    cache_->retirement_attempt = {};
    cache_->retirement_attempt.driver_call_limit = budget.driver_calls;
    const auto begin = device_->monotonic_time_milliseconds();
    auto step = [&]() -> Result<GraphicsRetirementStep> {
        if (!cache_->retired_scenes.empty())
            return cache_->retire_scene_unit(*device_, budget);
        cache_->retirement_attempt.phase =
            shared_retirement_phase(pipelines_.get(), environment_preparation_.get(), *device_);
        return retire_shared_preparations(*device_, pipelines_.get(),
                                          environment_preparation_.get(), budget);
    }();
    const auto end = device_->monotonic_time_milliseconds();
    if (step && (step.value().driver_calls > budget.driver_calls ||
                 step.value().deleted_objects > budget.objects))
        step = Error{ErrorCode::invalid_argument,
                     "Retirement exceeded its admitted object/call allowance"};
    if (!step) {
        cache_->retirement_attempt.milliseconds = end - begin;
        cache_->retired_status.last_failure = cache_->retirement_attempt;
    }
    return step;
}
Result<void> Renderer::complete_retirement()
{
    while (!retirement_status().empty()) {
        const auto step = advance_retirement_unit({});
        if (!step)
            return step.error();
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous graphics retirement did not advance"};
    }
    return {};
}
} // namespace elf3d::renderer
