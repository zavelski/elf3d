#include <elf3d/internal/scene.h>
#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

namespace {
using namespace elf3d;

[[nodiscard]] PrimitiveData geometry()
{
    PrimitiveData data;
    data.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    data.normals.resize(4, {0, 0, 1});
    data.texcoord0 = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    data.texcoord1 = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    data.colors = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}};
    data.indices = {0, 1, 2, 0, 2, 3};
    return data;
}

[[nodiscard]] Result<MaterialId> normal_material(Document& document, std::uint32_t uv)
{
    constexpr std::array<std::byte, 4> pixel{std::byte{128}, std::byte{128}, std::byte{255},
                                             std::byte{255}};
    const auto sampler = document.create_sampler().value();
    const auto image = document.create_image({1, 1, PixelFormat::rgba8_unorm, pixel}).value();
    const auto texture = document.create_texture({image, sampler}).value();
    ModelMaterialDescription material;
    material.normal_texture = texture;
    material.normal_texture_mapping.texcoord_set = uv;
    return document.create_material(material);
}

[[nodiscard]] Result<EntityId> prepare(scene::Storage& storage, std::uint32_t uv)
{
    Document document;
    const auto first = normal_material(document, uv).value();
    const auto mesh = document.create_mesh().value();
    auto data = geometry();
    const auto primitive = document.create_primitive(mesh, first, std::move(data)).value();
    const auto loaded = storage.set_document(std::move(document));
    if (!loaded)
        return loaded.error();
    const auto entity = storage.create_entity().value();
    const std::array<PrimitiveId, 1> primitives{primitive};
    const auto bound = storage.set_model_document_primitives(entity, primitives);
    if (!bound)
        return bound.error();
    const auto instance = storage.create_entity().value();
    const auto reused = storage.set_model_document_primitives(instance, primitives);
    if (!reused)
        return reused.error();
    return entity;
}

[[nodiscard]] bool canonical_geometry(const scene::Storage& storage)
{
    const auto data = storage.document().primitive_at(0).value().data;
    const auto expected = geometry();
    return data.positions.size() == 4U && data.tangents.empty() &&
           std::ranges::equal(data.indices, expected.indices);
}

[[nodiscard]] bool copied_attributes(const scene::RuntimePrimitiveView& runtime)
{
    const auto expected = geometry();
    for (std::size_t vertex = 0; vertex < runtime.vertex_count(); ++vertex) {
        const auto source = std::ranges::find(expected.positions, runtime.position(vertex));
        if (source == expected.positions.end())
            return false;
        const auto index = static_cast<std::size_t>(source - expected.positions.begin());
        if (runtime.color(vertex) != expected.colors[index] ||
            runtime.texcoord1(vertex) != expected.texcoord1[index])
            return false;
    }
    return true;
}

[[nodiscard]] bool uv1_orientation(const scene::RuntimePrimitiveView& runtime)
{
    bool positive = false;
    bool negative = false;
    for (std::size_t vertex = 0; vertex < runtime.vertex_count(); ++vertex) {
        const auto tangent = runtime.tangent(vertex);
        positive = positive || tangent.w > 0;
        negative = negative || tangent.w < 0;
        if (std::abs(tangent.y) < 0.6F)
            return false;
    }
    return positive && negative;
}

[[nodiscard]] int uniform_tangents()
{
    scene::Storage storage{detail::SceneHandleAccess::create_scene(19, 1)};
    const auto entity = prepare(storage, 1);
    if (!entity)
        return 1;
    const auto runtime = storage.runtime_primitive(entity.value(), 0).value();
    if (!runtime.has_tangent_basis() || runtime.vertex_count() != 6U ||
        !canonical_geometry(storage) || !copied_attributes(runtime))
        return 2;
    const auto instance = storage.entities()[1]->id;
    const auto reused = storage.runtime_primitive(instance, 0).value();
    return uv1_orientation(runtime) && runtime.mesh == reused.mesh ? 0 : 4;
}

} // namespace

int elf3d_scene_tangent_test()
{
    return uniform_tangents();
}
