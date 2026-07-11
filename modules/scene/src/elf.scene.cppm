module;

#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module elf.scene;

import elf.assets;
import elf.core;
import elf.math;
import elf.model;

export namespace elf3d::scene {

struct ModelComponent {
    std::vector<ModelPrimitiveBinding> primitives;
    std::vector<PrimitiveId> document_primitives;
};

enum class RuntimeAlphaMode : std::uint8_t {
    opaque,
    mask,
    blend,
};

enum class RuntimeTextureWrap : std::uint8_t {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

enum class RuntimeTextureFilter : std::uint8_t {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};

enum class RuntimeMaterialTextureSlot : std::uint8_t {
    base_color,
    metallic_roughness,
    normal,
    occlusion,
    emissive,
};

struct RuntimeTextureTransform {
    Float2 offset;
    Float2 scale{1.0F, 1.0F};
    float rotation_radians = 0.0F;

    bool operator==(const RuntimeTextureTransform&) const = default;
};

struct RuntimeTextureMapping {
    std::uint32_t texcoord_set = 0;
    RuntimeTextureTransform transform;

    bool operator==(const RuntimeTextureMapping&) const = default;
};

struct RuntimeSamplerDescription {
    RuntimeTextureWrap wrap_u = RuntimeTextureWrap::repeat;
    RuntimeTextureWrap wrap_v = RuntimeTextureWrap::repeat;
    RuntimeTextureFilter min_filter = RuntimeTextureFilter::linear;
    RuntimeTextureFilter mag_filter = RuntimeTextureFilter::linear;

    bool operator==(const RuntimeSamplerDescription&) const = default;
};

struct RuntimeMaterialView {
    Color4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
    bool double_sided = false;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    bool unlit = false;
    RuntimeAlphaMode alpha_mode = RuntimeAlphaMode::opaque;
    float alpha_cutoff = 0.5F;
    Float3 emissive_factor;
    float normal_scale = 1.0F;
    float occlusion_strength = 1.0F;
    float ior = 1.5F;
    float specular_factor = 1.0F;
    Float3 specular_color_factor{1.0F, 1.0F, 1.0F};
    RuntimeTextureMapping base_color_texture_mapping;
    RuntimeTextureMapping metallic_roughness_texture_mapping;
    RuntimeTextureMapping normal_texture_mapping;
    RuntimeTextureMapping occlusion_texture_mapping;
    RuntimeTextureMapping emissive_texture_mapping;
    bool has_base_color_texture = false;
    bool has_metallic_roughness_texture = false;
    bool has_normal_texture = false;
    bool has_occlusion_texture = false;
    bool has_emissive_texture = false;

    [[nodiscard]] bool has_texture(RuntimeMaterialTextureSlot slot) const noexcept;
};

struct RuntimeTextureView {
    std::uint64_t texture_identity = 0;
    std::uint64_t image_identity = 0;
    bool document_image = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::span<const std::byte> pixels;
    RuntimeSamplerDescription sampler;
};

// A borrowed geometry/material view for one runtime model primitive. It
// resolves document data when available and otherwise exposes the legacy
// scene-created asset through the same geometry accessors. The view remains
// valid until its Storage is mutated or destroyed.
class RuntimePrimitiveView final {
  public:
    MeshHandle mesh;
    PrimitiveId document_primitive;
    Bounds3 bounds;
    RuntimeMaterialView material_view;

    [[nodiscard]] std::size_t vertex_count() const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;
    [[nodiscard]] Float3 position(std::size_t index) const noexcept;
    [[nodiscard]] Float3 normal(std::size_t index) const noexcept;
    [[nodiscard]] Float2 texcoord0(std::size_t index) const noexcept;
    [[nodiscard]] Float2 texcoord1(std::size_t index) const noexcept;
    [[nodiscard]] Color4 color(std::size_t index) const noexcept;

  private:
    friend class Storage;

    std::span<const VertexPositionNormalTexCoord> compatibility_vertices_;
    std::span<const Float3> document_positions_;
    std::span<const Float3> document_normals_;
    std::span<const Float2> document_texcoord0_;
    std::span<const Float2> document_texcoord1_;
    std::span<const Color4> document_colors_;
    std::span<const std::uint32_t> indices_;
    std::array<TextureAssetHandle, 5> compatibility_textures_;
    std::array<TextureId, 5> document_textures_;
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
    [[nodiscard]] bool belongs_to_engine(std::uintptr_t engine_token) const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;
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
    [[nodiscard]] Result<Float4x4> world_matrix(EntityId entity) const noexcept;
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
    [[nodiscard]] std::optional<Bounds3> world_bounds() const noexcept;
    [[nodiscard]] std::optional<Bounds3>
    visible_world_bounds(const VisibilityFilter& filter) const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    [[nodiscard]] SceneHierarchyStatistics hierarchy_statistics() const noexcept;

  private:
    [[nodiscard]] Result<EntityRecord*> mutable_entity(EntityId entity) noexcept;
    [[nodiscard]] Result<MeshHandle> document_mesh_handle(PrimitiveId primitive);
    [[nodiscard]] bool is_document_mesh_handle(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<Bounds3> document_mesh_bounds(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<RuntimePrimitiveView>
    document_runtime_primitive(const ModelPrimitiveBinding& binding,
                               PrimitiveId document_primitive) const noexcept;
    [[nodiscard]] Result<RuntimePrimitiveView>
    compatibility_runtime_primitive(const ModelPrimitiveBinding& binding) const noexcept;
    [[nodiscard]] bool owns(EntityId entity) const noexcept;
    void update_effective_visibility_from(EntityId entity) noexcept;
    void update_all_effective_visibility() noexcept;
    void destroy_subtree(EntityId entity) noexcept;
    void remove_child(EntityId parent, EntityId child) noexcept;
    void increment_revision() noexcept;
    void increment_hierarchy_revision() noexcept;
    void increment_visibility_revision() noexcept;

    SceneId id_;
    assets::Storage assets_;
    std::unique_ptr<Document> document_;
    std::vector<std::optional<PrimitiveId>> document_mesh_primitives_;
    std::vector<std::optional<EntityRecord>> entities_;
    std::uint64_t revision_ = 0;
    std::uint64_t hierarchy_revision_ = 0;
    std::uint64_t visibility_revision_ = 0;
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
