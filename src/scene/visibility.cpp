#include <scene/scene.h>

#include <elf3d/core/assert.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace elf3d::scene {
namespace {

void expand_bounds(std::optional<Bounds3>& target, Bounds3 bounds) noexcept
{
    if (!target.has_value()) {
        target = bounds;
        return;
    }
    target->minimum.x = std::min(target->minimum.x, bounds.minimum.x);
    target->minimum.y = std::min(target->minimum.y, bounds.minimum.y);
    target->minimum.z = std::min(target->minimum.z, bounds.minimum.z);
    target->maximum.x = std::max(target->maximum.x, bounds.maximum.x);
    target->maximum.y = std::max(target->maximum.y, bounds.maximum.y);
    target->maximum.z = std::max(target->maximum.z, bounds.maximum.z);
}

} // namespace

Result<void> Storage::set_entity_visible(EntityId entity_id, bool visible)
{
    Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->local_visible == visible) {
        return {};
    }

    record.value()->local_visible = visible;
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_visibility_revision();
    return {};
}

Result<bool> Storage::entity_local_visibility(EntityId entity_id) const noexcept
{
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_visible;
}

Result<bool> Storage::entity_effective_visibility(EntityId entity_id) const noexcept
{
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->effective_visible;
}

Result<void> Storage::show_entity_and_ancestors(EntityId entity_id)
{
    Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }

    std::vector<EntityId> path;
    EntityId current = entity_id;
    while (current.is_valid()) {
        path.push_back(current);
        const Result<const EntityRecord*> current_record = entity(current);
        if (!current_record) {
            return current_record.error();
        }
        const EntityRecord& current_source = *current_record.value();
        if (!current_source.parent.has_value()) {
            break;
        }
        current = *current_source.parent;
    }

    bool changed = false;

    for (const EntityId path_entity : path) {
        Result<EntityRecord*> current_record = mutable_entity(path_entity);
        if (!current_record) {
            return current_record.error();
        }
        if (!current_record.value()->local_visible) {
            current_record.value()->local_visible = true;
            changed = true;
        }
    }
    if (!changed) {
        return {};
    }

    update_effective_visibility_from(path.back());
    increment_revision();
    increment_visibility_revision();
    return {};
}

Result<void> Storage::show_all_entities()
{
    bool changed = false;
    for (std::optional<EntityRecord>& record : entities_) {
        if (record.has_value() && !record->local_visible) {
            record->local_visible = true;
            changed = true;
        }
    }
    if (!changed) {
        return {};
    }

    update_all_effective_visibility();
    increment_revision();
    increment_visibility_revision();
    return {};
}

bool Storage::visible_bounds_cache_matches(const VisibilityFilter& filter) const noexcept
{
    const bool cacheable = filter.hierarchy_revision == hierarchy_revision_ &&
                           filter.visibility_revision == visibility_revision_;
    return cacheable && cached_visible_world_bounds_valid_ &&
           cached_visible_spatial_revision_ == model_spatial_revision_ &&
           cached_visible_hierarchy_revision_ == hierarchy_revision_ &&
           cached_visible_visibility_revision_ == visibility_revision_ &&
           cached_visible_isolated_roots_ == filter.isolated_roots &&
           cached_visible_hidden_primitives_ == filter.hidden_primitives &&
           cached_visible_isolated_primitives_ == filter.isolated_primitives;
}

void Storage::cache_visible_bounds(const VisibilityFilter& filter,
                                   std::optional<Bounds3> bounds) const
{
    if (filter.hierarchy_revision != hierarchy_revision_ ||
        filter.visibility_revision != visibility_revision_) {
        return;
    }
    cached_visible_world_bounds_ = bounds;
    cached_visible_spatial_revision_ = model_spatial_revision_;
    cached_visible_hierarchy_revision_ = hierarchy_revision_;
    cached_visible_visibility_revision_ = visibility_revision_;
    cached_visible_isolated_roots_ = filter.isolated_roots;
    cached_visible_hidden_primitives_ = filter.hidden_primitives;
    cached_visible_isolated_primitives_ = filter.isolated_primitives;
    cached_visible_world_bounds_valid_ = true;
}

std::optional<Bounds3> Storage::visible_world_bounds(const VisibilityFilter& filter) const
{
    if (visible_bounds_cache_matches(filter)) {
        return cached_visible_world_bounds_;
    }

    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord>& record : entities_) {
        if (!record.has_value() || !record->model.has_value() ||
            !entity_visible_in_filter(*this, filter, record->id)) {
            continue;
        }

        const std::optional<Bounds3> bounds =
            filter.hidden_primitives.empty() && filter.isolated_primitives.empty()
                ? entity_world_bounds(*record)
                : filtered_primitive_bounds(*this, filter, record->id);
        if (!bounds.has_value()) {
            continue;
        }
        expand_bounds(result, *bounds);
    }

    cache_visible_bounds(filter, result);
    return result;
}

