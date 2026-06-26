module;

#include <span>
#include <string_view>

module elf.scene;

namespace elf3d::scene {

ImportBuilder::ImportBuilder(Storage &storage) noexcept : storage_(&storage) {}

Result<EntityId> ImportBuilder::create_entity() {
    return storage_->create_entity();
}

Result<void> ImportBuilder::set_entity_name(EntityId entity, std::string_view name) {
    return storage_->set_entity_name(entity, name);
}

Result<void> ImportBuilder::set_local_matrix(EntityId entity, const Float4x4 &matrix) {
    return storage_->set_local_matrix(entity, matrix);
}

Result<void> ImportBuilder::set_parent(EntityId entity, EntityId parent) {
    return storage_->set_parent(entity, parent);
}

Result<MeshHandle> ImportBuilder::create_mesh(const MeshDataView &data) {
    return storage_->create_mesh(data);
}

Result<MeshHandle> ImportBuilder::create_mesh(const TexturedMeshDataView &data) {
    return storage_->create_mesh(data);
}

Result<ImageHandle> ImportBuilder::create_image(const ImageDescription &description) {
    return storage_->create_image(description);
}

Result<TextureAssetHandle>
ImportBuilder::create_texture(const TextureDescription &description) {
    return storage_->create_texture(description);
}

Result<MaterialHandle> ImportBuilder::create_material(const MaterialDescription &description) {
    return storage_->create_material(description);
}

Result<void>
ImportBuilder::set_model_primitives(EntityId entity,
                                    std::span<const ModelPrimitiveBinding> primitives) {
    return storage_->set_model_primitives(entity, primitives);
}

} // namespace elf3d::scene
