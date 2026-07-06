module;

#include <elf3d/core/assert.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.scene;

import elf.assets;
import elf.math;

namespace elf3d::scene {

bool valid_camera_description(const PerspectiveCameraDescription &description) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    return std::isfinite(description.vertical_field_of_view_radians) &&
           std::isfinite(description.near_plane) && std::isfinite(description.far_plane) &&
           description.vertical_field_of_view_radians > 0.0F &&
           description.vertical_field_of_view_radians < pi && description.near_plane > 0.0F &&
           description.far_plane > description.near_plane;
}

Storage::Storage(SceneId id) noexcept : id_(id), assets_(id) {}

SceneId Storage::id() const noexcept {
    return id_;
}

std::uint64_t Storage::revision() const noexcept {
    return revision_;
}

std::uint64_t Storage::hierarchy_revision() const noexcept {
    return hierarchy_revision_;
}

std::uint64_t Storage::visibility_revision() const noexcept {
    return visibility_revision_;
}

std::span<const std::optional<EntityRecord>> Storage::entities() const noexcept {
    return entities_;
}

const assets::Storage &Storage::assets() const noexcept {
    return assets_;
}

Result<EntityId> Storage::create_entity() {
    if (entities_.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_entity, "The scene entity identifier space is exhausted"};
    }

    const EntityId id = detail::SceneHandleAccess::create_entity(id_, entities_.size() + 1);
    entities_.push_back(EntityRecord{.id = id});
    increment_revision();
    increment_hierarchy_revision();
    return id;
}

Result<void> Storage::destroy_entity(EntityId entity_id) {
    const Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->parent.has_value()) {
        remove_child(record.value()->parent.value(), entity_id);
    }
    destroy_subtree(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::set_parent(EntityId entity_id, EntityId parent_id) {
    Result<EntityRecord *> child = mutable_entity(entity_id);
    if (!child) {
        return child.error();
    }
    Result<EntityRecord *> parent = mutable_entity(parent_id);
    if (!parent) {
        return parent.error();
    }
    if (entity_id == parent_id) {
        return Error{ErrorCode::invalid_parent_assignment, "An entity cannot be its own parent"};
    }
    if (child.value()->parent == parent_id) {
        return {};
    }

    std::optional<EntityId> ancestor = parent_id;
    while (ancestor.has_value()) {
        if (ancestor.value() == entity_id) {
            return Error{ErrorCode::hierarchy_cycle,
                         "The parent assignment would create a scene hierarchy cycle"};
        }
        const Result<const EntityRecord *> ancestor_record = entity(ancestor.value());
        if (!ancestor_record) {
            return ancestor_record.error();
        }
        ancestor = ancestor_record.value()->parent;
    }

    parent.value()->children.push_back(entity_id);

    if (child.value()->parent.has_value()) {
        remove_child(child.value()->parent.value(), entity_id);
    }
    child.value()->parent = parent_id;
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::clear_parent(EntityId entity_id) {
    Result<EntityRecord *> child = mutable_entity(entity_id);
    if (!child) {
        return child.error();
    }
    if (!child.value()->parent.has_value()) {
        return {};
    }
    remove_child(child.value()->parent.value(), entity_id);
    child.value()->parent.reset();
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::set_local_transform(EntityId entity_id, const Transform &transform) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!math::is_valid_transform(transform)) {
        return Error{
            ErrorCode::invalid_argument,
            "Entity transforms require finite values, a nonzero quaternion, and nonzero scale"};
    }
    record.value()->local_transform = math::normalized_transform(transform);
    record.value()->local_matrix = math::transform_matrix(record.value()->local_transform.value());
    increment_revision();
    return {};
}

Result<Transform> Storage::local_transform(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!record.value()->local_transform.has_value()) {
        return Error{ErrorCode::transform_requires_matrix_api,
                     "The entity uses an exact matrix transform; use local_matrix instead"};
    }
    return record.value()->local_transform.value();
}

Result<void> Storage::set_local_matrix(EntityId entity_id, const Float4x4 &matrix) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!math::is_valid_affine_matrix(matrix)) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "Entity matrices must be finite, affine, and invertible"};
    }
    record.value()->local_transform.reset();
    record.value()->local_matrix = matrix;
    increment_revision();
    return {};
}

Result<Float4x4> Storage::local_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_matrix;
}

