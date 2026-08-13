#include <elf3d/core/assert.h>
#include <elf3d/elf3d.h>

#include <memory>
#include <new>
#include <optional>
#include <utility>

import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

#include "viewport_impl.h"

namespace elf3d {
namespace {

[[noreturn]] void fatal_allocation_failure() noexcept {
    fatal_error("Elf3D memory allocation failed");
}

[[noreturn]] void fatal_unexpected_boundary_exception() noexcept {
    fatal_error("Elf3D boundary encountered an unexpected exception");
}

} // namespace

Viewport::Viewport(ConstructionKey, std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Viewport::~Viewport() noexcept = default;

Extent2D Viewport::extent() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->extent() : Extent2D{};
}

Result<void> Viewport::resize(Extent2D extent) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->resize(extent);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::set_clear_color(Color4 color) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_clear_color(color);
    }
}

Color4 Viewport::clear_color() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clear_color()
                                                          : Color4{};
}

void Viewport::set_basic_lighting(const BasicLighting& lighting) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_basic_lighting(lighting);
    }
}

BasicLighting Viewport::basic_lighting() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->basic_lighting()
                                                          : BasicLighting{};
}

void Viewport::set_render_shading_mode(RenderShadingMode mode) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_render_shading_mode(mode);
    }
}

RenderShadingMode Viewport::render_shading_mode() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->render_shading_mode()
                                                          : RenderShadingMode::standard;
}

std::uint64_t Viewport::render_revision() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->render_revision() : 0;
}

Result<Ray3> Viewport::make_picking_ray(const Scene& scene, EntityId camera_entity,
                                        Float2 position_pixels) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const std::shared_ptr<picking::PickingService> picking = impl_->picking.lock();
        if (picking == nullptr) {
            return Error{ErrorCode::graphics_shutdown,
                         "Viewport picking requires live engine services"};
        }
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport picking requires a live scene"};
        }
        return impl_->viewport->make_picking_ray(*picking, *storage, camera_entity,
                                                 position_pixels);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<std::optional<PickHit>> Viewport::pick(const Scene& scene, EntityId camera_entity,
                                              Float2 position_pixels,
                                              const PickOptions& options) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const std::shared_ptr<renderer::Renderer> renderer = impl_->renderer.lock();
        const std::shared_ptr<picking::PickingService> picking = impl_->picking.lock();
        if (renderer == nullptr || picking == nullptr) {
            return Error{ErrorCode::graphics_shutdown,
                         "Viewport picking requires live engine services"};
        }
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport picking requires a live scene"};
        }
        const viewport::ViewportPickRequest request{camera_entity, position_pixels, options};
        return impl_->viewport->pick(*renderer, *picking, *storage, request);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<std::optional<PickHit>> Viewport::select_at(const Scene& scene, EntityId camera_entity,
                                                   Float2 position_pixels) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const std::shared_ptr<renderer::Renderer> renderer = impl_->renderer.lock();
        const std::shared_ptr<picking::PickingService> picking = impl_->picking.lock();
        if (renderer == nullptr || picking == nullptr) {
            return Error{ErrorCode::graphics_shutdown,
                         "Viewport selection requires live engine services"};
        }
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport selection requires a live scene"};
        }
        return impl_->viewport->select_at(*renderer, *picking, *storage, camera_entity,
                                          position_pixels);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::set_selected_entity(const Scene& scene, EntityId selected_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport selection requires a live scene"};
        }
        return impl_->viewport->set_selected_entity(*storage, selected_entity);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::clear_selection() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_selection();
    }
}

bool Viewport::has_selection() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->has_selection();
}

std::optional<EntityId> Viewport::selected_entity() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selected_entity()
                                                          : std::nullopt;
}

std::optional<PickHit> Viewport::selection_hit() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_hit()
                                                          : std::nullopt;
}

SelectionSnapshot Viewport::selection_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_snapshot()
                                                          : SelectionSnapshot{};
}

Result<PickingStatistics> Viewport::picking_statistics() const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    const std::shared_ptr<picking::PickingService> picking = impl_->picking.lock();
    if (picking == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport statistics require live engine services"};
    }
    return impl_->viewport->picking_statistics(*picking);
}

Result<ProjectedViewportPoint>
Viewport::project_world_to_viewport(const Scene& scene, EntityId camera_entity,
                                    Float3 world_position) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport projection requires a live scene"};
        }
        return impl_->viewport->project_world_to_viewport(*storage, camera_entity, world_position);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<bool> Viewport::surface_anchor_visible(const Scene& scene,
                                              const ResolvedSurfaceAnchor& anchor) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Surface anchor visibility requires a live scene"};
        }
        return impl_->viewport->surface_anchor_visible(*storage, anchor);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::isolate_entity(const Scene& scene, EntityId isolated_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport isolation requires a live scene"};
        }
        return impl_->viewport->isolate_entity(*storage, isolated_entity);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::clear_isolation() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_isolation();
    }
}

bool Viewport::is_isolating() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->is_isolating();
}

std::optional<EntityId> Viewport::isolated_entity() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->isolated_entity()
                                                          : std::nullopt;
}

Result<std::optional<Bounds3>> Viewport::visible_bounds(const Scene& scene) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport visible-bounds query requires a live scene"};
        }
        return impl_->viewport->visible_bounds(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<std::optional<Bounds3>>
Viewport::unclipped_visible_bounds(const Scene& scene) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport visible-bounds query requires a live scene"};
        }
        return impl_->viewport->unclipped_visible_bounds(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::set_section_plane(const SectionPlane& plane) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_section_plane(plane);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::clear_section_plane() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_section_plane();
    }
}

Result<std::uint32_t> Viewport::add_clipping_box(const ClippingBox& box) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->add_clipping_box(box);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::set_clipping_box(std::uint32_t index, const ClippingBox& box) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_clipping_box(index, box);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::remove_clipping_box(std::uint32_t index) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->remove_clipping_box(index);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::clear_clipping_boxes() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_clipping_boxes();
    }
}

void Viewport::clear_clipping() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_clipping();
    }
}

ClippingSnapshot Viewport::clipping_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clipping_snapshot()
                                                          : ClippingSnapshot{};
}

Result<void> Viewport::render(const Scene& scene, EntityId camera_entity) noexcept {
    ViewportRenderOptions options;
    options.shading_mode = render_shading_mode();
    return render(scene, camera_entity, options);
}

Result<void> Viewport::render(const Scene& scene, EntityId camera_entity,
                              const ViewportRenderOptions& options) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    if (options.overlay_lines.size() > maximum_viewport_overlay_lines ||
        options.overlay_markers.size() > maximum_viewport_overlay_markers) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "Viewport rendering exceeds the overlay element limit"};
    }

    try {
        const std::shared_ptr<renderer::Renderer> renderer = impl_->renderer.lock();
        if (renderer == nullptr) {
            return Error{ErrorCode::graphics_shutdown,
                         "Viewport rendering requires live engine services"};
        }
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport rendering requires a live scene"};
        }
        return impl_->viewport->render(*renderer, *storage, camera_entity, options);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

RenderStatistics Viewport::render_statistics() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->statistics()
                                                          : RenderStatistics{};
}

TextureHandle Viewport::color_texture() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->color_texture()
                                                          : TextureHandle{};
}

bool Viewport::framebuffer_valid() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->framebuffer_valid();
}

} // namespace elf3d
