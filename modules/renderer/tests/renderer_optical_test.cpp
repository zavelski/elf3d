#include <elf3d/internal/renderer.h>
#include <elf3d/internal/scene.h>

#include "renderer_test_support.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
using namespace elf3d;
using namespace elf3d::renderer::tests;
constexpr std::uint64_t token = 17;

PrimitiveData quad(float depth)
{
    PrimitiveData data;
    data.positions = {{-1, -1, depth}, {1, -1, depth}, {1, 1, depth}, {-1, 1, depth}};
    data.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    data.texcoord0 = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    data.texcoord1 = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

int prepare_ordering_scene(scene::Storage& scene, std::array<PrimitiveId, 3>& primitives,
                           EntityId& entity)
{
    Document document;
    auto mesh = document.create_mesh();
    ModelMaterialDescription blend;
    blend.alpha_mode = AlphaMode::blend;
    blend.double_sided = true;
    auto material = document.create_material(blend);
    if (!mesh || !material)
        return 1;
    for (std::size_t i = 0; i < 3; ++i) {
        auto p = document.create_primitive(mesh.value(), material.value(),
                                           quad(static_cast<float>(i) - 1));
        if (!p)
            return 1;
        primitives[i] = p.value();
    }
    if (!scene.set_document(std::move(document)))
        return 1;
    auto model = scene.create_entity();
    if (!model || !scene.set_model_document_primitives(model.value(), primitives))
        return 1;
    entity = model.value();
    return 0;
}
int verify_reverse_order(scene::Storage& scene, EntityId camera, EntityId entity,
                         std::array<PrimitiveId, 3> primitives)
{
    Float4x4 matrix;
    matrix.elements[0] = -1;
    matrix.elements[10] = -1;
    matrix.elements[14] = -4;
    if (!scene.set_local_matrix(camera, matrix))
        return 1;
    auto back = renderer::build_render_list(scene, camera, {640, 360});
    if (!back || back.value().items.size() != 3)
        return 4;
    for (std::size_t i = 0; i < 3; ++i)
        if (back.value().items[i].primitive_index != 2 - i)
            return 5;
    // Same origin, actual geometry differs; changing submitted primitive order remains correct.
    std::swap(primitives[0], primitives[2]);
    if (!scene.set_model_document_primitives(entity, primitives))
        return 1;
    auto reordered = renderer::build_render_list(scene, camera, {640, 360});
    if (!reordered || reordered.value().items.front().primitive_index != 0)
        return 6;
    return 0;
}
int verify_ordering()
{
    scene::Storage scene(detail::SceneHandleAccess::create_scene(token, 81));
    std::array<PrimitiveId, 3> primitives{};
    EntityId entity;
    if (prepare_ordering_scene(scene, primitives, entity))
        return 1;
    auto camera = scene.create_perspective_camera({});
    if (!camera)
        return 1;
    Float4x4 matrix;
    matrix.elements[14] = 4;
    if (!scene.set_local_matrix(camera.value(), matrix))
        return 1;
    auto front = renderer::build_render_list(scene, camera.value(), {640, 360});
    if (!front || front.value().items.size() != 3)
        return 2;
    for (std::size_t i = 0; i < 3; ++i)
        if (front.value().items[i].primitive_index != i)
            return 3;
    return verify_reverse_order(scene, camera.value(), entity, primitives);
}

int bind_optical_scene(scene::Storage& scene, std::array<PrimitiveId, 2> primitives,
                       EntityId& model_entity, EntityId& camera_entity)
{
    auto model = scene.create_entity(), camera = scene.create_perspective_camera({});
    if (!model || !camera || !scene.set_model_document_primitives(model.value(), primitives))
        return 10;
    Float4x4 matrix;
    matrix.elements[14] = 4;
    if (!scene.set_local_matrix(camera.value(), matrix))
        return 10;
    model_entity = model.value();
    camera_entity = camera.value();
    return 0;
}

int prepare_optical_scene(scene::Storage& scene, EntityId& model_entity, EntityId& camera_entity)
{
    Document document;
    const std::array<std::byte, 4> pixel{std::byte{64}, std::byte{128}, std::byte{192},
                                         std::byte{32}};
    auto image = document.create_image({1, 1, PixelFormat::rgba8_unorm, pixel});
    auto sampler = document.create_sampler();
    if (!image || !sampler)
        return 10;
    auto texture = document.create_texture({image.value(), sampler.value()});
    if (!texture)
        return 10;
    ModelMaterialDescription description;
    description.metallic_factor = 0;
    description.ior = 0;
    description.specular_color_factor = {30, 2, .5F};
    description.base_color_texture = texture.value();
    description.normal_texture = texture.value();
    description.specular_texture = description.specular_color_texture = texture.value();
    description.specular_texture_mapping.texcoord_set = 1;
    description.transmission = ModelTransmissionDescription{1, texture.value(), {1}};
    description.volume =
        ModelVolumeDescription{1, texture.value(), {1}, .3F, {.007843F, .007843F, .007843F}};
    description.clearcoat = ModelClearcoatDescription{};
    auto& coat = *description.clearcoat;
    coat.factor = .7F;
    coat.roughness_factor = .2F;
    coat.texture = coat.roughness_texture = coat.normal_texture = texture.value();
    coat.normal_texture_mapping.texcoord_set = 1;
    coat.normal_scale = .4F;
    auto material = document.create_material(description), opaque = document.create_material();
    auto mesh = document.create_mesh();
    if (!material || !opaque || !mesh)
        return 10;
    auto glass = document.create_primitive(mesh.value(), material.value(), quad(0));
    auto background = document.create_primitive(mesh.value(), opaque.value(), quad(-1));
    if (!glass || !background || !scene.set_document(std::move(document)))
        return 10;
    return bind_optical_scene(scene, {glass.value(), background.value()}, model_entity,
                              camera_entity);
}
bool verify_optical_draw(const FakeDeviceState& state)
{
    if (state.indexed_batch_count != 1 || state.draws.size() != 2)
        return false;
    const auto& draw = state.draws[1];
    const std::array<bool, 8> checks{!state.draws[0].optical_queue,
                                     draw.optical_queue,
                                     draw.alpha_mode == AlphaMode::opaque,
                                     draw.ior == 0,
                                     draw.specular_color_factor.x == 30,
                                     !draw.clearcoat_uses_tangent_basis,
                                     draw.texture_mappings[11].texcoord_set == 1,
                                     state.texture_upload_count == 3};
    return std::all_of(checks.begin(), checks.end(), [](bool value) { return value; });
}
int verify_independent_optical_target(renderer::Renderer& renderer, scene::Storage& scene,
                                      renderer::RenderRequest request)
{
    Result<RenderStatistics> rendered = RenderStatistics{};
    FakeRenderTarget other;
    request.options.shading_mode = RenderShadingMode::standard;
    other.fail_snapshot = true;
    if (renderer.render(scene, other, request))
        return 18;
    other.fail_snapshot = false;
    rendered = renderer.render(scene, other, request);
    if (!rendered || other.snapshot_copies != 1 || other.snapshot.extent() != other.extent())
        return 19;
    return 0;
}

int verify_optical_lifetimes(renderer::Renderer& renderer, scene::Storage& scene,
                             FakeRenderTarget& target, renderer::RenderRequest request)
{
    auto rendered = renderer.render(scene, target, request);
    if (!rendered || rendered.value().gpu_texture_uploads != 0 || target.snapshot_copies != 2)
        return 16;
    request.options.shading_mode = RenderShadingMode::unlit;
    rendered = renderer.render(scene, target, request);
    if (!rendered || rendered.value().transmission_scene_copies != 0 || target.snapshot_copies != 2)
        return 17;
    return verify_independent_optical_target(renderer, scene, request);
}
int verify_optical_frame(const Result<RenderStatistics>& rendered, const FakeRenderTarget& target,
                         const FakeDeviceState& state)
{
    if (!rendered || target.snapshot_copies != 1 ||
        rendered.value().transmission_scene_copies != 1 ||
        rendered.value().estimated_resident_transmission_bytes !=
            target.transmission_scene_bytes() ||
        !verify_optical_draw(state))
        return 13;
    for (std::size_t i : {0U, 2U, 5U, 6U, 7U, 8U, 9U, 10U, 11U})
        if (!state.draw_texture_presence[1][i])
            return 14;
    if (state.texture_upload_count != 3)
        return 15; // LUT + shared image in linear/sRGB roles.
    return 0;
}

int verify_optical_transport()
{
    scene::Storage scene(detail::SceneHandleAccess::create_scene(token, 82));
    EntityId model, camera;
    if (prepare_optical_scene(scene, model, camera))
        return 10;
    auto primitive = scene.runtime_primitive(model, 0);
    if (!primitive || !primitive.value().has_tangent_basis() ||
        primitive.value().material_view.transmission_factor != 1 ||
        primitive.value().material_view.thickness_factor != 1)
        return 11;
    auto device = std::make_unique<FakeDevice>();
    auto* state = &device->state();
    auto renderer =
        renderer::Renderer::create(std::move(device), token, make_test_studio_environment_source());
    if (!renderer)
        return 12;
    FakeRenderTarget target;
    renderer::RenderRequest request;
    request.camera = camera;
    auto rendered = renderer.value()->render(scene, target, request);
    if (const auto frame = verify_optical_frame(rendered, target, *state))
        return frame;
    return verify_optical_lifetimes(*renderer.value(), scene, target, request);
}
} // namespace

int elf3d_renderer_optical_test()
{
    const int ordering = verify_ordering();
    if (ordering)
        return ordering;
    const int optical = verify_optical_transport();
    if (!optical)
        std::cout << "Primitive bounds sorting, optical queue, all texture slots and snapshot "
                     "scheduling passed\n";
    return optical;
}
