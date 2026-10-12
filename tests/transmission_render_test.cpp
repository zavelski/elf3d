#include <elf3d/elf3d.h>

#include <array>
#include <memory>

namespace {
using namespace elf3d;

struct OpticalFixture {
    std::unique_ptr<Scene> scene;
    std::unique_ptr<Viewport> viewport;
    EntityId camera;
    MaterialHandle glass;
    EntityId front;
};

[[nodiscard]] Result<void> create_optical_geometry(OpticalFixture& fixture)
{
    constexpr std::array<VertexPositionNormalTexCoord, 4> vertices{
        {{{-1, -1, 0}, {0, 0, 1}, {0, 0}, {0, 1}},
         {{1, -1, 0}, {0, 0, 1}, {1, 0}, {1, 1}},
         {{1, 1, 0}, {0, 0, 1}, {1, 1}, {1, 0}},
         {{-1, 1, 0}, {0, 0, 1}, {0, 1}, {0, 0}}}};
    constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    const auto mesh = fixture.scene->create_mesh(TexturedMeshDataView{vertices, indices});
    MaterialDescription background;
    background.metallic_factor = 0;
    background.base_color = {1, 0, 0, 1};
    background.unlit = true;
    const auto red = fixture.scene->create_material(background);
    if (!mesh || !red)
        return Error{ErrorCode::invalid_argument, "Optical test background creation failed"};
    const auto rear = fixture.scene->create_model_entity(mesh.value(), red.value());
    MaterialDescription glass;
    glass.metallic_factor = 0;
    glass.roughness_factor = .045F;
    glass.ior = 1;
    glass.transmission = TransmissionDescription{1};
    glass.volume = VolumeDescription{1, {}, {}, 1.0F, {1, 1, 1}};
    const auto material = fixture.scene->create_material(glass);
    if (!rear || !material)
        return Error{ErrorCode::invalid_argument, "Optical test glass creation failed"};
    const auto front = fixture.scene->create_model_entity(mesh.value(), material.value());
    if (!front)
        return front.error();
    Float4x4 transform;
    transform.elements[14] = .5F;
    fixture.glass = material.value();
    fixture.front = front.value();
    return fixture.scene->set_local_matrix(front.value(), transform);
}

[[nodiscard]] Result<OpticalFixture> create_fixture(Engine& engine)
{
    auto scene = engine.create_scene();
    auto viewport = engine.create_viewport({64, 64});
    if (!scene)
        return scene.error();
    if (!viewport)
        return viewport.error();
    OpticalFixture fixture{std::move(scene).value(), std::move(viewport).value()};
    const auto geometry = create_optical_geometry(fixture);
    if (!geometry)
        return geometry.error();
    const auto camera = fixture.scene->create_perspective_camera_entity({});
    if (!camera)
        return camera.error();
    fixture.camera = camera.value();
    Float4x4 transform;
    transform.elements[14] = 3;
    const auto positioned = fixture.scene->set_local_matrix(fixture.camera, transform);
    if (!positioned)
        return positioned.error();
    DisplayTransform display;
    display.tone_mapping = ToneMappingMode::none;
    fixture.viewport->set_display_transform(display);
    fixture.viewport->set_environment_lighting({0, 0});
    BasicLighting lighting;
    lighting.diffuse_intensity = 0;
    lighting.ambient_intensity = 0;
    fixture.viewport->set_basic_lighting(lighting);
    return fixture;
}

[[nodiscard]] bool center_is_red(Viewport& viewport)
{
    std::array<std::uint8_t, 64 * 64 * 4> pixels{};
    if (!viewport.read_color_pixels(pixels))
        return false;
    const std::size_t center = (32 * 64 + 32) * 4;
    return pixels[center] >= 250 && pixels[center + 1] <= 2 && pixels[center + 2] <= 2;
}

[[nodiscard]] bool verify_optical_modes(OpticalFixture& fixture)
{
    auto description = fixture.scene->material_description(fixture.glass);
    if (!description)
        return false;
    for (AlphaMode mode : {AlphaMode::opaque, AlphaMode::mask, AlphaMode::blend}) {
        description.value().alpha_mode = mode;
        if (!fixture.scene->set_material_description(fixture.glass, description.value()))
            return false;
        if (!fixture.viewport->render(*fixture.scene, fixture.camera) ||
            !center_is_red(*fixture.viewport))
            return false;
        const auto statistics = fixture.viewport->render_statistics();
        if (statistics.transmission_scene_copies != 1 || statistics.draw_calls != 2)
            return false;
    }
    return true;
}

[[nodiscard]] bool verify_scaled_absorption(OpticalFixture& fixture)
{
    auto original = fixture.scene->material_description(fixture.glass);
    if (!original)
        return false;
    auto absorbing = original.value();
    absorbing.alpha_mode = AlphaMode::opaque;
    absorbing.volume->attenuation_distance = .3F;
    absorbing.volume->attenuation_color = {.0078431377F, .0078431377F, .0078431377F};
    Float4x4 transform;
    transform.elements[0] = transform.elements[5] = transform.elements[10] = .3048F;
    transform.elements[14] = .5F;
    if (!fixture.scene->set_local_matrix(fixture.front, transform) ||
        !fixture.scene->set_material_description(fixture.glass, absorbing) ||
        !fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    std::array<std::uint8_t, 64 * 64 * 4> pixels{};
    if (!fixture.viewport->read_color_pixels(pixels))
        return false;
    constexpr std::size_t center = (32 * 64 + 32) * 4;
    const bool dark = pixels[center] >= 18 && pixels[center] <= 23 && pixels[center + 1] == 0;
    transform = Float4x4{};
    transform.elements[14] = .5F;
    if (!fixture.scene->set_local_matrix(fixture.front, transform) ||
        !fixture.scene->set_material_description(fixture.glass, original.value()))
        return false;
    return dark;
}

[[nodiscard]] bool verify_clearcoat_layer(OpticalFixture& fixture)
{
    auto material = fixture.scene->material_description(fixture.glass);
    if (!material)
        return false;
    material.value().alpha_mode = AlphaMode::opaque;
    material.value().clearcoat = ClearcoatDescription{};
    if (!fixture.scene->set_material_description(fixture.glass, material.value()) ||
        !fixture.viewport->render(*fixture.scene, fixture.camera) ||
        !center_is_red(*fixture.viewport))
        return false;
    material.value().clearcoat->factor = 1;
    material.value().clearcoat->roughness_factor = .2F;
    if (!fixture.scene->set_material_description(fixture.glass, material.value()) ||
        !fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    std::array<std::uint8_t, 64 * 64 * 4> pixels{};
    if (!fixture.viewport->read_color_pixels(pixels))
        return false;
    constexpr std::size_t center = (32 * 64 + 32) * 4;
    // No incident light: the coat transmits 96% of the base at normal incidence.
    return pixels[center] >= 249 && pixels[center] <= 252 && pixels[center + 1] == 0;
}

[[nodiscard]] bool verify_transactional_optical_resize(OpticalFixture& fixture)
{
    const auto texture = fixture.viewport->color_texture();
    const auto rejected = fixture.viewport->resize({0xffffffffU, 64});
    if (rejected || fixture.viewport->color_texture() != texture)
        return false;
    return fixture.viewport->render(*fixture.scene, fixture.camera) &&
           fixture.viewport->render_statistics().transmission_scene_copies == 1;
}

[[nodiscard]] bool verify_optical_zero_extent(OpticalFixture& fixture)
{
    if (!fixture.viewport->resize({0, 0}) || fixture.viewport->framebuffer_valid())
        return false;
    return fixture.viewport->resize({64, 64}) &&
           fixture.viewport->render(*fixture.scene, fixture.camera);
}

[[nodiscard]] bool verify_optical_resize(Engine& engine, OpticalFixture& fixture)
{
    auto second = engine.create_viewport({32, 16});
    if (!second)
        return false;
    if (!second.value()->render(*fixture.scene, fixture.camera))
        return false;
    if (second.value()->render_statistics().estimated_resident_transmission_bytes ==
        fixture.viewport->render_statistics().estimated_resident_transmission_bytes)
        return false;
    const auto original = fixture.viewport->color_texture();
    if (!fixture.viewport->resize({16, 32}) ||
        !fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    const auto expected = second.value()->render_statistics().estimated_resident_transmission_bytes;
    if (fixture.viewport->color_texture() == original ||
        fixture.viewport->render_statistics().estimated_resident_transmission_bytes != expected)
        return false;
    return verify_optical_zero_extent(fixture);
}

[[nodiscard]] bool verify_metallic_transmission_endpoint(OpticalFixture& fixture)
{
    auto material = fixture.scene->material_description(fixture.glass);
    if (!material)
        return false;
    material.value().metallic_factor = 1;
    material.value().alpha_mode = AlphaMode::opaque;
    if (!fixture.scene->set_material_description(fixture.glass, material.value()) ||
        !fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    if (fixture.viewport->render_statistics().transmission_scene_copies != 0)
        return false;
    material.value().metallic_factor = 0;
    return fixture.scene->set_material_description(fixture.glass, material.value()) &&
           fixture.viewport->render(*fixture.scene, fixture.camera);
}

[[nodiscard]] bool verify_optical_bypass(OpticalFixture& fixture)
{
    fixture.viewport->set_render_shading_mode(RenderShadingMode::unlit);
    if (!fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    if (fixture.viewport->render_statistics().transmission_scene_copies != 0)
        return false;
    fixture.viewport->set_render_shading_mode(RenderShadingMode::standard);
    auto description = fixture.scene->material_description(fixture.glass);
    if (!description)
        return false;
    description.value().transmission->factor = 0;
    if (!fixture.scene->set_material_description(fixture.glass, description.value()))
        return false;
    if (!fixture.viewport->render(*fixture.scene, fixture.camera))
        return false;
    return fixture.viewport->render_statistics().transmission_scene_copies == 0;
}
bool verify_optical_scene_replacement(Engine& engine, Viewport& viewport)
{
    auto replacement = engine.create_scene();
    if (!replacement)
        return false;
    auto camera = replacement.value()->create_perspective_camera_entity({});
    if (!camera || !viewport.render(*replacement.value(), camera.value()))
        return false;
    return viewport.render_statistics().transmission_scene_copies == 0;
}

bool verify_optical_fixture(Engine& engine, OpticalFixture& fixture)
{
    if (!verify_optical_modes(fixture) || !verify_scaled_absorption(fixture) ||
        !verify_clearcoat_layer(fixture) || !verify_transactional_optical_resize(fixture) ||
        !verify_optical_resize(engine, fixture) ||
        !verify_metallic_transmission_endpoint(fixture) || !verify_optical_bypass(fixture))
        return false;
    return true;
}
} // namespace

bool verify_transmission_rendering(elf3d::Engine& engine)
{
    auto created = create_fixture(engine);
    if (!created)
        return false;
    auto fixture = std::move(created).value();
    if (!verify_optical_fixture(engine, fixture))
        return false;
    return verify_optical_scene_replacement(engine, *fixture.viewport);
}
