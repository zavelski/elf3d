#include "viewer_document_browser.hpp"

namespace elf3d::viewer {
namespace {
[[nodiscard]] BrowserVisibility merged(BrowserVisibility a, BrowserVisibility b) noexcept
{
    if (a == BrowserVisibility::unavailable) {
        return b;
    }
    if (b == BrowserVisibility::unavailable || a == b) {
        return a;
    }
    return BrowserVisibility::mixed;
}

[[nodiscard]] BrowserVisibility instance_visibility(const BrowserRow& row, const Scene& scene)
{
    std::optional<EntityId> entity;
    if (!row.key) {
        return BrowserVisibility::unavailable;
    }
    if (const auto* node = std::get_if<NodeId>(&*row.key)) {
        const auto found = scene.entity_for_document_node(*node);
        if (found) {
            entity = found.value();
        }
    } else if (const auto* runtime = std::get_if<EntityId>(&*row.key)) {
        entity = *runtime;
    }
    if (!entity) {
        return BrowserVisibility::unavailable;
    }
    const auto visible = scene.entity_effective_visibility(*entity);
    if (!visible) {
        return BrowserVisibility::unavailable;
    }
    return visible.value() ? BrowserVisibility::visible : BrowserVisibility::hidden;
}

void visibility_step(DocumentBrowserState& state, const Scene& scene)
{
    auto& cache = state.visibility;
    if (cache.phase == BrowserVisibilityCache::Phase::capture) {
        if (cache.cursor == state.objects.rows.size()) {
            cache.phase = BrowserVisibilityCache::Phase::ancestors;
            return;
        }
        const auto& row = state.objects.rows[cache.cursor++];
        cache.pending.push_back(instance_visibility(row, scene));
    } else if (cache.cursor != 0) {
        const auto row = --cache.cursor;
        if (const auto parent = state.objects.rows[row].parent) {
            cache.pending[*parent] = merged(cache.pending[*parent], cache.pending[row]);
        }
    } else {
        cache.values.swap(cache.pending);
        cache.pending.clear();
        cache.phase = BrowserVisibilityCache::Phase::complete;
    }
}
} // namespace

void advance_browser_visibility(DocumentBrowserState& state, const Scene& scene, std::size_t units)
{
    if (!state.objects_built || state.scene != scene.id()) {
        return;
    }
    auto& cache = state.visibility;
    if (!cache.started || cache.revision != scene.visibility_revision()) {
        cache.started = true;
        cache.revision = scene.visibility_revision();
        cache.pending.clear();
        cache.cursor = 0;
        cache.phase = BrowserVisibilityCache::Phase::capture;
    }
    for (std::size_t unit = 0;
         unit < units && cache.phase != BrowserVisibilityCache::Phase::complete; ++unit) {
        visibility_step(state, scene);
    }
}

BrowserVisibility browser_subtree_visibility(const DocumentBrowserState& state, const Scene& scene,
                                             std::size_t row_index) noexcept
{
    const auto& cache = state.visibility;
    if (!state.objects_built || state.scene != scene.id() || !cache.started ||
        cache.revision != scene.visibility_revision() ||
        cache.phase != BrowserVisibilityCache::Phase::complete ||
        row_index >= cache.values.size()) {
        return BrowserVisibility::unavailable;
    }
    return cache.values[row_index];
}
} // namespace elf3d::viewer
