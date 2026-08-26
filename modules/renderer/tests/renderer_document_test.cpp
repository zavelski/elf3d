#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/model.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <array>
#include <cstdint>
#include <memory>

import elf.graphics;
import elf.model;
import elf.renderer;
import elf.scene;

#include "renderer_test_support.h"

namespace {

using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeDeviceState;
using elf3d::renderer::tests::FakeRenderTarget;

constexpr std::uint64_t engine_token = 17;

struct DocumentContext {
    DocumentContext()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 3)), scene(id) {}

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
    elf3d::EntityId camera;
};

[[nodiscard]] bool position_test_camera(elf3d::scene::Storage& scene, elf3d::EntityId camera) {
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    return static_cast<bool>(scene.set_local_transform(camera, transform));
}

[[nodiscard]] int prepare_document_scene(DocumentContext& context) {
    elf3d::Document document;
    const auto mesh = document.create_mesh("document-quad");
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color = {0.1F, 0.2F, 0.3F, 0.4F};
    material_description.double_sided = true;
    material_description.alpha_mode = elf3d::AlphaMode::blend;
    const auto material = document.create_material(material_description);
    if (!mesh || !material) {
        return 49;
    }

    elf3d::PrimitiveData quad;
    quad.positions = {
        {-1.0F, -1.0F, 0.0F},
        {1.0F, -1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F},
        {-1.0F, 1.0F, 0.0F},
    };
    quad.normals = {
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
    };
    quad.indices = {0, 1, 2, 0, 2, 3};
    const auto primitive =
        document.create_primitive(mesh.value(), material.value(), std::move(quad));
    if (!primitive || !context.scene.set_document(std::move(document))) {
        return 49;
    }
    const auto model = context.scene.create_entity();
    const auto camera =
        context.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera || !position_test_camera(context.scene, camera.value())) {
        return 49;
    }
    const std::array<elf3d::PrimitiveId, 1> primitives{{primitive.value()}};
    if (!context.scene.set_model_document_primitives(model.value(), primitives)) {
        return 49;
    }
    context.model = model.value();
    context.camera = camera.value();
    return 0;
}

[[nodiscard]] bool legacy_statistics_equal(const elf3d::RenderStatistics& actual,
                                           const elf3d::RenderStatistics& expected) noexcept {
    return actual.draw_calls == expected.draw_calls && actual.triangles == expected.triangles &&
           actual.vertices == expected.vertices && actual.indices == expected.indices &&
           actual.texture_bindings == expected.texture_bindings &&
           actual.gpu_texture_uploads == expected.gpu_texture_uploads &&
           actual.unique_gpu_textures == expected.unique_gpu_textures &&
           actual.overlay_lines == expected.overlay_lines &&
           actual.overlay_markers == expected.overlay_markers;
}

[[nodiscard]] bool has_expected_document_render(
    const DocumentContext& document, const elf3d::Result<elf3d::renderer::RenderList>& list,
    const elf3d::Result<elf3d::RenderStatistics>& render, const FakeDeviceState& device) {
    const elf3d::SceneStatistics expected_scene{2, 1, 1, 1, 1, 4, 6, 2};
    const elf3d::RenderStatistics expected_render{1, 2, 4, 6, 0, 0, 0, 0, 0};
    return list && list.value().items.size() == 1 && render &&
           document.scene.statistics() == expected_scene &&
           legacy_statistics_equal(render.value(), expected_render) && device.upload_count == 1 &&
           device.draws.size() == 1 && device.draws.back().double_sided &&
           device.draws.back().alpha_mode == elf3d::AlphaMode::blend;
}

} // namespace

int elf3d_renderer_document_test() {
    DocumentContext document;
    const int prepared = prepare_document_scene(document);
    if (prepared != 0) {
        return prepared;
    }
    auto device = std::make_unique<FakeDevice>();
    FakeDevice* device_state_owner = device.get();
    auto renderer = elf3d::renderer::Renderer::create(std::move(device), engine_token);
    if (!renderer) {
        return 49;
    }
    const auto list =
        elf3d::renderer::build_render_list(document.scene, document.camera, {640, 360});
    FakeRenderTarget target;
    const elf3d::renderer::RenderRequest request{document.camera, {}, {}, {}, {}};
    const auto render = renderer.value()->render(document.scene, target, request);
    if (!has_expected_document_render(document, list, render, device_state_owner->state())) {
        return 49;
    }
    return 0;
}
