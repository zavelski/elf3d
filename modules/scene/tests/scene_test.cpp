#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>
#include <optional>
#include <span>
#include <utility>

import elf.assets;
import elf.model;
import elf.scene;

namespace {

bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) < 0.0001F;
}

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(5, value);
}

constexpr std::array<elf3d::VertexPositionNormal, 3> triangle_vertices{{
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
}};
constexpr std::array<std::uint32_t, 3> triangle_indices{{0, 1, 2}};

struct HierarchyFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId root;
    elf3d::EntityId child;
    elf3d::EntityId grandchild;
};

struct RuntimeSceneFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId root;
    elf3d::EntityId child;
    elf3d::EntityId grandchild;
    elf3d::EntityId model;
};

[[nodiscard]] std::optional<HierarchyFixture> make_hierarchy_fixture(std::uint64_t value) {
    elf3d::scene::Storage scene{scene_id(value)};
    const elf3d::Result<elf3d::EntityId> root = scene.create_entity();
    const elf3d::Result<elf3d::EntityId> child = scene.create_entity();
    const elf3d::Result<elf3d::EntityId> grandchild = scene.create_entity();
    if (!root || !child || !grandchild || root.value() == child.value()) {
        return std::nullopt;
    }
    return HierarchyFixture{std::move(scene), root.value(), child.value(), grandchild.value()};
}

[[nodiscard]] bool configure_hierarchy(HierarchyFixture& fixture) {
    elf3d::Transform root_transform;
    root_transform.translation = {10.0F, 0.0F, 0.0F};
    elf3d::Transform child_transform;
    child_transform.translation = {0.0F, 2.0F, 0.0F};
    elf3d::Float4x4 grandchild_matrix;
    grandchild_matrix.elements[14] = 3.0F;
    return fixture.scene.set_local_transform(fixture.root, root_transform) &&
           fixture.scene.set_local_transform(fixture.child, child_transform) &&
           fixture.scene.set_local_matrix(fixture.grandchild, grandchild_matrix) &&
           fixture.scene.set_parent(fixture.child, fixture.root) &&
           fixture.scene.set_parent(fixture.grandchild, fixture.child) &&
           fixture.scene.set_entity_name(fixture.grandchild, "Imported child");
}

[[nodiscard]] bool has_expected_local_state(const HierarchyFixture& fixture) {
    elf3d::Transform child_transform;
    child_transform.translation = {0.0F, 2.0F, 0.0F};
    return fixture.scene.local_transform(fixture.child) &&
           fixture.scene.local_transform(fixture.child).value() == child_transform &&
           fixture.scene.local_transform(fixture.grandchild).error().code() ==
               elf3d::ErrorCode::transform_requires_matrix_api &&
           fixture.scene.entity_name(fixture.grandchild).value() == "Imported child";
}

[[nodiscard]] bool has_expected_world_matrix(const HierarchyFixture& fixture) {
    const elf3d::Result<elf3d::Float4x4> world = fixture.scene.world_matrix(fixture.grandchild);
    return world && nearly_equal(world.value().elements[12], 10.0F) &&
           nearly_equal(world.value().elements[13], 2.0F) &&
           nearly_equal(world.value().elements[14], 3.0F);
}

[[nodiscard]] bool rejects_invalid_parentage(HierarchyFixture& fixture) {
    return fixture.scene.set_parent(fixture.root, fixture.grandchild).error().code() ==
               elf3d::ErrorCode::hierarchy_cycle &&
           fixture.scene.set_parent(fixture.root, fixture.root).error().code() ==
               elf3d::ErrorCode::invalid_parent_assignment;
}

[[nodiscard]] bool rejects_foreign_entity(const HierarchyFixture& fixture) {
    const elf3d::EntityId foreign =
        elf3d::detail::SceneHandleAccess::create_entity(scene_id(99), 1);
    return fixture.scene.local_transform(foreign).error().code() ==
           elf3d::ErrorCode::invalid_entity;
}

