module;

#include <elf3d/viewport.h>

#include <optional>
#include <utility>

module elf.viewport;

import elf.clipping;
import elf.navigation;
import elf.renderer;
import elf.scene;
import elf.visibility;

namespace elf3d::viewport {

Result<void> OffscreenViewport::update_navigation(renderer::Renderer& renderer,
                                                  scene::Storage& scene, EntityId camera,
                                                  const NavigationInput& input) {
    state_->validate_visibility(scene);
    Result<scene::VisibilityFilter> visibility = state_->visibility_filter(scene);
    if (!visibility) {
        return visibility.error();
    }
    Result<clipping::ClippingFilter> clipping_filter = state_->clipping.filter();
    if (!clipping_filter) {
        return clipping_filter.error();
    }

    const navigation::NavigationUpdateRequest request{
        camera, extent(), input, state_->navigation.settings().drag_threshold_pixels};
    Result<navigation::NavigationUpdate> navigation =
        state_->navigation.update(scene, request, visibility.value());
    if (!navigation) {
        return navigation.error();
    }

    const InteractionFrame frame{camera, std::move(visibility).value(),
                                 std::move(clipping_filter).value()};
    return update_orbit_screen_anchor(renderer, scene, frame,
                                      navigation.value().orbit_start_position_pixels);
}

} // namespace elf3d::viewport
