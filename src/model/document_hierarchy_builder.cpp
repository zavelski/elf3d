#include <model/document_hierarchy_storage.h>

#include <algorithm>
#include <utility>

namespace elf3d {
namespace {
[[nodiscard]] bool copy_name(std::string& target, std::string_view source)
{
    if (target.size() == source.size()) {
        return true;
    }
    if (target.empty()) {
        target.reserve(source.size());
    }
    target.append(source.substr(target.size(), 4096));
    return target.size() == source.size();
}

template <class Id>
[[nodiscard]] bool copy_links(std::vector<Id>& target, std::span<const Id> source)
{
    if (target.size() == source.size()) {
        return true;
    }
    if (target.empty()) {
        target.reserve(source.size());
    }
    const auto chunk =
        source.subspan(target.size(), std::min<std::size_t>(64, source.size() - target.size()));
    target.insert(target.end(), chunk.begin(), chunk.end());
    return target.size() == source.size();
}

// IDs come from this source's already validated canonical relationships.
template <class Id> [[nodiscard]] std::size_t ordinal(Id id) noexcept
{
    return static_cast<std::size_t>(id.debug_value() - 1);
}
} // namespace

void DocumentHierarchySnapshot::Impl::next(Phase value) noexcept
{
    phase = value;
    cursor = 0;
    link_cursor = 0;
}

Result<void> DocumentHierarchySnapshot::Impl::scene_step(DocumentView source)
{
    if (cursor == source.scene_count()) {
        next(Phase::nodes);
        return {};
    }
    const auto scene = source.scene_at(cursor).value();
    if (scenes.size() == cursor) {
        scenes.push_back({.id = scene.id});
    }
    auto& record = scenes[cursor];
    if (copy_name(record.name, scene.name) && copy_links(record.roots, scene.roots)) {
        ++cursor;
    }
    return {};
}

Result<void> DocumentHierarchySnapshot::Impl::node_step(DocumentView source)
{
    if (cursor == source.node_count()) {
        next(Phase::meshes);
        return {};
    }
    const auto node = source.node_at(cursor).value();
    if (nodes.size() == cursor) {
        NodeRecord record;
        record.id = node.id;
        record.parent = node.parent;
        record.mesh = node.mesh;
        record.camera = node.perspective_camera.has_value();
        nodes.push_back(std::move(record));
    }
    auto& record = nodes[cursor];
    if (copy_name(record.name, node.name) && copy_links(record.children, node.children)) {
        ++cursor;
    }
    return {};
}

Result<void> DocumentHierarchySnapshot::Impl::mesh_step(DocumentView source)
{
    if (cursor == source.mesh_count()) {
        next(Phase::primitives);
        return {};
    }
    const auto mesh = source.mesh_at(cursor).value();
    if (meshes.size() == cursor) {
        meshes.push_back({.id = mesh.id});
    }
    auto& record = meshes[cursor];
    if (copy_name(record.name, mesh.name) && copy_links(record.primitives, mesh.primitives)) {
        ++cursor;
    }
    return {};
}

void DocumentHierarchySnapshot::Impl::primitive_step(DocumentView source)
{
    if (cursor == source.primitive_count()) {
        next(Phase::count_users);
        return;
    }
    const auto primitive = source.primitive_at(cursor++).value();
    primitives.push_back({primitive.id, primitive.mesh, primitive.material,
                          static_cast<std::uint64_t>(primitive.data.positions.size()),
                          static_cast<std::uint64_t>(primitive.data.indices.size() / 3)});
}

void DocumentHierarchySnapshot::Impl::usage_step()
{
    if (phase == Phase::reserve_users) {
        if (cursor == meshes.size()) {
            next(Phase::fill_users);
        } else {
            auto& mesh = meshes[cursor++];
            mesh.users.reserve(mesh.user_count);
        }
        return;
    }
    if (cursor == nodes.size()) {
        next(phase == Phase::count_users ? Phase::reserve_users : Phase::seed_reachable);
        return;
    }
    const auto& node = nodes[cursor++];
    if (!node.mesh) {
        return;
    }
    auto& mesh = meshes[ordinal(*node.mesh)];
    if (phase == Phase::count_users) {
        ++mesh.user_count;
    } else {
        mesh.users.push_back(node.id);
    }
}

void DocumentHierarchySnapshot::Impl::mark_reachable(NodeId id)
{
    auto& node = nodes[ordinal(id)];
    if (!node.reachable) {
        node.reachable = true;
        reachable_queue.push_back(id);
    }
}

void DocumentHierarchySnapshot::Impl::reachable_step()
{
    if (phase == Phase::seed_reachable) {
        if (cursor == scenes.size()) {
            next(Phase::visit_reachable);
        } else if (link_cursor == scenes[cursor].roots.size()) {
            ++cursor;
            link_cursor = 0;
        } else {
            mark_reachable(scenes[cursor].roots[link_cursor++]);
        }
        return;
    }
    if (cursor == reachable_queue.size()) {
        // Metadata-only temporary traversal storage; no payload owners.
        std::vector<NodeId>{}.swap(reachable_queue);
        next(Phase::complete);
        return;
    }
    const auto& node = nodes[ordinal(reachable_queue[cursor])];
    if (link_cursor == node.children.size()) {
        ++cursor;
        link_cursor = 0;
    } else {
        mark_reachable(node.children[link_cursor++]);
    }
}

Result<void> DocumentHierarchySnapshot::Impl::structural_step(DocumentView source)
{
    switch (phase) {
    case Phase::scenes:
        return scene_step(source);
    case Phase::nodes:
        return node_step(source);
    case Phase::meshes:
        return mesh_step(source);
    case Phase::primitives:
        primitive_step(source);
        break;
    default:
        break;
    }
    return {};
}

Result<void> DocumentHierarchySnapshot::Impl::step(DocumentView source)
{
    if (phase <= Phase::primitives) {
        return structural_step(source);
    }
    if (phase <= Phase::fill_users) {
        usage_step();
    } else if (phase != Phase::complete) {
        reachable_step();
    }
    return {};
}
} // namespace elf3d
