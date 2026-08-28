#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/model.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

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

[[nodiscard]] elf3d::Result<elf3d::TextureId> create_normal_texture(elf3d::Document& document) {
    constexpr std::array<std::byte, 4> normal_pixel{std::byte{128}, std::byte{128}, std::byte{255},
                                                    std::byte{255}};
    const auto image = document.create_image(
        elf3d::ModelImageDescription{1U, 1U, elf3d::PixelFormat::rgba8_unorm, normal_pixel});
    const auto sampler = document.create_sampler();
    if (!image || !sampler) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create normal texture inputs"};
    }
    return document.create_texture(elf3d::ModelTextureDescription{image.value(), sampler.value()});
}

[[nodiscard]] elf3d::PrimitiveData tangent_quad() {
    elf3d::PrimitiveData quad;
    quad.positions = {
        {-1.0F, -1.0F, 0.0F}, {1.0F, -1.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {-1.0F, 1.0F, 0.0F}};
    quad.normals = {{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    quad.indices = {0, 1, 2, 0, 2, 3};
    quad.texcoord0 = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}};
    quad.tangents = {{1.0F, 0.0F, 0.0F, 1.0F},
                     {1.0F, 0.0F, 0.0F, 1.0F},
                     {1.0F, 0.0F, 0.0F, 1.0F},
                     {1.0F, 0.0F, 0.0F, 1.0F}};
    return quad;
}

[[nodiscard]] int prepare_document_scene(DocumentContext& context) {
    elf3d::Document document;
    const auto mesh = document.create_mesh("document-quad");
    const auto normal_texture = create_normal_texture(document);
    if (!mesh) {
        return 49;
    }
    if (!normal_texture) {
        return 49;
    }
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color = {0.1F, 0.2F, 0.3F, 0.4F};
    material_description.double_sided = true;
    material_description.alpha_mode = elf3d::AlphaMode::blend;
    material_description.normal_texture = normal_texture.value();
    material_description.normal_scale = 0.65F;
    const auto material = document.create_material(material_description);
    if (!material) {
        return 49;
    }

    const auto primitive =
        document.create_primitive(mesh.value(), material.value(), tangent_quad());
    if (!primitive) {
        return 49;
    }
    if (!context.scene.set_document(std::move(document))) {
        return 49;
    }
    const auto model = context.scene.create_entity();
    const auto camera =
        context.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model) {
        return 49;
    }
    if (!camera) {
        return 49;
    }
    if (!position_test_camera(context.scene, camera.value())) {
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
    const elf3d::SceneStatistics expected_scene{2, 1, 1, 1, 1, 4, 6, 2, 1, 1, 1, 4, 0, 0, 1};
    const elf3d::RenderStatistics expected_render{1, 2, 4, 6, 1, 1, 1, 0, 0};
    if (!list || !render) {
        return false;
    }
    if (device.draws.empty() || device.draw_texture_presence.empty() ||
        device.texture_descriptions.empty()) {
        return false;
    }
    const std::array<bool, 12> matches{
        list.value().items.size() == 1,
        document.scene.statistics() == expected_scene,
        legacy_statistics_equal(render.value(), expected_render),
        device.upload_count == 1,
        device.mesh_layouts ==
            std::vector<elf3d::graphics::VertexLayout>{
                elf3d::graphics::VertexLayout::
                    position_normal_float3_texcoord2_float2_color_float4_tangent_float4},
        device.mesh_uploaded_bytes == std::vector<std::size_t>{4U * 18U * sizeof(float)},
        device.draws.size() == 2,
        device.draws.back().double_sided,
        device.draws.back().alpha_mode == elf3d::AlphaMode::blend,
        device.draws.back().normal_scale == 0.65F,
        device.draw_texture_presence.back()[2],
        device.texture_descriptions.back().format == elf3d::graphics::TextureFormat::rgba8_unorm,
    };
    return std::all_of(matches.begin(), matches.end(), [](bool value) { return value; });
}

[[nodiscard]] bool has_expected_unlit_render(const elf3d::Result<elf3d::RenderStatistics>& render,
                                             const FakeDeviceState& device) {
    if (!render || device.draw_texture_presence.empty()) {
        return false;
    }
    const std::array<bool, 3> matches{render.value().gpu_texture_uploads == 0,
                                      render.value().environment_preparations == 0,
                                      !device.draw_texture_presence.back()[2]};
    return std::all_of(matches.begin(), matches.end(), [](bool value) { return value; });
}

void report_document_render_failure(const elf3d::Result<elf3d::RenderStatistics>& render,
                                    const FakeDeviceState& state) {
    if (!render) {
        return;
    }
    const std::size_t mesh_bytes =
        state.mesh_uploaded_bytes.empty() ? 0U : state.mesh_uploaded_bytes[0];
    const bool normal_present =
        !state.draw_texture_presence.empty() && state.draw_texture_presence.back()[2];
    std::cerr << "document render stats bindings=" << render.value().texture_bindings
              << " uploads=" << render.value().gpu_texture_uploads
              << " unique=" << render.value().unique_gpu_textures
              << " mesh_uploads=" << state.upload_count << " mesh_bytes=" << mesh_bytes
              << " draws=" << state.draws.size() << " normal_present=" << normal_present
              << " texture_descriptions=" << state.texture_descriptions.size() << '\n';
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
    auto renderer = elf3d::renderer::Renderer::create(
        std::move(device), engine_token,
        elf3d::renderer::tests::make_test_studio_environment_source());
    if (!renderer) {
        return 49;
    }
    const auto list =
        elf3d::renderer::build_render_list(document.scene, document.camera, {640, 360});
    FakeRenderTarget target;
    elf3d::renderer::RenderRequest unlit_request{document.camera, {}, {}, {}, {}};
    unlit_request.options.shading_mode = elf3d::RenderShadingMode::unlit;
    const auto unlit_render = renderer.value()->render(document.scene, target, unlit_request);
    if (!has_expected_unlit_render(unlit_render, device_state_owner->state())) {
        return 49;
    }
    const elf3d::renderer::RenderRequest request{document.camera, {}, {}, {}, {}};
    const auto render = renderer.value()->render(document.scene, target, request);
    if (!has_expected_document_render(document, list, render, device_state_owner->state())) {
        report_document_render_failure(render, device_state_owner->state());
        return 49;
    }
    return 0;
}
