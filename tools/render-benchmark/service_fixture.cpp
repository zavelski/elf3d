#include "service_fixture.h"

#include <array>
#include <vector>

namespace elf3d::tools {
namespace {
[[nodiscard]] Result<void> add_model(Scene& scene, MeshHandle mesh,
                                     const MaterialDescription& description)
{
    const auto material = scene.create_material(description);
    if (!material)
        return material.error();
    const auto entity = scene.create_model_entity(mesh, material.value());
    if (!entity)
        return entity.error();
    return {};
}
[[nodiscard]] Result<MeshHandle> triangle(Scene& scene)
{
    const std::array<VertexPositionNormalTexCoord, 3> vertices{{{{-1, -1, 0}, {0, 0, 1}, {0, 0}},
                                                                {{1, -1, 0}, {0, 0, 1}, {1, 0}},
                                                                {{0, 1, 0}, {0, 0, 1}, {0.5F, 1}}}};
    constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
    return scene.create_mesh(TexturedMeshDataView{vertices, indices});
}
[[nodiscard]] Result<void> large_mesh(Scene& scene)
{
    constexpr std::uint32_t side = 512;
    constexpr std::uint32_t row = side + 1;
    std::vector<VertexPositionNormal> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(row * row);
    indices.reserve(side * side * 6);
    for (std::uint32_t y = 0; y <= side; ++y)
        for (std::uint32_t x = 0; x <= side; ++x)
            vertices.push_back({{static_cast<float>(x), static_cast<float>(y), 0}, {0, 0, 1}});
    for (std::uint32_t y = 0; y < side; ++y)
        for (std::uint32_t x = 0; x < side; ++x) {
            const auto a = y * row + x;
            const auto b = a + row;
            indices.insert(indices.end(), {a, a + 1, b, b, a + 1, b + 1});
        }
    const auto mesh = scene.create_mesh(MeshDataView{vertices, indices});
    if (!mesh)
        return mesh.error();
    return add_model(scene, mesh.value(), {});
}
[[nodiscard]] Result<void> many_meshes(Scene& scene)
{
    for (unsigned index = 0; index < 2048; ++index) {
        // Distinct owners intentionally avoid the shared-mesh upload shortcut.
        const auto mesh = triangle(scene);
        if (!mesh)
            return mesh.error();
        if (const auto model = add_model(scene, mesh.value(), {}); !model)
            return model.error();
    }
    return {};
}
[[nodiscard]] Result<void> images(Scene& scene, ServiceFixture fixture)
{
    const bool large = fixture == ServiceFixture::large_image;
    const Extent2D extent = large ? Extent2D{4096, 2048} : Extent2D{512, 512};
    const auto count = large ? 1U : 32U;
    std::vector<std::byte> pixels(std::size_t{extent.width} * extent.height * 4, std::byte{192});
    const auto mesh = triangle(scene);
    if (!mesh)
        return mesh.error();
    for (unsigned index = 0; index < count; ++index) {
        pixels[0] = static_cast<std::byte>(index); // Index is bounded by 32.
        const auto image =
            scene.create_image({extent.width, extent.height, PixelFormat::rgba8_unorm, pixels});
        if (!image)
            return image.error();
        TextureDescription texture;
        texture.image = image.value();
        texture.sampler.min_filter = TextureFilter::linear_mipmap_linear;
        const auto created = scene.create_texture(texture);
        if (!created)
            return created.error();
        MaterialDescription material;
        material.base_color_texture = created.value();
        if (const auto model = add_model(scene, mesh.value(), material); !model)
            return model.error();
    }
    return {};
}
} // namespace
Result<std::unique_ptr<Scene>> make_service_fixture(Engine& engine, ServiceFixture fixture)
{
    auto scene = engine.create_scene();
    if (!scene)
        return scene.error();
    const auto created = fixture == ServiceFixture::large_mesh    ? large_mesh(*scene.value())
                         : fixture == ServiceFixture::many_meshes ? many_meshes(*scene.value())
                                                                  : images(*scene.value(), fixture);
    if (!created)
        return created.error();
    return scene;
}
} // namespace elf3d::tools
