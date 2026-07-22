#include <elf3d/core/assert.h>
#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

import elf.scene;

namespace elf3d {

namespace {

std::vector<EntityId> collect_hierarchy_roots(const scene::Storage& storage) {
    std::vector<EntityId> roots;
    roots.reserve(storage.entities().size());
    for (const std::optional<scene::EntityRecord>& record : storage.entities()) {
        if (record.has_value() && !record->parent.has_value()) {
            roots.push_back(record->id);
        }
    }
    return roots;
}

} // namespace

class SceneHierarchySnapshot::Impl final {
  public:
    std::vector<SceneHierarchyItem> items;
    std::vector<std::string> names;
    std::uint64_t hierarchy_revision = 0;
    std::uint64_t visibility_revision = 0;
};

SceneHierarchySnapshot::SceneHierarchySnapshot() noexcept = default;

SceneHierarchySnapshot::SceneHierarchySnapshot(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

SceneHierarchySnapshot::~SceneHierarchySnapshot() noexcept = default;
SceneHierarchySnapshot::SceneHierarchySnapshot(SceneHierarchySnapshot&&) noexcept = default;
SceneHierarchySnapshot&
SceneHierarchySnapshot::operator=(SceneHierarchySnapshot&&) noexcept = default;

std::size_t SceneHierarchySnapshot::size() const noexcept {
    return impl_ != nullptr ? impl_->items.size() : 0;
}

Result<SceneHierarchyItem> SceneHierarchySnapshot::item_at(std::size_t index) const noexcept {
    if (impl_ == nullptr || index >= impl_->items.size()) {
        return Error{ErrorCode::invalid_hierarchy_snapshot_index,
                     "The hierarchy snapshot item index is out of range"};
    }
    return impl_->items[index];
}

Result<std::string_view> SceneHierarchySnapshot::name_at(std::size_t index) const noexcept {
    if (impl_ == nullptr || index >= impl_->names.size()) {
        return Error{ErrorCode::invalid_hierarchy_snapshot_index,
                     "The hierarchy snapshot name index is out of range"};
    }
    return std::string_view{impl_->names[index]};
}

std::uint64_t SceneHierarchySnapshot::hierarchy_revision() const noexcept {
    return impl_ != nullptr ? impl_->hierarchy_revision : 0;
}

std::uint64_t SceneHierarchySnapshot::visibility_revision() const noexcept {
    return impl_ != nullptr ? impl_->visibility_revision : 0;
}

Scene::ReleaseContext::ReleaseContext(std::uintptr_t context, ReleaseCallback callback) noexcept
    : context_(context), callback_(callback) {}

Scene::ReleaseContext::ReleaseContext(ReleaseContext&& other) noexcept
    : context_(std::exchange(other.context_, 0)),
      callback_(std::exchange(other.callback_, nullptr)) {}

Scene::ReleaseContext& Scene::ReleaseContext::operator=(ReleaseContext&& other) noexcept {
    if (this != &other) {
        context_ = std::exchange(other.context_, 0);
        callback_ = std::exchange(other.callback_, nullptr);
    }
    return *this;
}

void Scene::ReleaseContext::release(SceneId scene) const noexcept {
    if (callback_ == nullptr || context_ == 0) {
        return;
    }
    callback_(context_, scene);
}

class Scene::Impl final {
  public:
    Impl(SceneId id, ReleaseContext release_context) noexcept
        : storage(id), release_context(std::move(release_context)) {}

    ~Impl() {
        release_context.release(storage.id());
    }

    scene::Storage storage;
    ReleaseContext release_context;
};

Result<std::unique_ptr<Scene>> Scene::create(std::uint64_t engine_token, std::uint64_t scene_value,
                                             ReleaseContext release_context) noexcept {
    if (engine_token == 0 || scene_value == 0) {
        return Error{ErrorCode::invalid_argument,
                     "Scene creation requires a valid engine identity"};
    }
    const SceneId id = detail::SceneHandleAccess::create_scene(engine_token, scene_value);
    return std::make_unique<Scene>(ConstructionKey{},
                                   std::make_unique<Impl>(id, std::move(release_context)));
}

Scene::Scene(ConstructionKey, std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Scene::~Scene() noexcept = default;

SceneId Scene::id() const noexcept {
    return impl_ != nullptr ? impl_->storage.id() : SceneId{};
}

std::uint64_t Scene::revision() const noexcept {
    return impl_ != nullptr ? impl_->storage.revision() : 0;
}

Result<EntityId> Scene::create_entity() noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_entity()
               : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::destroy_entity(EntityId entity) noexcept {
    return impl_ != nullptr ? impl_->storage.destroy_entity(entity)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_parent(EntityId entity, EntityId parent) noexcept {
    return impl_ != nullptr ? impl_->storage.set_parent(entity, parent)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::clear_parent(EntityId entity) noexcept {
    return impl_ != nullptr ? impl_->storage.clear_parent(entity)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_local_transform(EntityId entity, const Transform& transform) noexcept {
    return impl_ != nullptr ? impl_->storage.set_local_transform(entity, transform)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<Transform> Scene::local_transform(EntityId entity) const noexcept {
    return impl_ != nullptr
               ? impl_->storage.local_transform(entity)
               : Result<Transform>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_local_matrix(EntityId entity, const Float4x4& matrix) noexcept {
    return impl_ != nullptr ? impl_->storage.set_local_matrix(entity, matrix)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<Float4x4> Scene::local_matrix(EntityId entity) const noexcept {
    if (impl_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The scene is empty"};
    }
    return impl_->storage.local_matrix(entity);
}

Result<void> Scene::set_entity_name(EntityId entity, std::string_view name) noexcept {
    return impl_ != nullptr ? impl_->storage.set_entity_name(entity, name)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<std::string_view> Scene::entity_name(EntityId entity) const noexcept {
    return impl_ != nullptr
               ? impl_->storage.entity_name(entity)
               : Result<std::string_view>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<EntityInfo> Scene::entity_info(EntityId entity) const noexcept {
    return impl_ != nullptr
               ? impl_->storage.entity_info(entity)
               : Result<EntityInfo>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<SceneHierarchySnapshot> Scene::hierarchy_snapshot() const noexcept {
    if (impl_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The scene is empty"};
    }

    auto snapshot = std::make_unique<SceneHierarchySnapshot::Impl>();
    snapshot->hierarchy_revision = impl_->storage.hierarchy_revision();
    snapshot->visibility_revision = impl_->storage.visibility_revision();

    const std::vector<EntityId> roots = collect_hierarchy_roots(impl_->storage);

    struct Frame {
        EntityId entity;
        std::uint32_t depth = 0;
    };
    std::vector<Frame> stack;
    stack.reserve(roots.size());
    for (auto iterator = roots.rbegin(); iterator != roots.rend(); ++iterator) {
        stack.push_back(Frame{*iterator, 0});
    }

    std::vector<std::uint64_t> visited;
    visited.reserve(impl_->storage.entities().size());
    while (!stack.empty()) {
        if (visited.size() >= impl_->storage.entities().size() + 1U) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Hierarchy snapshot traversal detected a cycle"};
        }
        const Frame frame = stack.back();
        stack.pop_back();
        const Result<const scene::EntityRecord*> record = impl_->storage.entity(frame.entity);
        if (!record) {
            return record.error();
        }
        const std::uint64_t debug_value = frame.entity.debug_value();
        if (std::find(visited.begin(), visited.end(), debug_value) != visited.end()) {
            return Error{ErrorCode::hierarchy_cycle,
                         "Hierarchy snapshot traversal encountered an entity twice"};
        }
        visited.push_back(debug_value);

        const scene::EntityRecord& entity_record = *record.value();
        SceneHierarchyItem item;
        item.entity = entity_record.id;
        item.parent = entity_record.parent;
        item.depth = frame.depth;
        item.child_count = static_cast<std::uint32_t>(std::min<std::size_t>(
            entity_record.children.size(),
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));
        item.has_model = entity_record.model.has_value();
        item.has_camera = entity_record.camera.has_value();
        item.local_visible = entity_record.local_visible;
        item.effective_visible = entity_record.effective_visible;
        item.renderable = entity_record.model.has_value();
        item.traversal_index = static_cast<std::uint64_t>(snapshot->items.size());
        snapshot->items.push_back(item);
        snapshot->names.push_back(entity_record.name);

        for (auto iterator = entity_record.children.rbegin();
             iterator != entity_record.children.rend(); ++iterator) {
            stack.push_back(Frame{*iterator, frame.depth + 1});
        }
    }
    return SceneHierarchySnapshot{std::move(snapshot)};
}

SceneHierarchyStatistics Scene::hierarchy_statistics() const noexcept {
    return impl_ != nullptr ? impl_->storage.hierarchy_statistics() : SceneHierarchyStatistics{};
}

std::uint64_t Scene::hierarchy_revision() const noexcept {
    return impl_ != nullptr ? impl_->storage.hierarchy_revision() : 0;
}

Result<MeshHandle> Scene::create_mesh(const MeshDataView& data) noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_mesh(data)
               : Result<MeshHandle>{Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
}

Result<MeshHandle> Scene::create_mesh(const TexturedMeshDataView& data) noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_mesh(data)
               : Result<MeshHandle>{Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
}

Result<Bounds3> Scene::mesh_bounds(MeshHandle mesh) const noexcept {
    return impl_ != nullptr
               ? impl_->storage.mesh_bounds(mesh)
               : Result<Bounds3>{Error{ErrorCode::invalid_mesh_handle, "The scene is empty"}};
}

Result<ImageHandle> Scene::create_image(const ImageDescription& description) noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_image(description)
               : Result<ImageHandle>{Error{ErrorCode::invalid_image_handle, "The scene is empty"}};
}

Result<TextureAssetHandle> Scene::create_texture(const TextureDescription& description) noexcept {
    return impl_ != nullptr ? impl_->storage.create_texture(description)
                            : Result<TextureAssetHandle>{Error{
                                  ErrorCode::invalid_texture_asset_handle, "The scene is empty"}};
}

Result<MaterialHandle> Scene::create_material(const MaterialDescription& description) noexcept {
    return impl_ != nullptr ? impl_->storage.create_material(description)
                            : Result<MaterialHandle>{
                                  Error{ErrorCode::invalid_material_handle, "The scene is empty"}};
}

Result<void> Scene::set_material_description(MaterialHandle material,
                                             const MaterialDescription& description) noexcept {
    return impl_ != nullptr
               ? impl_->storage.set_material(material, description)
               : Result<void>{Error{ErrorCode::invalid_material_handle, "The scene is empty"}};
}

Result<MaterialDescription> Scene::material_description(MaterialHandle material) const noexcept {
    return impl_ != nullptr ? impl_->storage.material(material)
                            : Result<MaterialDescription>{
                                  Error{ErrorCode::invalid_material_handle, "The scene is empty"}};
}

Result<EntityId> Scene::create_model_entity(MeshHandle mesh, MaterialHandle material) noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_model(mesh, material)
               : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void>
Scene::set_model_primitives(EntityId model_entity,
                            std::span<const ModelPrimitiveBinding> primitives) noexcept {
    return impl_ != nullptr ? impl_->storage.set_model_primitives(model_entity, primitives)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<EntityId>
Scene::create_perspective_camera_entity(const PerspectiveCameraDescription& description) noexcept {
    return impl_ != nullptr
               ? impl_->storage.create_perspective_camera(description)
               : Result<EntityId>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<PerspectiveCameraDescription>
Scene::perspective_camera_description(EntityId camera_entity) const noexcept {
    return impl_ != nullptr ? impl_->storage.perspective_camera(camera_entity)
                            : Result<PerspectiveCameraDescription>{
                                  Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_perspective_camera_description(
    EntityId camera_entity, const PerspectiveCameraDescription& description) noexcept {
    return impl_ != nullptr ? impl_->storage.set_perspective_camera(camera_entity, description)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::set_entity_local_visibility(EntityId entity, bool visible) noexcept {
    return impl_ != nullptr ? impl_->storage.set_entity_visible(entity, visible)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<bool> Scene::entity_local_visibility(EntityId entity) const noexcept {
    return impl_ != nullptr ? impl_->storage.entity_local_visibility(entity)
                            : Result<bool>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<bool> Scene::entity_effective_visibility(EntityId entity) const noexcept {
    return impl_ != nullptr ? impl_->storage.entity_effective_visibility(entity)
                            : Result<bool>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::show_entity_and_ancestors(EntityId entity) noexcept {
    return impl_ != nullptr ? impl_->storage.show_entity_and_ancestors(entity)
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

Result<void> Scene::show_all_entities() noexcept {
    return impl_ != nullptr ? impl_->storage.show_all_entities()
                            : Result<void>{Error{ErrorCode::invalid_entity, "The scene is empty"}};
}

std::uint64_t Scene::visibility_revision() const noexcept {
    return impl_ != nullptr ? impl_->storage.visibility_revision() : 0;
}

std::optional<Bounds3> Scene::world_bounds() const noexcept {
    ELF3D_ASSERT(impl_ != nullptr);
    return impl_->storage.world_bounds();
}

std::optional<Bounds3> Scene::visible_bounds() const noexcept {
    ELF3D_ASSERT(impl_ != nullptr);
    const Result<scene::VisibilityFilter> filter =
        scene::make_visibility_filter(impl_->storage, std::nullopt);
    ELF3D_ASSERT(filter.has_value());
    return impl_->storage.visible_world_bounds(filter.value());
}

SceneStatistics Scene::statistics() const noexcept {
    return impl_ != nullptr ? impl_->storage.statistics() : SceneStatistics{};
}

Result<void> Scene::export_loaded_document(std::string_view path_utf8) const noexcept {
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

const scene::Storage* scene::Access::storage(const Scene& scene) noexcept {
    return scene.impl_ != nullptr ? &scene.impl_->storage : nullptr;
}

scene::Storage* scene::Access::storage(Scene& scene) noexcept {
    return scene.impl_ != nullptr ? &scene.impl_->storage : nullptr;
}

} // namespace elf3d
