#include "renderer_pipeline_preparation.h"
#include "renderer_detail.h"
#include <renderer/renderer.h>

#include <string>

namespace elf3d::renderer {
namespace {
[[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
begin_pipeline(graphics::Device& device, ShaderPipelineKind kind)
{
    std::string core_source;
    std::string_view fragment = main_fragment_shader_source();
    if (kind == ShaderPipelineKind::core) {
        core_source = fragment;
        core_source.insert(core_source.find('\n') + 1, "#define ELF3D_EXTENDED_MATERIALS 0\n");
        fragment = core_source;
    }
    return device.begin_graphics_pipeline(
        {main_vertex_shader_source(), fragment,
         graphics::VertexLayout::
             position_normal_float3_texcoord2_float2_color_float4_tangent_float4});
}
[[nodiscard]] GraphicsPreparationPhase phase_of(graphics::PipelinePreparationPhase phase) noexcept
{
    using Phase = graphics::PipelinePreparationPhase;
    if (phase == Phase::vertex_compile)
        return GraphicsPreparationPhase::shader_vertex_compile;
    if (phase == Phase::fragment_compile)
        return GraphicsPreparationPhase::shader_fragment_compile;
    if (phase == Phase::link)
        return GraphicsPreparationPhase::shader_link;
    if (phase == Phase::uniforms || phase == Phase::samplers)
        return GraphicsPreparationPhase::shader_bindings;
    return GraphicsPreparationPhase::shader_setup;
}
} // namespace
Result<SceneResourceWork> PipelineResources::advance(graphics::Device& device,
                                                     ShaderPipelineKind kind,
                                                     const GraphicsPreparationBudget& budget)
{
    SceneResourceWork work;
    work.shared_resource = true;
    work.shader_pipeline = kind;
    work.phase = GraphicsPreparationPhase::shader_setup;
    work.shader_step = ShaderPreparationPhase::factory;
    attempt = {};
    attempt.phase = work.phase;
    attempt.shader_pipeline = kind;
    attempt.shader_step = work.shader_step;
    if (ready[static_cast<std::size_t>(kind)]) {
        work.progressed = true;
        return work;
    }
    if (failed)
        return Error{ErrorCode::invalid_argument, "Failed shader preparation requires retirement"};
    if (!pending) {
        if (budget.driver_calls < 1)
            return work;
        attempt.driver_call_limit = 1;
        auto created = begin_pipeline(device, kind);
        if (!created)
            return created.error();
        pending = std::move(created).value();
        active = kind;
        work.driver_calls = static_cast<std::uint32_t>(pending->driver_calls());
        work.progressed = true;
        return work;
    }
    return advance_pending(budget);
}
Result<SceneResourceWork>
PipelineResources::advance_pending(const GraphicsPreparationBudget& budget)
{
    SceneResourceWork work;
    work.shared_resource = true;
    const auto next = pending->request();
    if (!next)
        return next.error();
    work.shader_pipeline = active;
    work.phase = phase_of(next.value().phase);
    work.shader_step = next.value().phase;
    if (next.value().driver_call_limit > budget.driver_calls)
        return work;
    attempt.phase = work.phase;
    attempt.shader_pipeline = work.shader_pipeline;
    attempt.shader_step = work.shader_step;
    attempt.driver_call_limit = next.value().driver_call_limit;
    auto step = pending->advance();
    if (!step) {
        failed = true;
        return step.error();
    }
    work.driver_calls = step.value().driver_calls;
    work.progressed = true;
    if (step.value().ready) {
        const auto accepted = accept_pending();
        if (!accepted)
            return accepted.error();
    }
    return work;
}
Result<void> PipelineResources::accept_pending()
{
    auto pipeline = pending->take_pipeline();
    if (!pipeline) {
        failed = true;
        return pipeline.error();
    }
    ready[static_cast<std::size_t>(active)] = std::move(pipeline).value();
    pending.reset();
    return {};
}
Result<SceneResourceWork>
PipelineResources::advance_auxiliary(graphics::Device& device,
                                     const GraphicsPreparationBudget& budget)
{
    const auto status = device.auxiliary_graphics_status();
    SceneResourceWork work;
    work.shared_resource = true;
    work.phase = GraphicsPreparationPhase::shader_setup;
    work.shader_step = ShaderPreparationPhase::factory;
    if (status.complete()) {
        work.progressed = true;
        return work;
    }
    const auto kind = status.preparing        ? *status.preparing
                      : !status.picking_ready ? ShaderPipelineKind::picking
                      : !status.overlay_ready ? ShaderPipelineKind::overlay
                                              : ShaderPipelineKind::display_resolve;
    work.shader_pipeline = kind;
    attempt = {};
    attempt.phase = work.phase;
    attempt.shader_pipeline = kind;
    attempt.shader_step = work.shader_step;
    const auto request = device.request_auxiliary_graphics(kind);
    if (!request)
        return request.error();
    work.phase = phase_of(request.value().phase);
    work.shader_step = request.value().phase;
    if (request.value().driver_call_limit > budget.driver_calls)
        return work;
    attempt.phase = work.phase;
    attempt.shader_step = work.shader_step;
    attempt.driver_call_limit = request.value().driver_call_limit;
    const auto step = device.advance_auxiliary_graphics(kind);
    if (!step)
        return step.error();
    work.driver_calls = step.value().driver_calls;
    work.progressed = true;
    return work;
}
bool PipelineResources::complete() const noexcept
{
    return bool(ready[0]) && bool(ready[1]);
}
bool PipelineResources::needs_retirement() const noexcept
{
    return failed && bool(pending);
}
Result<GraphicsRetirementStep>
PipelineResources::retire_unit(const graphics::ResourceRetirementBudget& budget)
{
    GraphicsRetirementStep step;
    step.phase = GraphicsRetirementPhase::shader_object;
    if (!needs_retirement())
        return step;
    const auto retired = pending->advance_retirement(budget);
    if (!retired)
        return retired.error();
    step.driver_calls = retired.value().driver_calls;
    step.deleted_objects = retired.value().deleted_objects;
    step.progressed = retired.value().progressed || retired.value().complete;
    if (retired.value().complete) {
        pending.reset();
        failed = false;
    }
    return step;
}
} // namespace elf3d::renderer
