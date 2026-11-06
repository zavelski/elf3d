#include <core/exception_boundary.h>
#include <scene/scene.h>
#include <viewport/viewport.h>

#include "viewport_impl.h"

namespace elf3d {
Result<void> Viewport::set_primitive_local_visibility(const Scene& scene,
                                                      std::span<const PrimitiveInstance> primitives,
                                                      bool visible) noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    const auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Primitive visibility requires a live Scene"};
    return detail::fatal_exception_boundary([&] {
        return impl_->viewport->set_primitive_local_visibility(*storage, primitives, visible);
    });
}
Result<bool> Viewport::primitive_local_visibility(const Scene& scene,
                                                  PrimitiveInstance primitive) const noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    const auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Primitive visibility requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->primitive_local_visibility(*storage, primitive); });
}
Result<bool> Viewport::primitive_effective_visibility(const Scene& scene,
                                                      PrimitiveInstance primitive) const noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    const auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Primitive visibility requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->primitive_effective_visibility(*storage, primitive); });
}
void Viewport::clear_primitive_visibility() noexcept
{
    if (impl_ && impl_->viewport)
        impl_->viewport->clear_primitive_visibility();
}
Result<void> Viewport::isolate_primitives(const Scene& scene,
                                          std::span<const PrimitiveInstance> primitives) noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    const auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Primitive isolation requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->isolate_primitives(*storage, primitives); });
}
std::size_t Viewport::isolated_primitive_count() const noexcept
{
    return impl_ && impl_->viewport ? impl_->viewport->isolated_primitive_count() : 0;
}
Result<void>
Viewport::set_selected_primitives(const Scene& scene,
                                  std::span<const PrimitiveInstance> primitives) noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    const auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Primitive selection requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->set_selected_primitives(*storage, primitives); });
}
std::size_t Viewport::selected_primitive_count() const noexcept
{
    return impl_ && impl_->viewport ? impl_->viewport->selected_primitive_count() : 0;
}
Result<PrimitiveInstance> Viewport::selected_primitive_at(std::size_t index) const noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    return impl_->viewport->selected_primitive_at(index);
}
Result<void> Viewport::fit_to_entities(Scene& scene, EntityId camera,
                                       std::span<const EntityId> roots) noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Object fitting requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->fit_to_entities(*storage, camera, roots); });
}
Result<void> Viewport::fit_to_primitives(Scene& scene, EntityId camera,
                                         std::span<const PrimitiveInstance> primitives) noexcept
{
    if (!impl_ || !impl_->viewport)
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    auto* storage = scene::Access::storage(scene);
    if (!storage)
        return Error{ErrorCode::invalid_argument, "Resource fitting requires a live Scene"};
    return detail::fatal_exception_boundary(
        [&] { return impl_->viewport->fit_to_primitives(*storage, camera, primitives); });
}
} // namespace elf3d
