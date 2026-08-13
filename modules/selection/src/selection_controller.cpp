module;

#include <optional>

module elf.selection;

import elf.clipping;
import elf.picking;
import elf.scene;

namespace elf3d::selection {

Result<std::optional<PickHit>> SelectionController::select_at(picking::PickingService& picking,
                                                              const scene::Storage& scene,
                                                              SelectionTarget target) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return select_at(picking, scene, target, visibility.value());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService& picking, const scene::Storage& scene,
                               SelectionTarget target, const scene::VisibilityFilter& visibility) {
    return select_at(picking, scene, target, visibility, clipping::disabled_filter());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService& picking, const scene::Storage& scene,
                               SelectionTarget target, const scene::VisibilityFilter& visibility,
                               const clipping::ClippingFilter& clipping_filter) {
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
    entity_ = picked->entity;
    hit_ = picked;
    return hit_;
}

Result<std::optional<PickHit>> SelectionController::select_hit(const scene::Storage& scene,
                                                               const std::optional<PickHit>& hit) {
    if (!hit.has_value()) {
        clear();
        return std::optional<PickHit>{};
    }

    const Result<const scene::EntityRecord*> record = scene.entity(hit->entity);
    if (!record) {
        return record.error();
    }
    selected_scene_ = scene.id();
    entity_ = hit->entity;
    hit_ = *hit;
    return hit_;
}

Result<void> SelectionController::set_selected_entity(const scene::Storage& scene,
                                                      EntityId entity) {
    const Result<const scene::EntityRecord*> record = scene.entity(entity);
    if (!record) {
        return record.error();
    }
    if (entity_.has_value() && *entity_ == entity && selected_scene_ == scene.id() &&
        !hit_.has_value()) {
        return {};
    }
    selected_scene_ = scene.id();
    entity_ = entity;
    hit_.reset();
    return {};
}

void SelectionController::clear() noexcept {
    selected_scene_ = {};
    entity_.reset();
    hit_.reset();
}

void SelectionController::clear_scene(SceneId scene) noexcept {
    if (selected_scene_ == scene) {
        clear();
    }
}

void SelectionController::validate_against(const scene::Storage& scene) noexcept {
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

bool SelectionController::has_selection() const noexcept {
    return entity_.has_value();
}

std::optional<EntityId> SelectionController::selected_entity() const noexcept {
    return entity_;
}

std::optional<PickHit> SelectionController::selection_hit() const noexcept {
    return hit_;
}

SelectionSnapshot SelectionController::snapshot() const noexcept {
    SelectionSnapshot result;
    if (!entity_.has_value()) {
        return result;
    }
    result.entity = entity_;
    result.pick_hit = hit_;
    return result;
}

} // namespace elf3d::selection
