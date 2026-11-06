#ifndef ELF3D_SCENE_H
#define ELF3D_SCENE_H

#include <elf3d/assets.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/scene_load.h>
#include <elf3d/surface_anchor.h>

#include <cstddef>
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
class Scene;
struct PickHit;

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

// One canonical Document primitive instantiated by a runtime model entity.
// All material groups originating from that primitive share this identity.
struct PrimitiveInstance {
    EntityId entity;
    PrimitiveId primitive;

    bool operator==(const PrimitiveInstance&) const = default;
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
// Exclusive CPU-only payload of a retired Scene. Contains no Engine, renderer,
// graphics resource or live Scene owner. It may be moved to a CPU worker and
// destroyed there while the producing Elf3D/Model modules remain loaded.
class ELF3D_API SceneCpuRetirement final {
  public:
    SceneCpuRetirement() noexcept;
    ~SceneCpuRetirement() noexcept;
    SceneCpuRetirement(const SceneCpuRetirement&) = delete;
    SceneCpuRetirement& operator=(const SceneCpuRetirement&) = delete;
    SceneCpuRetirement(SceneCpuRetirement&&) noexcept;
    SceneCpuRetirement& operator=(SceneCpuRetirement&&) noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool has_document() const noexcept;
    // Optional synchronous recovery of the canonical owner; releases all other
    // retired CPU state on the calling thread. Error leaves this owner intact.
    [[nodiscard]] Result<Document> take_document() noexcept;

