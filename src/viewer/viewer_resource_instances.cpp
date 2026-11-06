#include "viewer_document_browser.hpp"
#include <new>

namespace elf3d::viewer {
namespace {
[[nodiscard]] Result<void> append_instances(const Scene& scene,
                                            const DocumentHierarchySnapshot& index, MeshId mesh,
                                            std::span<const PrimitiveId> primitives,
                                            std::vector<PrimitiveInstance>& output)
{
    const auto resource = index.mesh(mesh);
    if (!resource)
        return resource.error();
    for (const auto user : resource.value().users) {
        const auto entity = scene.entity_for_document_node(user);
        if (!entity)
            return entity.error();
        if (!entity.value())
            continue;
        for (const auto primitive : primitives)
            output.push_back({*entity.value(), primitive});
    }
    return {};
}
[[nodiscard]] Result<void> append_resource(const Scene& scene,
                                           const DocumentHierarchySnapshot& index,
                                           const BrowserSelectionKey& key,
                                           std::vector<PrimitiveInstance>& output)
{
    if (const auto* mesh = std::get_if<MeshId>(&key)) {
        const auto source = index.mesh(*mesh);
        if (!source)
            return source.error();
        return append_instances(scene, index, *mesh, source.value().primitives, output);
    }
    if (const auto* primitive = std::get_if<PrimitiveId>(&key)) {
        const auto source = index.primitive(*primitive);
        if (!source)
            return source.error();
        return append_instances(scene, index, source.value().mesh, {primitive, 1}, output);
    }
    return Error{ErrorCode::invalid_argument, "Resource actions require mesh or primitive keys"};
}
} // namespace
Result<void> apply_browser_resource_action(Scene& scene, Viewport& viewport,
                                           const DocumentHierarchySnapshot& index,
                                           const BrowserResourceRequest& request)
{
    const auto instances = browser_resource_instances(scene, index, {&request.key, 1});
    if (!instances)
        return instances.error();
    if (instances.value().empty())
        return Error{ErrorCode::scene_has_no_bounds, "This resource has no active scene instances"};
    switch (request.action) {
    case BrowserResourceAction::frame:
        return viewport.fit_to_primitives(scene, request.camera, instances.value());
    case BrowserResourceAction::hide:
        return viewport.set_primitive_local_visibility(scene, instances.value(), false);
    case BrowserResourceAction::show:
        return viewport.set_primitive_local_visibility(scene, instances.value(), true);
    case BrowserResourceAction::isolate:
        if (const auto shown =
                viewport.set_primitive_local_visibility(scene, instances.value(), true);
            !shown)
            return shown.error();
        return viewport.isolate_primitives(scene, instances.value());
    }
    return Error{ErrorCode::invalid_argument, "Unknown resource action"};
}
Result<std::vector<PrimitiveInstance>>
browser_resource_instances(const Scene& scene, const DocumentHierarchySnapshot& index,
                           std::span<const BrowserSelectionKey> keys)
{
    const auto document = scene.document();
    if (!document || !index.matches_document(*document))
        return Error{ErrorCode::invalid_argument,
                     "Resource actions require the current Document hierarchy index"};
    std::vector<PrimitiveInstance> result;
    for (const auto& key : keys) {
        if (const auto appended = append_resource(scene, index, key, result); !appended)
            return appended.error();
    }
    return result;
}
Result<void>
SelectionTool::resolve_browser_resources(Scene& scene, Viewport& viewport,
                                         const DocumentHierarchySnapshot& index) noexcept
{
    try {
        if (browser_domain_ != BrowserSelectionDomain::resources)
            return {};
        if (!resource_feedback_dirty_ && resource_feedback_hierarchy_ == scene.hierarchy_revision())
            return {};
        auto instances = browser_resource_instances(scene, index, browser_selection_);
        if (!instances)
            return instances.error();
        if (const auto selected = viewport.set_selected_primitives(scene, instances.value());
            !selected)
            return selected.error();
        resource_feedback_dirty_ = false;
        resource_feedback_hierarchy_ = scene.hierarchy_revision();
        return {};
    } catch (const std::bad_alloc&) {
        fatal_error("Resource selection allocation failed");
    } catch (...) {
        fatal_error("Unexpected resource selection exception");
    }
}
} // namespace elf3d::viewer
