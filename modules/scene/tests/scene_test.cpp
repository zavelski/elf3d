#include <elf3d/assets/handle_access.h>
#include <elf3d/scene/storage.h>

#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>
#include <optional>

namespace {

bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) < 0.0001F;
}

} // namespace

int main() {
    const elf3d::SceneId id = elf3d::detail::SceneHandleAccess::create_scene(5, 1);
    elf3d::scene::Storage scene{id};
    const elf3d::Result<elf3d::EntityId> root = scene.create_entity();
    const elf3d::Result<elf3d::EntityId> child = scene.create_entity();
    const elf3d::Result<elf3d::EntityId> grandchild = scene.create_entity();
    if (!root || !child || !grandchild || root.value() == child.value()) {
        return 1;
    }

    elf3d::Transform root_transform;
    root_transform.translation = {10.0F, 0.0F, 0.0F};
    elf3d::Transform child_transform;
    child_transform.translation = {0.0F, 2.0F, 0.0F};
    elf3d::Float4x4 grandchild_matrix;
    grandchild_matrix.elements[14] = 3.0F;
    if (!scene.set_local_transform(root.value(), root_transform) ||
        !scene.set_local_transform(child.value(), child_transform) ||
        !scene.set_local_matrix(grandchild.value(), grandchild_matrix) ||
        !scene.set_parent(child.value(), root.value()) ||
        !scene.set_parent(grandchild.value(), child.value()) ||
        !scene.set_entity_name(grandchild.value(), "Imported child")) {
        return 2;
    }
    if (!scene.local_transform(child.value()) ||
        scene.local_transform(child.value()).value() != child_transform ||
        scene.local_transform(grandchild.value()).error().code() !=
            elf3d::ErrorCode::transform_requires_matrix_api ||
        scene.entity_name(grandchild.value()).value() != "Imported child") {
        return 3;
    }
    const elf3d::Result<elf3d::Float4x4> world = scene.world_matrix(grandchild.value());
    if (!world || !nearly_equal(world.value().elements[12], 10.0F) ||
        !nearly_equal(world.value().elements[13], 2.0F) ||
        !nearly_equal(world.value().elements[14], 3.0F)) {
        return 4;
    }
    if (scene.set_parent(root.value(), grandchild.value()).error().code() !=
            elf3d::ErrorCode::hierarchy_cycle ||
        scene.set_parent(root.value(), root.value()).error().code() !=
            elf3d::ErrorCode::invalid_parent_assignment) {
        return 5;
    }

    const elf3d::EntityId foreign = elf3d::detail::SceneHandleAccess::create_entity(
        elf3d::detail::SceneHandleAccess::create_scene(5, 2), 1);
    if (scene.local_transform(foreign).error().code() != elf3d::ErrorCode::invalid_entity) {
        return 6;
    }

    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = scene.create_mesh({vertices, indices});
    const auto material = scene.create_material({elf3d::Color4{1.0F, 0.0F, 0.0F, 1.0F}});
    const auto model = scene.create_model(mesh.value(), material.value());
    if (!mesh || !material || !model) {
        return 7;
    }
    const std::array<elf3d::ModelPrimitiveBinding, 2> primitives{{
        {mesh.value(), material.value()},
        {mesh.value(), material.value()},
    }};
    if (!scene.set_model_primitives(model.value(), primitives)) {
        return 8;
    }

    elf3d::PerspectiveCameraDescription invalid_camera;
    invalid_camera.near_plane = 0.0F;
    if (scene.create_perspective_camera(invalid_camera).error().code() !=
        elf3d::ErrorCode::invalid_camera_configuration) {
        return 9;
    }
    if (!scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{})) {
        return 10;
    }

    const elf3d::Bounds3 bounds = scene.world_bounds();
    const elf3d::SceneStatistics statistics = scene.statistics();
    if (!bounds.is_valid || bounds.minimum != elf3d::Float3{0.0F, 0.0F, 0.0F} ||
        bounds.maximum != elf3d::Float3{1.0F, 1.0F, 0.0F} ||
        statistics != elf3d::SceneStatistics{5, 1, 1, 1, 1, 3, 3, 1}) {
        return 11;
    }

    const std::uint64_t hierarchy_before_visibility = scene.hierarchy_revision();
    if (!scene.set_parent(model.value(), root.value())) {
        return 13;
    }
    if (scene.hierarchy_revision() == hierarchy_before_visibility) {
        return 14;
    }
    const std::uint64_t visibility_before = scene.visibility_revision();
    if (!scene.set_entity_visible(root.value(), false) ||
        !scene.entity_local_visibility(child.value()).value() ||
        scene.entity_effective_visibility(child.value()).value() ||
        scene.entity_effective_visibility(model.value()).value()) {
        return 15;
    }
    const std::uint64_t visibility_after_hide = scene.visibility_revision();
    if (visibility_after_hide == visibility_before ||
        !scene.set_entity_visible(root.value(), false) ||
        scene.visibility_revision() != visibility_after_hide) {
        return 16;
    }
    const auto hidden_filter = elf3d::scene::make_visibility_filter(scene, std::nullopt);
    if (!hidden_filter || scene.visible_world_bounds(hidden_filter.value()).is_valid) {
        return 17;
    }
    if (!scene.show_entity_and_ancestors(model.value()) ||
        !scene.entity_effective_visibility(model.value()).value() ||
        !scene.visible_world_bounds(hidden_filter.value()).is_valid) {
        return 18;
    }
    if (!scene.set_entity_visible(child.value(), false) ||
        scene.entity_effective_visibility(grandchild.value()).value() ||
        !scene.show_entity_and_ancestors(grandchild.value()) ||
        !scene.entity_effective_visibility(grandchild.value()).value()) {
        return 19;
    }
    if (!scene.show_all_entities() || !scene.entity_effective_visibility(model.value()).value()) {
        return 20;
    }

    if (!scene.destroy_entity(root.value()) || scene.entity(child.value()) ||
        scene.entity(grandchild.value())) {
        return 12;
    }
    return 0;
}
