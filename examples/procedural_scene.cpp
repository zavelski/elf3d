#include <elf3d/elf3d.h>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

namespace elf3d_examples {

struct ProceduralScene {
    std::unique_ptr<elf3d::Scene> scene;
    elf3d::EntityId model_entity;
    elf3d::EntityId camera_entity;
};

[[nodiscard]] elf3d::Result<ProceduralScene>
create_procedural_scene(elf3d::Engine& engine) noexcept {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();

    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::Result<elf3d::MeshHandle> mesh = scene->create_mesh({vertices, indices});
    if (!mesh) {
        return mesh.error();
    }

    elf3d::MaterialDescription material_description;
    material_description.base_color = {0.2F, 0.6F, 1.0F, 1.0F};
    const elf3d::Result<elf3d::MaterialHandle> material =
        scene->create_material(material_description);
    if (!material) {
        return material.error();
    }

    const elf3d::Result<elf3d::EntityId> model =
        scene->create_model_entity(mesh.value(), material.value());
    if (!model) {
        return model.error();
    }
    const elf3d::Result<elf3d::EntityId> camera = scene->create_perspective_camera_entity({});
    if (!camera) {
        return camera.error();
    }

    elf3d::Transform camera_transform;
    camera_transform.translation = {0.0F, 0.0F, 3.0F};
    const elf3d::Result<void> positioned =
        scene->set_local_transform(camera.value(), camera_transform);
    if (!positioned) {
        return positioned.error();
    }

    return ProceduralScene{std::move(scene), model.value(), camera.value()};
}

} // namespace elf3d_examples