Result<Float4x4> Storage::world_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    Float4x4 result = record.value()->local_matrix;
    std::optional<EntityId> parent = record.value()->parent;
    std::size_t visited = 0;
    while (parent.has_value()) {
        ELF3D_ASSERT(++visited <= entities_.size());
        const Result<const EntityRecord *> parent_record = entity(parent.value());
        if (!parent_record) {
            return parent_record.error();
        }
        result = math::compose_world(parent_record.value()->local_matrix, result);
        parent = parent_record.value()->parent;
    }
    return result;
}

Result<void> Storage::set_entity_name(EntityId entity_id, std::string_view name) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->name == name) {
        return {};
    }
    record.value()->name.assign(name);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<std::string_view> Storage::entity_name(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return std::string_view{record.value()->name};
}

Result<EntityInfo> Storage::entity_info(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    EntityInfo info;
    info.entity = record.value()->id;
    info.parent = record.value()->parent;
    info.has_model = record.value()->model.has_value();
    info.has_camera = record.value()->camera.has_value();
    info.local_visible = record.value()->local_visible;
    info.effective_visible = record.value()->effective_visible;
    info.renderable = record.value()->model.has_value();
    return info;
}

Result<MeshHandle> Storage::create_mesh(const MeshDataView &data) {
    Result<MeshHandle> result = assets_.create_mesh(data);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MeshHandle> Storage::create_mesh(const TexturedMeshDataView &data) {
    Result<MeshHandle> result = assets_.create_mesh(data);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<Bounds3> Storage::mesh_bounds(MeshHandle mesh_handle) const noexcept {
    const Result<const assets::MeshAsset *> result = assets_.mesh(mesh_handle);
    if (!result) {
        return result.error();
    }
    return result.value()->bounds;
}

Result<ImageHandle> Storage::create_image(const ImageDescription &description) {
    Result<ImageHandle> result = assets_.create_image(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<TextureAssetHandle> Storage::create_texture(const TextureDescription &description) {
    Result<TextureAssetHandle> result = assets_.create_texture(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MaterialHandle> Storage::create_material(const MaterialDescription &description) {
    Result<MaterialHandle> result = assets_.create_material(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<void> Storage::set_material(MaterialHandle material_handle,
                                   const MaterialDescription &description) {
    Result<void> result = assets_.set_material(material_handle, description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MaterialDescription> Storage::material(MaterialHandle material_handle) const noexcept {
    const Result<const assets::MaterialAsset *> result = assets_.material(material_handle);
    if (!result) {
        return result.error();
    }
    return result.value()->description;
}

Result<EntityId> Storage::create_model(MeshHandle mesh, MaterialHandle material) {
    Result<EntityId> entity_result = create_entity();
    if (!entity_result) {
        return entity_result.error();
    }
    const std::array<ModelPrimitiveBinding, 1> primitives{{{mesh, material}}};
    const Result<void> model_result = set_model_primitives(entity_result.value(), primitives);
    if (!model_result) {
        const Result<void> destroy_result = destroy_entity(entity_result.value());
        ELF3D_ASSERT(destroy_result.has_value());
        return model_result.error();
    }
    return entity_result.value();
}

Result<void> Storage::set_model_primitives(EntityId entity_id,
                                           std::span<const ModelPrimitiveBinding> primitives) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (primitives.empty()) {
        return Error{ErrorCode::invalid_argument,
                     "A model requires at least one mesh/material primitive binding"};
    }
    for (const ModelPrimitiveBinding &primitive : primitives) {
        const Result<const assets::MeshAsset *> mesh_result = assets_.mesh(primitive.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset *> material_result =
            assets_.material(primitive.material);
        if (!material_result) {
            return material_result.error();
        }
    }

    ModelComponent model;
    model.primitives.assign(primitives.begin(), primitives.end());
    record.value()->model = std::move(model);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<EntityId>
Storage::create_perspective_camera(const PerspectiveCameraDescription &description) {
    if (!valid_camera_description(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }

    Result<EntityId> entity_result = create_entity();
    if (!entity_result) {
        return entity_result.error();
    }
    Result<EntityRecord *> record = mutable_entity(entity_result.value());
    record.value()->camera = description;
    increment_revision();
    increment_hierarchy_revision();
    return entity_result.value();
}

Result<void> Storage::attach_perspective_camera(EntityId entity_id,
                                                const PerspectiveCameraDescription &description) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!valid_camera_description(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }
    record.value()->camera = description;
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<PerspectiveCameraDescription>
Storage::perspective_camera(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!record.value()->camera.has_value()) {
        return Error{ErrorCode::entity_has_no_camera,
                     "The entity does not contain a perspective camera component"};
    }
    return record.value()->camera.value();
}

Result<void> Storage::set_perspective_camera(EntityId entity_id,
                                             const PerspectiveCameraDescription &description) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!record.value()->camera.has_value()) {
        return Error{ErrorCode::entity_has_no_camera,
                     "The entity does not contain a perspective camera component"};
    }
    if (!valid_camera_description(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }
    record.value()->camera = description;
    increment_revision();
    return {};
}

Result<void> Storage::set_entity_visible(EntityId entity_id, bool visible) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->local_visible == visible) {
        return {};
    }
    record.value()->local_visible = visible;
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_visibility_revision();
    return {};
}

Result<bool> Storage::entity_local_visibility(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_visible;
}

Result<bool> Storage::entity_effective_visibility(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->effective_visible;
}

Result<void> Storage::show_entity_and_ancestors(EntityId entity_id) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }

    std::vector<EntityId> path;
    EntityId current = entity_id;
    while (current.is_valid()) {
        path.push_back(current);
        const Result<const EntityRecord *> current_record = entity(current);
        if (!current_record) {
            return current_record.error();
        }
        if (!current_record.value()->parent.has_value()) {
            break;
        }
        current = current_record.value()->parent.value();
    }

    bool changed = false;
    for (const EntityId path_entity : path) {
        Result<EntityRecord *> current_record = mutable_entity(path_entity);
        if (!current_record) {
            return current_record.error();
        }
        if (!current_record.value()->local_visible) {
            current_record.value()->local_visible = true;
            changed = true;
        }
    }
    if (!changed) {
        return {};
    }

    update_effective_visibility_from(path.back());
    increment_revision();
    increment_visibility_revision();
    return {};
}

Result<void> Storage::show_all_entities() {
    bool changed = false;
    for (std::optional<EntityRecord> &record : entities_) {
        if (record.has_value() && !record->local_visible) {
            record->local_visible = true;
            changed = true;
        }
    }
    if (!changed) {
        return {};
    }
    update_all_effective_visibility();
    increment_revision();
    increment_visibility_revision();
    return {};
}

Result<const EntityRecord *> Storage::entity(EntityId entity_id) const noexcept {
    if (!owns(entity_id)) {
        return Error{ErrorCode::invalid_entity,
                     "The entity identifier is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(entity_id) - 1);
    if (index >= entities_.size() || !entities_[index].has_value()) {
        return Error{ErrorCode::invalid_entity, "The entity identifier is stale"};
    }
    return &entities_[index].value();
}

std::optional<Bounds3> Storage::world_bounds() const noexcept {
    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value() || !record->model.has_value()) {
            continue;
        }
        const Result<Float4x4> world_result = world_matrix(record->id);
        ELF3D_ASSERT(world_result.has_value());
        for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
            const Result<const assets::MeshAsset *> mesh_result = assets_.mesh(primitive.mesh);
            ELF3D_ASSERT(mesh_result.has_value());
            const Bounds3 &local = mesh_result.value()->bounds;
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
            for (const Float3 corner : corners) {
                const Float3 point = math::transform_point(world_result.value(), corner);
                ELF3D_ASSERT(math::is_finite(point));
                if (!result.has_value()) {
                    result = Bounds3{point, point};
                    continue;
                }
                result->minimum.x = std::min(result->minimum.x, point.x);
                result->minimum.y = std::min(result->minimum.y, point.y);
                result->minimum.z = std::min(result->minimum.z, point.z);
                result->maximum.x = std::max(result->maximum.x, point.x);
                result->maximum.y = std::max(result->maximum.y, point.y);
                result->maximum.z = std::max(result->maximum.z, point.z);
            }
        }
    }
    return result;
}

std::optional<Bounds3>
Storage::visible_world_bounds(const VisibilityFilter &filter) const noexcept {
    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value() || !record->model.has_value() ||
            !entity_visible_in_filter(*this, filter, record->id)) {
            continue;
        }
        const Result<Float4x4> world_result = world_matrix(record->id);
        ELF3D_ASSERT(world_result.has_value());
        for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
            const Result<const assets::MeshAsset *> mesh_result = assets_.mesh(primitive.mesh);
            ELF3D_ASSERT(mesh_result.has_value());
            const Bounds3 &local = mesh_result.value()->bounds;
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
            for (const Float3 corner : corners) {
                const Float3 point = math::transform_point(world_result.value(), corner);
                ELF3D_ASSERT(math::is_finite(point));
                if (!result.has_value()) {
                    result = Bounds3{point, point};
                    continue;
                }
                result->minimum.x = std::min(result->minimum.x, point.x);
                result->minimum.y = std::min(result->minimum.y, point.y);
                result->minimum.z = std::min(result->minimum.z, point.z);
                result->maximum.x = std::max(result->maximum.x, point.x);
                result->maximum.y = std::max(result->maximum.y, point.y);
                result->maximum.z = std::max(result->maximum.z, point.z);
            }
        }
    }
    return result;
}

SceneStatistics Storage::statistics() const noexcept {
    SceneStatistics result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value()) {
            continue;
        }
        ++result.entities;
        if (record->model.has_value()) {
            ++result.model_entities;
        }
    }

    result.mesh_assets = static_cast<std::uint64_t>(assets_.meshes().size());
    result.material_assets = static_cast<std::uint64_t>(assets_.materials().size());
    result.primitives = result.mesh_assets;
    for (const assets::MeshAsset &mesh : assets_.meshes()) {
        result.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
        result.indices += static_cast<std::uint64_t>(mesh.indices.size());
        result.triangles += static_cast<std::uint64_t>(mesh.indices.size() / 3);
    }
    result.image_assets = static_cast<std::uint64_t>(assets_.images().size());
    result.texture_assets = static_cast<std::uint64_t>(assets_.textures().size());
    const std::span<const assets::TextureAsset> textures = assets_.textures();
    for (std::size_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
        bool first_occurrence = true;
        for (std::size_t earlier_index = 0; earlier_index < texture_index; ++earlier_index) {
            if (textures[earlier_index].description.sampler ==
                textures[texture_index].description.sampler) {
                first_occurrence = false;
                break;
            }
        }
        if (first_occurrence) {
            ++result.sampler_descriptions;
        }
    }
    for (const assets::ImageAsset &image : assets_.images()) {
        result.decoded_image_bytes += static_cast<std::uint64_t>(image.pixels.size());
    }
    for (const assets::MaterialAsset &material : assets_.materials()) {
        if (material.description.base_color_texture.is_valid()) {
            ++result.materials_with_base_color_textures;
        }
        if (material.description.metallic_roughness_texture.is_valid()) {
            ++result.materials_with_metallic_roughness_textures;
        }
        if (material.description.normal_texture.is_valid()) {
            ++result.materials_with_normal_textures;
        }
        if (material.description.occlusion_texture.is_valid()) {
            ++result.materials_with_occlusion_textures;
        }
        if (material.description.emissive_texture.is_valid()) {
            ++result.materials_with_emissive_textures;
        }
    }
    return result;
}

