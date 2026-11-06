#include "viewer_tools.hpp"

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

namespace elf3d::viewer {
namespace {
[[nodiscard]] Result<std::vector<PrimitiveInstance>>
selected_resource_instances(const Viewport& viewport)
{
    std::vector<PrimitiveInstance> result;
    result.reserve(viewport.selected_primitive_count());
    for (std::size_t i = 0; i < viewport.selected_primitive_count(); ++i) {
        const auto key = viewport.selected_primitive_at(i);
        if (!key)
            return key.error();
        result.push_back(key.value());
    }
    return result;
}
[[nodiscard]] Result<void>
change_object_visibility(Scene& scene, std::span<const SelectionTool::Entry> entries, bool visible)
{
    for (const auto& entry : entries) {
        if (!entry.selected)
            continue;
        const auto changed = visible ? scene.show_entity_and_ancestors(entry.entity)
                                     : scene.set_entity_local_visibility(entry.entity, false);
        if (!changed)
            return changed.error();
    }
    return {};
}
} // namespace
Result<void> SelectionTool::synchronize(Scene& scene) noexcept
{
    if (scene_ == scene.id() && hierarchy_revision_ == scene.hierarchy_revision()) {
        return {};
    }
    try {
        const bool source_changed = scene_ != scene.id();
        entries_.clear();
        if (source_changed) {
            browser_selection_.clear();
            browser_primary_.reset();
            browser_domain_ = BrowserSelectionDomain::objects;
        }
        selected_count_ = 0;
        scene_ = {};
        const auto hierarchy = scene.hierarchy_snapshot();
        if (!hierarchy) {
            return hierarchy.error();
        }
        entries_.reserve(hierarchy.value().size());
        for (std::size_t index = 0; index < hierarchy.value().size(); ++index) {
            const auto item = hierarchy.value().item_at(index);
            if (!item) {
                return item.error();
            }
            entries_.push_back(
                {item.value().entity, item.value().has_camera && !item.value().renderable});
        }
        scene_ = scene.id();
        hierarchy_revision_ = scene.hierarchy_revision();
        refresh_browser_projection(scene);
        return {};
    } catch (const std::bad_alloc&) {
        fatal_error("Selection hierarchy allocation failed");
    } catch (...) {
        fatal_error("Unexpected selection hierarchy exception");
    }
}
std::span<const SelectionTool::Entry> SelectionTool::entries() const noexcept
{
    return entries_;
}
std::size_t SelectionTool::selected_count() const noexcept
{
    return selected_count_;
}
Result<void> SelectionTool::select_entity(Scene& scene, Viewport& viewport, EntityId entity,
                                          bool additive) noexcept
{
    if (const auto synced = synchronize(scene); !synced) {
        return synced.error();
    }
    if (const auto valid = scene.entity_info(entity); !valid) {
        return valid.error();
    }
    return apply_membership(scene, viewport, entity, additive);
}
Result<std::optional<PickHit>> SelectionTool::select_at(Scene& scene, Viewport& viewport,
                                                        EntityId camera, Float2 position_pixels,
                                                        bool additive) noexcept
{
    if (!enabled_) {
        return std::optional<PickHit>{};
    }
    if (const auto synced = synchronize(scene); !synced) {
        return synced.error();
    }
    auto hit = viewport.pick(scene, camera, position_pixels);
    if (!hit) {
        return hit.error();
    }
    const auto selected = apply_pick(scene, viewport, hit.value(), additive);
    if (!selected) {
        return selected.error();
    }
    return hit;
}
Result<void> SelectionTool::apply_pick(Scene& scene, Viewport& viewport,
                                       const std::optional<PickHit>& hit, bool additive) noexcept
{
    if (!hit)
        return additive ? Result<void>{} : set_browser_selection(scene, viewport, {});
    if (browser_domain_ == BrowserSelectionDomain::objects)
        return apply_membership(scene, viewport, hit->entity, additive);
    const auto primitive = scene.document_primitive(*hit);
    if (!primitive)
        return primitive.error();
    if (!primitive.value())
        return additive ? Result<void>{} : set_browser_selection(scene, viewport, {});
    return apply_membership(scene, viewport, *primitive.value(), additive);
}
void SelectionTool::clear(Viewport& viewport) noexcept
{
    for (Entry& entry : entries_) {
        entry.selected = false;
    }
    browser_selection_.clear();
    browser_primary_.reset();
    browser_domain_ = BrowserSelectionDomain::objects;
    selected_count_ = 0;
    viewport.clear_selection();
}
void SelectionTool::clear_scene(SceneId scene) noexcept
{
    if (scene_ == scene) {
        entries_.clear();
        browser_selection_.clear();
        browser_primary_.reset();
        browser_domain_ = BrowserSelectionDomain::objects;
        selected_count_ = 0;
        scene_ = {};
        hierarchy_revision_ = 0;
    }
}
Result<void> SelectionTool::set_selected_visibility(Scene& scene, Viewport& viewport,
                                                    bool visible) noexcept
{
    try {
        if (scene_ != scene.id()) {
            return Error{ErrorCode::invalid_entity, "Selection belongs to another Scene"};
        }
        if (browser_domain_ == BrowserSelectionDomain::resources) {
            const auto instances = selected_resource_instances(viewport);
            if (!instances)
                return instances.error();
            if (instances.value().empty())
                return Error{ErrorCode::scene_has_no_bounds,
                             "Selected resources have no active instances"};
            return viewport.set_primitive_local_visibility(scene, instances.value(), visible);
        }
        return change_object_visibility(scene, entries_, visible);
    } catch (const std::bad_alloc&) {
        fatal_error("Selection visibility allocation failed");
    } catch (...) {
        fatal_error("Unexpected selection visibility exception");
    }
}
Result<void> SelectionTool::frame_selected(Scene& scene, Viewport& viewport,
                                           EntityId camera) noexcept
{
    try {
        if (const auto synced = synchronize(scene); !synced)
            return synced.error();
        if (browser_domain_ == BrowserSelectionDomain::resources) {
            const auto instances = selected_resource_instances(viewport);
            if (!instances)
                return instances.error();
            return viewport.fit_to_primitives(scene, camera, instances.value());
        }
        std::vector<EntityId> roots;
        for (const auto& entry : entries_)
            if (entry.selected)
                roots.push_back(entry.entity);
        return viewport.fit_to_entities(scene, camera, roots);
    } catch (const std::bad_alloc&) {
        fatal_error("Selection framing allocation failed");
    } catch (...) {
        fatal_error("Unexpected selection framing exception");
    }
}
Result<void> SelectionTool::isolate_selected(Scene& scene, Viewport& viewport) noexcept
{
    try {
        if (const auto shown = set_selected_visibility(scene, viewport, true); !shown) {
            return shown.error();
        }
        if (browser_domain_ == BrowserSelectionDomain::resources) {
            const auto instances = selected_resource_instances(viewport);
            if (!instances)
                return instances.error();
            return viewport.isolate_primitives(scene, instances.value());
        }
        std::vector<EntityId> roots;
        for (const Entry& entry : entries_) {
            if (entry.selected) {
                roots.push_back(entry.entity);
            }
        }
        if (roots.empty()) {
            return Error{ErrorCode::invalid_argument, "Select objects before isolating"};
        }
        return viewport.isolate_entities(scene, roots);
    } catch (const std::bad_alloc&) {
        fatal_error("Selection isolation allocation failed");
    } catch (...) {
        fatal_error("Unexpected selection isolation exception");
    }
}
} // namespace elf3d::viewer
