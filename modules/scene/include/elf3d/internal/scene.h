#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <elf3d/assets.h>
#include <elf3d/core/diagnostics.h>
#include <elf3d/core/result.h>
#include <elf3d/internal/assets.h>
#include <elf3d/internal/math.h>
#include <elf3d/internal/scene_runtime.h>
#include <elf3d/math/value_types.h>
#include <elf3d/model.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/surface_anchor.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace elf3d::scene {
struct ModelComponent {
    std::vector<ModelPrimitiveBinding> primitives;
    std::vector<PrimitiveId> document_primitives;
};
struct EntityRecord {
    EntityId id;
    std::optional<EntityId> parent;
    std::vector<EntityId> children;
    std::string name;
    std::optional<Transform> local_transform{Transform{}};
    Float4x4 local_matrix{};
    std::optional<ModelComponent> model;
    std::optional<PerspectiveCameraDescription> camera;
    bool local_visible = true;
    bool effective_visible = true;
    mutable Float4x4 cached_world_matrix{};
    mutable math::Matrix3x3 cached_normal_matrix{};
    mutable bool cached_orientation_reversed = false;
    mutable bool cached_render_transform_valid = false;
    mutable std::optional<Bounds3> cached_world_bounds;
    mutable std::vector<Bounds3> cached_primitive_world_bounds;
    mutable bool world_matrix_dirty = true;
    mutable bool world_bounds_dirty = true;
};
struct VisibilityFilter {
    SceneId scene;
    std::optional<EntityId> isolated_root;
    std::uint64_t hierarchy_revision = 0;
    std::uint64_t visibility_revision = 0;
    std::vector<std::uint64_t> isolated_entity_values;
    [[nodiscard]] bool has_isolation() const noexcept
    {
        return isolated_root.has_value();
    }
};
class Storage final {
  public:
    explicit Storage(SceneId id) noexcept;
    [[nodiscard]] SceneId id() const noexcept;
    [[nodiscard]] bool belongs_to_engine(std::uint64_t engine_token) const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::uint64_t render_content_revision() const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;
    [[nodiscard]] std::uint64_t model_spatial_revision() const noexcept;
    [[nodiscard]] std::span<const std::optional<EntityRecord>> entities() const noexcept;
    [[nodiscard]] const assets::Storage& assets() const noexcept;
    [[nodiscard]] DocumentView document() const noexcept;
    [[nodiscard]] bool has_document() const noexcept;
    [[nodiscard]] Result<void> set_document(Document&& document);
    [[nodiscard]] Result<EntityId> create_entity();
    [[nodiscard]] Result<void> destroy_entity(EntityId entity);
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent);
    [[nodiscard]] Result<void> clear_parent(EntityId entity);
    [[nodiscard]] Result<void> set_local_transform(EntityId entity, const Transform& transform);
    [[nodiscard]] Result<Transform> local_transform(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const Float4x4& matrix);
    [[nodiscard]] Result<Float4x4> local_matrix(EntityId entity) const noexcept;
    [[nodiscard]] Result<Float4x4> world_matrix(EntityId entity) const;
    [[nodiscard]] Result<math::Matrix3x3> world_normal_matrix(EntityId entity) const;
    [[nodiscard]] Result<bool> world_orientation_reversed(EntityId entity) const;
    [[nodiscard]] Result<Bounds3> primitive_world_bounds(EntityId entity,
                                                         std::uint32_t primitive) const;
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name);
    [[nodiscard]] Result<std::string_view> entity_name(EntityId entity) const noexcept;
    [[nodiscard]] Result<EntityInfo> entity_info(EntityId entity) const noexcept;
    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView& data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView& data);
    [[nodiscard]] Result<Bounds3> mesh_bounds(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription& description);
    [[nodiscard]] Result<TextureAssetHandle> create_texture(const TextureDescription& description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription& description);
    [[nodiscard]] Result<void> set_material(MaterialHandle material,
                                            const MaterialDescription& description);
    [[nodiscard]] Result<MaterialDescription> material(MaterialHandle material) const noexcept;
    [[nodiscard]] Result<EntityId> create_model(MeshHandle mesh, MaterialHandle material);
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId entity, std::span<const ModelPrimitiveBinding> primitives);
    [[nodiscard]] Result<void>
    set_model_document_primitives(EntityId entity,
                                  std::span<const PrimitiveId> document_primitives);
    [[nodiscard]] Result<RuntimePrimitiveView>
    runtime_primitive(EntityId entity, std::uint32_t primitive_index) const noexcept;
    [[nodiscard]] Result<SurfaceAnchor> create_surface_anchor(const PickHit& hit) const noexcept;
    [[nodiscard]] Result<ResolvedSurfaceAnchor>
    resolve_surface_anchor(const SurfaceAnchor& anchor) const;
    [[nodiscard]] Result<RuntimeTextureView>
    runtime_texture(const RuntimePrimitiveView& primitive,
                    RuntimeMaterialTextureSlot slot) const noexcept;
    [[nodiscard]] Result<EntityId>
    create_perspective_camera(const PerspectiveCameraDescription& description);
    [[nodiscard]] Result<void>
    attach_perspective_camera(EntityId entity, const PerspectiveCameraDescription& description);
    [[nodiscard]] Result<PerspectiveCameraDescription>
    perspective_camera(EntityId entity) const noexcept;
    [[nodiscard]] Result<void>
    set_perspective_camera(EntityId entity, const PerspectiveCameraDescription& description);

    [[nodiscard]] Result<void> set_entity_visible(EntityId entity, bool visible);
    [[nodiscard]] Result<bool> entity_local_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<bool> entity_effective_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> show_entity_and_ancestors(EntityId entity);
    [[nodiscard]] Result<void> show_all_entities();
    [[nodiscard]] Result<const EntityRecord*> entity(EntityId entity) const noexcept;
    [[nodiscard]] std::optional<Bounds3> world_bounds() const;
    [[nodiscard]] std::optional<Bounds3> visible_world_bounds(const VisibilityFilter& filter) const;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    [[nodiscard]] SceneHierarchyStatistics hierarchy_statistics() const noexcept;

  private:
    [[nodiscard]] Result<EntityRecord*> mutable_entity(EntityId entity) noexcept;
    [[nodiscard]] Result<void> prepare_document_groups(const PrimitiveView& view,
                                                       std::vector<std::size_t>& groups);
    [[nodiscard]] Result<std::vector<MeshHandle>> document_mesh_handles(PrimitiveId primitive);
    [[nodiscard]] bool is_document_mesh_handle(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<Bounds3> document_mesh_bounds(MeshHandle mesh) const noexcept;
    [[nodiscard]] std::optional<Bounds3> entity_world_bounds(const EntityRecord& entity) const;
    void update_entity_world_bounds(const EntityRecord& entity) const;
    [[nodiscard]] Result<RuntimePrimitiveView>
    document_runtime_primitive(const ModelPrimitiveBinding& binding,
                               PrimitiveId document_primitive) const noexcept;
    [[nodiscard]] Result<RuntimePrimitiveView>
    compatibility_runtime_primitive(const ModelPrimitiveBinding& binding) const noexcept;
    [[nodiscard]] bool owns(EntityId entity) const noexcept;
    void update_effective_visibility_from(EntityId entity);
    void update_all_effective_visibility() noexcept;
    void destroy_subtree(EntityId entity) noexcept;
    void remove_child(EntityId parent, EntityId child) noexcept;
    [[nodiscard]] bool invalidate_spatial_subtree(EntityId entity);
    void invalidate_all_model_bounds() noexcept;
    [[nodiscard]] bool visible_bounds_cache_matches(const VisibilityFilter& filter) const noexcept;
    void cache_visible_bounds(const VisibilityFilter& filter,
                              std::optional<Bounds3> bounds) const noexcept;
    void increment_revision() noexcept;
    void increment_render_content_revision() noexcept;
    void increment_hierarchy_revision() noexcept;
    void increment_visibility_revision() noexcept;
    void increment_model_spatial_revision() noexcept;

    SceneId id_;
    assets::Storage assets_;
    std::unique_ptr<Document> document_;
    struct DocumentRuntimePrimitive {
        PrimitiveId source;
        MaterialId material;
        Bounds3 bounds;
        std::optional<PrimitiveData> data;
    };
    std::vector<DocumentRuntimePrimitive> document_mesh_primitives_;
    std::vector<std::vector<std::size_t>> document_primitive_groups_;
    std::vector<std::optional<EntityRecord>> entities_;
    std::uint64_t revision_ = 0;
    std::uint64_t render_content_revision_ = 0;
    std::uint64_t hierarchy_revision_ = 0;
    std::uint64_t visibility_revision_ = 0;
    std::uint64_t model_spatial_revision_ = 0;
    mutable std::optional<Bounds3> cached_world_bounds_;
    mutable std::optional<Bounds3> cached_visible_world_bounds_;
    mutable std::optional<EntityId> cached_visible_isolated_root_;
    mutable std::uint64_t cached_world_bounds_spatial_revision_ = 0;
    mutable std::uint64_t cached_visible_spatial_revision_ = 0;
    mutable std::uint64_t cached_visible_hierarchy_revision_ = 0;
    mutable std::uint64_t cached_visible_visibility_revision_ = 0;
    mutable bool cached_world_bounds_valid_ = false;
    mutable bool cached_visible_world_bounds_valid_ = false;
};

class Access final {
  public:
    [[nodiscard]] static Storage* storage(Scene& scene) noexcept;
    [[nodiscard]] static const Storage* storage(const Scene& scene) noexcept;
};
[[nodiscard]] bool
valid_camera_description(const PerspectiveCameraDescription& description) noexcept;
[[nodiscard]] Result<VisibilityFilter>
make_visibility_filter(const Storage& scene, std::optional<EntityId> isolated_root);
[[nodiscard]] bool entity_visible_in_filter(const Storage& scene, const VisibilityFilter& filter,
                                            EntityId entity) noexcept;
[[nodiscard]] Result<void> populate_from_document(Document&& document,
                                                  DocumentSceneId default_scene, Storage& storage);

} // namespace elf3d::scene
