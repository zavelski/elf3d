#include "viewer_document_browser.hpp"

namespace elf3d::viewer {
namespace {
[[nodiscard]] BrowserVisibility merged(BrowserVisibility a, BrowserVisibility b) noexcept
{
    if (a == BrowserVisibility::unavailable)
        return b;
    if (b == BrowserVisibility::unavailable || a == b)
        return a;
    return BrowserVisibility::mixed;
}
[[nodiscard]] BrowserResourceVisibility merged(BrowserResourceVisibility a,
                                               BrowserResourceVisibility b) noexcept
{
    return {merged(a.local, b.local), merged(a.effective, b.effective)};
}
[[nodiscard]] bool matches(const BrowserResourceVisibilityCache& cache, const Scene& scene,
                           const Viewport& viewport) noexcept
{
    return cache.started && cache.scene_revision == scene.visibility_revision() &&
           cache.view_revision == viewport.render_revision() &&
           cache.hierarchy_revision == scene.hierarchy_revision();
}
[[nodiscard]] BrowserResourceVisibility instance_visibility(const Scene& scene,
                                                            const Viewport& viewport, NodeId user,
                                                            PrimitiveId primitive)
{
    const auto entity = scene.entity_for_document_node(user);
    if (!entity || !entity.value())
        return {};
    const PrimitiveInstance instance{*entity.value(), primitive};
    const auto local = viewport.primitive_local_visibility(scene, instance);
    const auto effective = viewport.primitive_effective_visibility(scene, instance);
    if (!local || !effective)
        return {};
    return {local.value() ? BrowserVisibility::visible : BrowserVisibility::hidden,
            effective.value() ? BrowserVisibility::visible : BrowserVisibility::hidden};
}
void finish_row(BrowserResourceVisibilityCache& cache)
{
    cache.pending.push_back(cache.accumulated);
    cache.accumulated = {};
    cache.user = 0;
    ++cache.row;
}
void capture_step(DocumentBrowserState& state, const Scene& scene, const Viewport& viewport)
{
    auto& cache = state.resource_visibility;
    if (cache.row == state.meshes.rows.size()) {
        cache.phase = BrowserVisibilityCache::Phase::ancestors;
        return;
    }
    const auto& row = state.meshes.rows[cache.row];
    if (row.kind != BrowserRowKind::primitive) {
        finish_row(cache);
        return;
    }
    const auto primitive = browser_row_id<PrimitiveId>(row);
    const auto mesh = state.index.primitive(primitive).value().mesh;
    const auto users = state.index.mesh(mesh).value().users;
    if (cache.user == users.size()) {
        finish_row(cache);
        return;
    }
    cache.accumulated = merged(
        cache.accumulated, instance_visibility(scene, viewport, users[cache.user++], primitive));
}
void visibility_step(DocumentBrowserState& state, const Scene& scene, const Viewport& viewport)
{
    auto& cache = state.resource_visibility;
    if (cache.phase == BrowserVisibilityCache::Phase::capture) {
        capture_step(state, scene, viewport);
    } else if (cache.row != 0) {
        const auto row = --cache.row;
        if (const auto parent = state.meshes.rows[row].parent)
            cache.pending[*parent] = merged(cache.pending[*parent], cache.pending[row]);
    } else {
        cache.values.swap(cache.pending);
        cache.pending.clear();
        cache.phase = BrowserVisibilityCache::Phase::complete;
    }
}
} // namespace
void advance_browser_resource_visibility(DocumentBrowserState& state, const Scene& scene,
                                         const Viewport& viewport, std::size_t units)
{
    if (!state.meshes_built || state.scene != scene.id())
        return;
    auto& cache = state.resource_visibility;
    if (!matches(cache, scene, viewport)) {
        cache.started = true;
        cache.scene_revision = scene.visibility_revision();
        cache.view_revision = viewport.render_revision();
        cache.hierarchy_revision = scene.hierarchy_revision();
        cache.pending.clear();
        cache.row = 0;
        cache.user = 0;
        cache.accumulated = {};
        cache.phase = BrowserVisibilityCache::Phase::capture;
    }
    for (std::size_t i = 0; i < units && cache.phase != BrowserVisibilityCache::Phase::complete;
         ++i)
        visibility_step(state, scene, viewport);
}
BrowserResourceVisibility browser_resource_visibility(const DocumentBrowserState& state,
                                                      const Scene& scene, const Viewport& viewport,
                                                      std::size_t row) noexcept
{
    const auto& cache = state.resource_visibility;
    if (state.scene != scene.id() || !matches(cache, scene, viewport) ||
        cache.phase != BrowserVisibilityCache::Phase::complete || row >= cache.values.size())
        return {};
    return cache.values[row];
}
} // namespace elf3d::viewer
