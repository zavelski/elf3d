#ifndef ELF3D_SCENE_H
#define ELF3D_SCENE_H

#include <elf3d/assets.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/model_types.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace elf3d {

namespace scene {
class Access;
}

class Engine;

struct SceneStatistics {
    std::uint64_t entities = 0;
    std::uint64_t model_entities = 0;
    std::uint64_t mesh_assets = 0;
    std::uint64_t material_assets = 0;
    std::uint64_t primitives = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t triangles = 0;
    std::uint64_t image_assets = 0;
    std::uint64_t texture_assets = 0;
    std::uint64_t sampler_descriptions = 0;
    std::uint64_t decoded_image_bytes = 0;
    std::uint64_t materials_with_base_color_textures = 0;
    std::uint64_t materials_with_metallic_roughness_textures = 0;
    std::uint64_t materials_with_normal_textures = 0;
    std::uint64_t materials_with_occlusion_textures = 0;
    std::uint64_t materials_with_emissive_textures = 0;

    bool operator==(const SceneStatistics&) const = default;
};

struct EntityInfo {
    EntityId entity;
    std::optional<EntityId> parent;

    bool has_model = false;
    bool has_camera = false;
    bool local_visible = true;
    bool effective_visible = true;
    bool renderable = false;

    bool operator==(const EntityInfo&) const = default;
};

struct SceneHierarchyItem {
    EntityId entity;
    std::optional<EntityId> parent;

    std::uint32_t depth = 0;
    std::uint32_t child_count = 0;

    bool has_model = false;
    bool has_camera = false;
    bool local_visible = true;
    bool effective_visible = true;
    bool renderable = false;

    std::uint64_t traversal_index = 0;

    bool operator==(const SceneHierarchyItem&) const = default;
};

struct SceneHierarchyStatistics {
    std::uint64_t entities = 0;
    std::uint64_t root_entities = 0;
    std::uint64_t maximum_depth = 0;
    std::uint64_t locally_hidden_entities = 0;
    std::uint64_t effectively_hidden_entities = 0;
    std::uint64_t visible_renderable_entities = 0;

    bool operator==(const SceneHierarchyStatistics&) const = default;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class ELF3D_API SceneHierarchySnapshot final {
  public:
    SceneHierarchySnapshot() noexcept;
    ~SceneHierarchySnapshot() noexcept;

    SceneHierarchySnapshot(const SceneHierarchySnapshot&) = delete;
    SceneHierarchySnapshot& operator=(const SceneHierarchySnapshot&) = delete;
    SceneHierarchySnapshot(SceneHierarchySnapshot&&) noexcept;
    SceneHierarchySnapshot& operator=(SceneHierarchySnapshot&&) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Result<SceneHierarchyItem> item_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<std::string_view> name_at(std::size_t index) const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;

  private:
    friend class Scene;

    class Impl;
    explicit SceneHierarchySnapshot(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class ELF3D_API Scene final {
  private:
    class Impl;
    struct ConstructionKey final {};

  public:
    // Scene operations are single-threaded and must run on the host thread that
    // coordinates scene updates and rendering. The creating Engine must outlive
    // this Scene.
    ~Scene() noexcept;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    [[nodiscard]] SceneId id() const noexcept;

    [[nodiscard]] Result<EntityId> create_entity() noexcept;
    // Destruction recursively destroys every descendant of the entity.
    [[nodiscard]] Result<void> destroy_entity(EntityId entity) noexcept;
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent) noexcept;
    [[nodiscard]] Result<void> clear_parent(EntityId entity) noexcept;
    [[nodiscard]] Result<void> set_local_transform(EntityId entity,
                                                   const Transform& transform) noexcept;
    [[nodiscard]] Result<Transform> local_transform(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const Float4x4& matrix) noexcept;
    [[nodiscard]] Result<Float4x4> local_matrix(EntityId entity) const noexcept;
    // Names are copied as UTF-8. The returned view is invalidated by renaming
    // or destroying the entity or Scene.
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name) noexcept;
    [[nodiscard]] Result<std::string_view> entity_name(EntityId entity) const noexcept;
    [[nodiscard]] Result<EntityInfo> entity_info(EntityId entity) const noexcept;
    [[nodiscard]] Result<SceneHierarchySnapshot> hierarchy_snapshot() const noexcept;
    [[nodiscard]] SceneHierarchyStatistics hierarchy_statistics() const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;

    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView& data) noexcept;
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView& data) noexcept;
    [[nodiscard]] Result<Bounds3> mesh_bounds(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription& description) noexcept;
    [[nodiscard]] Result<TextureAssetHandle>
    create_texture(const TextureDescription& description) noexcept;
    [[nodiscard]] Result<MaterialHandle>
    create_material(const MaterialDescription& description) noexcept;
    [[nodiscard]] Result<void>
    set_material_description(MaterialHandle material,
                             const MaterialDescription& description) noexcept;
    [[nodiscard]] Result<MaterialDescription>
    material_description(MaterialHandle material) const noexcept;

    [[nodiscard]] Result<EntityId> create_model_entity(MeshHandle mesh,
                                                       MaterialHandle material) noexcept;
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId model_entity,
                         std::span<const ModelPrimitiveBinding> primitives) noexcept;
    [[nodiscard]] Result<EntityId>
    create_perspective_camera_entity(const PerspectiveCameraDescription& description) noexcept;
    // Camera operations require a live entity from this Scene containing a
    // perspective-camera component.
    [[nodiscard]] Result<PerspectiveCameraDescription>
    perspective_camera_description(EntityId camera_entity) const noexcept;
    [[nodiscard]] Result<void>
    set_perspective_camera_description(EntityId camera_entity,
                                       const PerspectiveCameraDescription& description) noexcept;

    [[nodiscard]] Result<void> set_entity_local_visibility(EntityId entity, bool visible) noexcept;
    [[nodiscard]] Result<bool> entity_local_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<bool> entity_effective_visibility(EntityId entity) const noexcept;
    [[nodiscard]] Result<void> show_entity_and_ancestors(EntityId entity) noexcept;
    [[nodiscard]] Result<void> show_all_entities() noexcept;
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;

    [[nodiscard]] std::optional<Bounds3> world_bounds() const noexcept;
    [[nodiscard]] std::optional<Bounds3> visible_bounds() const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    // Exports the canonical document retained by a loaded scene. Scene-created
    // compatibility assets and runtime-only visibility/tool state are not exported.
    [[nodiscard]] Result<void> export_loaded_document(std::string_view path_utf8) const noexcept;

    explicit Scene(ConstructionKey, std::unique_ptr<Impl> impl) noexcept;

  private:
    friend class Engine;
    friend class scene::Access;

    class ReleaseContext final {
      public:
        using ReleaseCallback = void (*)(std::uintptr_t context, SceneId scene) noexcept;

        ReleaseContext() noexcept = default;
        ReleaseContext(std::uintptr_t context, ReleaseCallback callback) noexcept;
        ReleaseContext(const ReleaseContext&) = delete;
        ReleaseContext& operator=(const ReleaseContext&) = delete;
        ReleaseContext(ReleaseContext&& other) noexcept;
        ReleaseContext& operator=(ReleaseContext&& other) noexcept;
        void release(SceneId scene) const noexcept;

      private:
        std::uintptr_t context_ = 0;
        ReleaseCallback callback_ = nullptr;
    };

    [[nodiscard]] static Result<std::unique_ptr<Scene>>
    create(std::uint64_t engine_token, std::uint64_t scene_value,
           ReleaseContext release_context) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
