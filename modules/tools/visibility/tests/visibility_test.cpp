#include <elf3d/assets/handle_access.h>
#include <elf3d/tools/visibility/visibility_controller.h>

#include <cstdint>

namespace {

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(41, value);
}

} // namespace

int main() {
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
    if (!visibility.isolate_entity(scene, root) || !visibility.is_isolating() ||
        visibility.isolated_entity() != root) {
        return 3;
    }

    const elf3d::Result<elf3d::scene::VisibilityFilter> filter = visibility.filter_for(scene);
    if (!filter || !filter.value().has_isolation() ||
        !elf3d::scene::entity_visible_in_filter(scene, filter.value(), root) ||
        !elf3d::scene::entity_visible_in_filter(scene, filter.value(), child) ||
        elf3d::scene::entity_visible_in_filter(scene, filter.value(), sibling)) {
        return 4;
    }

    if (!scene.set_entity_visible(child, false)) {
        return 5;
    }
    const elf3d::Result<elf3d::scene::VisibilityFilter> hidden_filter =
        visibility.filter_for(scene);
    if (!hidden_filter ||
        elf3d::scene::entity_visible_in_filter(scene, hidden_filter.value(), child)) {
        return 6;
    }

    if (!scene.destroy_entity(root)) {
        return 7;
    }
    const elf3d::Result<elf3d::scene::VisibilityFilter> cleared = visibility.filter_for(scene);
    if (!cleared || visibility.is_isolating() || cleared.value().has_isolation()) {
        return 8;
    }

    elf3d::scene::Storage other_scene{scene_id(2)};
    const elf3d::EntityId other = other_scene.create_entity().value();
    if (visibility.isolate_entity(scene, other).error().code() != elf3d::ErrorCode::invalid_entity) {
        return 9;
    }

    return 0;
}
