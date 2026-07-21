#include <elf3d/model/detail/document_storage.h>

#include <memory>
#include <utility>

namespace elf3d {

using model::detail::copy_primitive_data;
using model::detail::finite;
using model::detail::primitive_bounds;
using model::detail::valid_filter;
using model::detail::valid_mag_filter;
using model::detail::valid_material_factors;
using model::detail::valid_material_mappings;
using model::detail::valid_perspective_camera;
using model::detail::valid_wrap;
using model::detail::validate_image_description;
using model::detail::validate_primitive_data;

Result<DocumentSceneId> Document::create_scene(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const DocumentSceneId id = model::detail::DocumentHandleAccess::create_scene(
        storage_->token(), static_cast<std::uint64_t>(storage_->scenes.size() + 1U));
    Storage::SceneRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->scenes.push_back(std::move(record));
    if (!storage_->default_scene.has_value()) {
        storage_->default_scene = id;
    }
    storage_->note_mutation();
    return id;
}

Result<NodeId> Document::create_node(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const NodeId id = model::detail::DocumentHandleAccess::create_node(
        storage_->token(), static_cast<std::uint64_t>(storage_->nodes.size() + 1U));
    Storage::NodeRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->nodes.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<MeshId> Document::create_mesh(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const MeshId id = model::detail::DocumentHandleAccess::create_mesh(
        storage_->token(), static_cast<std::uint64_t>(storage_->meshes.size() + 1U));
    Storage::MeshRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->meshes.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<MaterialId> Document::create_material(const ModelMaterialDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!storage_->valid_material_textures(description)) {
        return Error{ErrorCode::invalid_texture_id,
                     "Material texture identifiers must belong to the same document"};
    }
    if (!valid_material_factors(description) || !valid_material_mappings(description)) {
        return Error{ErrorCode::invalid_material_description,
                     "Material factors and texture mappings must be finite and valid"};
    }
    const MaterialId id = model::detail::DocumentHandleAccess::create_material(
        storage_->token(), static_cast<std::uint64_t>(storage_->materials.size() + 1U));
    storage_->materials.push_back(Storage::MaterialRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<ImageId> Document::create_image(const ModelImageDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const Result<void> validation = validate_image_description(description);
    if (!validation) {
        return validation.error();
    }
    const ImageId id = model::detail::DocumentHandleAccess::create_image(
        storage_->token(), static_cast<std::uint64_t>(storage_->images.size() + 1U));
    Storage::ImageRecord record;
    record.id = id;
    record.width = description.width;
    record.height = description.height;
    record.format = description.format;
    record.pixels.assign(description.pixels.begin(), description.pixels.end());
    record.source_mime_type = description.source_mime_type;
    record.source_bytes.assign(description.source_bytes.begin(), description.source_bytes.end());
    storage_->images.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<SamplerId> Document::create_sampler(const SamplerDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!valid_wrap(description.wrap_u) || !valid_wrap(description.wrap_v) ||
        !valid_filter(description.min_filter) || !valid_mag_filter(description.mag_filter)) {
        return Error{ErrorCode::invalid_sampler_description,
                     "Sampler wrap and filter values must be valid"};
    }
    const SamplerId id = model::detail::DocumentHandleAccess::create_sampler(
        storage_->token(), static_cast<std::uint64_t>(storage_->samplers.size() + 1U));
    storage_->samplers.push_back(Storage::SamplerRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<TextureId> Document::create_texture(const ModelTextureDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!storage_->image(description.image)) {
        return Error{ErrorCode::invalid_image_id, "Texture image must belong to the same document"};
    }
    if (!storage_->sampler(description.sampler)) {
        return Error{ErrorCode::invalid_sampler_id,
                     "Texture sampler must belong to the same document"};
    }
    const TextureId id = model::detail::DocumentHandleAccess::create_texture(
        storage_->token(), static_cast<std::uint64_t>(storage_->textures.size() + 1U));
    storage_->textures.push_back(Storage::TextureRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<PrimitiveId> Document::create_primitive(MeshId mesh_id, MaterialId material_id,
                                               const PrimitiveDataView& data) {
    const Result<void> validation = validate_primitive_data(data);
    if (!validation) {
        return validation.error();
    }
    return create_primitive(mesh_id, material_id, copy_primitive_data(data));
}

Result<PrimitiveId> Document::create_primitive(MeshId mesh_id, MaterialId material_id,
                                               PrimitiveData&& data) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_id, "Primitive creation requires a live document"};
    }
    Result<Storage::MeshRecord*> mesh_record = storage_->mutable_mesh(mesh_id);
    if (!mesh_record) {
        return mesh_record.error();
    }
    if (!storage_->material(material_id)) {
        return Error{ErrorCode::invalid_material_id,
                     "Primitive material must belong to the same document"};
    }
    const Result<void> validation = validate_primitive_data(data.view());
    if (!validation) {
        return validation.error();
    }
    const Result<Bounds3> bounds = primitive_bounds(data.view());
    if (!bounds) {
        return bounds.error();
    }
    const PrimitiveId id = model::detail::DocumentHandleAccess::create_primitive(
        storage_->token(), static_cast<std::uint64_t>(storage_->primitives.size() + 1U));
    Storage::PrimitiveRecord record;
    record.id = id;
    record.mesh = mesh_id;
    record.material = material_id;
    record.bounds = bounds.value();
    record.data = std::move(data);
    storage_->primitives.push_back(std::move(record));
    mesh_record.value()->primitives.push_back(id);
    storage_->update_mesh_bounds(*mesh_record.value());
    storage_->note_mutation();
    return id;
}

Result<void> Document::add_scene_root(DocumentSceneId scene_id, NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_document_scene_id, "The document is empty"};
    }
    Result<Storage::SceneRecord*> scene_record = storage_->mutable_scene(scene_id);
    if (!scene_record) {
        return scene_record.error();
    }
    const Result<const Storage::NodeRecord*> node_record = storage_->node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->parent.has_value()) {
        return Error{ErrorCode::invalid_parent_assignment,
                     "A scene root node cannot already have a parent"};
    }
    if (std::find(scene_record.value()->roots.begin(), scene_record.value()->roots.end(),
                  node_id) == scene_record.value()->roots.end()) {
        scene_record.value()->roots.push_back(node_id);
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::set_default_scene(DocumentSceneId scene_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_document_scene_id, "The document is empty"};
    }
    if (!storage_->scene(scene_id)) {
        return Error{ErrorCode::invalid_document_scene_id,
                     "The default scene must belong to the same document"};
    }
    if (storage_->default_scene != scene_id) {
        storage_->default_scene = scene_id;
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::clear_default_scene() noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_document_scene_id, "The document is empty"};
    }
    if (storage_->default_scene.has_value()) {
        storage_->default_scene.reset();
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::set_parent(NodeId node_id, NodeId parent_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    Result<Storage::NodeRecord*> child = storage_->mutable_node(node_id);
    if (!child) {
        return child.error();
    }
    Result<Storage::NodeRecord*> parent = storage_->mutable_node(parent_id);
    if (!parent) {
        return parent.error();
    }
    if (node_id == parent_id) {
        return Error{ErrorCode::invalid_parent_assignment, "A node cannot be its own parent"};
    }
    const Result<void> cycle_result = storage_->reject_parent_cycle(node_id, parent_id);
    if (!cycle_result) {
        return cycle_result.error();
    }
    if (child.value()->parent == parent_id) {
        return {};
    }
    storage_->detach_from_existing_parent(*child.value(), node_id);
    child.value()->parent = parent_id;
    parent.value()->children.push_back(node_id);
    storage_->remove_scene_root_entries(node_id);
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_parent(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->parent.has_value()) {
        return {};
    }
    Result<Storage::NodeRecord*> parent_record =
        storage_->mutable_node(*node_record.value()->parent);
    if (parent_record) {
        auto& siblings = parent_record.value()->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
    }
    node_record.value()->parent.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::set_node_mesh(NodeId node_id, MeshId mesh_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!storage_->mesh(mesh_id)) {
        return Error{ErrorCode::invalid_mesh_id, "Node mesh must belong to the same document"};
    }
    if (node_record.value()->mesh == mesh_id) {
        return {};
    }
    node_record.value()->mesh = mesh_id;
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_node_mesh(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->mesh.has_value()) {
        return {};
    }
    node_record.value()->mesh.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::set_node_matrix(NodeId node_id, const Float4x4& matrix) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    if (!finite(matrix)) {
        return Error{ErrorCode::invalid_transform_matrix, "Node matrices must be finite"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->local_matrix == matrix) {
        return {};
    }
    node_record.value()->local_matrix = matrix;
    storage_->note_mutation();
    return {};
}

Result<void>
Document::set_node_perspective_camera(NodeId node_id,
                                      const PerspectiveCameraDescription& description) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    if (!valid_perspective_camera(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->perspective_camera == description) {
        return {};
    }
    node_record.value()->perspective_camera = description;
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_node_perspective_camera(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->perspective_camera.has_value()) {
        return {};
    }
    node_record.value()->perspective_camera.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::replace_primitive(PrimitiveId primitive_id, const PrimitiveDataView& data) {
    const Result<void> validation = validate_primitive_data(data);
    if (!validation) {
        return validation.error();
    }
    return replace_primitive(primitive_id, copy_primitive_data(data));
}

Result<void> Document::replace_primitive(PrimitiveId primitive_id, PrimitiveData&& data) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_primitive_id, "The document is empty"};
    }
    Result<Storage::PrimitiveRecord*> primitive_record = storage_->mutable_primitive(primitive_id);
    if (!primitive_record) {
        return primitive_record.error();
    }
    const Result<void> validation = validate_primitive_data(data.view());
    if (!validation) {
        return validation.error();
    }
    const Result<Bounds3> bounds = primitive_bounds(data.view());
    if (!bounds) {
        return bounds.error();
    }
    primitive_record.value()->data = std::move(data);
    primitive_record.value()->bounds = bounds.value();
    Result<Storage::MeshRecord*> mesh_record =
        storage_->mutable_mesh(primitive_record.value()->mesh);
    if (mesh_record) {
        storage_->update_mesh_bounds(*mesh_record.value());
    }
    storage_->note_mutation();
    return {};
}

} // namespace elf3d
