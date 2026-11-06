#include <elf3d/elf3d.h>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<void> pick_and_set_selection(elf3d::Viewport& viewport,
                                                         const elf3d::Scene& scene,
                                                         elf3d::EntityId camera_entity,
                                                         elf3d::Float2 position_pixels) noexcept
{
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
                   elf3d::EntityId camera_entity, elf3d::Float2 position_pixels) noexcept
{
    return viewport.select_at(scene, camera_entity, position_pixels);
}

// Resource selection uses the canonical mapping, never PickHit::primitive_index
// as a source-table index. No vertex/index/image storage is copied.
[[nodiscard]] elf3d::Result<void> select_picked_primitive(elf3d::Viewport& viewport,
                                                          const elf3d::Scene& scene,
                                                          const elf3d::PickHit& hit) noexcept
{
    const auto primitive = scene.document_primitive(hit);
    if (!primitive)
        return primitive.error();
    if (!primitive.value())
        return elf3d::Error{elf3d::ErrorCode::invalid_primitive_id,
                            "The picked runtime object has no retained Document primitive"};
    const elf3d::PrimitiveInstance instance{hit.entity, *primitive.value()};
    return viewport.set_selected_primitives(scene, {&instance, 1});
}

[[nodiscard]] elf3d::Result<void> render_primitive_selection(elf3d::Viewport& viewport,
                                                             const elf3d::Scene& scene,
                                                             elf3d::EntityId camera) noexcept
{
    elf3d::ViewportRenderOptions options;
    options.primitive_highlight = elf3d::SelectionHighlight{{1, 0.55F, 0.05F, 1}, 0.45F};
    return viewport.render(scene, camera, options);
}

// This setting belongs to the view. Subsequent picks and navigation depth see
// both faces too; authored materials and file output remain unchanged.
[[nodiscard]] elf3d::Result<void> render_both_faces(elf3d::Viewport& viewport,
                                                    const elf3d::Scene& scene,
                                                    elf3d::EntityId camera) noexcept
{
    viewport.set_backface_culling_enabled(false);
    return viewport.render(scene, camera);
}

} // namespace elf3d_examples
