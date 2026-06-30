#ifndef ELF3D_SCENE_H
#define ELF3D_SCENE_H

#include <elf3d/assets.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace elf3d {

namespace scene {
class Access;
}

class Engine;

struct PerspectiveCameraDescription {
    float vertical_field_of_view_radians = 1.0471975512F;
    float near_plane = 0.1F;
    float far_plane = 1000.0F;

    bool operator==(const PerspectiveCameraDescription &) const = default;
};

struct SceneLoadOptions {
    bool generate_missing_normals = true;
    bool import_node_names = true;
};

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

    bool operator==(const SceneStatistics &) const = default;
};

struct EntityInfo {
    EntityId entity;
    EntityId parent;

    bool has_model = false;
    bool has_camera = false;
    bool local_visible = true;
    bool effective_visible = true;
    bool renderable = false;

    bool operator==(const EntityInfo &) const = default;
};

struct SceneHierarchyItem {
    EntityId entity;
    EntityId parent;

    std::uint32_t depth = 0;
    std::uint32_t child_count = 0;

    bool has_model = false;
    bool has_camera = false;
    bool local_visible = true;
    bool effective_visible = true;
    bool renderable = false;

    std::uint64_t traversal_index = 0;

    bool operator==(const SceneHierarchyItem &) const = default;
};

struct SceneHierarchyStatistics {
    std::uint64_t entities = 0;
    std::uint64_t root_entities = 0;
    std::uint64_t maximum_depth = 0;
    std::uint64_t locally_hidden_entities = 0;
    std::uint64_t effectively_hidden_entities = 0;
    std::uint64_t visible_renderable_entities = 0;

    bool operator==(const SceneHierarchyStatistics &) const = default;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
class ELF3D_API SceneHierarchySnapshot final {
  public:
    SceneHierarchySnapshot();
    ~SceneHierarchySnapshot();

    SceneHierarchySnapshot(const SceneHierarchySnapshot &) = delete;
    SceneHierarchySnapshot &operator=(const SceneHierarchySnapshot &) = delete;
    SceneHierarchySnapshot(SceneHierarchySnapshot &&) noexcept;
    SceneHierarchySnapshot &operator=(SceneHierarchySnapshot &&) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] Result<SceneHierarchyItem> item(std::size_t index) const;
    [[nodiscard]] Result<std::string_view> name(std::size_t index) const;
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
  public:
    // Scene operations are single-threaded and must run on the host thread that
    // coordinates scene updates and rendering. The creating Engine must outlive
    // this Scene.
    ~Scene();

    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&) noexcept;
    Scene &operator=(Scene &&) noexcept;

    [[nodiscard]] SceneId id() const noexcept;

    [[nodiscard]] Result<EntityId> create_entity();
    // Destruction recursively destroys every descendant of the entity.
    [[nodiscard]] Result<void> destroy_entity(EntityId entity);
    [[nodiscard]] Result<void> set_parent(EntityId entity, EntityId parent);
    [[nodiscard]] Result<void> clear_parent(EntityId entity);
    [[nodiscard]] Result<void> set_local_transform(EntityId entity, const Transform &transform);
    [[nodiscard]] Result<Transform> local_transform(EntityId entity) const;
    [[nodiscard]] Result<void> set_local_matrix(EntityId entity, const Float4x4 &matrix);
    [[nodiscard]] Result<Float4x4> local_matrix(EntityId entity) const;
    // Names are copied as UTF-8. The returned view is invalidated by renaming
    // or destroying the entity or Scene.
    [[nodiscard]] Result<void> set_entity_name(EntityId entity, std::string_view name);
    [[nodiscard]] Result<std::string_view> entity_name(EntityId entity) const;
    [[nodiscard]] Result<EntityInfo> entity_info(EntityId entity) const;
    [[nodiscard]] Result<SceneHierarchySnapshot> hierarchy_snapshot() const;
    [[nodiscard]] SceneHierarchyStatistics hierarchy_statistics() const noexcept;
    [[nodiscard]] std::uint64_t hierarchy_revision() const noexcept;

    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView &data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView &data);
    [[nodiscard]] Result<Bounds3> mesh_bounds(MeshHandle mesh) const;
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription &description);
    [[nodiscard]] Result<TextureAssetHandle> create_texture(const TextureDescription &description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription &description);
    [[nodiscard]] Result<void> set_material(MaterialHandle material,
                                            const MaterialDescription &description);
    [[nodiscard]] Result<MaterialDescription> material(MaterialHandle material) const;

    [[nodiscard]] Result<EntityId> create_model(MeshHandle mesh, MaterialHandle material);
    [[nodiscard]] Result<void>
    set_model_primitives(EntityId entity, std::span<const ModelPrimitiveBinding> primitives);
    [[nodiscard]] Result<EntityId>
    create_perspective_camera(const PerspectiveCameraDescription &description);
    [[nodiscard]] Result<PerspectiveCameraDescription> perspective_camera(EntityId entity) const;
    [[nodiscard]] Result<void>
    set_perspective_camera(EntityId entity, const PerspectiveCameraDescription &description);

    [[nodiscard]] Result<void> set_entity_visible(EntityId entity, bool visible);
    [[nodiscard]] Result<bool> entity_local_visibility(EntityId entity) const;
    [[nodiscard]] Result<bool> entity_effective_visibility(EntityId entity) const;
    [[nodiscard]] Result<void> show_entity_and_ancestors(EntityId entity);
    [[nodiscard]] Result<void> show_all_entities();
    [[nodiscard]] std::uint64_t visibility_revision() const noexcept;

    [[nodiscard]] Bounds3 world_bounds() const noexcept;
    [[nodiscard]] Bounds3 visible_bounds() const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;

  private:
    friend class Engine;
    friend class scene::Access;

    class Impl;
    class ReleaseContext final {
      public:
        using ReleaseCallback = void (*)(const std::shared_ptr<void> &context,
                                         SceneId scene) noexcept;

        ReleaseContext(std::weak_ptr<void> context, ReleaseCallback callback) noexcept;
        void release(SceneId scene) const noexcept;

      private:
        std::weak_ptr<void> context_;
        ReleaseCallback callback_ = nullptr;
    };

    explicit Scene(std::unique_ptr<Impl> impl) noexcept;
    [[nodiscard]] static Result<std::unique_ptr<Scene>>
    create(std::uintptr_t engine_token, std::uint64_t scene_value,
           std::shared_ptr<ReleaseContext> release_context) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