void Storage::update_effective_visibility_from(EntityId entity_id)
{
    ELF3D_ASSERT(mutable_entity(entity_id).has_value());
    std::vector<EntityId> stack{entity_id};
    while (!stack.empty()) {
        const EntityId current_id = stack.back();
        stack.pop_back();
        Result<EntityRecord*> current = mutable_entity(current_id);
        ELF3D_ASSERT(current.has_value());

        bool effective = current.value()->local_visible;
        if (current.value()->parent.has_value()) {
            const Result<const EntityRecord*> parent = entity(*current.value()->parent);
            ELF3D_ASSERT(parent.has_value());
            effective = effective && parent.value()->effective_visible;
        }

        current.value()->effective_visible = effective;

        for (const EntityId child : current.value()->children) {
            stack.push_back(child);
        }
    }
}

void Storage::update_all_effective_visibility() noexcept
{
    for (std::optional<EntityRecord>& record : entities_) {
        if (!record.has_value()) {
            continue;
        }
        bool effective = record->local_visible;
        std::optional<EntityId> parent = record->parent;
        std::size_t visited = 0;
        while (parent.has_value()) {
            ELF3D_ASSERT(++visited <= entities_.size());
            const Result<const EntityRecord*> parent_record = entity(*parent);
            ELF3D_ASSERT(parent_record.has_value());
            effective = effective && parent_record.value()->local_visible;
            parent = parent_record.value()->parent;
        }
        record->effective_visible = effective;
    }
}

Result<VisibilityFilter> make_visibility_filter(const Storage& scene,
                                                std::optional<EntityId> isolated_root)
{
    if (!isolated_root) {
        return make_visibility_filter(scene, std::span<const EntityId>{});
    }
    return make_visibility_filter(scene, std::span<const EntityId>{&*isolated_root, 1});
}

namespace {
struct IsolationVisit {
    EntityId entity;
    bool exiting = false;
};
[[nodiscard]] Result<void> append_isolated_subtree(const Storage& scene, EntityId root,
                                                   std::vector<std::uint8_t>& colors,
                                                   VisibilityFilter& filter,
                                                   std::vector<IsolationVisit>& stack)
{
    stack.clear();
    stack.push_back({root});
    while (!stack.empty()) {
        const auto visit = stack.back();
        stack.pop_back();
        const auto record = scene.entity(visit.entity);
        if (!record) {
            return record.error();
        }
        // Scene's own validated ID-to-slot mapping; never used by applications.
        const auto index =
            static_cast<std::size_t>(detail::SceneHandleAccess::value(visit.entity) - 1);
        if (visit.exiting) {
            colors[index] = 2;
            continue;
        }
        if (colors[index] == 2) {
            continue;
        }
        if (colors[index] == 1) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Viewport isolation detected a hierarchy cycle"};
        }
        colors[index] = 1;
        filter.isolated_entity_values.push_back(detail::SceneHandleAccess::value(visit.entity));
        stack.push_back({visit.entity, true});
        for (EntityId child : record.value()->children) {
            stack.push_back({child});
        }
    }
    return {};
}
} // namespace

Result<VisibilityFilter> make_visibility_filter(const Storage& scene,
                                                std::span<const EntityId> isolated_roots)
{
    VisibilityFilter filter;
    filter.scene = scene.id();
    filter.hierarchy_revision = scene.hierarchy_revision();
    filter.visibility_revision = scene.visibility_revision();
    if (isolated_roots.empty()) {
        return filter;
    }

    std::vector<bool> roots_seen(scene.entities().size(), false);
    std::vector<std::uint8_t> colors(scene.entities().size(), 0);
    for (EntityId root : isolated_roots) {
        if (const auto record = scene.entity(root); !record) {
            return record.error();
        }
        const auto index = static_cast<std::size_t>(detail::SceneHandleAccess::value(root) - 1);
        if (!roots_seen[index]) {
            roots_seen[index] = true;
            filter.isolated_roots.push_back(root);
        }
    }
    std::vector<IsolationVisit> stack;
    for (EntityId root : filter.isolated_roots) {
        if (const auto added = append_isolated_subtree(scene, root, colors, filter, stack);
            !added) {
            return added.error();
        }
    }

    std::sort(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end());
    filter.isolated_entity_values.erase(
        std::unique(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end()),
        filter.isolated_entity_values.end());
    return filter;
}

bool entity_visible_in_filter(const Storage& scene, const VisibilityFilter& filter,
                              EntityId entity_id) noexcept
{
    const Result<const EntityRecord*> record = scene.entity(entity_id);
    if (!record || !record.value()->effective_visible) {
        return false;
    }
    if (filter.scene != scene.id() && (filter.has_isolation() || !filter.hidden_primitives.empty()))
        return false;
    if (filter.isolated_roots.empty()) {
        return true;
    }
    const std::uint64_t value = detail::SceneHandleAccess::value(entity_id);
    return std::binary_search(filter.isolated_entity_values.begin(),
                              filter.isolated_entity_values.end(), value);
}

} // namespace elf3d::scene
