#pragma once

#include "renderer_test_support.h"

namespace elf3d::renderer::tests::scenario {

using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeDeviceState;
using elf3d::renderer::tests::FakeRenderTarget;

inline constexpr std::uint64_t engine_token = 11;

[[nodiscard]] inline bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept
{
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] inline bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                       float tolerance = 0.0001F) noexcept
{
    return nearly_equal(left.x, right.x, tolerance) && nearly_equal(left.y, right.y, tolerance) &&
           nearly_equal(left.z, right.z, tolerance);
}
struct RendererContext {
    RendererContext()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 1)), scene(id)
    {
    }

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::MeshHandle mesh;
    elf3d::TextureAssetHandle texture;
    elf3d::TextureAssetHandle clamped_texture;
    elf3d::MaterialHandle material;
    elf3d::MaterialHandle double_sided;
    elf3d::EntityId model;
    elf3d::EntityId camera;
    elf3d::EntityId non_camera;
    std::unique_ptr<elf3d::renderer::Renderer> renderer;
    FakeRenderTarget target;

    [[nodiscard]] FakeDeviceState& device_state() noexcept
    {
        return static_cast<FakeDevice&>(renderer->device()).state();
    }
};
[[nodiscard]] int verify_gpu_pick(RendererContext& context);
[[nodiscard]] int verify_focus_anchor(RendererContext& context);

} // namespace elf3d::renderer::tests::scenario
