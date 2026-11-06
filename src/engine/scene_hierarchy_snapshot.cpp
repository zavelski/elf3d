#include "scene_hierarchy_snapshot.h"

#include <core/exception_boundary.h>
#include <memory>
#include <utility>

namespace elf3d {
SceneHierarchySnapshot::SceneHierarchySnapshot() noexcept = default;
SceneHierarchySnapshot::SceneHierarchySnapshot(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}
SceneHierarchySnapshot::~SceneHierarchySnapshot() noexcept = default;
SceneHierarchySnapshot::SceneHierarchySnapshot(SceneHierarchySnapshot&&) noexcept = default;
SceneHierarchySnapshot&
SceneHierarchySnapshot::operator=(SceneHierarchySnapshot&&) noexcept = default;

Result<SceneHierarchySnapshot> SceneHierarchySnapshot::create(const Scene& source) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<SceneHierarchySnapshot> {
        const auto* storage = scene::Access::storage(source);
        if (!storage || !source.id().is_valid()) {
            return Error{ErrorCode::invalid_entity, "Hierarchy capture requires a live Scene"};
        }
        auto result = std::make_unique<Impl>();
        result->owner = source.id();
        result->hierarchy_revision = source.hierarchy_revision();
        result->visibility_revision = source.visibility_revision();
        const auto count = storage->entities().size();
        result->items.reserve(count);
        result->names.reserve(count);
        result->visited.reserve(count);
        result->roots.reserve(count);
        result->stack.reserve(count);
        return SceneHierarchySnapshot{std::move(result)};
    });
}

Result<bool> SceneHierarchySnapshot::advance(const Scene& source, std::size_t work_units) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<bool> {
        if (!matches_scene(source) || work_units == 0) {
            return Error{ErrorCode::invalid_argument,
                         "Hierarchy source changed, is foreign, or the work budget is zero"};
        }
        const auto* storage = scene::Access::storage(source);
        for (std::size_t unit = 0; unit < work_units && !ready(); ++unit) {
            if (const auto advanced = impl_->step(*storage); !advanced) {
                return advanced.error();
            }
        }
        return ready();
    });
}

bool SceneHierarchySnapshot::ready() const noexcept
{
    return impl_ && impl_->phase == Impl::Phase::complete;
}
bool SceneHierarchySnapshot::matches_scene(const Scene& source) const noexcept
{
    return impl_ && impl_->owner == source.id() &&
           impl_->hierarchy_revision == source.hierarchy_revision() &&
           impl_->visibility_revision == source.visibility_revision();
}
std::size_t SceneHierarchySnapshot::size() const noexcept
{
    return ready() ? impl_->items.size() : 0;
}
Result<SceneHierarchyItem> SceneHierarchySnapshot::item_at(std::size_t index) const noexcept
{
    if (index >= size()) {
        return Error{ErrorCode::invalid_hierarchy_snapshot_index, "Hierarchy item is unavailable"};
    }
    return impl_->items[index];
}
Result<std::string_view> SceneHierarchySnapshot::name_at(std::size_t index) const noexcept
{
    if (index >= size()) {
        return Error{ErrorCode::invalid_hierarchy_snapshot_index, "Hierarchy name is unavailable"};
    }
    return std::string_view{impl_->names[index]};
}
std::uint64_t SceneHierarchySnapshot::hierarchy_revision() const noexcept
{
    return impl_ ? impl_->hierarchy_revision : 0;
}
std::uint64_t SceneHierarchySnapshot::visibility_revision() const noexcept
{
    return impl_ ? impl_->visibility_revision : 0;
}
} // namespace elf3d
