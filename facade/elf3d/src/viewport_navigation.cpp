#include <elf3d/core/assert.h>
#include <elf3d/elf3d.h>

#include <memory>
#include <new>
#include <optional>

import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

#include "viewport_impl.h"

namespace elf3d {
namespace {

[[noreturn]] void fatal_navigation_allocation_failure() noexcept {
    fatal_error("Elf3D viewport navigation memory allocation failed");
}

[[noreturn]] void fatal_unexpected_navigation_boundary_exception() noexcept {
    fatal_error("Elf3D viewport navigation boundary encountered an unexpected exception");
}

} // namespace

Result<void> Viewport::update_navigation(Scene& scene, EntityId camera_entity,
                                         const NavigationInput& input) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    try {
        const std::shared_ptr<renderer::Renderer> renderer = impl_->renderer.lock();
        if (renderer == nullptr) {
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
        return impl_->viewport->update_navigation(*renderer, *storage, camera_entity, input);
    } catch (const std::bad_alloc&) {
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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
        fatal_navigation_allocation_failure();
    } catch (...) {
        fatal_unexpected_navigation_boundary_exception();
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

} // namespace elf3d
