#include "renderer_cache.hpp"
#include "renderer_detail.h"
#include "renderer_environment_preparation.h"
#include "renderer_pipeline_preparation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace elf3d::renderer {
namespace {
[[nodiscard]] bool active_slot(const scene::RuntimePrimitiveView& primitive,
                               std::size_t index) noexcept
{
    const auto slot = static_cast<scene::RuntimeMaterialTextureSlot>(index);
    return active_material_texture(index, primitive.material_view, true) &&
           primitive.material_view.has_texture(slot) &&
           (index != 2 || primitive.has_tangent_basis());
}
[[nodiscard]] bool valid_budget(const GraphicsPreparationBudget& budget) noexcept
{
    return std::isfinite(budget.milliseconds) && budget.milliseconds >= 0;
}
[[nodiscard]] Result<void> refresh_source_revision(const scene::Storage& scene,
                                                   SceneResourcePreparation& state)
{
    if (state.status.source_revision == scene.render_content_revision())
        return {};
    if (state.phase != SceneResourcePhase::ready)
        return Error{ErrorCode::invalid_argument, "Scene changed during graphics preparation"};
    // Completed immutable GPU resources can be reused. Old draw metadata is
    // removed one record at a time before walking the new runtime content.
    state = {};
    state.status.state = GraphicsPreparationState::uploading;
    state.status.source_revision = scene.render_content_revision();
    state.status.entity_slots = scene.entities().size();
    return {};
}
[[nodiscard]] Result<SceneResourceWork> advance_scan(const scene::Storage& scene,
                                                     SceneResourcePreparation& state)
{
    SceneResourceWork step;
    step.progressed = true;
    const auto entities = scene.entities();
    if (state.entity_index == entities.size()) {
        state.phase = state.status.completed_primitives == 0 ? SceneResourcePhase::ready
                                                             : SceneResourcePhase::environment;
        if (state.phase == SceneResourcePhase::ready)
            state.status.state = GraphicsPreparationState::ready;
        step.phase = GraphicsPreparationPhase::scanning;
        return step;
    }
    const auto& entity = entities[state.entity_index];
    if (!entity || !entity->model) {
        ++state.entity_index;
    } else if (state.primitive_index >= entity->model->primitives.size()) {
        ++state.entity_index;
        state.primitive_index = 0;
    } else {
        if (entity->model->primitives.size() > std::numeric_limits<std::uint32_t>::max())
            return Error{ErrorCode::resource_limit_exceeded,
                         "Scene model has too many runtime primitives"};
        state.phase = SceneResourcePhase::mesh;
    }
    state.status.completed_entity_slots = state.entity_index;
    return step;
}
[[nodiscard]] SceneResourceWork mesh_work(const MeshWorkStep& native) noexcept
{
    SceneResourceWork work;
    work.phase = GraphicsPreparationPhase::mesh_setup;
    if (native.phase == MeshWorkPhase::packing)
        work.phase = GraphicsPreparationPhase::vertex_packing;
    else if (native.native_phase == graphics::MeshPreparationPhase::vertices)
        work.phase = GraphicsPreparationPhase::vertex_upload;
    else if (native.native_phase == graphics::MeshPreparationPhase::indices)
        work.phase = GraphicsPreparationPhase::index_upload;
    work.packed_bytes = native.packed_bytes;
    work.uploaded_bytes = native.uploaded_bytes;
    work.allocated_bytes = native.allocated_bytes;
    work.driver_calls = native.driver_calls;
    work.completed_meshes = native.completed_meshes;
    work.progressed = native.progressed || native.ready;
    return work;
}
[[nodiscard]] SceneResourceWork texture_work(const TextureWorkStep& native) noexcept
{
    SceneResourceWork work;
    work.phase = GraphicsPreparationPhase::texture_setup;
    if (native.phase == graphics::TexturePreparationPhase::pixels)
        work.phase = GraphicsPreparationPhase::texture_upload;
    else if (native.phase == graphics::TexturePreparationPhase::mipmaps)
        work.phase = GraphicsPreparationPhase::texture_mipmaps;
    work.uploaded_bytes = native.uploaded_bytes;
    work.allocated_bytes = native.allocated_bytes;
    work.driver_calls = native.driver_calls;
    work.completed_textures = native.completed_textures;
    work.progressed = native.progressed || native.ready;
    return work;
}
[[nodiscard]] GraphicsPreparationActivity::Step
source_step(const scene::Storage& scene, const SceneResourcePreparation& state) noexcept
{
    GraphicsPreparationActivity::Step step;
    const auto entities = scene.entities();
    if (state.entity_index < entities.size() && entities[state.entity_index])
        step.entity = entities[state.entity_index]->id;
    step.primitive_index = state.primitive_index;
    if (state.phase == SceneResourcePhase::textures &&
        state.texture_slot < graphics::material_texture_count)
        step.texture_slot =
            static_cast<MaterialTextureSlot>(state.texture_slot); // bounded slot cursor
    return step;
}
[[nodiscard]] GraphicsPreparationPhase admission_phase(SceneResourcePhase phase) noexcept
{
    if (phase == SceneResourcePhase::mesh)
        return GraphicsPreparationPhase::mesh_setup;
    if (phase == SceneResourcePhase::textures)
        return GraphicsPreparationPhase::texture_setup;
    if (phase == SceneResourcePhase::environment)
        return GraphicsPreparationPhase::environment_setup;
    if (phase == SceneResourcePhase::pipelines || phase == SceneResourcePhase::auxiliary)
        return GraphicsPreparationPhase::shader_setup;
    if (phase == SceneResourcePhase::clearing_metadata ||
        phase == SceneResourcePhase::draw_metadata)
        return GraphicsPreparationPhase::draw_metadata;
    return GraphicsPreparationPhase::scanning;
}
[[nodiscard]] GraphicsPreparationFailure
annotate_failure(GraphicsPreparationFailure attempt,
                 const GraphicsPreparationActivity::Step& source, double milliseconds) noexcept
{
    attempt.milliseconds = milliseconds;
    if (!attempt.environment_texture && !attempt.shader_pipeline) {
        attempt.entity = source.entity;
        attempt.primitive_index = source.primitive_index;
        attempt.texture_slot = source.texture_slot;
    }
    return attempt;
}
[[nodiscard]] GraphicsPreparationFailure work_failure(const SceneResourceWork& work,
                                                      std::uint32_t call_limit) noexcept
{
    GraphicsPreparationFailure failure;
    failure.phase = work.phase;
    failure.environment_texture = work.environment_texture;
    failure.shader_pipeline = work.shader_pipeline;
    failure.shader_step = work.shader_step;
    failure.mip_level = work.mip_level;
    failure.cube_face = work.cube_face;
    failure.driver_call_limit = call_limit;
    return failure;
}
void record_work(SceneResourcePreparation& state, GraphicsPreparationActivity& activity,
                 const SceneResourceWork& work, GraphicsPreparationActivity::Step step) noexcept
{
    ++activity.work_units;
    activity.driver_calls += work.driver_calls;
    activity.packed_bytes += work.packed_bytes;
    activity.uploaded_bytes += work.uploaded_bytes;
    activity.allocated_bytes += work.allocated_bytes;
    step.phase = work.phase;
    step.driver_calls = work.driver_calls;
    step.uploaded_bytes = work.uploaded_bytes;
    step.allocated_bytes = work.allocated_bytes;
    step.environment_texture = work.environment_texture;
    step.shader_pipeline = work.shader_pipeline;
    step.shader_step = work.shader_step;
    step.mip_level = work.mip_level;
    step.cube_face = work.cube_face;
    activity.last_step = step;
    if (step.milliseconds > activity.longest_step.milliseconds)
        activity.longest_step = step;
    if (!work.shared_resource) {
        state.status.uploaded_bytes += work.uploaded_bytes;
        state.status.allocated_bytes += work.allocated_bytes;
    }
    state.status.completed_meshes += work.completed_meshes;
    state.status.completed_textures += work.completed_textures;
}
} // namespace
Result<SceneResourceWork>
Renderer::advance_scene_texture(const scene::Storage& scene, SceneResourcePreparation& state,
                                const GraphicsPreparationBudget& remaining)
{
    SceneResourceWork step;
    step.phase = GraphicsPreparationPhase::texture_setup;
    step.progressed = true;
    if (state.texture_slot == graphics::material_texture_count) {
        state.phase = SceneResourcePhase::draw_metadata;
        return step;
    }
    const auto& entity = *scene.entities()[state.entity_index];
    const auto primitive = scene.runtime_primitive(entity.id, state.primitive_index);
    if (!primitive)
        return primitive.error();
    while (state.texture_slot < graphics::material_texture_count) {
        if (active_slot(primitive.value(), state.texture_slot))
            break;
        ++state.texture_slot;
    }
    if (state.texture_slot == graphics::material_texture_count) {
        state.phase = SceneResourcePhase::draw_metadata;
        return step;
    }
    const auto index = state.texture_slot;
    const auto slot = static_cast<scene::RuntimeMaterialTextureSlot>(index);
    const auto texture = scene.runtime_texture(primitive.value(), slot);
    if (!texture)
        return texture.error();
    const auto prepared = prepare_texture_step(
        scene.id(), texture.value(), texture_color_space(index),
        {remaining.upload_bytes, remaining.driver_calls, remaining.transfer_bytes});
    if (!prepared)
        return prepared.error();
    if (prepared.value().ready)
        ++state.texture_slot;
    return texture_work(prepared.value());
}
SceneResourceWork Renderer::clear_scene_draw_metadata(const scene::Storage& scene,
                                                      SceneResourcePreparation& state)
{
    SceneResourceWork step;
    step.progressed = true;
    step.phase = GraphicsPreparationPhase::draw_metadata;
    auto& cached = cache_->scene(scene.id());
    if (cached.draw_packet_revision == scene.render_content_revision() ||
        cached.draw_packets.empty()) {
        cached.draw_packet_revision = scene.render_content_revision();
        state.phase = SceneResourcePhase::scanning;
    } else {
        auto entity = cached.draw_packets.begin();
        if (entity->second.primitives.empty())
            cached.draw_packets.erase(entity);
        else
            entity->second.primitives.erase(entity->second.primitives.begin());
    }
    return step;
}
Result<SceneResourceWork>
Renderer::advance_scene_resource_unit(const scene::Storage& scene, SceneResourcePreparation& state,
                                      const GraphicsPreparationBudget& remaining)
{
    if (state.phase == SceneResourcePhase::clearing_metadata)
        return clear_scene_draw_metadata(scene, state);
    if (state.phase == SceneResourcePhase::scanning)
        return advance_scan(scene, state);
    if (state.phase == SceneResourcePhase::textures)
        return advance_scene_texture(scene, state, remaining);
    if (state.phase >= SceneResourcePhase::environment &&
        state.phase <= SceneResourcePhase::auxiliary)
        return advance_scene_environment(state, remaining);
    const auto& entity = *scene.entities()[state.entity_index];
    const auto primitive = scene.runtime_primitive(entity.id, state.primitive_index);
    if (!primitive)
        return primitive.error();
    if (state.phase == SceneResourcePhase::mesh) {
        const auto prepared =
            prepare_mesh_step(scene.id(), primitive.value(),
                              {remaining.upload_bytes, remaining.driver_calls,
                               remaining.packing_bytes, remaining.transfer_bytes});
        if (!prepared)
            return prepared.error();
        if (prepared.value().ready) {
            state.texture_slot = 0;
            state.phase = SceneResourcePhase::textures;
        }
        return mesh_work(prepared.value());
    }
    return finish_scene_primitive(scene, state);
}
Result<SceneResourceWork> Renderer::finish_scene_primitive(const scene::Storage& scene,
                                                           SceneResourcePreparation& state)
{
    const auto& entity = *scene.entities()[state.entity_index];
    RenderItem item;
    item.entity = entity.id;
    item.primitive_index = state.primitive_index;
    RenderStatistics statistics;
    const auto packet_index = cached_draw_packet_index(scene, item, statistics);
    if (!packet_index)
        return packet_index.error();
    auto& packet = mutable_draw_packet(scene, item, packet_index.value());
    const auto primitive = scene.runtime_primitive(entity.id, state.primitive_index);
    if (!primitive)
        return primitive.error();
    std::uint64_t uploads = 0;
    const auto textures = prepare_draw_textures(scene, primitive.value(), packet, uploads, true);
    if (!textures)
        return textures.error();
    // Every required mesh/texture was admitted through bounded steps above.
    ELF3D_ASSERT(statistics.gpu_buffer_uploads == 0 && statistics.gpu_texture_uploads == 0 &&
                 uploads == 0);
    packet.lit_textures_prepared = true;
    ++state.primitive_index;
    ++state.status.completed_primitives;
    state.phase = SceneResourcePhase::scanning;
    SceneResourceWork step;
    step.phase = GraphicsPreparationPhase::draw_metadata;
    step.progressed = true;
    return step;
}
SceneGraphicsPreparationStatus
Renderer::scene_resource_status(const scene::Storage& scene) const noexcept
{
    SceneGraphicsPreparationStatus status;
    status.source_revision = scene.render_content_revision();
    status.entity_slots = scene.entities().size();
    const auto found = cache_->scenes.find(scene.id());
    if (found != cache_->scenes.end() && found->first == scene.id() && found->second->preparation) {
        status = found->second->preparation->status;
        if (status.source_revision != scene.render_content_revision())
            status.state = GraphicsPreparationState::failed;
    }
    status.shared_environment_bytes = environment_ ? environment_->resident_bytes
                                      : environment_preparation_
                                          ? environment_preparation_->allocated_bytes()
                                          : 0;
    status.environment_ready = environment_ != nullptr;
    status.pipelines_ready = pipelines_ && pipelines_->complete();
    status.auxiliary_graphics_ready = device_->auxiliary_graphics_status().complete();
    return status;
}
Result<void> Renderer::validate_scene_resources(const scene::Storage& scene) const
{
    const auto status = scene_resource_status(scene);
    if (status.state == GraphicsPreparationState::cpu_ready)
        return {}; // Existing synchronous consumers retain explicit lazy creation.
    if (status.state != GraphicsPreparationState::ready ||
        status.source_revision != scene.render_content_revision())
        return Error{ErrorCode::invalid_argument,
                     "Scene graphics preparation is incomplete or stale"};
    return {};
}
Result<void> Renderer::advance_scene_resource_batch(const scene::Storage& scene,
                                                    SceneResourcePreparation& state,
                                                    const GraphicsPreparationBudget& budget,
                                                    SceneGraphicsPreparationReport& report)
{
    const auto begin = device_->monotonic_time_milliseconds();
    while (state.phase != SceneResourcePhase::ready &&
           report.activity.work_units < budget.work_units) {
        auto remaining = budget;
        remaining.driver_calls -= report.activity.driver_calls;
        remaining.upload_bytes -= static_cast<std::size_t>(report.activity.uploaded_bytes);
        auto identity = source_step(scene, state);
        cache_->preparation_attempt = {};
        cache_->preparation_attempt.phase = admission_phase(state.phase);
        const auto unit_begin = device_->monotonic_time_milliseconds();
        const auto work = advance_scene_resource_unit(scene, state, remaining);
        const auto unit_end = device_->monotonic_time_milliseconds();
        if (!work) {
            state.status.failure =
                annotate_failure(cache_->preparation_attempt, identity, unit_end - unit_begin);
            state.failure = work.error();
            state.status.state = GraphicsPreparationState::failed;
            return work.error();
        }
        report.activity.milliseconds = unit_end - begin;
        identity.phase = work.value().phase;
        identity.milliseconds = unit_end - unit_begin;
        report.activity.last_step = identity;
        if (!work.value().progressed) {
            report.activity.budget_exhausted = true;
            break;
        }
        if (work.value().driver_calls > remaining.driver_calls ||
            work.value().uploaded_bytes > remaining.upload_bytes) {
            state.status.failure =
                annotate_failure(work_failure(work.value(), remaining.driver_calls), identity,
                                 unit_end - unit_begin);
            return Error{ErrorCode::invalid_argument,
                         "Resource preparation exceeded its remaining frame allowance"};
        }
        record_work(state, report.activity, work.value(), identity);
        if (report.activity.milliseconds >= budget.milliseconds) {
            report.activity.budget_exhausted = true;
            break;
        }
    }
    report.activity.budget_exhausted =
        report.activity.budget_exhausted || report.activity.work_units == budget.work_units;
    return {};
}
Result<void> Renderer::admit_scene_resources(const scene::Storage& scene)
{
    if (!scene.belongs_to_engine(engine_token_))
        return Error{ErrorCode::foreign_engine_object,
                     "Graphics admission requires this Renderer's Scene"};
    auto& cached = cache_->scene(scene.id());
    if (!cached.preparation) {
        cached.preparation.emplace();
        cached.preparation->status.source_revision = scene.render_content_revision();
        cached.preparation->status.entity_slots = scene.entities().size();
        cached.preparation->status.state = GraphicsPreparationState::uploading;
    }
    auto& state = *cached.preparation;
    if (const auto refreshed = refresh_source_revision(scene, state); !refreshed)
        return refreshed.error();
    if (state.failure)
        return *state.failure;
    return {};
}
Result<SceneGraphicsPreparationReport>
Renderer::advance_scene_resources(const scene::Storage& scene,
                                  const GraphicsPreparationBudget& budget)
{
    if (!scene.belongs_to_engine(engine_token_) || !valid_budget(budget))
        return Error{
            ErrorCode::invalid_argument,
            "Graphics preparation requires an owned Scene and a finite nonnegative time budget"};
    SceneGraphicsPreparationReport report;
    report.status = scene_resource_status(scene);
    if (budget.milliseconds == 0 || budget.work_units == 0)
        return report;
    if (const auto admitted = admit_scene_resources(scene); !admitted)
        return admitted.error();
    auto& state = *cache_->scene(scene.id()).preparation;
    if (const auto advanced = advance_scene_resource_batch(scene, state, budget, report);
        !advanced) {
        state.failure = advanced.error();
        state.status.state = GraphicsPreparationState::failed;
        return advanced.error();
    }
    report.status = scene_resource_status(scene);
    return report;
}
} // namespace elf3d::renderer
