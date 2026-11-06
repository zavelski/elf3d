#include <scene/scene.h>

#include <algorithm>
#include <span>

namespace elf3d::scene {
Result<void> Storage::validate_primitive_instance(PrimitiveInstance instance) const noexcept
{
    const auto record = entity(instance.entity);
    if (!record)
        return record.error();
    if (!has_document())
        return Error{ErrorCode::invalid_primitive_id,
                     "Primitive instances require a retained Document"};
    const auto primitive = document().primitive(instance.primitive);
    if (!primitive)
        return primitive.error();
    const auto& model = record.value()->model;
    if (!model || std::find(model->document_primitives.begin(), model->document_primitives.end(),
                            instance.primitive) == model->document_primitives.end())
        return Error{ErrorCode::invalid_primitive_id,
                     "The Document primitive is not instantiated by this entity"};
    return {};
}

bool primitive_instance_less(PrimitiveInstance a, PrimitiveInstance b) noexcept
{
    // Ordering is local to one validated Scene/Document domain. Containment
    // additionally compares complete typed IDs, including their owner identity.
    const auto left = detail::SceneHandleAccess::value(a.entity);
    const auto right = detail::SceneHandleAccess::value(b.entity);
    if (left != right)
        return left < right;
    return a.primitive.debug_value() < b.primitive.debug_value();
}
bool primitive_instance_contains(std::span<const PrimitiveInstance> sorted,
                                 PrimitiveInstance instance) noexcept
{
    const auto found =
        std::lower_bound(sorted.begin(), sorted.end(), instance, primitive_instance_less);
    return found != sorted.end() && *found == instance;
}
Result<std::vector<PrimitiveInstance>>
normalize_primitive_instances(const Storage& scene, std::span<const PrimitiveInstance> instances)
{
    for (const auto instance : instances) {
        if (const auto valid = scene.validate_primitive_instance(instance); !valid)
            return valid.error();
    }
    std::vector<PrimitiveInstance> result{instances.begin(), instances.end()};
    std::sort(result.begin(), result.end(), primitive_instance_less);
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool primitive_visible_in_filter(const Storage& scene, const VisibilityFilter& filter,
                                 PrimitiveInstance instance) noexcept
{
    if (!entity_visible_in_filter(scene, filter, instance.entity))
        return false;
    if (primitive_instance_contains(filter.hidden_primitives, instance))
        return false;
    return filter.isolated_primitives.empty() ||
           primitive_instance_contains(filter.isolated_primitives, instance);
}
bool primitive_visible_in_filter(const Storage& scene, const VisibilityFilter& filter,
                                 EntityId entity, std::uint32_t primitive_index) noexcept
{
    if (!entity_visible_in_filter(scene, filter, entity))
        return false;
    if (filter.hidden_primitives.empty() && filter.isolated_primitives.empty())
        return true;
    const auto primitive = scene.document_primitive(entity, primitive_index);
    if (!primitive)
        return false;
    if (!primitive.value())
        return filter.isolated_primitives.empty();
    return primitive_visible_in_filter(scene, filter, {entity, *primitive.value()});
}
std::optional<Bounds3> filtered_primitive_bounds(const Storage& scene,
                                                 const VisibilityFilter& filter, EntityId entity)
{
    const auto record = scene.entity(entity);
    if (!record || !record.value()->model)
        return std::nullopt;
    std::optional<Bounds3> result;
    const auto count = record.value()->model->primitives.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto index = static_cast<std::uint32_t>(i); // Runtime binding count is validated.
        if (!primitive_visible_in_filter(scene, filter, entity, index))
            continue;
        const auto bounds = scene.primitive_world_bounds(entity, index);
        if (!bounds)
            continue;
        if (!result) {
            result = bounds.value();
            continue;
        }
        const auto& b = bounds.value();
        result->minimum.x = std::min(result->minimum.x, b.minimum.x);
        result->minimum.y = std::min(result->minimum.y, b.minimum.y);
        result->minimum.z = std::min(result->minimum.z, b.minimum.z);
        result->maximum.x = std::max(result->maximum.x, b.maximum.x);
        result->maximum.y = std::max(result->maximum.y, b.maximum.y);
        result->maximum.z = std::max(result->maximum.z, b.maximum.z);
    }
    return result;
}
} // namespace elf3d::scene
