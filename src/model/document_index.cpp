#include <elf3d/model.h>
#include <model/document_storage.h>

namespace elf3d {
Result<std::size_t> Document::index_of(NodeId id) const noexcept
{
    if (!storage_)
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
    const auto record = storage_->node(id);
    if (!record)
        return record.error();
    // Storage verified ownership and the encoded slot against its table size.
    return static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
}
Result<std::size_t> Document::index_of(MeshId id) const noexcept
{
    if (!storage_)
        return Error{ErrorCode::invalid_mesh_id, "The document is empty"};
    const auto record = storage_->mesh(id);
    if (!record)
        return record.error();
    return static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
}
Result<std::size_t> Document::index_of(PrimitiveId id) const noexcept
{
    if (!storage_)
        return Error{ErrorCode::invalid_primitive_id, "The document is empty"};
    const auto record = storage_->primitive(id);
    if (!record)
        return record.error();
    return static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
}
Result<std::size_t> Document::index_of(MaterialId id) const noexcept
{
    if (!storage_)
        return Error{ErrorCode::invalid_material_id, "The document is empty"};
    const auto record = storage_->material(id);
    if (!record)
        return record.error();
    return static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
}
Result<std::size_t> DocumentView::index_of(NodeId id) const noexcept
{
    return document_ ? document_->index_of(id)
                     : Result<std::size_t>{Error{ErrorCode::invalid_node_id, "The view is empty"}};
}
Result<std::size_t> DocumentView::index_of(MeshId id) const noexcept
{
    return document_ ? document_->index_of(id)
                     : Result<std::size_t>{Error{ErrorCode::invalid_mesh_id, "The view is empty"}};
}
Result<std::size_t> DocumentView::index_of(PrimitiveId id) const noexcept
{
    return document_
               ? document_->index_of(id)
               : Result<std::size_t>{Error{ErrorCode::invalid_primitive_id, "The view is empty"}};
}
Result<std::size_t> DocumentView::index_of(MaterialId id) const noexcept
{
    return document_
               ? document_->index_of(id)
               : Result<std::size_t>{Error{ErrorCode::invalid_material_id, "The view is empty"}};
}
} // namespace elf3d
