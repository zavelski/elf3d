#include <elf3d/scene/storage.h>

#include <elf3d/assets/handle_access.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

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

    try {
        const EntityId id = detail::SceneHandleAccess::create_entity(id_, entities_.size() + 1);
        entities_.push_back(EntityRecord{.id = id});
        increment_revision();
        increment_hierarchy_revision();
        return id;
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Entity creation failed while allocating storage"};
    }
}

Result<void> Storage::destroy_entity(EntityId entity_id) {
    const Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->parent.has_value()) {
        remove_child(record.value()->parent.value(), entity_id);
    }
    destroy_recursive(entity_id);
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

    try {
        parent.value()->children.push_back(entity_id);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Parent assignment failed while allocating child storage"};
    }

    if (child.value()->parent.has_value()) {
        remove_child(child.value()->parent.value(), entity_id);
    }
    child.value()->parent = parent_id;
    mark_world_dirty(entity_id);
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
    mark_world_dirty(entity_id);
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
    mark_world_dirty(entity_id);
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

Result<void> Storage::set_local_matrix(EntityId entity_id, const math::Matrix4 &matrix) {
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
    mark_world_dirty(entity_id);
    increment_revision();
    return {};
}

Result<math::Matrix4> Storage::local_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_matrix;
}

Result<math::Matrix4> Storage::world_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord *> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!record.value()->world_dirty) {
        return record.value()->world_matrix;
    }

    if (record.value()->parent.has_value()) {
        const Result<math::Matrix4> parent_world = world_matrix(record.value()->parent.value());
        if (!parent_world) {
            return parent_world.error();
        }
        record.value()->world_matrix =
            math::compose_world(parent_world.value(), record.value()->local_matrix);
    } else {
        record.value()->world_matrix = record.value()->local_matrix;
    }
    record.value()->world_dirty = false;
    return record.value()->world_matrix;
}

Result<void> Storage::set_entity_name(EntityId entity_id, std::string_view name) {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (record.value()->name == name) {
        return {};
    }
    try {
        record.value()->name.assign(name);
        increment_revision();
        increment_hierarchy_revision();
        return {};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Entity naming failed while copying UTF-8 text"};
    }
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
    info.parent = record.value()->parent.value_or(EntityId{});
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
        (void)destroy_result;
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

    try {
        ModelComponent model;
        model.primitives.assign(primitives.begin(), primitives.end());
        record.value()->model = std::move(model);
        increment_revision();
        increment_hierarchy_revision();
        return {};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Model creation failed while copying primitive bindings"};
    }
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
    try {
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
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Visibility update failed while allocating ancestor storage"};
    }

    bool changed = false;
    for (const EntityId current : path) {
        Result<EntityRecord *> current_record = mutable_entity(current);
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

Bounds3 Storage::world_bounds() const noexcept {
    Bounds3 result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value() || !record->model.has_value()) {
            continue;
        }
        const Result<math::Matrix4> world_result = world_matrix(record->id);
        if (!world_result) {
            continue;
        }
        const math::Matrix4 &world = world_result.value();
        for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
            const Result<const assets::MeshAsset *> mesh_result = assets_.mesh(primitive.mesh);
            if (!mesh_result || !mesh_result.value()->bounds.is_valid) {
                continue;
            }
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
                const math::Vector4 transformed =
                    world * math::Vector4{corner.x, corner.y, corner.z, 1.0F};
                const Float3 point{transformed.x, transformed.y, transformed.z};
                if (!math::is_finite(point)) {
                    return Bounds3{};
                }
                if (!result.is_valid) {
                    result.minimum = point;
                    result.maximum = point;
                    result.is_valid = true;
                    continue;
                }
                result.minimum.x = std::min(result.minimum.x, point.x);
                result.minimum.y = std::min(result.minimum.y, point.y);
                result.minimum.z = std::min(result.minimum.z, point.z);
                result.maximum.x = std::max(result.maximum.x, point.x);
                result.maximum.y = std::max(result.maximum.y, point.y);
                result.maximum.z = std::max(result.maximum.z, point.z);
            }
        }
    }
    return result;
}

