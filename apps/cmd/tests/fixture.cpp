#include "fixture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

elf3d::Result<elf3d::Document> make_fixture()
{
    elf3d::Document document;
    const auto scene = document.create_scene("First");
    const auto second = document.create_scene("Second");
    const auto root = document.create_node("Shared root");
    const auto child = document.create_node("Child");
    const auto instance = document.create_node("Instance");
    const auto camera = document.create_node("Camera");
    const auto mesh = document.create_mesh("Shared mesh");
    const std::array<std::byte, 4> pixels{std::byte{40}, std::byte{80}, std::byte{120},
                                          std::byte{255}};
    const auto image = document.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixels});
    const auto sampler = document.create_sampler();
    const std::array created{scene.has_value(), second.has_value(),   root.has_value(),
                             child.has_value(), instance.has_value(), camera.has_value(),
                             mesh.has_value(),  image.has_value(),    sampler.has_value()};
    if (!std::all_of(created.begin(), created.end(), [](bool value) { return value; })) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Fixture construction failed"};
    }
    const auto texture = document.create_texture({image.value(), sampler.value()});
    if (!texture) {
        return texture.error();
    }
    elf3d::ModelMaterialDescription description;
    description.base_color = {0.2F, 0.4F, 0.6F, 1.0F};
    description.roughness_factor = 0.7F;
    description.base_color_texture = texture.value();
    const auto material = document.create_material(description);
    if (!material) {
        return material.error();
    }
    elf3d::PrimitiveData geometry;
    geometry.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    geometry.texcoord0 = {{0, 0}, {1, 0}, {0, 1}};
    geometry.indices = {0U, 1U, 2U};
    const auto primitive =
        document.create_primitive(mesh.value(), material.value(), std::move(geometry));
    if (!primitive) {
        return primitive.error();
    }
    // A 90-degree Z rotation with scale 2 and authored translation.
    elf3d::Float4x4 matrix;
    matrix.elements = {0, 2, 0, 0, -2, 0, 0, 0, 0, 0, 2, 0, 10, 20, 30, 1};
    elf3d::Float4x4 child_matrix;
    child_matrix.elements[12] = 5.0F;
    const std::array configured{document.set_node_mesh(child.value(), mesh.value()),
                                document.set_node_mesh(instance.value(), mesh.value()),
                                document.set_parent(child.value(), root.value()),
                                document.set_node_matrix(root.value(), matrix),
                                document.set_node_matrix(child.value(), child_matrix),
                                document.set_node_perspective_camera(camera.value(), {}),
                                document.add_scene_root(scene.value(), root.value()),
                                document.add_scene_root(second.value(), root.value()),
                                document.add_scene_root(second.value(), instance.value()),
                                document.add_scene_root(second.value(), camera.value()),
                                document.clear_default_scene()};
    for (const auto& operation : configured) {
        if (!operation) {
            return operation.error();
        }
    }
    return document;
}
