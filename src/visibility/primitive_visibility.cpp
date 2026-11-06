#include <visibility/visibility.h>

#include <algorithm>
#include <iterator>
#include <utility>

namespace elf3d::visibility {
Result<void> VisibilityController::isolate_primitives(const scene::Storage& scene,
                                                      std::span<const PrimitiveInstance> primitives)
{
    auto normalized = scene::normalize_primitive_instances(scene, primitives);
    if (!normalized)
        return normalized.error();
    isolated_scene_ = primitives.empty() ? SceneId{} : scene.id();
    isolated_roots_.clear();
    isolated_primitives_ = std::move(normalized).value();
    validated_hierarchy_revision_ = 0;
    return {};
}
Result<bool> VisibilityController::set_primitive_visibility(
    const scene::Storage& scene, std::span<const PrimitiveInstance> primitives, bool visible)
{
    auto normalized = scene::normalize_primitive_instances(scene, primitives);
    if (!normalized)
        return normalized.error();
    validate_primitive_state(scene);
    const auto& keys = normalized.value();
    std::vector<PrimitiveInstance> result;
    result.reserve(hidden_primitives_.size() + keys.size());
    if (visible)
        std::set_difference(hidden_primitives_.begin(), hidden_primitives_.end(), keys.begin(),
                            keys.end(), std::back_inserter(result), scene::primitive_instance_less);
    else
        std::set_union(hidden_primitives_.begin(), hidden_primitives_.end(), keys.begin(),
                       keys.end(), std::back_inserter(result), scene::primitive_instance_less);
    const bool changed = result != hidden_primitives_;
    hidden_primitives_ = std::move(result);
    hidden_scene_ = hidden_primitives_.empty() ? SceneId{} : scene.id();
    return changed;
}
Result<bool> VisibilityController::primitive_local_visibility(const scene::Storage& scene,
                                                              PrimitiveInstance primitive) const
{
    if (const auto valid = scene.validate_primitive_instance(primitive); !valid)
        return valid.error();
    return hidden_scene_ != scene.id() ||
           !scene::primitive_instance_contains(hidden_primitives_, primitive);
}
Result<bool> VisibilityController::primitive_effective_visibility(const scene::Storage& scene,
                                                                  PrimitiveInstance primitive)
{
    if (const auto valid = scene.validate_primitive_instance(primitive); !valid)
        return valid.error();
    auto filter = filter_for(scene);
    if (!filter)
        return filter.error();
    return scene::primitive_visible_in_filter(scene, filter.value(), primitive);
}
bool VisibilityController::clear_primitive_visibility() noexcept
{
    const bool changed = !hidden_primitives_.empty();
    hidden_primitives_.clear();
    hidden_scene_ = {};
    return changed;
}
std::size_t VisibilityController::isolated_primitive_count() const noexcept
{
    return isolated_primitives_.size();
}
void VisibilityController::validate_primitive_state(const scene::Storage& scene) noexcept
{
    if (validated_scene_ == scene.id() &&
        validated_hierarchy_revision_ == scene.hierarchy_revision())
        return;
    if (hidden_scene_ != scene.id())
        (void)clear_primitive_visibility();
    else
        std::erase_if(hidden_primitives_, [&](PrimitiveInstance key) {
            return !scene.validate_primitive_instance(key);
        });
    if (!isolated_primitives_.empty()) {
        if (isolated_scene_ != scene.id())
            clear_isolation();
        else {
            std::erase_if(isolated_primitives_, [&](PrimitiveInstance key) {
                return !scene.validate_primitive_instance(key);
            });
            if (isolated_primitives_.empty())
                isolated_scene_ = {};
        }
    }
    validated_scene_ = scene.id();
    validated_hierarchy_revision_ = scene.hierarchy_revision();
}
} // namespace elf3d::visibility