SceneHierarchyStatistics Storage::hierarchy_statistics() const noexcept {
    SceneHierarchyStatistics result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value()) {
            continue;
        }
        ++result.entities;
        if (!record->local_visible) {
            ++result.locally_hidden_entities;
        }
        if (!record->effective_visible) {
            ++result.effectively_hidden_entities;
        }
        if (record->model.has_value() && record->effective_visible) {
            ++result.visible_renderable_entities;
        }
        if (!record->parent.has_value()) {
            ++result.root_entities;
        }
        std::uint64_t depth = 0;
        std::optional<EntityId> parent = record->parent;
        while (parent.has_value()) {
            ELF3D_ASSERT(depth < entities_.size());
            ++depth;
            const Result<const EntityRecord *> parent_record = entity(parent.value());
            ELF3D_ASSERT(parent_record.has_value());
            parent = parent_record.value()->parent;
        }
        result.maximum_depth = std::max(result.maximum_depth, depth);
    }
    return result;
}

Result<EntityRecord *> Storage::mutable_entity(EntityId entity_id) noexcept {
    if (!owns(entity_id)) {
        return Error{ErrorCode::invalid_entity,
                     "The entity identifier is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(entity_id) - 1);
    if (index >= entities_.size() || !entities_[index].has_value()) {
        return Error{ErrorCode::invalid_entity, "The entity identifier is stale"};
    }
    return &entities_[index].value();
}

bool Storage::owns(EntityId entity_id) const noexcept {
    return entity_id.is_valid() && detail::SceneHandleAccess::scene(entity_id) == id_;
}

void Storage::update_effective_visibility_from(EntityId entity_id) noexcept {
    ELF3D_ASSERT(mutable_entity(entity_id).has_value());
    std::vector<EntityId> stack{entity_id};
    while (!stack.empty()) {
        const EntityId current_id = stack.back();
        stack.pop_back();
        Result<EntityRecord *> current = mutable_entity(current_id);
        ELF3D_ASSERT(current.has_value());

        bool effective = current.value()->local_visible;
        if (current.value()->parent.has_value()) {
            const Result<const EntityRecord *> parent = entity(current.value()->parent.value());
            ELF3D_ASSERT(parent.has_value());
            effective = effective && parent.value()->effective_visible;
        }
        current.value()->effective_visible = effective;

        for (const EntityId child : current.value()->children) {
            stack.push_back(child);
        }
    }
}

void Storage::update_all_effective_visibility() noexcept {
    for (std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value()) {
            continue;
        }
        bool effective = record->local_visible;
        std::optional<EntityId> parent = record->parent;
        std::size_t visited = 0;
        while (parent.has_value()) {
            ELF3D_ASSERT(++visited <= entities_.size());
            const Result<const EntityRecord *> parent_record = entity(parent.value());
            ELF3D_ASSERT(parent_record.has_value());
            effective = effective && parent_record.value()->local_visible;
            parent = parent_record.value()->parent;
        }
        record->effective_visible = effective;
    }
}

void Storage::destroy_subtree(EntityId entity_id) noexcept {
    while (true) {
        Result<EntityRecord *> current = mutable_entity(entity_id);
        ELF3D_ASSERT(current.has_value());
        while (!current.value()->children.empty()) {
            const EntityId child = current.value()->children.back();
            current = mutable_entity(child);
            ELF3D_ASSERT(current.has_value());
        }
        const EntityId leaf = current.value()->id;
        if (current.value()->parent.has_value()) {
            remove_child(current.value()->parent.value(), leaf);
        }
        const std::size_t index =
            static_cast<std::size_t>(detail::SceneHandleAccess::value(leaf) - 1);
        entities_[index].reset();
        if (leaf == entity_id) {
            return;
        }
    }
}

void Storage::remove_child(EntityId parent_id, EntityId child) noexcept {
    Result<EntityRecord *> parent = mutable_entity(parent_id);
    if (!parent) {
        return;
    }
    auto &children = parent.value()->children;
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
}

void Storage::increment_revision() noexcept {
    ++revision_;
    if (revision_ == 0) {
        ++revision_;
    }
}

void Storage::increment_hierarchy_revision() noexcept {
    ++hierarchy_revision_;
    if (hierarchy_revision_ == 0) {
        ++hierarchy_revision_;
    }
}

void Storage::increment_visibility_revision() noexcept {
    ++visibility_revision_;
    if (visibility_revision_ == 0) {
        ++visibility_revision_;
    }
}

Result<VisibilityFilter> make_visibility_filter(const Storage &scene,
                                                std::optional<EntityId> isolated_root) {
    VisibilityFilter filter;
    filter.scene = scene.id();
    filter.hierarchy_revision = scene.hierarchy_revision();
    filter.visibility_revision = scene.visibility_revision();
    if (!isolated_root.has_value()) {
        return filter;
    }

    const Result<const EntityRecord *> root = scene.entity(isolated_root.value());
    if (!root) {
        return root.error();
    }

    filter.isolated_root = isolated_root.value();
    std::vector<EntityId> stack;
    stack.push_back(isolated_root.value());
    std::size_t visited = 0;
    while (!stack.empty()) {
        if (++visited > scene.entities().size()) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Viewport isolation traversal detected a hierarchy cycle"};
        }
        const EntityId current = stack.back();
        stack.pop_back();
        filter.isolated_entity_values.push_back(detail::SceneHandleAccess::value(current));
        const Result<const EntityRecord *> record = scene.entity(current);
        if (!record) {
            return record.error();
        }
        for (const EntityId child : record.value()->children) {
            stack.push_back(child);
        }
    }
    std::sort(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end());
    filter.isolated_entity_values.erase(
        std::unique(filter.isolated_entity_values.begin(), filter.isolated_entity_values.end()),
        filter.isolated_entity_values.end());
    return filter;
}

bool entity_visible_in_filter(const Storage &scene, const VisibilityFilter &filter,
                              EntityId entity_id) noexcept {
    const Result<const EntityRecord *> record = scene.entity(entity_id);
    if (!record || !record.value()->effective_visible) {
        return false;
    }
    if (!filter.isolated_root.has_value()) {
        return true;
    }
    if (filter.scene != scene.id()) {
        return false;
    }
    const std::uint64_t value = detail::SceneHandleAccess::value(entity_id);
    return std::binary_search(filter.isolated_entity_values.begin(),
                              filter.isolated_entity_values.end(), value);
}

} // namespace elf3d::scene
