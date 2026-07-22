#include <elf3d/assets.h>
#include <elf3d/graphics.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

import elf.assets;
import elf.graphics;
import elf.renderer;
import elf.scene;

#include "renderer_test_support.h"

namespace {

using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeRenderTarget;

constexpr std::uint64_t engine_token = 11;
constexpr std::array<std::uint32_t, 3> indices{{0, 1, 2}};
constexpr std::array<elf3d::VertexPositionNormal, 3> point_vertices{{
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
}};
constexpr std::array<elf3d::VertexPositionNormal, 3> triangle_vertices{{
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
}};

struct FrustumScene {
    FrustumScene()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 5)), scene(id) {}

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::EntityId camera;
};

[[nodiscard]] bool add_model(FrustumScene& context, elf3d::MeshHandle mesh,
                             elf3d::MaterialHandle material, elf3d::Float3 position,
                             elf3d::Float3 scale = {1.0F, 1.0F, 1.0F}) {
    const auto model = context.scene.create_model(mesh, material);
    if (!model) {
        return false;
    }
    elf3d::Transform transform;
    transform.translation = position;
    transform.scale = scale;
    return static_cast<bool>(context.scene.set_local_transform(model.value(), transform));
}

[[nodiscard]] bool add_boundary_models(FrustumScene& context, elf3d::MeshHandle mesh,
                                       elf3d::MaterialHandle material) {
    constexpr float depth = 5.0F;
    constexpr float aspect = 640.0F / 360.0F;
    const float half_height =
        depth *
        std::tan(elf3d::PerspectiveCameraDescription{}.vertical_field_of_view_radians * 0.5F);
    const float half_width = half_height * aspect;
    const std::array<elf3d::Float3, 6> positions{{
        {-half_width, 0.0F, -depth},
        {half_width, 0.0F, -depth},
        {0.0F, -half_height, -depth},
        {0.0F, half_height, -depth},
        {0.0F, 0.0F, -0.1F},
        {0.0F, 0.0F, -1000.0F},
    }};
    for (const elf3d::Float3 position : positions) {
        if (!add_model(context, mesh, material, position)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool add_outside_models(FrustumScene& context, elf3d::MeshHandle mesh,
                                      elf3d::MaterialHandle material) {
    constexpr float depth = 5.0F;
    constexpr float aspect = 640.0F / 360.0F;
    const float half_height =
        depth *
        std::tan(elf3d::PerspectiveCameraDescription{}.vertical_field_of_view_radians * 0.5F);
    const float half_width = half_height * aspect;
    const std::array<elf3d::Float3, 6> positions{{
        {-half_width - 1.0F, 0.0F, -depth},
        {half_width + 1.0F, 0.0F, -depth},
        {0.0F, -half_height - 1.0F, -depth},
        {0.0F, half_height + 1.0F, -depth},
        {0.0F, 0.0F, -0.05F},
        {0.0F, 0.0F, -1100.0F},
    }};
    for (const elf3d::Float3 position : positions) {
        if (!add_model(context, mesh, material, position)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] int prepare_frustum_scene(FrustumScene& context) {
    const auto point_mesh = context.scene.create_mesh({point_vertices, indices});
    const auto triangle_mesh = context.scene.create_mesh({triangle_vertices, indices});
    const auto material = context.scene.create_material({});
    const auto camera = context.scene.create_perspective_camera({});
    if (!point_mesh || !triangle_mesh || !material || !camera) {
        return 1;
    }
    if (!add_boundary_models(context, point_mesh.value(), material.value()) ||
        !add_outside_models(context, point_mesh.value(), material.value())) {
        return 2;
    }
    if (!add_model(context, triangle_mesh.value(), material.value(), {0.0F, 0.0F, -5.0F},
                   {-1.0F, 1.0F, 1.0F})) {
        return 2;
    }
    context.camera = camera.value();
    return 0;
}

[[nodiscard]] bool has_expected_culling(const elf3d::renderer::RenderList& list) {
    return list.candidate_primitives == 13 && list.frustum_culled_primitives == 6 &&
           list.items.size() == 7;
}

[[nodiscard]] int run_frustum_test() {
    FrustumScene context;
    const int prepared = prepare_frustum_scene(context);
    if (prepared != 0) {
        return prepared;
    }
    const auto list = elf3d::renderer::build_render_list(context.scene, context.camera, {640, 360});
    if (!list || !has_expected_culling(list.value())) {
        return 3;
    }
    auto renderer_result =
        elf3d::renderer::Renderer::create(std::make_unique<FakeDevice>(), engine_token);
    if (!renderer_result) {
        return 4;
    }
    FakeRenderTarget target;
    const elf3d::renderer::RenderRequest request{context.camera};
    const auto render = renderer_result.value()->render(context.scene, target, request);
    if (!render || render.value().candidate_primitives != 13 ||
        render.value().frustum_culled_primitives != 6 || render.value().visible_primitives != 7 ||
        render.value().draw_calls != 7) {
        return 5;
    }
    return 0;
}

} // namespace

int elf3d_renderer_frustum_test() {
    return run_frustum_test();
}
