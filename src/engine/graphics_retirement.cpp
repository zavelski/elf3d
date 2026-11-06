#include "graphics_retirement.h"

#include <algorithm>
#include <cmath>
#include <picking/picking.h>
#include <renderer/renderer.h>

namespace elf3d::detail {
namespace {
[[nodiscard]] GraphicsRetirementPhase phase(picking::PickingRetirementPhase source) noexcept
{
    switch (source) {
    case picking::PickingRetirementPhase::triangles:
        return GraphicsRetirementPhase::picking_triangles;
    case picking::PickingRetirementPhase::order:
        return GraphicsRetirementPhase::picking_order;
    case picking::PickingRetirementPhase::nodes:
        return GraphicsRetirementPhase::picking_nodes;
    case picking::PickingRetirementPhase::metadata:
        return GraphicsRetirementPhase::cache_metadata;
    }
    return GraphicsRetirementPhase::cache_metadata;
}
[[nodiscard]] Result<GraphicsRetirementStep>
advance_unit(renderer::Renderer& renderer, picking::PickingService& picking,
             const graphics::ResourceRetirementBudget& budget)
{
    auto step = renderer.advance_retirement_unit(budget);
    if (!step)
        return step.error();
    if (step.value().progressed)
        return step;
    const auto cpu = picking.advance_retirement();
    GraphicsRetirementStep result;
    result.phase = phase(cpu.phase);
    result.scene = cpu.scene;
    result.progressed = cpu.progressed;
    result.freed_cpu_bytes = cpu.freed_bytes;
    return result;
}
void record_step(GraphicsRetirementActivity& activity, const GraphicsRetirementStep& step) noexcept
{
    ++activity.work_units;
    activity.driver_calls += step.driver_calls;
    activity.deleted_objects += step.deleted_objects;
    activity.freed_cpu_bytes += step.freed_cpu_bytes;
    activity.last_step = step;
    if (step.milliseconds > activity.longest_step.milliseconds)
        activity.longest_step = step;
}
[[nodiscard]] Result<void> advance_batch(renderer::Renderer& renderer,
                                         picking::PickingService& picking,
                                         const GraphicsRetirementBudget& budget,
                                         GraphicsRetirementActivity& activity)
{
    const auto begin = renderer.device().monotonic_time_milliseconds();
    while (activity.work_units < budget.work_units) {
        const graphics::ResourceRetirementBudget remaining{
            budget.objects - activity.deleted_objects, budget.driver_calls - activity.driver_calls};
        const auto unit_begin = renderer.device().monotonic_time_milliseconds();
        auto step = advance_unit(renderer, picking, remaining);
        const auto end = renderer.device().monotonic_time_milliseconds();
        activity.milliseconds = end - begin;
        if (!step)
            return step.error();
        step.value().milliseconds = end - unit_begin;
        if (!step.value().progressed) {
            activity.budget_exhausted = true;
            break;
        }
        if (step.value().driver_calls > remaining.driver_calls ||
            step.value().deleted_objects > remaining.objects)
            return Error{ErrorCode::invalid_argument,
                         "Retirement exceeded its remaining service allowance"};
        record_step(activity, step.value());
        if (activity.milliseconds >= budget.milliseconds) {
            activity.budget_exhausted = true;
            break;
        }
    }
    activity.budget_exhausted =
        activity.budget_exhausted || activity.work_units == budget.work_units;
    return {};
}
} // namespace
GraphicsRetirementStatus graphics_retirement_status(const renderer::Renderer& renderer,
                                                    const picking::PickingService& picking) noexcept
{
    auto status = renderer.retirement_status();
    const auto cpu = picking.retirement_status();
    status.picking_scenes = cpu.scenes;
    status.picking_meshes = cpu.meshes;
    status.picking_bytes = cpu.bytes;
    return status;
}
Result<GraphicsRetirementReport> advance_graphics_retirement(renderer::Renderer& renderer,
                                                             picking::PickingService& picking,
                                                             const GraphicsRetirementBudget& budget)
{
    if (!std::isfinite(budget.milliseconds) || budget.milliseconds < 0)
        return Error{ErrorCode::invalid_argument,
                     "Retirement requires a finite nonnegative time budget"};
    GraphicsRetirementReport report;
    report.status = graphics_retirement_status(renderer, picking);
    if (report.status.empty() || budget.milliseconds == 0 || budget.work_units == 0)
        return report;
    if (const auto advanced = advance_batch(renderer, picking, budget, report.activity); !advanced)
        return advanced.error();
    report.status = graphics_retirement_status(renderer, picking);
    return report;
}
Result<SceneGraphicsPreparationReport>
advance_scene_graphics(renderer::Renderer& renderer, picking::PickingService& picking,
                       const scene::Storage& scene, const GraphicsPreparationBudget& budget)
{
    const auto retirement = advance_graphics_retirement(
        renderer, picking,
        {budget.milliseconds, budget.work_units, budget.driver_calls, budget.retired_objects});
    if (!retirement)
        return retirement.error();
    if (budget.milliseconds > 0 && budget.work_units > 0) {
        if (const auto admitted = renderer.admit_scene_resources(scene); !admitted)
            return admitted.error();
    }
    auto remaining = budget;
    remaining.milliseconds =
        std::max(0.0, budget.milliseconds - retirement.value().activity.milliseconds);
    remaining.driver_calls -= retirement.value().activity.driver_calls;
    remaining.work_units -= retirement.value().activity.work_units;
    if (!retirement.value().status.empty()) {
        SceneGraphicsPreparationReport report;
        report.status = renderer.scene_resource_status(scene);
        report.retirement = retirement.value();
        return report;
    }
    auto report = renderer.advance_scene_resources(scene, remaining);
    if (report)
        report.value().retirement = retirement.value();
    return report;
}
} // namespace elf3d::detail
