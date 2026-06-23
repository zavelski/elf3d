#include <elf3d/assets/handle_access.h>
#include <elf3d/picking/service.h>

#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace {

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.001F) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z) <= tolerance;
}

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(23, value);
}

struct PickScene {
    elf3d::scene::Storage scene;
    elf3d::EntityId near_model;
    elf3d::EntityId far_model;
    elf3d::EntityId camera;
};

[[nodiscard]] elf3d::MeshHandle create_triangle_mesh(elf3d::scene::Storage &scene, elf3d::Float3 a,
                                                     elf3d::Float3 b, elf3d::Float3 c) {
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {a, {0.0F, 0.0F, 1.0F}},
        {b, {0.0F, 0.0F, 1.0F}},
        {c, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    return scene.create_mesh({vertices, indices}).value();
}

[[nodiscard]] elf3d::MaterialHandle create_material(elf3d::scene::Storage &scene,
                                                    bool double_sided = false) {
    elf3d::MaterialDescription material_description;
    material_description.double_sided = double_sided;
    return scene.create_material(material_description).value();
}

[[nodiscard]] PickScene make_pick_scene() {
    elf3d::scene::Storage scene{scene_id(1)};
    const elf3d::MeshHandle mesh = create_triangle_mesh(scene, {-0.75F, -0.75F, 0.0F},
                                                        {0.75F, -0.75F, 0.0F}, {0.0F, 0.75F, 0.0F});
    const elf3d::MaterialHandle material = create_material(scene);
    const elf3d::EntityId far_model = scene.create_model(mesh, material).value();
    elf3d::Transform far_transform;
    far_transform.translation = {0.0F, 0.0F, -4.0F};
    static_cast<void>(scene.set_local_transform(far_model, far_transform));
    const elf3d::EntityId near_model = scene.create_model(mesh, material).value();
    elf3d::Transform near_transform;
    near_transform.translation = {0.0F, 0.0F, -2.0F};
    static_cast<void>(scene.set_local_transform(near_model, near_transform));
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return PickScene{std::move(scene), near_model, far_model, camera};
}

[[nodiscard]] PickScene make_slanted_depth_scene() {
    elf3d::scene::Storage scene{scene_id(7)};
    const elf3d::MeshHandle mesh = create_triangle_mesh(
        scene, {-1.0F, -1.0F, 0.5F}, {1.0F, -1.0F, 0.5F}, {0.0F, 1.0F, -0.5F});
    const elf3d::MaterialHandle material = create_material(scene, true);
    const elf3d::EntityId far_model = scene.create_model(mesh, material).value();
    elf3d::Transform far_transform;
    far_transform.translation = {0.0F, 0.0F, -4.0F};
    static_cast<void>(scene.set_local_transform(far_model, far_transform));
    const elf3d::EntityId near_model = scene.create_model(mesh, material).value();
    elf3d::Transform near_transform;
    near_transform.translation = {0.0F, 0.0F, -2.0F};
    static_cast<void>(scene.set_local_transform(near_model, near_transform));
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return PickScene{std::move(scene), near_model, far_model, camera};
}

} // namespace

