#include "prepared_scene_document.h"
#include "scene_cpu_retirement.h"
#include <core/exception_boundary.h>
#include <elf3d/core/assert.h>
#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <memory>
#include <optional>
#include <scene/scene.h>
#include <string_view>
#include <type_traits>
#include <utility>

namespace elf3d {

Scene::ReleaseContext::ReleaseContext(std::uintptr_t context, ReleaseCallback callback) noexcept
    : context_(context), callback_(callback)
{
}

Scene::ReleaseContext::ReleaseContext(ReleaseContext&& other) noexcept
    : context_(std::exchange(other.context_, 0)), callback_(std::exchange(other.callback_, nullptr))
{
}

Scene::ReleaseContext& Scene::ReleaseContext::operator=(ReleaseContext&& other) noexcept
{
    if (this != &other) {
        context_ = std::exchange(other.context_, 0);
        callback_ = std::exchange(other.callback_, nullptr);
    }
    return *this;
}

void Scene::ReleaseContext::release(SceneId scene) const noexcept
{
    if (callback_ == nullptr || context_ == 0) {
        return;
    }
    callback_(context_, scene);
}

class Scene::Impl final {
  public:
    Impl(SceneId id, ReleaseContext release_context) noexcept
        : storage(id), release_id(id), release_context(std::move(release_context))
    {
    }

    Impl(scene::Storage&& prepared, ReleaseContext release_context) noexcept
        : storage(std::move(prepared)), release_id(storage.id()),
          release_context(std::move(release_context))
    {
        static_assert(std::is_nothrow_move_constructible_v<scene::Storage>);
    }

    ~Impl()
    {
        release_context.release(release_id);
    }

    scene::Storage storage;
    SceneId release_id;
    ReleaseContext release_context;
};

Result<std::unique_ptr<Scene>> Scene::create(std::uint64_t engine_token, std::uint64_t scene_value,
                                             ReleaseContext release_context) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<std::unique_ptr<Scene>> {
        if (engine_token == 0 || scene_value == 0) {
            return Error{ErrorCode::invalid_argument,
                         "Scene creation requires a valid engine identity"};
        }

        const SceneId id = detail::SceneHandleAccess::create_scene(engine_token, scene_value);
        return std::make_unique<Scene>(ConstructionKey{},
                                       std::make_unique<Impl>(id, std::move(release_context)));
    });
}

Scene::Scene(ConstructionKey, std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
{
}
Scene::~Scene() noexcept = default;

Result<std::unique_ptr<Scene>> Scene::create(PreparedSceneDocument&& prepared,
                                             ReleaseContext release_context) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<std::unique_ptr<Scene>> {
        if (!prepared.ready()) {
            return Error{ErrorCode::invalid_argument,
                         "Scene installation requires completed CPU preparation"};
        }
        auto impl =
            std::make_unique<Impl>(std::move(*prepared.impl_->storage), std::move(release_context));
        prepared.impl_.reset();
        return std::make_unique<Scene>(ConstructionKey{}, std::move(impl));
    });
}

std::optional<DocumentView> Scene::document() const noexcept
{
    if (impl_ == nullptr || !impl_->storage.has_document()) {
        return std::nullopt;
    }
    return impl_->storage.document();
}

Result<Document> Scene::take_document() noexcept
{
    if (impl_ == nullptr || !impl_->storage.has_document()) {
        return Error{ErrorCode::invalid_argument, "The scene has no canonical document to take"};
    }
    Document result = impl_->storage.release_document();
    impl_.reset();
    return result;
}

Result<SceneCpuRetirement> Scene::retire_cpu_state() noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<SceneCpuRetirement> {
        if (impl_ == nullptr) {
            return Error{ErrorCode::invalid_argument, "The Scene is already retired"};
        }
        static_assert(std::is_nothrow_move_constructible_v<scene::Storage>);
        auto storage = std::make_unique<scene::Storage>(std::move(impl_->storage));
        auto retired = std::make_unique<SceneCpuRetirement::Impl>(std::move(storage));
        // The separate release identity retires graphics/picking independently
        // of the moved CPU state; it is never carried into the CPU-only owner.
        impl_.reset();
        return SceneCpuRetirement{std::move(retired)};
    });
}

SceneId Scene::id() const noexcept
{
    return impl_ != nullptr ? impl_->storage.id() : SceneId{};
}

std::uint64_t Scene::revision() const noexcept
{
    return impl_ != nullptr ? impl_->storage.revision() : 0;
}