[[nodiscard]] int verify_hierarchy_contract() {
    std::optional<HierarchyFixture> fixture = make_hierarchy_fixture(1);
    if (!fixture) {
        return 1;
    }
    if (!configure_hierarchy(*fixture)) {
        return 2;
    }
    if (!has_expected_local_state(*fixture)) {
        return 3;
    }
    if (!has_expected_world_matrix(*fixture)) {
        return 4;
    }
    if (!rejects_invalid_parentage(*fixture)) {
        return 5;
    }
    if (!rejects_foreign_entity(*fixture)) {
        return 6;
    }
    return 0;
}

[[nodiscard]] std::optional<RuntimeSceneFixture> make_runtime_scene_fixture() {
    std::optional<HierarchyFixture> hierarchy = make_hierarchy_fixture(2);
    if (!hierarchy || !configure_hierarchy(*hierarchy)) {
        return std::nullopt;
    }
    const auto mesh = hierarchy->scene.create_mesh({triangle_vertices, triangle_indices});
    const auto material = hierarchy->scene.create_material({elf3d::Color4{1.0F, 0.0F, 0.0F, 1.0F}});
    if (!mesh || !material) {
        return std::nullopt;
    }
    const auto model = hierarchy->scene.create_model(mesh.value(), material.value());
    if (!model) {
        return std::nullopt;
    }
    const std::array<elf3d::ModelPrimitiveBinding, 2> primitives{{
        {mesh.value(), material.value()},
        {mesh.value(), material.value()},
    }};
    if (!hierarchy->scene.set_model_primitives(model.value(), primitives)) {
        return std::nullopt;
    }
    return RuntimeSceneFixture{std::move(hierarchy->scene), hierarchy->root, hierarchy->child,
                               hierarchy->grandchild, model.value()};
}

[[nodiscard]] int verify_camera_and_statistics(RuntimeSceneFixture& fixture) {
    elf3d::PerspectiveCameraDescription invalid_camera;
    invalid_camera.near_plane = 0.0F;
    if (fixture.scene.create_perspective_camera(invalid_camera).error().code() !=
        elf3d::ErrorCode::invalid_camera_configuration) {
        return 9;
    }
    if (!fixture.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{})) {
        return 10;
    }

    const std::optional<elf3d::Bounds3> bounds = fixture.scene.world_bounds();
    const elf3d::SceneStatistics statistics = fixture.scene.statistics();
    if (!bounds.has_value() || bounds->minimum != elf3d::Float3{0.0F, 0.0F, 0.0F} ||
        bounds->maximum != elf3d::Float3{1.0F, 1.0F, 0.0F} ||
        statistics != elf3d::SceneStatistics{5, 1, 1, 1, 1, 3, 3, 1}) {
        return 11;
    }
    return 0;
}

[[nodiscard]] bool parent_change_invalidates_spatial_cache(RuntimeSceneFixture& fixture) {
    const std::uint64_t hierarchy_before_visibility = fixture.scene.hierarchy_revision();
    const std::uint64_t spatial_before_parent = fixture.scene.model_spatial_revision();
    if (!fixture.scene.set_parent(fixture.model, fixture.root)) {
        return false;
    }
    const auto moved_bounds = fixture.scene.primitive_world_bounds(fixture.model, 0);
    const bool hierarchy_changed =
        fixture.scene.hierarchy_revision() != hierarchy_before_visibility;
    const bool spatial_changed = fixture.scene.model_spatial_revision() != spatial_before_parent;
    const bool bounds_moved = moved_bounds && moved_bounds.value().minimum.x == 12.0F &&
                              moved_bounds.value().maximum.x == 13.0F;
    return hierarchy_changed && spatial_changed && bounds_moved;
}

