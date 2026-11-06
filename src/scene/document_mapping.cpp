#include <scene/scene.h>

namespace elf3d::scene {
Result<void> Storage::bind_document_node(EntityId entity_id, NodeId node)
{
    if (!has_document()) {
        return Error{ErrorCode::invalid_node_id, "Node mapping requires a retained Document"};
    }
    const auto source = document().node(node);
    const auto target = mutable_entity(entity_id);
    if (!source) {
        return source.error();
    }
    if (!target) {
        return target.error();
    }
    const auto index = static_cast<std::size_t>(node.debug_value() - 1);
    if (document_entities_[index] || target.value()->document_node) {
        return Error{ErrorCode::invalid_argument, "Document node is already mapped"};
    }
    document_entities_[index] = entity_id;
    target.value()->document_node = node;
    return {};
}

Result<std::optional<EntityId>> Storage::entity_for_document_node(NodeId node) const noexcept
{
    if (!has_document()) {
        return Error{ErrorCode::invalid_node_id, "Node mapping requires a retained Document"};
    }
    const auto source = document().node(node);
    if (!source) {
        return source.error();
    }
    const auto mapped = document_entities_[static_cast<std::size_t>(node.debug_value() - 1)];
    return mapped && entity(*mapped) ? mapped : std::optional<EntityId>{};
}

Result<std::optional<NodeId>> Storage::document_node(EntityId entity_id) const noexcept
{
    const auto target = entity(entity_id);
    if (!target) {
        return target.error();
    }
    return target.value()->document_node;
}

Result<std::optional<PrimitiveId>>
Storage::document_primitive(EntityId entity_id,
                            std::uint32_t runtime_primitive_index) const noexcept
{
    const auto target = entity(entity_id);
    if (!target) {
        return target.error();
    }
    const auto& model = target.value()->model;
    if (!model || runtime_primitive_index >= model->primitives.size()) {
        return Error{ErrorCode::invalid_argument, "Runtime primitive binding is unavailable"};
    }
    if (runtime_primitive_index >= model->document_primitives.size()) {
        return std::optional<PrimitiveId>{};
    }
    return std::optional<PrimitiveId>{model->document_primitives[runtime_primitive_index]};
}

Result<std::optional<PrimitiveId>> Storage::document_primitive(const PickHit& hit) const noexcept
{
    const auto mapped = document_primitive(hit.entity, hit.primitive_index);
    if (!mapped) {
        return mapped.error();
    }
    const auto& model = entity(hit.entity).value()->model.value();
    if (model.primitives[hit.primitive_index].mesh != hit.mesh) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "Pick hit mesh does not match its runtime binding"};
    }
    return mapped.value();
}
} // namespace elf3d::scene
