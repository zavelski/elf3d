#ifndef ELF3D_SCENE_IMPORT_BUILDER_H
#define ELF3D_SCENE_IMPORT_BUILDER_H

#include <elf3d/scene/storage.h>

namespace elf3d::scene {

// Narrow construction surface for validated importers. It preserves Storage
// validation and does not expose the scene's container layout.
class ImportBuilder final {
  public:
    explicit ImportBuilder(Storage &storage) noexcept;

    [[nodiscard]] Result<EntityId> create_entity();
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name);
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const math::Matrix4 &matrix);
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent);
    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView &data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView &data);
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription &description);
    [[nodiscard]] Result<TextureAssetHandle>
    create_texture(const TextureDescription &description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription &description);
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId entity, std::span<const ModelPrimitiveBinding> primitives);

  private:
    Storage *storage_ = nullptr;
};

} // namespace elf3d::scene

#endif
