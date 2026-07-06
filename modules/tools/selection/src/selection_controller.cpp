module;

#include <algorithm>
#include <cmath>
#include <optional>

module elf.tool.selection;

import elf.clipping;
import elf.picking;
import elf.scene;

namespace elf3d::tools::selection {
namespace {

[[nodiscard]] Color4 sanitized_color(Color4 color) noexcept {
    const auto channel = [](float value, float fallback) {
        if (!std::isfinite(value)) {
            return fallback;
        }
        return std::clamp(value, 0.0F, 1.0F);
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 0.55F), channel(color.blue, 0.05F),
                  channel(color.alpha, 1.0F)};
}

[[nodiscard]] bool valid_settings(const SelectionSettings &settings) noexcept {
    return std::isfinite(settings.click_drag_threshold_pixels) &&
           settings.click_drag_threshold_pixels >= 0.0F &&
           std::isfinite(settings.highlight_strength) && settings.highlight_strength >= 0.0F &&
           settings.highlight_strength <= 1.0F;
}

} // namespace

Result<std::optional<PickHit>> SelectionController::select_at(picking::PickingService &picking,
                                                              const scene::Storage &scene,
                                                              EntityId camera, Extent2D extent,
                                                              Float2 position_pixels) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return select_at(picking, scene, camera, extent, position_pixels, visibility.value());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService &picking, const scene::Storage &scene,
                               EntityId camera, Extent2D extent, Float2 position_pixels,
                               const scene::VisibilityFilter &visibility) {
    return select_at(picking, scene, camera, extent, position_pixels, visibility,
                     clipping::disabled_filter());
}

Result<std::optional<PickHit>>
SelectionController::select_at(picking::PickingService &picking, const scene::Storage &scene,
                               EntityId camera, Extent2D extent, Float2 position_pixels,
                               const scene::VisibilityFilter &visibility,
                               const clipping::ClippingFilter &clipping_filter) {
    if (!enabled_) {
        return std::optional<PickHit>{};
    }

    const Result<std::optional<PickHit>> pick_result =
        picking.pick(scene, camera, extent, position_pixels, PickOptions{}, visibility,
                     clipping_filter);
    if (!pick_result) {
        return pick_result.error();
    }
    if (!pick_result.value().has_value()) {
        clear();
        return std::optional<PickHit>{};
    }

    selected_scene_ = scene.id();
    entity_ = pick_result.value()->entity;
    hit_ = pick_result.value();
    return hit_;
}

Result<std::optional<PickHit>>
SelectionController::select_hit(const scene::Storage &scene, const std::optional<PickHit> &hit) {
    if (!enabled_) {
        return std::optional<PickHit>{};
    }
    if (!hit.has_value()) {
        clear();
        return std::optional<PickHit>{};
    }

    const Result<const scene::EntityRecord *> record = scene.entity(hit->entity);
    if (!record) {
        return record.error();
    }
    selected_scene_ = scene.id();
    entity_ = hit->entity;
    hit_ = hit.value();
    return hit_;
}

Result<void> SelectionController::set_selected_entity(const scene::Storage &scene,
                                                      EntityId entity) {
    const Result<const scene::EntityRecord *> record = scene.entity(entity);
    if (!record) {
        return record.error();
    }
    if (entity_.has_value() && entity_.value() == entity && selected_scene_ == scene.id() &&
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

void SelectionController::validate_against(const scene::Storage &scene) noexcept {
    if (!entity_.has_value()) {
        return;
    }
    if (selected_scene_ != scene.id()) {
        clear();
        return;
    }
    const Result<const scene::EntityRecord *> record = scene.entity(entity_.value());
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

void SelectionController::set_enabled(bool enabled) noexcept {
    enabled_ = enabled;
}

bool SelectionController::enabled() const noexcept {
    return enabled_;
}

Result<void> SelectionController::set_settings(const SelectionSettings &settings) noexcept {
    if (!valid_settings(settings)) {
        return Error{ErrorCode::invalid_selection_settings,
                     "Selection settings require a finite non-negative click threshold and "
                     "highlight strength in [0, 1]"};
    }
    settings_ = settings;
    settings_.highlight_color = sanitized_color(settings.highlight_color);
    return {};
}

SelectionSettings SelectionController::settings() const noexcept {
    return settings_;
}

} // namespace elf3d::tools::selection
