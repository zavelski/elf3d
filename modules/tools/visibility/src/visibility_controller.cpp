#include <elf3d/tools/visibility/visibility_controller.h>

namespace elf3d::tools::visibility {

Result<void> VisibilityController::isolate_entity(const scene::Storage &scene, EntityId entity) {
    const Result<const scene::EntityRecord *> record = scene.entity(entity);
    if (!record) {
        return record.error();
    }
    if (isolated_scene_ == scene.id() && isolated_entity_.has_value() &&
        isolated_entity_.value() == entity) {
        return {};
    }
    isolated_scene_ = scene.id();
    isolated_entity_ = entity;
    return {};
}

void VisibilityController::clear_isolation() noexcept {
    isolated_scene_ = {};
    isolated_entity_.reset();
}

void VisibilityController::clear_scene(SceneId scene) noexcept {
    if (isolated_scene_ == scene) {
        clear_isolation();
    }
}

void VisibilityController::validate_against(const scene::Storage &scene) noexcept {
    if (!isolated_entity_.has_value()) {
        return;
    }
    if (isolated_scene_ != scene.id()) {
        clear_isolation();
        return;
    }
    const Result<const scene::EntityRecord *> record = scene.entity(isolated_entity_.value());
    if (!record) {
        clear_isolation();
    }
}

bool VisibilityController::is_isolating() const noexcept {
    return isolated_entity_.has_value();
}

std::optional<EntityId> VisibilityController::isolated_entity() const noexcept {
    return isolated_entity_;
}

Result<scene::VisibilityFilter> VisibilityController::filter_for(const scene::Storage &scene) {
    validate_against(scene);
    if (!isolated_entity_.has_value()) {
        return scene::make_visibility_filter(scene, std::nullopt);
    }
    return scene::make_visibility_filter(scene, isolated_entity_);
}

} // namespace elf3d::tools::visibility
