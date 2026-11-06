#pragma once

#include "offscreen_viewport_test_support.h"

namespace elf3d::viewport::tests::scenario {

[[nodiscard]] inline bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept
{
    return std::abs(left - right) <= tolerance;
}

struct ViewportContext {
    ViewportContext()
        : scene_id(elf3d::detail::SceneHandleAccess::create_scene(1, 1)), scene(scene_id)
    {
    }

    elf3d::SceneId scene_id;
    elf3d::scene::Storage scene;
    std::unique_ptr<elf3d::renderer::Renderer> renderer;
    elf3d::picking::PickingService picking_service;
    elf3d::EntityId camera;
    elf3d::EntityId model;
    std::unique_ptr<elf3d::viewport::OffscreenViewport> viewport;

    [[nodiscard]] FakeDeviceState& device_state() noexcept
    {
        return static_cast<FakeDevice&>(renderer->device()).state();
    }
};

[[nodiscard]] inline elf3d::Result<void> update_navigation(ViewportContext& context,
                                                           const elf3d::NavigationInput& input)
{
    return context.viewport->update_navigation(*context.renderer, context.scene, context.camera,
                                               input);
}

[[nodiscard]] int verify_dynamic_anchor_navigation(ViewportContext& context);
[[nodiscard]] int verify_eye_orbit(ViewportContext& context);
[[nodiscard]] int verify_disabled_focus_depth(ViewportContext& context);
[[nodiscard]] int verify_explicit_examine_pivot(ViewportContext& context);

} // namespace elf3d::viewport::tests::scenario