[[nodiscard]] int verify_visibility_revisions(RuntimeSceneFixture& fixture) {
    if (!parent_change_invalidates_spatial_cache(fixture)) {
        return 14;
    }
    const std::uint64_t visibility_before = fixture.scene.visibility_revision();
    if (!fixture.scene.set_entity_visible(fixture.root, false) ||
        !fixture.scene.entity_local_visibility(fixture.child).value() ||
        fixture.scene.entity_effective_visibility(fixture.child).value() ||
        fixture.scene.entity_effective_visibility(fixture.model).value()) {
        return 15;
    }
    const std::uint64_t visibility_after_hide = fixture.scene.visibility_revision();
    if (visibility_after_hide == visibility_before ||
        !fixture.scene.set_entity_visible(fixture.root, false) ||
        fixture.scene.visibility_revision() != visibility_after_hide) {
        return 16;
    }
    return 0;
}

[[nodiscard]] bool camera_motion_preserves_model_spatial_revision(RuntimeSceneFixture& fixture,
                                                                  std::uint64_t revision) {
    const auto camera = fixture.scene.create_perspective_camera({});
    if (!camera) {
        return false;
    }
    elf3d::Transform camera_transform;
    camera_transform.translation = {0.0F, 0.0F, 5.0F};
    return fixture.scene.set_local_transform(camera.value(), camera_transform) &&
           fixture.scene.model_spatial_revision() == revision;
}

[[nodiscard]] bool cached_primitive_bounds_are_stable(RuntimeSceneFixture& fixture) {
    const auto initial = fixture.scene.primitive_world_bounds(fixture.model, 0);
    const auto reused = fixture.scene.primitive_world_bounds(fixture.model, 0);
    return initial && reused && initial.value() == reused.value();
}

[[nodiscard]] bool moved_model_bounds_are_current(RuntimeSceneFixture& fixture) {
    const auto moved = fixture.scene.primitive_world_bounds(fixture.model, 1);
    const auto scene_bounds = fixture.scene.world_bounds();
    return moved && scene_bounds && moved.value().minimum.x == 2.0F &&
           moved.value().maximum.x == 3.0F && scene_bounds.value() == moved.value();
}

[[nodiscard]] int verify_spatial_cache(RuntimeSceneFixture& fixture) {
    const std::uint64_t initial_spatial_revision = fixture.scene.model_spatial_revision();
    if (!camera_motion_preserves_model_spatial_revision(fixture, initial_spatial_revision)) {
        return 25;
    }
    if (!cached_primitive_bounds_are_stable(fixture)) {
        return 26;
    }
    elf3d::Transform model_transform;
    model_transform.translation = {2.0F, 0.0F, 0.0F};
    if (!fixture.scene.set_local_transform(fixture.model, model_transform) ||
        fixture.scene.model_spatial_revision() == initial_spatial_revision) {
        return 27;
    }
    if (!moved_model_bounds_are_current(fixture)) {
        return 28;
    }
    return 0;
}

[[nodiscard]] int verify_root_visibility(RuntimeSceneFixture& fixture) {
    const auto hidden_filter = elf3d::scene::make_visibility_filter(fixture.scene, std::nullopt);
    if (!hidden_filter || fixture.scene.visible_world_bounds(hidden_filter.value()).has_value()) {
        return 17;
    }
    if (!fixture.scene.show_entity_and_ancestors(fixture.model) ||
        !fixture.scene.entity_effective_visibility(fixture.model).value() ||
        !fixture.scene.visible_world_bounds(hidden_filter.value()).has_value()) {
        return 18;
    }
    return 0;
}

[[nodiscard]] int verify_child_visibility(RuntimeSceneFixture& fixture) {
    if (!fixture.scene.set_entity_visible(fixture.child, false) ||
        fixture.scene.entity_effective_visibility(fixture.grandchild).value() ||
        !fixture.scene.show_entity_and_ancestors(fixture.grandchild) ||
        !fixture.scene.entity_effective_visibility(fixture.grandchild).value()) {
        return 19;
    }
    if (!fixture.scene.show_all_entities() ||
        !fixture.scene.entity_effective_visibility(fixture.model).value()) {
        return 20;
    }
    return 0;
}

[[nodiscard]] int verify_recursive_destroy(RuntimeSceneFixture& fixture) {
    if (!fixture.scene.destroy_entity(fixture.root) || fixture.scene.entity(fixture.child) ||
        fixture.scene.entity(fixture.grandchild)) {
        return 12;
    }
    return 0;
}

