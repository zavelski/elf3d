#include <elf3d/elf3d.h>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<void> pick_and_set_selection(elf3d::Viewport& viewport,
                                                         const elf3d::Scene& scene,
                                                         elf3d::EntityId camera_entity,
                                                         elf3d::Float2 position_pixels) noexcept {
    const elf3d::Result<std::optional<elf3d::PickHit>> hit =
        viewport.pick(scene, camera_entity, position_pixels);
    if (!hit) {
        return hit.error();
    }
    if (!hit.value().has_value()) {
        return {};
    }

    const elf3d::PickHit& picked = *hit.value();
    return viewport.set_selected_entity(scene, picked.entity);
}

[[nodiscard]] elf3d::Result<std::optional<elf3d::PickHit>>
select_at_position(elf3d::Viewport& viewport, const elf3d::Scene& scene,
                   elf3d::EntityId camera_entity, elf3d::Float2 position_pixels) noexcept {
    return viewport.select_at(scene, camera_entity, position_pixels);
}

} // namespace elf3d_examples