Result<EntityId> Scene::create_entity() noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<EntityId> {
        return impl_ != nullptr
                   ? impl_->storage.create_entity()
                   : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::destroy_entity(EntityId entity) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.destroy_entity(entity)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::set_parent(EntityId entity, EntityId parent) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_parent(entity, parent)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::clear_parent(EntityId entity) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.clear_parent(entity)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::set_local_transform(EntityId entity, const Transform& transform) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_local_transform(entity, transform)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<Transform> Scene::local_transform(EntityId entity) const noexcept
{
    return impl_ != nullptr
               ? impl_->storage.local_transform(entity)
               : Result<Transform>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_local_matrix(EntityId entity, const Float4x4& matrix) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_local_matrix(entity, matrix)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<Float4x4> Scene::local_matrix(EntityId entity) const noexcept
{
    if (impl_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The scene is empty"};
    }
    return impl_->storage.local_matrix(entity);
}

Result<void> Scene::set_entity_name(EntityId entity, std::string_view name) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_entity_name(entity, name)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<std::string_view> Scene::entity_name(EntityId entity) const noexcept
{
    return impl_ != nullptr
               ? impl_->storage.entity_name(entity)
               : Result<std::string_view>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<EntityInfo> Scene::entity_info(EntityId entity) const noexcept
{
    return impl_ != nullptr
               ? impl_->storage.entity_info(entity)
               : Result<EntityInfo>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<std::optional<EntityId>> Scene::entity_for_document_node(NodeId node) const noexcept
{
    return impl_ ? impl_->storage.entity_for_document_node(node)
                 : Result<std::optional<EntityId>>{
                       Error{ErrorCode::invalid_node_id, "The scene is empty"}};
}

Result<std::optional<NodeId>> Scene::document_node(EntityId entity) const noexcept
{
    return impl_ ? impl_->storage.document_node(entity)
                 : Result<std::optional<NodeId>>{
                       Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<std::optional<PrimitiveId>>
Scene::document_primitive(EntityId entity, std::uint32_t runtime_primitive_index) const noexcept
{
    return impl_ ? impl_->storage.document_primitive(entity, runtime_primitive_index)
                 : Result<std::optional<PrimitiveId>>{
                       Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<std::optional<PrimitiveId>> Scene::document_primitive(const PickHit& hit) const noexcept
{
    return impl_ ? impl_->storage.document_primitive(hit)
                 : Result<std::optional<PrimitiveId>>{
                       Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<SceneHierarchySnapshot> Scene::hierarchy_snapshot() const noexcept
{
    auto capture = SceneHierarchySnapshot::create(*this);
    if (!capture) {
        return capture.error();
    }
    while (!capture.value().ready()) {
        if (const auto advanced = capture.value().advance(*this, 4096); !advanced) {
            return advanced.error();
        }
    }
    return std::move(capture).value();
}

SceneHierarchyStatistics Scene::hierarchy_statistics() const noexcept
{
    return impl_ != nullptr ? impl_->storage.hierarchy_statistics() : SceneHierarchyStatistics{};
}

std::uint64_t Scene::hierarchy_revision() const noexcept
{
    return impl_ != nullptr ? impl_->storage.hierarchy_revision() : 0;
}

Result<MeshHandle> Scene::create_mesh(const MeshDataView& data) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<MeshHandle> {
        return impl_ != nullptr ? impl_->storage.create_mesh(data)
                                : Result<MeshHandle>{
                                      Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
    });
}

Result<MeshHandle> Scene::create_mesh(const TexturedMeshDataView& data) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<MeshHandle> {
        return impl_ != nullptr ? impl_->storage.create_mesh(data)
                                : Result<MeshHandle>{
                                      Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
    });
}

Result<Bounds3> Scene::mesh_bounds(MeshHandle mesh) const noexcept
{
    return impl_ != nullptr
               ? impl_->storage.mesh_bounds(mesh)
               : Result<Bounds3>{Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
}

Result<ImageHandle> Scene::create_image(const ImageDescription& description) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<ImageHandle> {
        return impl_ != nullptr ? impl_->storage.create_image(description)
                                : Result<ImageHandle>{
                                      Error{ErrorCode::invalid_image_handle, "The scene is empty"}};
    });
}

Result<TextureAssetHandle> Scene::create_texture(const TextureDescription& description) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<TextureAssetHandle> {
        return impl_ != nullptr
                   ? impl_->storage.create_texture(description)
                   : Result<TextureAssetHandle>{
                         Error{ErrorCode::invalid_texture_asset_handle, "The scene is empty"}};
    });
}

Result<MaterialHandle> Scene::create_material(const MaterialDescription& description) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<MaterialHandle> {
        return impl_ != nullptr ? impl_->storage.create_material(description)
                                : Result<MaterialHandle>{Error{ErrorCode::invalid_material_handle,
                                                               "The scene is empty"}};
    });
}

Result<void> Scene::set_material_description(MaterialHandle material,
                                             const MaterialDescription& description) noexcept
{
    return impl_ != nullptr
               ? impl_->storage.set_material(material, description)
               : Result<void>{Error{ErrorCode::invalid_material_handle, "The scene is empty"}};
}

Result<MaterialDescription> Scene::material_description(MaterialHandle material) const noexcept
{
    return impl_ != nullptr ? impl_->storage.material(material)
                            : Result<MaterialDescription>{
                                  Error{ErrorCode::invalid_material_handle, "The scene is empty"}};
}

Result<EntityId> Scene::create_model_entity(MeshHandle mesh, MaterialHandle material) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<EntityId> {
        return impl_ != nullptr
                   ? impl_->storage.create_model(mesh, material)
                   : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::set_model_primitives(EntityId model_entity,
                                         std::span<const ModelPrimitiveBinding> primitives) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_model_primitives(model_entity, primitives)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<EntityId>
Scene::create_perspective_camera_entity(const PerspectiveCameraDescription& description) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<EntityId> {
        return impl_ != nullptr
                   ? impl_->storage.create_perspective_camera(description)
                   : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<PerspectiveCameraDescription>
Scene::perspective_camera_description(EntityId camera_entity) const noexcept
{
    return impl_ != nullptr ? impl_->storage.perspective_camera(camera_entity)
                            : Result<PerspectiveCameraDescription>{
                                  Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void>
Scene::set_perspective_camera_description(EntityId camera_entity,
                                          const PerspectiveCameraDescription& description) noexcept
{
    return impl_ != nullptr ? impl_->storage.set_perspective_camera(camera_entity, description)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_entity_local_visibility(EntityId entity, bool visible) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.set_entity_visible(entity, visible)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<bool> Scene::entity_local_visibility(EntityId entity) const noexcept
{
    return impl_ != nullptr ? impl_->storage.entity_local_visibility(entity)
                            : Result<bool>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<bool> Scene::entity_effective_visibility(EntityId entity) const noexcept
{
    return impl_ != nullptr ? impl_->storage.entity_effective_visibility(entity)
                            : Result<bool>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::show_entity_and_ancestors(EntityId entity) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        return impl_ != nullptr
                   ? impl_->storage.show_entity_and_ancestors(entity)
                   : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
    });
}

Result<void> Scene::show_all_entities() noexcept
{
    return impl_ != nullptr ? impl_->storage.show_all_entities()
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

std::uint64_t Scene::visibility_revision() const noexcept
{
    return impl_ != nullptr ? impl_->storage.visibility_revision() : 0;
}

Result<SurfaceAnchor> Scene::create_surface_anchor(const PickHit& hit) const noexcept
{
    return impl_ != nullptr
               ? impl_->storage.create_surface_anchor(hit)
               : Result<SurfaceAnchor>{Error{ErrorCode::invalid_surface_anchor_hit,
                                             "Surface anchor creation requires a live scene"}};
}

Result<ResolvedSurfaceAnchor>
Scene::resolve_surface_anchor(const SurfaceAnchor& anchor) const noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<ResolvedSurfaceAnchor> {
        return impl_ != nullptr ? impl_->storage.resolve_surface_anchor(anchor)
                                : Result<ResolvedSurfaceAnchor>{
                                      Error{ErrorCode::invalid_surface_anchor,
                                            "Surface anchor resolution requires a live scene"}};
    });
}

std::optional<Bounds3> Scene::world_bounds() const noexcept
{
    return detail::fatal_exception_boundary([&]() -> std::optional<Bounds3> {
        ELF3D_ASSERT(impl_ != nullptr);
        return impl_->storage.world_bounds();
    });
}

std::optional<Bounds3> Scene::visible_bounds() const noexcept
{
    return detail::fatal_exception_boundary([&]() -> std::optional<Bounds3> {
        ELF3D_ASSERT(impl_ != nullptr);
        const Result<scene::VisibilityFilter> filter =
            scene::make_visibility_filter(impl_->storage, std::nullopt);
        ELF3D_ASSERT(filter.has_value());
        return impl_->storage.visible_world_bounds(filter.value());
    });
}

SceneStatistics Scene::statistics() const noexcept
{
    return impl_ != nullptr ? impl_->storage.statistics() : SceneStatistics{};
}

Result<void> Scene::export_loaded_document(std::string_view path_utf8) const noexcept
{
    if (impl_ == nullptr || !impl_->storage.has_document()) {
        return Error{ErrorCode::invalid_argument,
                     "Only a scene loaded from a model document can export its loaded document"};
    }

    const Result<ModelWriteReport> saved = save_document(path_utf8, impl_->storage.document());
    if (!saved) {
        return saved.error();
    }
    return {};
}

const scene::Storage* scene::Access::storage(const Scene& scene) noexcept
{
    return scene.impl_ != nullptr ? &scene.impl_->storage : nullptr;
}

scene::Storage* scene::Access::storage(Scene& scene) noexcept
{
    return scene.impl_ != nullptr ? &scene.impl_->storage : nullptr;
}

} // namespace elf3d
