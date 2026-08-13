module;

#include <elf3d/clipping.h>
#include <elf3d/core/assert.h>

#include <algorithm>
#include <optional>
#include <span>

module elf.clipping.runtime;

import elf.clipping;
import elf.math;
import elf.scene;

namespace elf3d::clipping_runtime {
Result<void> ClippingController::set_section_plane(const SectionPlane& plane) noexcept {
    const Result<SectionPlane> normalized = elf3d::clipping::normalized_section_plane(plane);
    if (!normalized) {
        return normalized.error();
    }
    if (section_plane_ == plane) {
        return {};
    }
    section_plane_ = plane;
    increment_revision();
    return {};
}

void ClippingController::clear_section_plane() noexcept {
    if (!section_plane_.enabled) {
        return;
    }
    section_plane_.enabled = false;
    increment_revision();
}

Result<std::uint32_t> ClippingController::add_box(const ClippingBox& box) {
    if (box_count_ >= maximum_clipping_boxes) {
        return Error{ErrorCode::clipping_box_limit_exceeded,
                     "A viewport supports at most three clipping boxes"};
    }
    const Result<ClippingBox> validated = elf3d::clipping::validated_clipping_box(box);
    if (!validated) {
        return validated.error();
    }
    boxes_[box_count_] = box;
    const std::uint32_t index = box_count_;
    ++box_count_;
    increment_revision();
    return index;
}

Result<void> ClippingController::set_box(std::uint32_t index, const ClippingBox& box) noexcept {
    if (index >= box_count_) {
        return Error{ErrorCode::invalid_clipping_box_index,
                     "The clipping box index is outside the viewport box range"};
    }
    const Result<ClippingBox> validated = elf3d::clipping::validated_clipping_box(box);
    if (!validated) {
        return validated.error();
    }
    if (boxes_[index] == box) {
        return {};
    }
    boxes_[index] = box;
    increment_revision();
    return {};
}

Result<void> ClippingController::remove_box(std::uint32_t index) noexcept {
    if (index >= box_count_) {
        return Error{ErrorCode::invalid_clipping_box_index,
                     "The clipping box index is outside the viewport box range"};
    }
    for (std::uint32_t current = index; current + 1U < box_count_; ++current) {
        boxes_[current] = boxes_[current + 1U];
    }
    boxes_[box_count_ - 1U] = {};
    --box_count_;
    increment_revision();
    return {};
}

void ClippingController::clear_boxes() noexcept {
    if (box_count_ == 0) {
        return;
    }
    boxes_ = {};
    box_count_ = 0;
    increment_revision();
}

void ClippingController::clear() noexcept {
    const bool had_plane = section_plane_.enabled;
    const bool had_boxes = box_count_ != 0;
    if (!had_plane && !had_boxes) {
        return;
    }
    section_plane_.enabled = false;
    boxes_ = {};
    box_count_ = 0;
    increment_revision();
}

ClippingSnapshot ClippingController::snapshot() const noexcept {
    ClippingSnapshot result;
    result.section_plane = section_plane_;
    result.box_count = box_count_;
    for (std::uint32_t index = 0; index < box_count_; ++index) {
        result.boxes[index] = boxes_[index];
    }
    result.revision = revision_;
    return result;
}

Result<elf3d::clipping::ClippingFilter> ClippingController::filter() const {
    return elf3d::clipping::make_filter(
        section_plane_, std::span<const ClippingBox>{boxes_.data(), box_count_}, revision_);
}

std::uint64_t ClippingController::revision() const noexcept {
    return revision_;
}

void ClippingController::increment_revision() noexcept {
    ++revision_;
    if (revision_ == 0) {
        ++revision_;
    }
}

std::optional<Bounds3> visible_bounds(const scene::Storage& scene,
                                      const scene::VisibilityFilter& visibility,
                                      const elf3d::clipping::ClippingFilter& filter) noexcept {
    if (!filter.has_clipping()) {
        return scene.visible_world_bounds(visibility);
    }
    std::optional<Bounds3> result;
    for (const std::optional<scene::EntityRecord>& record : scene.entities()) {
        if (!record.has_value() || !record->model.has_value() ||
            !scene::entity_visible_in_filter(scene, visibility, record->id)) {
            continue;
        }
        for (std::uint32_t primitive_index = 0; primitive_index < record->model->primitives.size();
             ++primitive_index) {
            const Result<Bounds3> world_bounds =
                scene.primitive_world_bounds(record->id, primitive_index);
            ELF3D_ASSERT(world_bounds.has_value());
            const std::optional<Bounds3> clipped =
                elf3d::clipping::clipped_bounds(filter, world_bounds.value());
            if (!clipped.has_value()) {
                continue;
            }
            if (!result.has_value()) {
                result = clipped;
            } else {
                result->minimum.x = std::min(result->minimum.x, clipped->minimum.x);
                result->minimum.y = std::min(result->minimum.y, clipped->minimum.y);
                result->minimum.z = std::min(result->minimum.z, clipped->minimum.z);
                result->maximum.x = std::max(result->maximum.x, clipped->maximum.x);
                result->maximum.y = std::max(result->maximum.y, clipped->maximum.y);
                result->maximum.z = std::max(result->maximum.z, clipped->maximum.z);
            }
        }
    }
    return result;
}

} // namespace elf3d::clipping_runtime
