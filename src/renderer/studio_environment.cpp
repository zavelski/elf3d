#include "renderer_cache.hpp"
#include "renderer_environment_preparation.h"
#include "renderer_pipeline_preparation.h"

#include <utility>

namespace elf3d::renderer {
namespace {
[[nodiscard]] bool failed_cleanup_complete(const std::unique_ptr<EnvironmentPreparation>& state)
{
    return state && state->failed && !state->needs_retirement();
}
void advance_shared_phase(SceneResourcePreparation& state, bool ready) noexcept
{
    if (!ready)
        return;
    if (state.phase == SceneResourcePhase::environment)
        state.phase = SceneResourcePhase::pipelines;
    else if (state.phase == SceneResourcePhase::pipelines)
        state.phase = SceneResourcePhase::auxiliary;
    else {
        state.phase = SceneResourcePhase::ready;
        state.status.state = GraphicsPreparationState::ready;
    }
}
} // namespace
Result<SceneResourceWork>
Renderer::advance_environment_resources(const GraphicsPreparationBudget& budget)
{
    SceneResourceWork ready;
    ready.phase = GraphicsPreparationPhase::environment_ready;
    ready.shared_resource = true;
    ready.progressed = true;
    if (environment_)
        return ready;
    if (!environment_source_)
        return Error{ErrorCode::graphics_shutdown,
                     "Renderer studio environment source is unavailable"};
    if (failed_cleanup_complete(environment_preparation_))
        environment_preparation_.reset();
    if (!environment_preparation_)
        environment_preparation_ = std::make_unique<EnvironmentPreparation>();
    auto resource = environment_source_->bytes();
    if (!resource) {
        environment_preparation_->failed = true;
        return resource.error();
    }
    auto step = environment_preparation_->advance(*device_, resource.value(), budget);
    if (!step)
        cache_->preparation_attempt = environment_preparation_->attempt;
    if (!step)
        return step.error();
    if (environment_preparation_->phase != EnvironmentPreparation::Phase::ready)
        return step;
    environment_ = environment_preparation_->take_resources();
    environment_preparation_.reset(); // every native owner has been moved
    return step;
}
Result<SceneResourceWork>
Renderer::advance_scene_environment(SceneResourcePreparation& state,
                                    const GraphicsPreparationBudget& budget)
{
    if (state.phase == SceneResourcePhase::environment) {
        auto step = advance_environment_resources(budget);
        if (step)
            advance_shared_phase(state, bool(environment_));
        return step;
    }
    if (!pipelines_)
        pipelines_ = std::make_unique<PipelineResources>();
    if (state.phase == SceneResourcePhase::pipelines) {
        const auto kind =
            pipelines_->ready[0] ? ShaderPipelineKind::extended : ShaderPipelineKind::core;
        auto step = pipelines_->advance(*device_, kind, budget);
        if (!step)
            cache_->preparation_attempt = pipelines_->attempt;
        if (step)
            advance_shared_phase(state, pipelines_->complete());
        return step;
    }
    auto step = pipelines_->advance_auxiliary(*device_, budget);
    if (!step)
        cache_->preparation_attempt = pipelines_->attempt;
    if (step)
        advance_shared_phase(state, device_->auxiliary_graphics_status().complete());
    return step;
}

Result<bool> Renderer::ensure_environment_resources()
{
    if (environment_)
        return false;
    if (environment_preparation_ && environment_preparation_->failed) {
        const auto cleaned = complete_retirement();
        if (!cleaned)
            return cleaned.error();
    }
    for (;;) {
        auto step = advance_environment_resources({});
        if (!step) {
            const auto error = step.error();
            const auto cleaned = complete_retirement();
            return cleaned ? Result<bool>{error} : Result<bool>{cleaned.error()};
        }
        if (environment_)
            return true;
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous environment preparation did not advance"};
    }
}
} // namespace elf3d::renderer
