#include <elf3d/model.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

import elf.assets;
import elf.graphics;
import elf.renderer;
import elf.scene;

#include "renderer_test_support.h"

namespace {

using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeDeviceState;
using elf3d::renderer::tests::FakeRenderTarget;

constexpr std::uint64_t engine_token = 11;

[[nodiscard]] bool position_test_camera(elf3d::scene::Storage& scene, elf3d::EntityId camera) {
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    return static_cast<bool>(scene.set_local_transform(camera, transform));
}

[[nodiscard]] elf3d::PrimitiveData layout_triangle(std::size_t layout_index) {
    elf3d::PrimitiveData data;
    data.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    data.normals = {{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    data.indices = {0, 1, 2};
    if (layout_index >= 1U) {
        data.texcoord0 = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
    }
    if (layout_index >= 2U) {
        data.texcoord1 = data.texcoord0;
        data.colors = {
            {1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}};
    }
    return data;
}

[[nodiscard]] bool add_layout_primitives(elf3d::Document& document, elf3d::MeshId mesh,
                                         elf3d::MaterialId material,
                                         std::array<elf3d::PrimitiveId, 3>& primitives) {
    for (std::size_t index = 0; index < primitives.size(); ++index) {
        const auto primitive = document.create_primitive(mesh, material, layout_triangle(index));
        if (!primitive) {
            return false;
        }
        primitives[index] = primitive.value();
    }
    return true;
}

[[nodiscard]] bool has_expected_layout_uploads(const FakeDeviceState& device) {
    constexpr std::array<elf3d::graphics::VertexLayout, 3> layouts{{
        elf3d::graphics::VertexLayout::position_normal_float3,
        elf3d::graphics::VertexLayout::position_normal_float3_texcoord_float2,
        elf3d::graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4,
    }};
    constexpr std::array<std::size_t, 3> bytes{{72U, 96U, 168U}};
    return device.mesh_layouts ==
               std::vector<elf3d::graphics::VertexLayout>{layouts.begin(), layouts.end()} &&
           device.mesh_uploaded_bytes == std::vector<std::size_t>{bytes.begin(), bytes.end()};
}

struct LayoutScene {
    LayoutScene()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 4)), scene(id) {}

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::EntityId camera;
};

[[nodiscard]] int prepare_layout_scene(LayoutScene& context) {
    elf3d::Document document;
    const auto mesh = document.create_mesh("layout-mesh");
    const auto material = document.create_material({});
    if (!mesh || !material) {
        return 1;
    }
    std::array<elf3d::PrimitiveId, 3> primitives;
    if (!add_layout_primitives(document, mesh.value(), material.value(), primitives)) {
        return 1;
    }
    if (!context.scene.set_document(std::move(document))) {
        return 1;
    }
    const auto model = context.scene.create_entity();
    const auto camera = context.scene.create_perspective_camera({});
    if (!model || !camera || !position_test_camera(context.scene, camera.value())) {
        return 2;
    }
    if (!context.scene.set_model_document_primitives(model.value(), primitives)) {
        return 2;
    }
    context.camera = camera.value();
    return 0;
}

[[nodiscard]] bool renders_expected(elf3d::renderer::Renderer& renderer,
                                    const LayoutScene& context) {
    FakeRenderTarget target;
    const elf3d::renderer::RenderRequest request{context.camera};
    const auto first = renderer.render(context.scene, target, request);
    const auto second = renderer.render(context.scene, target, request);
    const auto& device = static_cast<FakeDevice&>(renderer.device()).state();
    if (!first || !second) {
        return false;
    }
    return first.value().gpu_buffer_uploads == 3 && second.value().gpu_buffer_uploads == 0 &&
           has_expected_layout_uploads(device);
}

[[nodiscard]] int run_layout_test() {
    LayoutScene context;
    const int prepared = prepare_layout_scene(context);
    if (prepared != 0) {
        return prepared;
    }
    auto renderer_result =
        elf3d::renderer::Renderer::create(std::make_unique<FakeDevice>(), engine_token);
    if (!renderer_result) {
        return 3;
    }
    auto renderer = std::move(renderer_result.value());
    if (!renders_expected(*renderer, context)) {
        return 4;
    }
    renderer->release_scene(context.id);
    return 0;
}

} // namespace

int elf3d_renderer_layout_test() {
    return run_layout_test();
}
