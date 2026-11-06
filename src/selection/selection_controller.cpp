#include <selection/selection.h>

#include <clipping/clipping.h>
#include <limits>
#include <optional>
#include <picking/picking.h>
#include <scene/scene.h>

namespace elf3d::selection {

Result<std::optional<PickHit>> SelectionController::select_at(picking::PickingService& picking,
                                                              const scene::Storage& scene,
                                                              SelectionTarget target)
{
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return select_at(picking, scene, target, visibility.value());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService& picking, const scene::Storage& scene,
                               SelectionTarget target, const scene::VisibilityFilter& visibility)
{
    return select_at(picking, scene, target, visibility, clipping::disabled_filter());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService& picking, const scene::Storage& scene,
                               SelectionTarget target, const scene::VisibilityFilter& visibility,
                               const clipping::ClippingFilter& clipping_filter)
{
    const picking::PickRequest request{target.camera, target.extent, target.position_pixels, {}};
    const Result<std::optional<PickHit>> pick_result =
        picking.pick(scene, request, visibility, clipping_filter);
    if (!pick_result) {
        return pick_result.error();
    }

    const std::optional<PickHit>& picked = pick_result.value();
    if (!picked.has_value()) {
        clear();
        return std::optional<PickHit>{};
    }

    selected_scene_ = scene.id();
    primitives_.clear();
    entity_ = picked->entity;
    hit_ = picked;
    return hit_;
}

Result<std::optional<PickHit>> SelectionController::select_hit(const scene::Storage& scene,
                                                               const std::optional<PickHit>& hit)
{
    if (!hit.has_value()) {
        clear();
        return std::optional<PickHit>{};
    }

    const Result<const scene::EntityRecord*> record = scene.entity(hit->entity);
    if (!record) {
        return record.error();
    }
    selected_scene_ = scene.id();
    primitives_.clear();
    entity_ = hit->entity;
    hit_ = *hit;
    return hit_;
}

Result<void> SelectionController::set_selected_entity(const scene::Storage& scene, EntityId entity)
{
    const Result<const scene::EntityRecord*> record = scene.entity(entity);
    if (!record) {
        return record.error();
    }
    if (entity_.has_value() && *entity_ == entity && selected_scene_ == scene.id() &&
        !hit_.has_value()) {
        return {};
    }
    selected_scene_ = scene.id();
    primitives_.clear();
    entity_ = entity;
    hit_.reset();
    return {};
}

void SelectionController::clear() noexcept
{
    selected_scene_ = {};
    entity_.reset();
    hit_.reset();
    primitives_.clear();
}

void SelectionController::clear_scene(SceneId scene) noexcept
{
    if (selected_scene_ == scene) {
        clear();
    }
}

void SelectionController::validate_against(const scene::Storage& scene) noexcept
{
    if (!primitives_.empty()) {
        validate_primitives(scene);
        return;
    }
    if (!entity_.has_value()) {
        return;
    }
    if (selected_scene_ != scene.id()) {
        clear();
        return;
    }

    const Result<const scene::EntityRecord*> record = scene.entity(*entity_);
    if (!record) {
        clear();
    }
}

bool SelectionController::has_selection() const noexcept
{
    return entity_.has_value() || !primitives_.empty();
}

void SelectionController::validate_primitives(const scene::Storage& scene) noexcept
{
    if (selected_scene_ != scene.id()) {
        clear();
        return;
    }
    if (validated_hierarchy_revision_ == scene.hierarchy_revision())
        return;
    for (const auto instance : primitives_) {
        if (!scene.validate_primitive_instance(instance)) {
            clear();
            return;
        }
    }
    validated_hierarchy_revision_ = scene.hierarchy_revision();
}

std::optional<EntityId> SelectionController::selected_entity() const noexcept
{
    return entity_;
}

std::optional<PickHit> SelectionController::selection_hit() const noexcept
{
    return hit_;
}

SelectionSnapshot SelectionController::snapshot() const noexcept
{
    SelectionSnapshot result;
    if (!primitives_.empty()) {
        result.primitive_count = primitives_.size();
        result.primitive_revision = primitive_revision_;
    }
    if (!entity_.has_value()) {
        return result;
    }
    result.entity = entity_;
    result.pick_hit = hit_;
    return result;
}

Result<void>
SelectionController::set_selected_primitives(const scene::Storage& scene,
                                             std::span<const PrimitiveInstance> primitives)
{
    auto normalized = scene::normalize_primitive_instances(scene, primitives);
    if (!normalized)
        return normalized.error();
    if (normalized.value().empty()) {
        clear();
        return {};
    }
    if (selected_scene_ == scene.id() && primitives_ == normalized.value())
        return {};
    if (primitive_revision_ == std::numeric_limits<std::uint64_t>::max())
        fatal_error("Primitive selection revision exhausted");
    ++primitive_revision_;
    selected_scene_ = scene.id();
    entity_.reset();
    hit_.reset();
    primitives_ = std::move(normalized).value();
    validated_hierarchy_revision_ = scene.hierarchy_revision();
    return {};
}
std::span<const PrimitiveInstance> SelectionController::selected_primitives() const noexcept
{
    return primitives_;
}

} // namespace elf3d::selection
