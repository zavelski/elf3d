#include <elf3d/model.h>

#include <core/exception_boundary.h>
#include <model/document_hierarchy_storage.h>
#include <model/document_storage.h>

#include <utility>

namespace elf3d {
DocumentHierarchySnapshot::DocumentHierarchySnapshot() noexcept = default;
DocumentHierarchySnapshot::~DocumentHierarchySnapshot() noexcept = default;
DocumentHierarchySnapshot::DocumentHierarchySnapshot(DocumentHierarchySnapshot&&) noexcept =
    default;
DocumentHierarchySnapshot&
DocumentHierarchySnapshot::operator=(DocumentHierarchySnapshot&&) noexcept = default;

Result<DocumentHierarchySnapshot> DocumentHierarchySnapshot::create(DocumentView source) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<DocumentHierarchySnapshot> {
        if (!source.document_ || !source.document_->storage_) {
            return Error{ErrorCode::invalid_argument, "Hierarchy requires a live Document"};
        }
        DocumentHierarchySnapshot result;
        result.impl_ = std::make_unique<Impl>();
        auto& index = *result.impl_;
        index.owner = source.document_->storage_->token();
        index.revision = source.revision();
        index.default_scene = source.default_scene();
        index.scenes.reserve(source.scene_count());
        index.nodes.reserve(source.node_count());
        index.meshes.reserve(source.mesh_count());
        index.primitives.reserve(source.primitive_count());
        index.reachable_queue.reserve(source.node_count());
        return result;
    });
}

Result<bool> DocumentHierarchySnapshot::advance(DocumentView source,
                                                std::size_t work_units) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<bool> {
        if (!matches_document(source) || work_units == 0) {
            return Error{ErrorCode::invalid_argument,
                         "Hierarchy source changed, is foreign, or the work budget is zero"};
        }
        for (std::size_t unit = 0; unit < work_units && !ready(); ++unit) {
            if (const auto stepped = impl_->step(source); !stepped) {
                return stepped.error();
            }
        }
        return ready();
    });
}

bool DocumentHierarchySnapshot::ready() const noexcept
{
    return impl_ && impl_->phase == Impl::Phase::complete;
}

bool DocumentHierarchySnapshot::matches_document(DocumentView source) const noexcept
{
    return impl_ && source.document_ && source.document_->storage_ &&
           impl_->owner == source.document_->storage_->token() &&
           impl_->revision == source.revision();
}

std::uint64_t DocumentHierarchySnapshot::document_revision() const noexcept
{
    return impl_ ? impl_->revision : 0;
}

std::optional<DocumentSceneId> DocumentHierarchySnapshot::default_scene() const noexcept
{
    return ready() ? impl_->default_scene : std::nullopt;
}

std::size_t DocumentHierarchySnapshot::scene_count() const noexcept
{
    return ready() ? impl_->scenes.size() : 0;
}
std::size_t DocumentHierarchySnapshot::node_count() const noexcept
{
    return ready() ? impl_->nodes.size() : 0;
}
std::size_t DocumentHierarchySnapshot::mesh_count() const noexcept
{
    return ready() ? impl_->meshes.size() : 0;
}
std::size_t DocumentHierarchySnapshot::primitive_count() const noexcept
{
    return ready() ? impl_->primitives.size() : 0;
}

Result<DocumentHierarchyScene> DocumentHierarchySnapshot::scene_at(std::size_t index) const noexcept
{
    if (index >= scene_count()) {
        return Error{ErrorCode::invalid_document_scene_id, "Hierarchy scene is unavailable"};
    }
    const auto& record = impl_->scenes[index];
    return DocumentHierarchyScene{record.id, record.name, record.roots};
}

Result<DocumentHierarchyNode> DocumentHierarchySnapshot::node_at(std::size_t index) const noexcept
{
    if (index >= node_count()) {
        return Error{ErrorCode::invalid_node_id, "Hierarchy node is unavailable"};
    }
    const auto& record = impl_->nodes[index];
    return DocumentHierarchyNode{record.id,   record.name,   record.parent, record.children,
                                 record.mesh, record.camera, record.light,  record.reachable};
}

Result<DocumentHierarchyMesh> DocumentHierarchySnapshot::mesh_at(std::size_t index) const noexcept
{
    if (index >= mesh_count()) {
        return Error{ErrorCode::invalid_mesh_id, "Hierarchy mesh is unavailable"};
    }
    const auto& record = impl_->meshes[index];
    return DocumentHierarchyMesh{record.id, record.name, record.primitives, record.users};
}

Result<DocumentHierarchyPrimitive>
DocumentHierarchySnapshot::primitive_at(std::size_t index) const noexcept
{
    if (index >= primitive_count()) {
        return Error{ErrorCode::invalid_primitive_id, "Hierarchy primitive is unavailable"};
    }
    return impl_->primitives[index];
}

Result<DocumentHierarchyNode> DocumentHierarchySnapshot::node(NodeId id) const noexcept
{
    if (!ready() || !id.is_valid() ||
        model::detail::DocumentHandleAccess::document_token(id) != impl_->owner) {
        return Error{ErrorCode::invalid_node_id, "Hierarchy node belongs to another Document"};
    }
    return node_at(static_cast<std::size_t>(id.debug_value() - 1));
}

Result<DocumentHierarchyMesh> DocumentHierarchySnapshot::mesh(MeshId id) const noexcept
{
    if (!ready() || !id.is_valid() ||
        model::detail::DocumentHandleAccess::document_token(id) != impl_->owner) {
        return Error{ErrorCode::invalid_mesh_id, "Hierarchy mesh belongs to another Document"};
    }
    return mesh_at(static_cast<std::size_t>(id.debug_value() - 1));
}

Result<DocumentHierarchyPrimitive>
DocumentHierarchySnapshot::primitive(PrimitiveId id) const noexcept
{
    if (!ready() || !id.is_valid() ||
        model::detail::DocumentHandleAccess::document_token(id) != impl_->owner) {
        return Error{ErrorCode::invalid_primitive_id,
                     "Hierarchy primitive belongs to another Document"};
    }
    return primitive_at(static_cast<std::size_t>(id.debug_value() - 1));
}

Result<DocumentHierarchySnapshot> Document::hierarchy_snapshot() const noexcept
{
    auto created = DocumentHierarchySnapshot::create(view());
    if (!created) {
        return created.error();
    }
    auto result = std::move(created).value();
    while (!result.ready()) {
        const auto advanced = result.advance(view(), 4096);
        if (!advanced) {
            return advanced.error();
        }
    }
    return result;
}
} // namespace elf3d
