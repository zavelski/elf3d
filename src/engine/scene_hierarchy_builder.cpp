#include "scene_hierarchy_snapshot.h"

#include <algorithm>
#include <assets/assets.h>
#include <limits>

namespace elf3d {
namespace {
[[nodiscard]] SceneHierarchyItem make_item(const scene::EntityRecord& record, std::uint32_t depth,
                                           std::size_t ordinal)
{
    SceneHierarchyItem item;
    item.entity = record.id;
    item.parent = record.parent;
    item.depth = depth;
    item.child_count = static_cast<std::uint32_t>(
        std::min<std::size_t>(record.children.size(), std::numeric_limits<std::uint32_t>::max()));
    item.has_model = record.model.has_value();
    item.has_camera = record.camera.has_value();
    item.local_visible = record.local_visible;
    item.effective_visible = record.effective_visible;
    item.renderable = item.has_model;
    item.traversal_index = static_cast<std::uint64_t>(ordinal);
    return item;
}
} // namespace

void SceneHierarchySnapshot::Impl::scan_root(const scene::Storage& source)
{
    if (slot_cursor == source.entities().size()) {
        phase = Phase::walk;
        return;
    }
    const auto& record = source.entities()[slot_cursor++];
    visited.push_back(0);
    if (record) {
        ++live_count;
        if (!record->parent) {
            roots.push_back(record->id);
        }
    }
}

Result<void> SceneHierarchySnapshot::Impl::enter_entity(Frame& frame,
                                                        const scene::EntityRecord& record)
{
    const auto value = detail::SceneHandleAccess::value(record.id);
    if (value == 0 || value > visited.size()) {
        return Error{ErrorCode::invalid_entity, "Hierarchy capture encountered an invalid entity"};
    }
    const auto slot = static_cast<std::size_t>(value - 1); // bounded by visited.size()
    if (visited[slot] != 0) {
        return Error{ErrorCode::hierarchy_cycle, "Hierarchy capture encountered an entity twice"};
    }
    visited[slot] = 1;
    frame.row = items.size();
    items.push_back(make_item(record, frame.depth, items.size()));
    names.emplace_back();
    names.back().reserve(record.name.size());
    return {};
}

Result<void> SceneHierarchySnapshot::Impl::walk_step(const scene::Storage& source)
{
    if (stack.empty()) {
        if (root_cursor < roots.size()) {
            stack.push_back(Frame{roots[root_cursor++]});
            return {};
        }
        if (items.size() != live_count) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Hierarchy capture could not reach every entity"};
        }
        phase = Phase::complete;
        return {};
    }
    auto& frame = stack.back();
    const auto found = source.entity(frame.entity);
    if (!found) {
        return found.error();
    }
    const auto& record = *found.value();
    if (!frame.row) {
        return enter_entity(frame, record);
    }
    if (frame.name_offset < record.name.size()) {
        const auto count = std::min<std::size_t>(1024, record.name.size() - frame.name_offset);
        names[*frame.row].append(record.name, frame.name_offset, count);
        frame.name_offset += count;
        return {};
    }
    if (frame.child < record.children.size()) {
        if (stack.size() == visited.size()) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Hierarchy capture traversal exceeded the entity count"};
        }
        if (frame.depth == std::numeric_limits<std::uint32_t>::max()) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "Hierarchy capture depth limit exceeded"};
        }
        const auto next = record.children[frame.child++];
        const auto depth = frame.depth + 1;
        stack.push_back(Frame{next, depth});
    } else {
        stack.pop_back();
    }
    return {};
}

Result<void> SceneHierarchySnapshot::Impl::step(const scene::Storage& source)
{
    if (phase == Phase::roots) {
        scan_root(source);
        return {};
    }
    return phase == Phase::walk ? walk_step(source) : Result<void>{};
}
} // namespace elf3d