int main() {
    elf3d::Ray3 ray{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}};
    elf3d::Ray3 miss_ray{{2.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}};
    elf3d::Ray3 inside_ray{{0.0F, 0.0F, -1.5F}, {0.0F, 0.0F, -1.0F}};
    elf3d::Ray3 parallel_ray{{2.0F, 0.0F, -1.5F}, {0.0F, 1.0F, 0.0F}};
    elf3d::Ray3 tangent_ray{{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}};
    elf3d::Ray3 negative_direction_ray{{0.0F, 0.0F, -3.0F}, {0.0F, 0.0F, 1.0F}};
    const elf3d::Bounds3 bounds{{-1.0F, -1.0F, -2.0F}, {1.0F, 1.0F, -1.0F}, true};
    elf3d::picking::RayBoundsHit bounds_hit;
    if (!elf3d::picking::intersect_ray_bounds(ray, bounds, bounds_hit) ||
        !nearly_equal(bounds_hit.entry_distance, 1.0F) ||
        elf3d::picking::intersect_ray_bounds(miss_ray, bounds, bounds_hit) ||
        !elf3d::picking::intersect_ray_bounds(inside_ray, bounds, bounds_hit) ||
        !elf3d::picking::intersect_ray_bounds(tangent_ray, bounds, bounds_hit) ||
        !elf3d::picking::intersect_ray_bounds(negative_direction_ray, bounds, bounds_hit) ||
        elf3d::picking::intersect_ray_bounds(parallel_ray, bounds, bounds_hit) ||
        elf3d::picking::intersect_ray_bounds(ray, elf3d::Bounds3{}, bounds_hit)) {
        return 1;
    }

    const elf3d::Float3 a{-1.0F, -1.0F, -3.0F};
    const elf3d::Float3 b{1.0F, -1.0F, -3.0F};
    const elf3d::Float3 c{0.0F, 1.0F, -3.0F};
    const std::optional<elf3d::picking::TriangleHit> front =
        elf3d::picking::intersect_ray_triangle(ray, a, b, c, true);
    const std::optional<elf3d::picking::TriangleHit> back_rejected =
        elf3d::picking::intersect_ray_triangle(ray, a, c, b, true);
    const std::optional<elf3d::picking::TriangleHit> back_double_sided =
        elf3d::picking::intersect_ray_triangle(ray, a, c, b, false);
    const std::optional<elf3d::picking::TriangleHit> behind =
        elf3d::picking::intersect_ray_triangle({{0.0F, 0.0F, -5.0F}, {0.0F, 0.0F, -1.0F}}, a, b, c,
                                               false);
    const std::optional<elf3d::picking::TriangleHit> edge = elf3d::picking::intersect_ray_triangle(
        {{0.0F, -1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}}, a, b, c, false);
    const std::optional<elf3d::picking::TriangleHit> vertex =
        elf3d::picking::intersect_ray_triangle({{-1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, -1.0F}}, a, b, c,
                                               false);
    const std::optional<elf3d::picking::TriangleHit> degenerate =
        elf3d::picking::intersect_ray_triangle(ray, a, a, c, false);
    if (!front.has_value() || back_rejected.has_value() || !back_double_sided.has_value() ||
        behind.has_value() || !edge.has_value() || !vertex.has_value() || degenerate.has_value() ||
        !nearly_equal(front->barycentric_coordinates.x + front->barycentric_coordinates.y +
                          front->barycentric_coordinates.z,
                      1.0F)) {
        return 2;
    }

    PickScene fixture = make_pick_scene();
    elf3d::picking::PickingService service;
    const elf3d::Result<elf3d::Ray3> center_ray =
        service.make_picking_ray(fixture.scene, fixture.camera, {800, 600}, {399.5F, 299.5F});
    const elf3d::Result<elf3d::Ray3> top_left_ray =
        service.make_picking_ray(fixture.scene, fixture.camera, {800, 600}, {0.0F, 0.0F});
    const elf3d::Result<elf3d::Ray3> portrait_ray =
        service.make_picking_ray(fixture.scene, fixture.camera, {300, 900}, {149.5F, 449.5F});
    const elf3d::Result<elf3d::Ray3> out_of_range =
        service.make_picking_ray(fixture.scene, fixture.camera, {800, 600}, {800.0F, 0.0F});
    if (!center_ray || !top_left_ray || !portrait_ray ||
        out_of_range.error().code() != elf3d::ErrorCode::invalid_viewport_position ||
        !nearly_equal(center_ray.value().direction, {0.0F, 0.0F, -1.0F}) ||
        !(top_left_ray.value().direction.x < 0.0F && top_left_ray.value().direction.y > 0.0F &&
          top_left_ray.value().direction.z < 0.0F) ||
        !elf3d::picking::is_valid_ray(portrait_ray.value())) {
        return 3;
    }

    const elf3d::Result<std::optional<elf3d::PickHit>> pick =
        service.pick(fixture.scene, fixture.camera, {800, 600}, {399.5F, 299.5F});
    const elf3d::PickingStatistics first_statistics = service.statistics();
    const elf3d::Result<std::optional<elf3d::PickHit>> second_pick =
        service.pick(fixture.scene, fixture.camera, {800, 600}, {399.5F, 299.5F});
    const elf3d::PickingStatistics second_statistics = service.statistics();
    if (!pick || !pick.value().has_value() || !second_pick || !second_pick.value().has_value() ||
        pick.value()->entity != fixture.near_model || pick.value()->triangle_index != 0 ||
        pick.value()->primitive_index != 0 ||
        !nearly_equal(pick.value()->world_position, {0.0F, 0.0F, -2.0F}) ||
        !nearly_equal(pick.value()->world_distance, 2.0F) ||
        first_statistics.latest_bvh_builds != 1 || first_statistics.cached_mesh_bvhs != 1 ||
        second_statistics.latest_bvh_builds != 0 || second_statistics.cached_mesh_bvhs != 1) {
        return 4;
    }

    PickScene slanted_fixture = make_slanted_depth_scene();
    elf3d::picking::PickingService clipping_service;
    const elf3d::scene::VisibilityFilter slanted_visibility =
        elf3d::scene::make_visibility_filter(slanted_fixture.scene, std::nullopt).value();
    elf3d::SectionPlane depth_plane;
    depth_plane.enabled = true;
    depth_plane.point = {0.0F, 0.0F, -2.25F};
    depth_plane.normal = {0.0F, 0.0F, -1.0F};
    const elf3d::clipping::ClippingFilter depth_filter =
        elf3d::clipping::make_filter(depth_plane, {}, 1).value();
    const elf3d::Result<std::optional<elf3d::PickHit>> clipped_nearest =
        clipping_service.pick_ray(slanted_fixture.scene, ray, elf3d::PickOptions{},
                                  slanted_visibility, depth_filter);
    const elf3d::PickingStatistics clipped_statistics = clipping_service.statistics();
    if (!clipped_nearest || !clipped_nearest.value().has_value() ||
        clipped_nearest.value()->entity != slanted_fixture.far_model ||
        !nearly_equal(clipped_nearest.value()->world_position, {0.0F, 0.0F, -4.0F}) ||
        clipped_statistics.latest_clipping_bounds_rejected != 0 ||
        clipped_statistics.latest_clipping_hits_rejected == 0 ||
        clipped_statistics.latest_clipping_hits_accepted == 0 ||
        clipped_statistics.latest_bvh_builds != 1) {
        return 401;
    }
    const std::array<elf3d::ClippingBox, 2> union_boxes{{
        {{-0.5F, -0.5F, -4.25F}, {0.5F, 0.5F, -3.75F}, true},
        {{2.0F, 2.0F, -4.25F}, {3.0F, 3.0F, -3.75F}, true},
    }};
    const elf3d::clipping::ClippingFilter box_filter =
        elf3d::clipping::make_filter(elf3d::SectionPlane{}, union_boxes, 2).value();
    const elf3d::Result<std::optional<elf3d::PickHit>> box_clipped =
        clipping_service.pick_ray(slanted_fixture.scene, ray, elf3d::PickOptions{},
                                  slanted_visibility, box_filter);
    const elf3d::PickingStatistics box_statistics = clipping_service.statistics();
    if (!box_clipped || !box_clipped.value().has_value() ||
        box_clipped.value()->entity != slanted_fixture.far_model ||
        box_statistics.latest_clipping_bounds_rejected != 1 ||
        box_statistics.latest_bvh_builds != 0) {
        return 402;
    }
    if (!fixture.scene.set_entity_visible(fixture.near_model, false)) {
        return 41;
    }
    const elf3d::Result<std::optional<elf3d::PickHit>> hidden_near_pick =
        service.pick(fixture.scene, fixture.camera, {800, 600}, {399.5F, 299.5F});
    if (!hidden_near_pick || !hidden_near_pick.value().has_value() ||
        hidden_near_pick.value()->entity != fixture.far_model) {
        return 42;
    }
    if (!fixture.scene.set_entity_visible(fixture.far_model, false)) {
        return 43;
    }
    const elf3d::Result<std::optional<elf3d::PickHit>> all_hidden_pick =
        service.pick(fixture.scene, fixture.camera, {800, 600}, {399.5F, 299.5F});
    if (!all_hidden_pick || all_hidden_pick.value().has_value()) {
        return 44;
    }
    if (!fixture.scene.show_all_entities()) {
        return 45;
    }

    const elf3d::Result<std::optional<elf3d::PickHit>> empty =
        service.pick(fixture.scene, fixture.camera, {800, 600}, {0.0F, 0.0F});
    if (!empty || empty.value().has_value()) {
        return 5;
    }

    service.release_scene(fixture.scene.id());
    if (service.statistics().cached_mesh_bvhs != 0) {
        return 6;
    }

    elf3d::scene::Storage hierarchy_scene{scene_id(2)};
    const elf3d::EntityId parent = hierarchy_scene.create_entity().value();
    const elf3d::MeshHandle hierarchy_mesh = create_triangle_mesh(
        hierarchy_scene, {-0.5F, -0.5F, 0.0F}, {0.5F, -0.5F, 0.0F}, {0.0F, 0.5F, 0.0F});
    const elf3d::MaterialHandle hierarchy_material = create_material(hierarchy_scene, true);
    const elf3d::EntityId hierarchy_model =
        hierarchy_scene.create_model(hierarchy_mesh, hierarchy_material).value();
    const elf3d::EntityId hierarchy_camera =
        hierarchy_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    elf3d::Transform parent_transform;
    parent_transform.translation = {0.0F, 0.0F, -3.0F};
    elf3d::Transform model_transform;
    model_transform.scale = {-2.0F, 0.5F, 1.0F};
    static_cast<void>(hierarchy_scene.set_local_transform(parent, parent_transform));
    static_cast<void>(hierarchy_scene.set_local_transform(hierarchy_model, model_transform));
    static_cast<void>(hierarchy_scene.set_parent(hierarchy_model, parent));
    const elf3d::Result<std::optional<elf3d::PickHit>> hierarchy_pick =
        service.pick(hierarchy_scene, hierarchy_camera, {800, 600}, {399.5F, 299.5F});
    if (!hierarchy_pick || !hierarchy_pick.value().has_value() ||
        hierarchy_pick.value()->entity != hierarchy_model ||
        !nearly_equal(hierarchy_pick.value()->world_position, {0.0F, 0.0F, -3.0F}) ||
        !std::isfinite(hierarchy_pick.value()->world_normal.z)) {
        return 7;
    }

    elf3d::scene::Storage primitive_scene{scene_id(3)};
    const elf3d::MeshHandle side_mesh = create_triangle_mesh(
        primitive_scene, {2.5F, -0.5F, -2.0F}, {3.5F, -0.5F, -2.0F}, {3.0F, 0.5F, -2.0F});
    const elf3d::MeshHandle center_mesh = create_triangle_mesh(
        primitive_scene, {-0.5F, -0.5F, -2.0F}, {0.5F, -0.5F, -2.0F}, {0.0F, 0.5F, -2.0F});
    const elf3d::MaterialHandle primitive_material = create_material(primitive_scene);
    const elf3d::EntityId primitive_model =
        primitive_scene.create_model(side_mesh, primitive_material).value();
    const std::array<elf3d::ModelPrimitiveBinding, 2> primitive_bindings{{
        {side_mesh, primitive_material},
        {center_mesh, primitive_material},
    }};
    static_cast<void>(primitive_scene.set_model_primitives(primitive_model, primitive_bindings));
    const elf3d::EntityId primitive_camera =
        primitive_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    const elf3d::Result<std::optional<elf3d::PickHit>> primitive_pick =
        service.pick(primitive_scene, primitive_camera, {800, 600}, {399.5F, 299.5F});
    if (!primitive_pick || !primitive_pick.value().has_value() ||
        primitive_pick.value()->entity != primitive_model ||
        primitive_pick.value()->mesh != center_mesh ||
        primitive_pick.value()->primitive_index != 1 ||
        primitive_pick.value()->triangle_index != 0) {
        return 8;
    }

    elf3d::scene::Storage large_scene{scene_id(4)};
    const elf3d::MeshHandle large_mesh = create_triangle_mesh(
        large_scene, {-0.5F, -0.5F, 0.0F}, {0.5F, -0.5F, 0.0F}, {0.0F, 0.5F, 0.0F});
    const elf3d::MaterialHandle large_material = create_material(large_scene);
    const elf3d::EntityId large_model =
        large_scene.create_model(large_mesh, large_material).value();
    const elf3d::EntityId large_camera =
        large_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    elf3d::Transform large_model_transform;
    large_model_transform.translation = {1.0e6F, 0.0F, -2.0F};
    elf3d::Transform large_camera_transform;
    large_camera_transform.translation = {1.0e6F, 0.0F, 0.0F};
    static_cast<void>(large_scene.set_local_transform(large_model, large_model_transform));
    static_cast<void>(large_scene.set_local_transform(large_camera, large_camera_transform));
    const elf3d::Result<std::optional<elf3d::PickHit>> large_pick =
        service.pick(large_scene, large_camera, {800, 600}, {399.5F, 299.5F});
    if (!large_pick || !large_pick.value().has_value() ||
        large_pick.value()->entity != large_model ||
        !nearly_equal(large_pick.value()->world_distance, 2.0F) ||
        !std::isfinite(large_pick.value()->world_position.x)) {
        return 9;
    }

    elf3d::picking::PickingService cache_service;
    elf3d::scene::Storage first_cache_scene{scene_id(5)};
    const elf3d::MeshHandle first_cache_mesh = create_triangle_mesh(
        first_cache_scene, {-0.5F, -0.5F, -2.0F}, {0.5F, -0.5F, -2.0F}, {0.0F, 0.5F, -2.0F});
    const elf3d::MaterialHandle first_cache_material = create_material(first_cache_scene);
    static_cast<void>(first_cache_scene.create_model(first_cache_mesh, first_cache_material));
    const elf3d::EntityId first_cache_camera =
        first_cache_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    elf3d::scene::Storage second_cache_scene{scene_id(6)};
    const elf3d::MeshHandle second_cache_mesh = create_triangle_mesh(
        second_cache_scene, {-0.5F, -0.5F, -2.0F}, {0.5F, -0.5F, -2.0F}, {0.0F, 0.5F, -2.0F});
    const elf3d::MaterialHandle second_cache_material = create_material(second_cache_scene);
    static_cast<void>(second_cache_scene.create_model(second_cache_mesh, second_cache_material));
    const elf3d::EntityId second_cache_camera =
        second_cache_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    static_cast<void>(
        cache_service.pick(first_cache_scene, first_cache_camera, {800, 600}, {399.5F, 299.5F}));
    static_cast<void>(
        cache_service.pick(second_cache_scene, second_cache_camera, {800, 600}, {399.5F, 299.5F}));
    if (cache_service.statistics().cached_mesh_bvhs != 2) {
        return 10;
    }
    cache_service.release_scene(first_cache_scene.id());
    if (cache_service.statistics().cached_mesh_bvhs != 1) {
        return 11;
    }
    cache_service.release_scene(second_cache_scene.id());
    if (cache_service.statistics().cached_mesh_bvhs != 0) {
        return 12;
    }

    return 0;
}