[[nodiscard]] int verify_runtime_scene_contract() {
    std::optional<RuntimeSceneFixture> fixture = make_runtime_scene_fixture();
    if (!fixture) {
        return 7;
    }
    const int camera = verify_camera_and_statistics(*fixture);
    if (camera != 0) {
        return camera;
    }
    const int spatial = verify_spatial_cache(*fixture);
    if (spatial != 0) {
        return spatial;
    }
    const int revisions = verify_visibility_revisions(*fixture);
    if (revisions != 0) {
        return revisions;
    }
    const int root_visibility = verify_root_visibility(*fixture);
    if (root_visibility != 0) {
        return root_visibility;
    }
    const int child_visibility = verify_child_visibility(*fixture);
    if (child_visibility != 0) {
        return child_visibility;
    }
    return verify_recursive_destroy(*fixture);
}

[[nodiscard]] std::optional<elf3d::scene::Storage> make_document_scene() {
    elf3d::scene::Storage document_scene{scene_id(3)};
    elf3d::Document document;
    const auto document_material = document.create_material({});
    const auto document_mesh = document.create_mesh("Document mesh");
    const std::array<elf3d::Float3, 3> document_positions{{
        {2.0F, 0.0F, 0.0F},
        {3.0F, 0.0F, 0.0F},
        {2.0F, 1.0F, 0.0F},
    }};
    const auto document_primitive =
        document.create_primitive(document_mesh.value(), document_material.value(),
                                  {document_positions, {}, {}, {}, {}, triangle_indices});
    const auto document_node = document.create_node("Document node");
    const auto document_root = document.create_scene("Document scene");
    if (!document_material || !document_mesh || !document_primitive || !document_node ||
        !document_root || !document.set_node_mesh(document_node.value(), document_mesh.value()) ||
        !document.add_scene_root(document_root.value(), document_node.value()) ||
        !elf3d::scene::populate_from_document(std::move(document), document_root.value(),
                                              document_scene)) {
        return std::nullopt;
    }
    return document_scene;
}

[[nodiscard]] bool has_expected_document_record(const elf3d::scene::Storage& scene) {
    const std::span<const std::optional<elf3d::scene::EntityRecord>> document_records =
        scene.entities();
    return document_records.size() == 1 && document_records.front().has_value() &&
           document_records.front()->model.has_value() && scene.assets().meshes().empty() &&
           scene.assets().materials().empty() && scene.assets().images().empty() &&
           scene.assets().textures().empty();
}

[[nodiscard]] bool has_expected_document_runtime(const elf3d::scene::Storage& scene) {
    const elf3d::scene::EntityRecord& record = *scene.entities().front();
    const auto runtime = scene.runtime_primitive(record.id, 0);
    return runtime && runtime.value().mesh.is_valid() &&
           runtime.value().document_primitive.is_valid();
}

[[nodiscard]] bool has_expected_document_bounds(const elf3d::scene::Storage& scene) {
    const elf3d::scene::EntityRecord& record = *scene.entities().front();
    const auto runtime = scene.runtime_primitive(record.id, 0);
    const auto bounds = scene.mesh_bounds(runtime.value().mesh);
    return bounds && bounds.value() == elf3d::Bounds3{{2.0F, 0.0F, 0.0F}, {3.0F, 1.0F, 0.0F}};
}

[[nodiscard]] int verify_document_scene_contract() {
    std::optional<elf3d::scene::Storage> scene = make_document_scene();
    if (!scene) {
        return 21;
    }
    if (!has_expected_document_record(*scene)) {
        return 22;
    }
    if (!has_expected_document_runtime(*scene)) {
        return 23;
    }
    if (!has_expected_document_bounds(*scene)) {
        return 24;
    }
    return 0;
}

} // namespace

int elf3d_scene_test() {
    const int hierarchy = verify_hierarchy_contract();
    if (hierarchy != 0) {
        return hierarchy;
    }
    const int runtime = verify_runtime_scene_contract();
    if (runtime != 0) {
        return runtime;
    }
    return verify_document_scene_contract();
}