Bounds3 Storage::visible_world_bounds(const VisibilityFilter &filter) const noexcept {
    Bounds3 result;
    for (const std::optional<EntityRecord> &record : entities_) {
        if (!record.has_value() || !record->model.has_value() ||
            !entity_visible_in_filter(*this, filter, record->id)) {
            continue;
        }
        const Result<math::Matrix4> world_result = world_matrix(record->id);
        if (!world_result) {
            continue;
        }
        const math::Matrix4 &world = world_result.value();
        for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
            const Result<const assets::MeshAsset *> mesh_result = assets_.mesh(primitive.mesh);
            if (!mesh_result || !mesh_result.value()->bounds.is_valid) {
                continue;
            }
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
                const math::Vector4 transformed =
                    world * math::Vector4{corner.x, corner.y, corner.z, 1.0F};
                const Float3 point{transformed.x, transformed.y, transformed.z};
                if (!math::is_finite(point)) {
                    return Bounds3{};
                }
                if (!result.is_valid) {
                    result.minimum = point;
                    result.maximum = point;
                    result.is_valid = true;
                    continue;
                }
                result.minimum.x = std::min(result.minimum.x, point.x);
                result.minimum.y = std::min(result.minimum.y, point.y);
                result.minimum.z = std::min(result.minimum.z, point.z);
                result.maximum.x = std::max(result.maximum.x, point.x);
                result.maximum.y = std::max(result.maximum.y, point.y);
                result.maximum.z = std::max(result.maximum.z, point.z);
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
    std::vector<SamplerDescription> samplers;
    samplers.reserve(assets_.textures().size());
    for (const assets::TextureAsset &texture : assets_.textures()) {
        if (std::find(samplers.begin(), samplers.end(), texture.description.sampler) ==
            samplers.end()) {
            samplers.push_back(texture.description.sampler);
        }
    }
    result.sampler_descriptions = static_cast<std::uint64_t>(samplers.size());
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
    }
    return result;
}

SceneHierarchyStatistics Storage::hierarchy_statistics() const noexcept {
    SceneHierarchyStatistics result;
    std::vector<EntityId> roots;
    try {
        roots.reserve(entities_.size());
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
                roots.push_back(record->id);
            }
        }

        result.root_entities = static_cast<std::uint64_t>(roots.size());
        struct Frame {
            EntityId entity;
            std::uint64_t depth = 0;
        };
        std::vector<Frame> stack;
        stack.reserve(roots.size());
        for (auto iterator = roots.rbegin(); iterator != roots.rend(); ++iterator) {
            stack.push_back(Frame{*iterator, 0});
        }
        while (!stack.empty()) {
            const Frame frame = stack.back();
            stack.pop_back();
            result.maximum_depth = std::max(result.maximum_depth, frame.depth);
            const Result<const EntityRecord *> record = entity(frame.entity);
            if (!record) {
                continue;
            }
            for (auto iterator = record.value()->children.rbegin();
                 iterator != record.value()->children.rend(); ++iterator) {
                stack.push_back(Frame{*iterator, frame.depth + 1});
            }
        }
    } catch (...) {
        return {};
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

void Storage::mark_world_dirty(EntityId entity_id) noexcept {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record || record.value()->world_dirty) {
        return;
    }
    record.value()->world_dirty = true;
    for (const EntityId child : record.value()->children) {
        mark_world_dirty(child);
    }
}

void Storage::update_effective_visibility_from(EntityId entity_id) noexcept {
    Result<EntityRecord *> first = mutable_entity(entity_id);
    if (!first) {
        return;
    }
    bool parent_effective = true;
    if (first.value()->parent.has_value()) {
        const Result<const EntityRecord *> parent = entity(first.value()->parent.value());
        if (!parent) {
            return;
        }
        parent_effective = parent.value()->effective_visible;
    }

    struct Frame {
        EntityId entity;
        bool parent_effective = true;
    };
    std::vector<Frame> stack;
    try {
        stack.push_back(Frame{entity_id, parent_effective});
    } catch (...) {
        return;
    }

    std::size_t visited = 0;
    while (!stack.empty()) {
        if (++visited > entities_.size()) {
            return;
        }
        const Frame frame = stack.back();
        stack.pop_back();
        Result<EntityRecord *> record = mutable_entity(frame.entity);
        if (!record) {
            continue;
        }
        const bool effective = record.value()->local_visible && frame.parent_effective;
        record.value()->effective_visible = effective;
        try {
            for (const EntityId child : record.value()->children) {
                stack.push_back(Frame{child, effective});
            }
        } catch (...) {
            return;
        }
    }
}

void Storage::update_all_effective_visibility() noexcept {
    for (const std::optional<EntityRecord> &record : entities_) {
        if (record.has_value() && !record->parent.has_value()) {
            update_effective_visibility_from(record->id);
        }
    }
}

void Storage::destroy_recursive(EntityId entity_id) noexcept {
    Result<EntityRecord *> record = mutable_entity(entity_id);
    if (!record) {
        return;
    }
    for (const EntityId child : record.value()->children) {
        destroy_recursive(child);
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(entity_id) - 1);
    entities_[index].reset();
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

    try {
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
            std::unique(filter.isolated_entity_values.begin(),
                        filter.isolated_entity_values.end()),
            filter.isolated_entity_values.end());
        return filter;
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport isolation filter construction failed while allocating storage"};
    }
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
