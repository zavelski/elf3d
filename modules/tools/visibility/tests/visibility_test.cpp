#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <cstdint>
#include <optional>

import elf.assets;
import elf.scene;
import elf.tool.visibility;

namespace {

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(41, value);
}

[[nodiscard]] bool is_isolated_branch_visible(
    const elf3d::scene::Storage &scene, const elf3d::scene::VisibilityFilter &filter,
    elf3d::EntityId root, elf3d::EntityId child, elf3d::EntityId sibling) {
    return filter.has_isolation() &&
           elf3d::scene::entity_visible_in_filter(scene, filter, root) &&
           elf3d::scene::entity_visible_in_filter(scene, filter, child) &&
           !elf3d::scene::entity_visible_in_filter(scene, filter, sibling);
}

[[nodiscard]] bool is_cleared_filter(
    const elf3d::tools::visibility::VisibilityController &visibility,
    const elf3d::Result<elf3d::scene::VisibilityFilter> &filter) {
    return filter && !visibility.is_isolating() && !filter.value().has_isolation();
}

[[nodiscard]] bool has_active_isolation(
    elf3d::scene::Storage &scene, elf3d::tools::visibility::VisibilityController &visibility,
    elf3d::EntityId root) {
    return visibility.isolate_entity(scene, root) && visibility.is_isolating() &&
           visibility.isolated_entity() == root;
}

[[nodiscard]] bool is_hidden_in_filter(
    const elf3d::scene::Storage &scene,
    const elf3d::Result<elf3d::scene::VisibilityFilter> &filter, elf3d::EntityId entity) {
    return filter && !elf3d::scene::entity_visible_in_filter(scene, filter.value(), entity);
}

[[nodiscard]] bool rejects_foreign_entity(
    elf3d::scene::Storage &scene, elf3d::tools::visibility::VisibilityController &visibility) {
    elf3d::scene::Storage other_scene{scene_id(2)};
    const elf3d::EntityId other = other_scene.create_entity().value();
    return visibility.isolate_entity(scene, other).error().code() == elf3d::ErrorCode::invalid_entity;
}

int verify_isolation_lifecycle(elf3d::scene::Storage &scene,
                               elf3d::tools::visibility::VisibilityController &visibility,
                               elf3d::EntityId root, elf3d::EntityId child,
                               elf3d::EntityId sibling) {
    const elf3d::Result<elf3d::scene::VisibilityFilter> filter = visibility.filter_for(scene);
    if (!filter || !is_isolated_branch_visible(scene, filter.value(), root, child, sibling)) {
        return 4;
    }
    if (!scene.set_entity_visible(child, false)) {
        return 5;
    }
    const elf3d::Result<elf3d::scene::VisibilityFilter> hidden_filter =
        visibility.filter_for(scene);
    if (!is_hidden_in_filter(scene, hidden_filter, child)) {
        return 6;
    }
    if (!scene.destroy_entity(root)) {
        return 7;
    }
    const elf3d::Result<elf3d::scene::VisibilityFilter> cleared = visibility.filter_for(scene);
    if (!is_cleared_filter(visibility, cleared)) {
        return 8;
    }
    return rejects_foreign_entity(scene, visibility) ? 0 : 9;
}

} // namespace

int elf3d_tool_visibility_test() {
    elf3d::scene::Storage scene{scene_id(1)};
    const elf3d::EntityId root = scene.create_entity().value();
    const elf3d::EntityId child = scene.create_entity().value();
    const elf3d::EntityId sibling = scene.create_entity().value();
    if (!scene.set_parent(child, root)) {
        return 1;
    }

    elf3d::tools::visibility::VisibilityController visibility;
    if (visibility.is_isolating()) {
        return 2;
    }
    if (!has_active_isolation(scene, visibility, root)) {
        return 3;
    }

    return verify_isolation_lifecycle(scene, visibility, root, child, sibling);
}
