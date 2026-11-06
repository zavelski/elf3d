#include <visibility/visibility.h>

#include <optional>
#include <scene/scene.h>
#include <utility>

namespace elf3d::visibility {

Result<void> VisibilityController::isolate_entity(const scene::Storage& scene, EntityId entity)
{
    return isolate_entities(scene, {&entity, 1});
}

Result<void> VisibilityController::isolate_entities(const scene::Storage& scene,
                                                    std::span<const EntityId> entities)
{
    auto filter = scene::make_visibility_filter(scene, entities);
    if (!filter) {
        return filter.error();
    }
    isolated_scene_ = scene.id();
    isolated_roots_ = std::move(filter).value().isolated_roots;
    isolated_primitives_.clear();
    return {};
}

void VisibilityController::clear_isolation() noexcept
{
    isolated_scene_ = {};
    isolated_roots_.clear();
    isolated_primitives_.clear();
}

void VisibilityController::clear_scene(SceneId scene) noexcept
{
    if (isolated_scene_ == scene) {
        clear_isolation();
    }
    if (hidden_scene_ == scene)
        (void)clear_primitive_visibility();
}

void VisibilityController::validate_against(const scene::Storage& scene) noexcept
{
    validate_primitive_state(scene);
    if (isolated_roots_.empty()) {
        return;
    }
    if (isolated_scene_ != scene.id()) {
        clear_isolation();
        return;
    }

    for (EntityId root : isolated_roots_) {
        if (!scene.entity(root)) {
            clear_isolation();
            return;
        }
    }
}

bool VisibilityController::is_isolating() const noexcept
{
    return !isolated_roots_.empty() || !isolated_primitives_.empty();
}

std::optional<EntityId> VisibilityController::isolated_entity() const noexcept
{
    return isolated_roots_.empty() ? std::nullopt : std::optional{isolated_roots_.front()};
}

std::size_t VisibilityController::isolated_entity_count() const noexcept
{
    return isolated_roots_.size();
}

Result<scene::VisibilityFilter> VisibilityController::filter_for(const scene::Storage& scene)
{
    validate_against(scene);
    auto filter = scene::make_visibility_filter(scene, isolated_roots_);
    if (!filter)
        return filter.error();
    filter.value().hidden_primitives = hidden_primitives_;
    filter.value().isolated_primitives = isolated_primitives_;
    return filter;
}

} // namespace elf3d::visibility
