module;

#include <elf3d/core/assert.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.scene;

import elf.assets;
import elf.math;
import elf.model;

namespace elf3d::scene {
namespace {

constexpr std::uint64_t document_mesh_handle_marker = std::uint64_t{1} << 63U;
constexpr std::uint64_t document_mesh_handle_value_mask = ~document_mesh_handle_marker;

} // namespace

bool valid_camera_description(const PerspectiveCameraDescription& description) noexcept {
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

bool Storage::belongs_to_engine(std::uint64_t engine_token) const noexcept {
    return detail::SceneHandleAccess::engine_token(id_) == engine_token;
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

const assets::Storage& Storage::assets() const noexcept {
    return assets_;
}

DocumentView Storage::document() const noexcept {
    return document_ != nullptr ? document_->view() : DocumentView{};
}

bool Storage::has_document() const noexcept {
    return document_ != nullptr;
}

Result<void> Storage::set_document(Document&& document) {
    auto replacement = std::make_unique<Document>(std::move(document));
    document_mesh_primitives_.clear();
    document_ = std::move(replacement);
    increment_revision();
    return {};
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
    const Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    EntityRecord& target = *record.value();
    if (target.parent.has_value()) {
        remove_child(*target.parent, entity_id);
    }
    destroy_subtree(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::set_parent(EntityId entity_id, EntityId parent_id) {
    Result<EntityRecord*> child = mutable_entity(entity_id);
    if (!child) {
        return child.error();
    }
    Result<EntityRecord*> parent = mutable_entity(parent_id);
    if (!parent) {
        return parent.error();
    }
    if (entity_id == parent_id) {
        return Error{ErrorCode::invalid_parent_assignment, "An entity cannot be its own parent"};
    }
    EntityRecord& child_record = *child.value();
    EntityRecord& parent_record = *parent.value();
    if (child_record.parent == parent_id) {
        return {};
    }

    std::optional<EntityId> ancestor = parent_id;
    while (ancestor.has_value()) {
        const EntityId ancestor_id = *ancestor;
        if (ancestor_id == entity_id) {
            return Error{ErrorCode::hierarchy_cycle,
                         "The parent assignment would create a scene hierarchy cycle"};
        }
        const Result<const EntityRecord*> ancestor_result = entity(ancestor_id);
        if (!ancestor_result) {
            return ancestor_result.error();
        }
        ancestor = ancestor_result.value()->parent;
    }

    parent_record.children.push_back(entity_id);

    if (child_record.parent.has_value()) {
        remove_child(*child_record.parent, entity_id);
    }
    child_record.parent = parent_id;
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::clear_parent(EntityId entity_id) {
    Result<EntityRecord*> child = mutable_entity(entity_id);
    if (!child) {
        return child.error();
    }
    if (!child.value()->parent.has_value()) {
        return {};
    }
    remove_child(*child.value()->parent, entity_id);
    child.value()->parent.reset();
    update_effective_visibility_from(entity_id);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<void> Storage::set_local_transform(EntityId entity_id, const Transform& transform) {
    Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!math::is_valid_transform(transform)) {
        return Error{
            ErrorCode::invalid_argument,
            "Entity transforms require finite values, a nonzero quaternion, and nonzero scale"};
    }
    EntityRecord& target = *record.value();
    target.local_transform = math::normalized_transform(transform);
    ELF3D_ASSERT(target.local_transform.has_value());
    target.local_matrix = math::transform_matrix(*target.local_transform);
    increment_revision();
    return {};
}

Result<Transform> Storage::local_transform(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    const EntityRecord& target = *record.value();
    if (!target.local_transform.has_value()) {
        return Error{ErrorCode::transform_requires_matrix_api,
                     "The entity uses an exact matrix transform; use local_matrix instead"};
    }
    return *target.local_transform;
}

Result<void> Storage::set_local_matrix(EntityId entity_id, const Float4x4& matrix) {
    Result<EntityRecord*> record = mutable_entity(entity_id);
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
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return record.value()->local_matrix;
}

Result<Float4x4> Storage::world_matrix(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    const EntityRecord& target = *record.value();
    Float4x4 result = target.local_matrix;
    std::optional<EntityId> parent = target.parent;
    std::size_t visited = 0;
    while (parent.has_value()) {
        ELF3D_ASSERT(++visited <= entities_.size());
        const Result<const EntityRecord*> parent_record = entity(*parent);
        if (!parent_record) {
            return parent_record.error();
        }
        result = math::compose_world(parent_record.value()->local_matrix, result);
        parent = parent_record.value()->parent;
    }
    return result;
}

Result<void> Storage::set_entity_name(EntityId entity_id, std::string_view name) {
    Result<EntityRecord*> record = mutable_entity(entity_id);
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
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    return std::string_view{record.value()->name};
}

Result<EntityInfo> Storage::entity_info(EntityId entity_id) const noexcept {
    const Result<const EntityRecord*> record = entity(entity_id);
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

Result<MeshHandle> Storage::create_mesh(const MeshDataView& data) {
    Result<MeshHandle> result = assets_.create_mesh(data);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MeshHandle> Storage::create_mesh(const TexturedMeshDataView& data) {
    Result<MeshHandle> result = assets_.create_mesh(data);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<Bounds3> Storage::mesh_bounds(MeshHandle mesh_handle) const noexcept {
    if (is_document_mesh_handle(mesh_handle)) {
        return document_mesh_bounds(mesh_handle);
    }
    const Result<const assets::MeshAsset*> result = assets_.mesh(mesh_handle);
    if (!result) {
        return result.error();
    }
    return result.value()->bounds;
}

Result<ImageHandle> Storage::create_image(const ImageDescription& description) {
    Result<ImageHandle> result = assets_.create_image(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<TextureAssetHandle> Storage::create_texture(const TextureDescription& description) {
    Result<TextureAssetHandle> result = assets_.create_texture(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MaterialHandle> Storage::create_material(const MaterialDescription& description) {
    Result<MaterialHandle> result = assets_.create_material(description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<void> Storage::set_material(MaterialHandle material_handle,
                                   const MaterialDescription& description) {
    Result<void> result = assets_.set_material(material_handle, description);
    if (result) {
        increment_revision();
    }
    return result;
}

Result<MaterialDescription> Storage::material(MaterialHandle material_handle) const noexcept {
    const Result<const assets::MaterialAsset*> result = assets_.material(material_handle);
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
    Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (primitives.empty()) {
        return Error{ErrorCode::invalid_argument,
                     "A model requires at least one mesh/material primitive binding"};
    }
    for (const ModelPrimitiveBinding& primitive : primitives) {
        const Result<const assets::MeshAsset*> mesh_result = assets_.mesh(primitive.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset*> material_result =
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

Result<void>
Storage::set_model_document_primitives(EntityId entity_id,
                                       std::span<const PrimitiveId> document_primitives) {
    if (document_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "Document primitive bindings require a scene document"};
    }
    Result<EntityRecord*> record = mutable_entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (document_primitives.empty()) {
        return Error{ErrorCode::invalid_argument,
                     "A document model requires at least one primitive binding"};
    }
    ModelComponent model;
    model.primitives.reserve(document_primitives.size());
    model.document_primitives.reserve(document_primitives.size());
    for (const PrimitiveId primitive : document_primitives) {
        const Result<MeshHandle> mesh = document_mesh_handle(primitive);
        if (!mesh) {
            return mesh.error();
        }
        model.primitives.push_back(ModelPrimitiveBinding{mesh.value(), MaterialHandle{}});
        model.document_primitives.push_back(primitive);
    }
    record.value()->model = std::move(model);
    increment_revision();
    increment_hierarchy_revision();
    return {};
}

Result<MeshHandle> Storage::document_mesh_handle(PrimitiveId primitive) {
    if (document_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "A document mesh handle requires a scene document"};
    }
    const Result<PrimitiveView> view = document_->primitive(primitive);
    if (!view) {
        return view.error();
    }
    const std::uint64_t value = primitive.debug_value();
    if (value == 0 || value >= document_mesh_handle_marker ||
        value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The document primitive cannot receive a runtime mesh handle"};
    }
    const std::size_t index = static_cast<std::size_t>(value);
    if (index >= document_mesh_primitives_.size()) {
        document_mesh_primitives_.resize(index + 1U);
    }
    document_mesh_primitives_[index] = primitive;
    return detail::SceneHandleAccess::create_mesh(id_, document_mesh_handle_marker | value);
}

bool Storage::is_document_mesh_handle(MeshHandle mesh) const noexcept {
    return mesh.is_valid() && detail::SceneHandleAccess::scene(mesh) == id_ &&
           (detail::SceneHandleAccess::value(mesh) & document_mesh_handle_marker) != 0;
}

Result<Bounds3> Storage::document_mesh_bounds(MeshHandle mesh) const noexcept {
    if (document_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The document mesh handle is stale because the document is unavailable"};
    }
    const std::uint64_t value =
        detail::SceneHandleAccess::value(mesh) & document_mesh_handle_value_mask;
    if (value == 0 ||
        value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::invalid_mesh_handle, "The document mesh handle is invalid"};
    }
    const std::size_t index = static_cast<std::size_t>(value);
    if (index >= document_mesh_primitives_.size() ||
        !document_mesh_primitives_[index].has_value()) {
        return Error{ErrorCode::invalid_mesh_handle, "The document mesh handle is stale"};
    }
    const Result<PrimitiveView> primitive = document_->primitive(*document_mesh_primitives_[index]);
    if (!primitive) {
        return primitive.error();
    }
    return primitive.value().bounds;
}

Result<RuntimePrimitiveView>
Storage::runtime_primitive(EntityId entity_id, std::uint32_t primitive_index) const noexcept {
    const Result<const EntityRecord*> record_result = entity(entity_id);
    if (!record_result) {
        return record_result.error();
    }
    const std::optional<ModelComponent>& model = record_result.value()->model;
    if (!model.has_value()) {
        return Error{ErrorCode::invalid_argument,
                     "Runtime primitive access requires a model entity"};
    }
    if (static_cast<std::size_t>(primitive_index) >= model->primitives.size()) {
        return Error{ErrorCode::invalid_argument,
                     "Runtime primitive access refers to an invalid model primitive"};
    }

    const ModelPrimitiveBinding& binding = model->primitives[primitive_index];
    PrimitiveId document_primitive;
    if (static_cast<std::size_t>(primitive_index) < model->document_primitives.size()) {
        document_primitive = model->document_primitives[primitive_index];
    }
    if (document_primitive.is_valid()) {
        return document_runtime_primitive(binding, document_primitive);
    }

    return compatibility_runtime_primitive(binding);
}

Result<EntityId>
Storage::create_perspective_camera(const PerspectiveCameraDescription& description) {
    if (!valid_camera_description(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }

    Result<EntityId> entity_result = create_entity();
    if (!entity_result) {
        return entity_result.error();
    }
    Result<EntityRecord*> record = mutable_entity(entity_result.value());
    record.value()->camera = description;
    increment_revision();
    increment_hierarchy_revision();
    return entity_result.value();
}

Result<void> Storage::attach_perspective_camera(EntityId entity_id,
                                                const PerspectiveCameraDescription& description) {
    Result<EntityRecord*> record = mutable_entity(entity_id);
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
    const Result<const EntityRecord*> record = entity(entity_id);
    if (!record) {
        return record.error();
    }
    if (!record.value()->camera.has_value()) {
        return Error{ErrorCode::entity_has_no_camera,
                     "The entity does not contain a perspective camera component"};
    }
    return *record.value()->camera;
}

Result<void> Storage::set_perspective_camera(EntityId entity_id,
                                             const PerspectiveCameraDescription& description) {
    Result<EntityRecord*> record = mutable_entity(entity_id);
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

Result<const EntityRecord*> Storage::entity(EntityId entity_id) const noexcept {
    if (!owns(entity_id)) {
        return Error{ErrorCode::invalid_entity,
                     "The entity identifier is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(entity_id) - 1);
    if (index >= entities_.size() || !entities_[index].has_value()) {
        return Error{ErrorCode::invalid_entity, "The entity identifier is stale"};
    }
    return &*entities_[index];
}

std::optional<Bounds3> Storage::world_bounds() const noexcept {
    std::optional<Bounds3> result;
    for (const std::optional<EntityRecord>& record : entities_) {
        if (!record.has_value() || !record->model.has_value()) {
            continue;
        }
        expand_world_bounds(result, *record);
    }
    return result;
}

void Storage::expand_world_bounds(std::optional<Bounds3>& bounds,
                                  const EntityRecord& entity) const noexcept {
    ELF3D_ASSERT(entity.model.has_value());
    const Result<Float4x4> world_result = world_matrix(entity.id);
    ELF3D_ASSERT(world_result.has_value());
    for (std::uint32_t primitive_index = 0; primitive_index < entity.model->primitives.size();
         ++primitive_index) {
        const Result<RuntimePrimitiveView> primitive =
            runtime_primitive(entity.id, primitive_index);
        ELF3D_ASSERT(primitive.has_value());
        const Bounds3& local = primitive.value().bounds;
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
            if (!bounds.has_value()) {
                bounds = Bounds3{point, point};
                continue;
            }
            bounds->minimum.x = std::min(bounds->minimum.x, point.x);
            bounds->minimum.y = std::min(bounds->minimum.y, point.y);
            bounds->minimum.z = std::min(bounds->minimum.z, point.z);
            bounds->maximum.x = std::max(bounds->maximum.x, point.x);
            bounds->maximum.y = std::max(bounds->maximum.y, point.y);
            bounds->maximum.z = std::max(bounds->maximum.z, point.z);
        }
    }
}

SceneHierarchyStatistics Storage::hierarchy_statistics() const noexcept {
    SceneHierarchyStatistics result;
    for (const std::optional<EntityRecord>& record : entities_) {
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
            const Result<const EntityRecord*> parent_record = entity(*parent);
            ELF3D_ASSERT(parent_record.has_value());
            parent = parent_record.value()->parent;
        }
        result.maximum_depth = std::max(result.maximum_depth, depth);
    }
    return result;
}

Result<EntityRecord*> Storage::mutable_entity(EntityId entity_id) noexcept {
    if (!owns(entity_id)) {
        return Error{ErrorCode::invalid_entity,
                     "The entity identifier is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(entity_id) - 1);
    if (index >= entities_.size() || !entities_[index].has_value()) {
        return Error{ErrorCode::invalid_entity, "The entity identifier is stale"};
    }
    return &*entities_[index];
}

bool Storage::owns(EntityId entity_id) const noexcept {
    return entity_id.is_valid() && detail::SceneHandleAccess::scene(entity_id) == id_;
}

void Storage::destroy_subtree(EntityId entity_id) noexcept {
    while (true) {
        Result<EntityRecord*> current = mutable_entity(entity_id);
        ELF3D_ASSERT(current.has_value());
        while (!current.value()->children.empty()) {
            const EntityId child = current.value()->children.back();
            current = mutable_entity(child);
            ELF3D_ASSERT(current.has_value());
        }
        const EntityId leaf = current.value()->id;
        if (current.value()->parent.has_value()) {
            remove_child(*current.value()->parent, leaf);
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
    Result<EntityRecord*> parent = mutable_entity(parent_id);
    if (!parent) {
        return;
    }
    auto& children = parent.value()->children;
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

} // namespace elf3d::scene
