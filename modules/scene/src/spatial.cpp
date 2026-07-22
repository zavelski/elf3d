module;

#include <elf3d/core/assert.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

module elf.scene;

import elf.math;

namespace elf3d::scene {
namespace {

void expand(std::optional<Bounds3>& bounds, Float3 point) noexcept {
    if (!bounds.has_value()) {
        bounds = Bounds3{point, point};
        return;
    }
    bounds->minimum.x = std::min(bounds->minimum.x, point.x);
    bounds->minimum.y = std::min(bounds->minimum.y, point.y);
    bounds->minimum.z = std::min(bounds->minimum.z, point.z);
    bounds->maximum.x = std::max(bounds->maximum.x, point.x);
    bounds->maximum.y = std::max(bounds->maximum.y, point.y);
    bounds->maximum.z = std::max(bounds->maximum.z, point.z);
}

void expand(std::optional<Bounds3>& bounds, Bounds3 other) noexcept {
    expand(bounds, other.minimum);
    expand(bounds, other.maximum);
}

[[nodiscard]] Bounds3 transform_bounds(Bounds3 local, const Float4x4& world) noexcept {
    const std::array<Float3, 8> corners{{
        {local.minimum.x, local.minimum.y, local.minimum.z},
        {local.maximum.x, local.minimum.y, local.minimum.z},
        {local.minimum.x, local.maximum.y, local.minimum.z},
        {local.maximum.x, local.maximum.y, local.minimum.z},
        {local.minimum.x, local.minimum.y, local.maximum.z},
        {local.maximum.x, local.minimum.y, local.maximum.z},
        {local.minimum.x, local.maximum.y, local.maximum.z},
        {local.maximum.x, local.maximum.y, local.maximum.z},
    }};
    std::optional<Bounds3> result;
    for (const Float3 corner : corners) {
        expand(result, math::transform_point(world, corner));
    }
    ELF3D_ASSERT(result.has_value());
    return *result;
}

} // namespace

std::uint64_t Storage::model_spatial_revision() const noexcept {
    return model_spatial_revision_;
}

Result<Float4x4> Storage::world_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> target_result = entity(entity_id);
    if (!target_result) {
        return target_result.error();
    }
    const EntityRecord* current = target_result.value();
    if (!current->world_matrix_dirty) {
        return current->cached_world_matrix;
    }

    std::vector<const EntityRecord*> dirty_path;
    while (current->world_matrix_dirty) {
        dirty_path.push_back(current);
        if (!current->parent.has_value()) {
            current = nullptr;
            break;
        }
        const Result<const EntityRecord*> parent = entity(*current->parent);
        if (!parent) {
            return parent.error();
        }
        current = parent.value();
    }

    Float4x4 world = current != nullptr ? current->cached_world_matrix : Float4x4{};
    for (auto iterator = dirty_path.rbegin(); iterator != dirty_path.rend(); ++iterator) {
        const EntityRecord* record = *iterator;
        world = math::compose_world(world, record->local_matrix);
        record->cached_world_matrix = world;
        record->cached_render_transform_valid = false;
        record->world_matrix_dirty = false;
    }
    return target_result.value()->cached_world_matrix;
}

Result<math::Matrix3x3> Storage::world_normal_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    const Result<Float4x4> world = world_matrix(entity_id);
    if (!world) {
        return world.error();
    }
    if (!record.value()->cached_render_transform_valid) {
        const Result<math::Matrix3x3> normals = math::normal_matrix(world.value());
        const Result<bool> orientation = math::orientation_reversed(world.value());
        if (!normals) {
            return normals.error();
        }
        if (!orientation) {
            return orientation.error();
        }
        record.value()->cached_normal_matrix = normals.value();
        record.value()->cached_orientation_reversed = orientation.value();
        record.value()->cached_render_transform_valid = true;
    }
    return record.value()->cached_normal_matrix;
}

Result<bool> Storage::world_orientation_reversed(EntityId entity_id) const noexcept {
    const Result<math::Matrix3x3> normals = world_normal_matrix(entity_id);
    if (!normals) {
        return normals.error();
    }
    const Result<const EntityRecord*> record = entity(entity_id);
    ELF3D_ASSERT(record.has_value());
    return record.value()->cached_orientation_reversed;
}

void Storage::update_entity_world_bounds(const EntityRecord& record) const noexcept {
    record.cached_world_bounds.reset();
    record.cached_primitive_world_bounds.clear();
    if (!record.model.has_value()) {
        record.world_bounds_dirty = false;
        return;
    }
    const Result<Float4x4> world = world_matrix(record.id);
    ELF3D_ASSERT(world.has_value());
    record.cached_primitive_world_bounds.reserve(record.model->primitives.size());
    for (std::uint32_t index = 0; index < record.model->primitives.size(); ++index) {
        const Result<RuntimePrimitiveView> primitive = runtime_primitive(record.id, index);
        ELF3D_ASSERT(primitive.has_value());
        const Bounds3 bounds = transform_bounds(primitive.value().bounds, world.value());
        record.cached_primitive_world_bounds.push_back(bounds);
        expand(record.cached_world_bounds, bounds);
    }
    record.world_bounds_dirty = false;
}

std::optional<Bounds3> Storage::entity_world_bounds(const EntityRecord& record) const noexcept {
    if (record.world_bounds_dirty) {
        update_entity_world_bounds(record);
    }
    return record.cached_world_bounds;
}

Result<Bounds3> Storage::primitive_world_bounds(EntityId entity_id,
                                                std::uint32_t primitive) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->world_bounds_dirty) {
        update_entity_world_bounds(*record.value());
    }
    if (primitive >= record.value()->cached_primitive_world_bounds.size()) {
        return Error{ErrorCode::invalid_argument, "World bounds require a valid model primitive"};
    }
    return record.value()->cached_primitive_world_bounds[primitive];
}

std::optional<Bounds3> Storage::world_bounds() const noexcept {
    if (cached_world_bounds_valid_ &&
        cached_world_bounds_spatial_revision_ == model_spatial_revision_) {
        return cached_world_bounds_;
    }
    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord>& record : entities_) {
        if (record.has_value() && record->model.has_value()) {
            const std::optional<Bounds3> bounds = entity_world_bounds(*record);
            if (bounds.has_value()) {
                expand(result, *bounds);
            }
        }
    }
    cached_world_bounds_ = result;
    cached_world_bounds_spatial_revision_ = model_spatial_revision_;
    cached_world_bounds_valid_ = true;
    return result;
}

bool Storage::invalidate_spatial_subtree(EntityId entity_id) noexcept {
    bool affects_model = false;
    std::vector<EntityId> stack{entity_id};
    while (!stack.empty()) {
        const EntityId current_id = stack.back();
        stack.pop_back();
        const Result<EntityRecord*> current = mutable_entity(current_id);
        ELF3D_ASSERT(current.has_value());
        current.value()->world_matrix_dirty = true;
        current.value()->cached_render_transform_valid = false;
        current.value()->world_bounds_dirty = true;
        affects_model = affects_model || current.value()->model.has_value();
        stack.insert(stack.end(), current.value()->children.begin(),
                     current.value()->children.end());
    }
    return affects_model;
}

void Storage::invalidate_all_model_bounds() noexcept {
    for (std::optional<EntityRecord>& record : entities_) {
        if (record.has_value() && record->model.has_value()) {
            record->world_bounds_dirty = true;
        }
    }
}

void Storage::increment_model_spatial_revision() noexcept {
    ++model_spatial_revision_;
    if (model_spatial_revision_ == 0) {
        ++model_spatial_revision_;
    }
}

} // namespace elf3d::scene