  private:
    friend class Scene;
    class Impl;
    explicit SceneCpuRetirement(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

struct ScenePreparationOptions {
    // Allocate headroom on the CPU preparation lane for application runtime
    // entities, such as a view camera. No canonical Document records are added.
    std::size_t additional_runtime_entities = 0;
};

// Exclusive CPU preparation with a reserved future Scene identity. Contains no
// live Scene, Engine observer, release ticket or graphics object. Reserve on the
// Engine thread, move to an application CPU lane, prepare there, then transfer
// back for installation. All operations require exclusive access to this owner.
class ELF3D_API PreparedSceneDocument final {
  public:
    PreparedSceneDocument() noexcept;
    ~PreparedSceneDocument() noexcept;
    PreparedSceneDocument(const PreparedSceneDocument&) = delete;
    PreparedSceneDocument& operator=(const PreparedSceneDocument&) = delete;
    PreparedSceneDocument(PreparedSceneDocument&&) noexcept;
    PreparedSceneDocument& operator=(PreparedSceneDocument&&) noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool ready() const noexcept;
    // Runs validation and all Document-to-runtime CPU derivation on the calling
    // thread. Success adopts the sole Document; an error preserves its owner,
    // payloads and IDs. A successful preparation cannot be overwritten.
    [[nodiscard]] Result<void> prepare(Document&& document, DocumentSceneId selected_scene,
                                       const ScenePreparationOptions& options = {}) noexcept;
    // Reads/parses/decodes and prepares on the calling CPU thread. A failure
    // leaves the reservation unused; no partial Document is published. Reports
    // retain the same supported-data diagnostics as Engine::load_scene.
    [[nodiscard]] Result<SceneLoadReport>
    load(std::string_view path_utf8, const ModelLoadOptions& options = {},
         const ScenePreparationOptions& preparation = {}) noexcept;
    // Operation-scoped observation on this owner's current thread only. The
    // view expires on recovery, installation or destruction, not on a move.
    [[nodiscard]] std::optional<DocumentView> document() const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    [[nodiscard]] std::optional<Bounds3> world_bounds() const noexcept;
    // Recovers the exact canonical owner and consumes this preparation. Derived
    // CPU data is released on the calling thread. Error preserves this owner.
    [[nodiscard]] Result<Document> take_document() noexcept;

  private:
    friend class Engine;
    friend class Scene;
    class Impl;
    explicit PreparedSceneDocument(std::unique_ptr<Impl> impl) noexcept;
    [[nodiscard]] static Result<PreparedSceneDocument> reserve(std::uint64_t engine_token,
                                                               std::uint64_t scene_value) noexcept;
    [[nodiscard]] bool belongs_to_engine(std::uint64_t engine_token) const noexcept;
    std::unique_ptr<Impl> impl_;
};

class ELF3D_API SceneHierarchySnapshot final {
  public:
    SceneHierarchySnapshot() noexcept;
    ~SceneHierarchySnapshot() noexcept;

    SceneHierarchySnapshot(const SceneHierarchySnapshot&) = delete;
    SceneHierarchySnapshot& operator=(const SceneHierarchySnapshot&) = delete;
    SceneHierarchySnapshot(SceneHierarchySnapshot&&) noexcept;
    SceneHierarchySnapshot& operator=(SceneHierarchySnapshot&&) noexcept;

    // Captures metadata on the Scene-owning thread, with no retained Scene
    // borrow or geometry/image payload copy. Creation reserves metadata only.
    [[nodiscard]] static Result<SceneHierarchySnapshot> create(const Scene& source) noexcept;
    // One unit advances a structural action or copies at most 1024 name bytes.
    // Changing hierarchy or visibility requires a fresh capture. Camera motion
    // does not invalidate capture. Results become readable only when ready.
    [[nodiscard]] Result<bool> advance(const Scene& source, std::size_t work_units = 256) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool matches_scene(const Scene& source) const noexcept;
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
    // Monotonically increases after any scene mutation, including camera changes.
    [[nodiscard]] std::uint64_t revision() const noexcept;

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
    // Authored identity is distinct from runtime entities. A valid inactive or
    // detached Document node has no runtime entity; runtime-only entities have
    // no Document node. Foreign/stale identifiers fail explicitly.
    [[nodiscard]] Result<std::optional<EntityId>>
    entity_for_document_node(NodeId node) const noexcept;
    [[nodiscard]] Result<std::optional<NodeId>> document_node(EntityId entity) const noexcept;
    // Runtime material grouping may split one canonical primitive. The index is
    // a runtime model binding index, never a glTF primitive table index.
    [[nodiscard]] Result<std::optional<PrimitiveId>>
    document_primitive(EntityId entity, std::uint32_t runtime_primitive_index) const noexcept;
    // Also validates the hit's runtime mesh binding before resolving provenance.
    [[nodiscard]] Result<std::optional<PrimitiveId>>
    document_primitive(const PickHit& hit) const noexcept;
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

    // Creates a stable Scene-owned triangle reference from a validated pick.
    // Resolution recomputes world values after transform changes and rejects
    // stale or foreign geometry.
    [[nodiscard]] Result<SurfaceAnchor> create_surface_anchor(const PickHit& hit) const noexcept;
    [[nodiscard]] Result<ResolvedSurfaceAnchor>
    resolve_surface_anchor(const SurfaceAnchor& anchor) const noexcept;

    [[nodiscard]] std::optional<Bounds3> world_bounds() const noexcept;
    [[nodiscard]] std::optional<Bounds3> visible_bounds() const noexcept;
    [[nodiscard]] SceneStatistics statistics() const noexcept;
    // Read-only canonical data; absent for procedural or retired Scenes. The
    // borrowed view and its children expire on take_document() or destruction.
    // Runtime entity/asset edits are not written back to this Document.
    [[nodiscard]] std::optional<DocumentView> document() const noexcept;
    // Transfers canonical ownership without copying and retires this Scene,
    // releasing its GPU/picking caches. All Scene IDs/handles/views are invalid
    // afterward; Document IDs survive. Error leaves the Scene unchanged.
    // Requires the creating Engine/context to be live on the owning thread.
    [[nodiscard]] Result<Document> take_document() noexcept;
    // On the framework/context thread, releases GPU/picking bindings and moves
    // all canonical/derived CPU storage without cloning payloads. This Scene
    // becomes retired; all its IDs/handles/views expire before return. The
    // returned owner has no context/thread affinity for CPU-only destruction.
    // Call only after the current frame's Scene observers have expired.
    // An error leaves the Scene unchanged. Graphics release is currently
    // synchronous.
    [[nodiscard]] Result<SceneCpuRetirement> retire_cpu_state() noexcept;
    // Exports the canonical document retained by a loaded/in-memory scene. Scene-created
    // compatibility assets and runtime-only visibility/tool state are not exported.
    // Use save_document with document() for the complete ModelWriteReport.
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
    [[nodiscard]] static Result<std::unique_ptr<Scene>>
    create(PreparedSceneDocument&& prepared, ReleaseContext release_context) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
