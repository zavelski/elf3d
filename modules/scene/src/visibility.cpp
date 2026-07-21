module;

#include <elf3d/core/assert.h>

#include <algorithm>
#include <optional>
#include <vector>

module elf.scene;

namespace elf3d::scene {

Result<void> Storage::set_entity_visible(EntityId entity_id, bool visible) {
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

Result<bool> Storage::entity_local_visibility(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_visible;
}

Result<bool> Storage::entity_effective_visibility(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->effective_visible;
}

Result<void> Storage::show_entity_and_ancestors(EntityId entity_id) {
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

Result<void> Storage::show_all_entities() {
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

std::optional<Bounds3>
Storage::visible_world_bounds(const VisibilityFilter& filter) const noexcept {
    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord>& record : entities_) {
        if (!record.has_value() || !record->model.has_value() ||
            !entity_visible_in_filter(*this, filter, record->id)) {
            continue;
        }
        expand_world_bounds(result, *record);
    }
    return result;
}

void Storage::update_effective_visibility_from(EntityId entity_id) noexcept {
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

void Storage::update_all_effective_visibility() noexcept {
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
                                                std::optional<EntityId> isolated_root) {
    VisibilityFilter filter;
    filter.scene = scene.id();
    filter.hierarchy_revision = scene.hierarchy_revision();
    filter.visibility_revision = scene.visibility_revision();
    if (!isolated_root.has_value()) {
        return filter;
    }

    const EntityId isolated_root_id = *isolated_root;
    const Result<const EntityRecord*> root = scene.entity(isolated_root_id);
    if (!root) {
        return root.error();
    }

    filter.isolated_root = isolated_root_id;
    std::vector<EntityId> stack;
    stack.push_back(isolated_root_id);
    std::size_t visited = 0;
    while (!stack.empty()) {
        if (++visited > scene.entities().size()) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Viewport isolation traversal detected a hierarchy cycle"};
        }
        const EntityId current = stack.back();
        stack.pop_back();
        filter.isolated_entity_values.push_back(detail::SceneHandleAccess::value(current));
        const Result<const EntityRecord*> record = scene.entity(current);
        if (!record) {
            return record.error();
        }
        for (const EntityId child : record.value()->children) {
            stack.push_back(child);
        }
    }
    std::sort(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end());
    filter.isolated_entity_values.erase(
        std::unique(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end()),
        filter.isolated_entity_values.end());
    return filter;
}

bool entity_visible_in_filter(const Storage& scene, const VisibilityFilter& filter,
                              EntityId entity_id) noexcept {
    const Result<const EntityRecord*> record = scene.entity(entity_id);
    if (!record || !record.value()->effective_visible) {
        return false;
    }
    if (!filter.isolated_root.has_value()) {
        return true;
    }
    if (filter.scene != scene.id()) {
        return false;
    }
    const std::uint64_t value = detail::SceneHandleAccess::value(entity_id);
    return std::binary_search(filter.isolated_entity_values.begin(),
                              filter.isolated_entity_values.end(), value);
}

} // namespace elf3d::scene
