module;

#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/scene.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module elf.scene;

import elf.assets;

export namespace elf3d::scene {

struct ModelComponent {
    std::vector<ModelPrimitiveBinding> primitives;
};

struct EntityRecord {
    EntityId id;
    std::optional<EntityId> parent;
    std::vector<EntityId> children;
    std::string name;
    std::optional<Transform> local_transform{Transform{}};
    Float4x4 local_matrix{};
    mutable Float4x4 world_matrix{};
    mutable bool world_dirty = true;
    std::optional<ModelComponent> model;
    std::optional<PerspectiveCameraDescription> camera;
    bool local_visible = true;
    bool effective_visible = true;
};

struct VisibilityFilter {
    SceneId scene;
    std::optional<EntityId> isolated_root;
    std::uint64_t hierarchy_revision = 0;
    std::uint64_t visibility_revision = 0;
    std::vector<std::uint64_t> isolated_entity_values;

    [[nodiscard]] bool has_isolation() const noexcept {
        return isolated_root.has_value();
    }
};

class Storage final {
  public:
    explicit Storage(SceneId id) noexcept;

    [[nodiscard]] SceneId id() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;
    [[nodiscard]] std::span<const std::optional<EntityRecord>> entities() const noexcept;
    [[nodiscard]] const assets::Storage &assets() const noexcept;

    [[nodiscard]] Result<EntityId> create_entity();
    [[nodiscard]] Result<void> destroy_entity(EntityId entity);
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent);
    [[nodiscard]] Result<void> clear_parent(EntityId entity);
    [[nodiscard]] Result<void> set_local_transform(EntityId entity, const Transform &transform);
    [[nodiscard]] Result<Transform> local_transform(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const Float4x4 &matrix);
    [[nodiscard]] Result<Float4x4> local_matrix(EntityId entity) const noexcept;
    [[nodiscard]] Result<Float4x4> world_matrix(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name);
    [[nodiscard]] Result<std::string_view> entity_name(EntityId entity) const noexcept;
    [[nodiscard]] Result<EntityInfo> entity_info(EntityId entity) const noexcept;

    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView &data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView &data);
    [[nodiscard]] Result<Bounds3> mesh_bounds(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription &description);
    [[nodiscard]] Result<TextureAssetHandle> create_texture(const TextureDescription &description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription &description);
    [[nodiscard]] Result<void> set_material(MaterialHandle material,
                                            const MaterialDescription &description);
    [[nodiscard]] Result<MaterialDescription> material(MaterialHandle material) const noexcept;

    [[nodiscard]] Result<EntityId> create_model(MeshHandle mesh, MaterialHandle material);
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId entity, std::span<const ModelPrimitiveBinding> primitives);
    [[nodiscard]] Result<EntityId>
    create_perspective_camera(const PerspectiveCameraDescription &description);
    [[nodiscard]] Result<void>
    attach_perspective_camera(EntityId entity, const PerspectiveCameraDescription &description);
    [[nodiscard]] Result<PerspectiveCameraDescription>
    perspective_camera(EntityId entity) const noexcept;
    [[nodiscard]] Result<void>
    set_perspective_camera(EntityId entity, const PerspectiveCameraDescription &description);

    [[nodiscard]] Result<void> set_entity_visible(EntityId entity, bool visible);
    [[nodiscard]] Result<bool> entity_local_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<bool> entity_effective_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> show_entity_and_ancestors(EntityId entity);
    [[nodiscard]] Result<void> show_all_entities();

    [[nodiscard]] Result<const EntityRecord *> entity(EntityId entity) const noexcept;
    [[nodiscard]] Bounds3 world_bounds() const noexcept;
    [[nodiscard]] Bounds3 visible_world_bounds(const VisibilityFilter &filter) const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    [[nodiscard]] SceneHierarchyStatistics hierarchy_statistics() const noexcept;

  private:
    [[nodiscard]] Result<EntityRecord *> mutable_entity(EntityId entity) noexcept;
    [[nodiscard]] bool owns(EntityId entity) const noexcept;
    void mark_world_dirty(EntityId entity) noexcept;
    void update_effective_visibility_from(EntityId entity) noexcept;
    void update_all_effective_visibility() noexcept;
    void destroy_recursive(EntityId entity) noexcept;
    void remove_child(EntityId parent, EntityId child) noexcept;
    void increment_revision() noexcept;
    void increment_hierarchy_revision() noexcept;
    void increment_visibility_revision() noexcept;

    SceneId id_;
    assets::Storage assets_;
    std::vector<std::optional<EntityRecord>> entities_;
    std::uint64_t revision_ = 0;
    std::uint64_t hierarchy_revision_ = 0;
    std::uint64_t visibility_revision_ = 0;
};

class Access final {
  public:
    [[nodiscard]] static Storage *storage(Scene &scene) noexcept;
    [[nodiscard]] static const Storage *storage(const Scene &scene) noexcept;
};

class ImportBuilder final {
  public:
    explicit ImportBuilder(Storage &storage) noexcept;

    [[nodiscard]] Result<EntityId> create_entity();
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name);
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const Float4x4 &matrix);
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent);
    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView &data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView &data);
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription &description);
    [[nodiscard]] Result<TextureAssetHandle> create_texture(const TextureDescription &description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription &description);
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId entity, std::span<const ModelPrimitiveBinding> primitives);
    [[nodiscard]] Result<void>
    set_perspective_camera(EntityId entity, const PerspectiveCameraDescription &description);

  private:
    Storage *storage_ = nullptr;
};

[[nodiscard]] bool
valid_camera_description(const PerspectiveCameraDescription &description) noexcept;
[[nodiscard]] Result<VisibilityFilter>
make_visibility_filter(const Storage &scene, std::optional<EntityId> isolated_root);
[[nodiscard]] bool entity_visible_in_filter(const Storage &scene, const VisibilityFilter &filter,
                                            EntityId entity) noexcept;

} // namespace elf3d::scene
