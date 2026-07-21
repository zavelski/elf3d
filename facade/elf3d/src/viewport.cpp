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

Result<void> Viewport::update_navigation(Scene& scene, EntityId camera_entity,
                                         const ViewportInput& input) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const std::shared_ptr<renderer::Renderer> renderer = impl_->renderer.lock();
        const std::shared_ptr<picking::PickingService> picking = impl_->picking.lock();
        if (renderer == nullptr || picking == nullptr) {
            return Error{ErrorCode::graphics_shutdown,
                         "Viewport navigation requires live engine services"};
        }
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport navigation requires a live scene"};
        }
        const Result<PerspectiveCameraDescription> camera =
            storage->perspective_camera(camera_entity);
        if (!camera) {
            return camera.error();
        }
        return impl_->viewport->update_navigation(*renderer, *picking, *storage, camera_entity,
                                                  input);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::set_examine_pivot(Scene& scene, EntityId camera_entity,
                                         Float3 world_position) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport pivot update requires a live scene"};
        }
        return impl_->viewport->set_examine_pivot(*storage, camera_entity, world_position);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::fit_to_scene(Scene& scene, EntityId camera_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport fitting requires a live scene"};
        }
        return impl_->viewport->fit_to_scene(*storage, camera_entity);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::reset_view(Scene& scene, EntityId camera_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport reset requires a live scene"};
        }
        return impl_->viewport->reset_view(*storage, camera_entity);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::synchronize_navigation(const Scene& scene, EntityId camera_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport navigation synchronization requires a live scene"};
        }
        return impl_->viewport->synchronize_navigation(*storage, camera_entity);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::cancel_interaction() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->cancel_interaction();
    }
}

void Viewport::set_navigation_enabled(bool enabled) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_navigation_enabled(enabled);
    }
}

bool Viewport::navigation_enabled() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->navigation_enabled();
}

Result<void> Viewport::set_navigation_settings(const OrbitNavigationSettings& settings) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_navigation_settings(settings);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

OrbitNavigationSettings Viewport::navigation_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->navigation_settings()
                                                          : OrbitNavigationSettings{};
}

std::optional<NavigationSnapshot> Viewport::navigation_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->navigation_snapshot()
                                                          : std::nullopt;
}

void Viewport::set_active_tool(ViewportTool tool) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_active_tool(tool);
    }
}

ViewportTool Viewport::active_tool() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->active_tool()
                                                          : ViewportTool::selection;
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

void Viewport::set_selection_enabled(bool enabled) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_selection_enabled(enabled);
    }
}

bool Viewport::selection_enabled() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->selection_enabled();
}

Result<void> Viewport::set_selection_settings(const SelectionSettings& settings) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_selection_settings(settings);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

SelectionSettings Viewport::selection_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_settings()
                                                          : SelectionSettings{};
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

Result<void> Viewport::begin_distance_measurement() noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->begin_distance_measurement();
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

void Viewport::cancel_distance_measurement() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->cancel_distance_measurement();
    }
}

void Viewport::clear_distance_measurement() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_distance_measurement();
    }
}

DistanceMeasurementSnapshot
Viewport::distance_measurement_snapshot(const Scene& scene) const noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        DistanceMeasurementSnapshot result;
        result.diagnostic =
            Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
        return result;
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            DistanceMeasurementSnapshot result;
            result.diagnostic =
                Error{ErrorCode::invalid_argument, "Viewport measurement requires a live scene"};
            return result;
        }
        return impl_->viewport->distance_measurement_snapshot(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void>
Viewport::set_measurement_settings(const DistanceMeasurementSettings& settings) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_measurement_settings(settings);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

DistanceMeasurementSettings Viewport::measurement_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->measurement_settings()
                                                          : DistanceMeasurementSettings{};
}

MeasurementStatistics Viewport::measurement_statistics() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr
               ? impl_->viewport->measurement_statistics()
               : MeasurementStatistics{};
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

Result<void> Viewport::hide_selected_in_scene(Scene& scene) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport hide-selected requires a live scene"};
        }
        return impl_->viewport->hide_selected(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::show_selected_in_scene(Scene& scene) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport show-selected requires a live scene"};
        }
        return impl_->viewport->show_selected(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<void> Viewport::isolate_selected(const Scene& scene) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport isolate-selected requires a live scene"};
        }
        return impl_->viewport->isolate_selected(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
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

Result<void> Viewport::set_clipping_helpers_visible(bool visible) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    return impl_->viewport->set_clipping_helpers_visible(visible);
}

Result<void>
Viewport::set_clipping_helper_settings(const ClippingHelperSettings& settings) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    return impl_->viewport->set_clipping_helper_settings(settings);
}

Result<void> Viewport::reset_clipping_box_to_visible_bounds(const Scene& scene,
                                                            std::uint32_t index) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport clipping box reset requires a live scene"};
        }
        return impl_->viewport->reset_clipping_box_to_visible_bounds(*storage, index);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<std::uint32_t> Viewport::add_clipping_box_from_visible_bounds(const Scene& scene) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage* storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport clipping box creation requires a live scene"};
        }
        return impl_->viewport->add_clipping_box_from_visible_bounds(*storage);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

ClippingSnapshot Viewport::clipping_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clipping_snapshot()
                                                          : ClippingSnapshot{};
}

Result<void> Viewport::render(const Scene& scene, EntityId camera_entity) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
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
        return impl_->viewport->render(*renderer, *storage, camera_entity);
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
