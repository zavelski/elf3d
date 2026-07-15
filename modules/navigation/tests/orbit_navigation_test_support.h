#pragma once

namespace elf3d::navigation::test_support {
struct SceneFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
    elf3d::EntityId camera;
    elf3d::EntityId second_camera;
};

[[nodiscard]] inline bool nearly_equal(float left, float right,
                                       float tolerance = 0.0005F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] inline float length(elf3d::Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] inline elf3d::Float3 subtract(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline elf3d::Float3 add(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline elf3d::Float3 multiply(elf3d::Float3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] inline float dot(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                       float tolerance = 0.0005F) noexcept {
    return length(subtract(left, right)) <= tolerance;
}

[[nodiscard]] inline elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(19, value);
}

[[nodiscard]] inline SceneFixture make_scene(std::uint64_t id_value, elf3d::Float3 minimum,
                                             elf3d::Float3 maximum) {
    elf3d::scene::Storage scene{scene_id(id_value)};
    const std::array<elf3d::VertexPositionNormal, 8> vertices{{
        {minimum, {0.0F, 1.0F, 0.0F}},
        {{maximum.x, minimum.y, minimum.z}, {0.0F, 1.0F, 0.0F}},
        {{minimum.x, maximum.y, minimum.z}, {0.0F, 1.0F, 0.0F}},
        {{maximum.x, maximum.y, minimum.z}, {0.0F, 1.0F, 0.0F}},
        {{minimum.x, minimum.y, maximum.z}, {0.0F, 1.0F, 0.0F}},
        {{maximum.x, minimum.y, maximum.z}, {0.0F, 1.0F, 0.0F}},
        {{minimum.x, maximum.y, maximum.z}, {0.0F, 1.0F, 0.0F}},
        {maximum, {0.0F, 1.0F, 0.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::MeshHandle mesh = scene.create_mesh({vertices, indices}).value();
    const elf3d::MaterialHandle material = scene.create_material({}).value();
    const elf3d::EntityId model = scene.create_model(mesh, material).value();
    (void)model;
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    const elf3d::EntityId second_camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return SceneFixture{std::move(scene), model, camera, second_camera};
}

[[nodiscard]] inline elf3d::Float3 camera_position(const elf3d::scene::Storage& scene,
                                                   elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    return {world[3].x, world[3].y, world[3].z};
}

[[nodiscard]] inline elf3d::Float3 camera_forward(const elf3d::scene::Storage& scene,
                                                  elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    elf3d::math::Vector3 right{world[0]};
    elf3d::math::Vector3 up{world[1]};
    right = glm::normalize(right);
    up = glm::normalize(up - right * glm::dot(right, up));
    const elf3d::math::Vector3 backward = glm::normalize(glm::cross(right, up));
    return elf3d::math::to_float3(-backward);
}

[[nodiscard]] inline elf3d::Float3 camera_right(const elf3d::scene::Storage& scene,
                                                elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    return elf3d::math::to_float3(glm::normalize(elf3d::math::Vector3{world[0]}));
}

[[nodiscard]] inline bool camera_looks_at(const elf3d::scene::Storage& scene,
                                          elf3d::EntityId camera, elf3d::Float3 pivot) {
    const elf3d::Float3 position = camera_position(scene, camera);
    const elf3d::math::Vector3 to_pivot =
        glm::normalize(elf3d::math::to_vector(subtract(pivot, position)));
    const elf3d::math::Vector3 forward = elf3d::math::to_vector(camera_forward(scene, camera));
    return glm::dot(to_pivot, forward) > 0.999F;
}

inline void set_camera_position(elf3d::scene::Storage& scene, elf3d::EntityId camera,
                                elf3d::Float3 position) {
    elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    world[3] = elf3d::math::Vector4{position.x, position.y, position.z, 1.0F};
    static_cast<void>(scene.set_local_matrix(camera, elf3d::math::to_float4x4(world)));
}

[[nodiscard]] inline float signed_camera_distance_to(const elf3d::scene::Storage& scene,
                                                     elf3d::EntityId camera,
                                                     elf3d::Float3 world_position) {
    return dot(subtract(world_position, camera_position(scene, camera)),
               camera_forward(scene, camera));
}

[[nodiscard]] inline bool depth_ratio_within_limit(const elf3d::scene::Storage& scene,
                                                   elf3d::EntityId camera) {
    const elf3d::PerspectiveCameraDescription description =
        scene.perspective_camera(camera).value();
    return description.near_plane > 0.0F && description.far_plane > description.near_plane &&
           description.far_plane / description.near_plane <= 10000.5F;
}

[[nodiscard]] inline elf3d::Float2 project_to_ndc(const elf3d::scene::Storage& scene,
                                                  elf3d::EntityId camera, elf3d::Extent2D extent,
                                                  elf3d::Float3 world_position) {
    const elf3d::Float4x4 camera_world = scene.world_matrix(camera).value();
    const elf3d::math::Matrix4 view =
        elf3d::math::to_matrix(elf3d::math::camera_view_matrix(camera_world).value());
    const elf3d::PerspectiveCameraDescription description =
        scene.perspective_camera(camera).value();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const elf3d::math::Matrix4 projection = elf3d::math::to_matrix(
        elf3d::math::perspective_matrix(description.vertical_field_of_view_radians, aspect,
                                        description.near_plane, description.far_plane)
            .value());
    const elf3d::math::Vector4 clip =
        projection * view *
        elf3d::math::Vector4{world_position.x, world_position.y, world_position.z, 1.0F};
    return elf3d::Float2{clip.x / clip.w, clip.y / clip.w};
}

[[nodiscard]] inline bool bounds_visible(const elf3d::scene::Storage& scene, elf3d::EntityId camera,
                                         elf3d::Extent2D extent) {
    const std::optional<elf3d::Bounds3> bounds_result = scene.world_bounds();
    if (!bounds_result.has_value()) {
        return false;
    }
    const elf3d::Bounds3 bounds = *bounds_result;
    const elf3d::Float4x4 camera_world = scene.world_matrix(camera).value();
    const elf3d::math::Matrix4 view =
        elf3d::math::to_matrix(elf3d::math::camera_view_matrix(camera_world).value());
    const elf3d::PerspectiveCameraDescription description =
        scene.perspective_camera(camera).value();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const elf3d::math::Matrix4 projection = elf3d::math::to_matrix(
        elf3d::math::perspective_matrix(description.vertical_field_of_view_radians, aspect,
                                        description.near_plane, description.far_plane)
            .value());
    const std::array<elf3d::Float3, 8> corners{{
        {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
    }};
    for (const elf3d::Float3 corner : corners) {
        const elf3d::math::Vector4 clip =
            projection * view * elf3d::math::Vector4{corner.x, corner.y, corner.z, 1.0F};
        if (clip.w <= 0.0F) {
            return false;
        }
        const float x = clip.x / clip.w;
        const float y = clip.y / clip.w;
        const float z = clip.z / clip.w;
        if (std::abs(x) > 1.001F || std::abs(y) > 1.001F || z < -1.001F || z > 1.001F) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline float maximum_projected_bounds_extent(const elf3d::scene::Storage& scene,
                                                           elf3d::EntityId camera,
                                                           elf3d::Extent2D extent) {
    const std::optional<elf3d::Bounds3> bounds_result = scene.world_bounds();
    if (!bounds_result.has_value()) {
        return 0.0F;
    }
    const elf3d::Bounds3 bounds = *bounds_result;
    const std::array<elf3d::Float3, 8> corners{{
        {bounds.minimum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.minimum.z},
        {bounds.minimum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.minimum.y, bounds.maximum.z},
        {bounds.minimum.x, bounds.maximum.y, bounds.maximum.z},
        {bounds.maximum.x, bounds.maximum.y, bounds.maximum.z},
    }};
    float maximum_extent = 0.0F;
    for (const elf3d::Float3 corner : corners) {
        const elf3d::Float2 projected = project_to_ndc(scene, camera, extent, corner);
        if (!std::isfinite(projected.x) || !std::isfinite(projected.y)) {
            return 0.0F;
        }
        maximum_extent = std::max({maximum_extent, std::abs(projected.x), std::abs(projected.y)});
    }
    return maximum_extent;
}

[[nodiscard]] inline elf3d::ViewportInput hovered_input() noexcept {
    elf3d::ViewportInput input;
    input.is_hovered = true;
    input.is_focused = true;
    return input;
}

inline constexpr float navigation_test_click_threshold = 4.0F;

struct NavigationTestContext {
    explicit NavigationTestContext(std::uint64_t scene_value)
        : fixture(make_scene(scene_value, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F})) {}

    SceneFixture fixture;
    elf3d::navigation::OrbitNavigationController navigation;
};

[[nodiscard]] inline elf3d::Result<elf3d::navigation::NavigationUpdate>
update_navigation(NavigationTestContext& context, const elf3d::ViewportInput& input) {
    return context.navigation.update(context.fixture.scene, context.fixture.camera, {800, 600},
                                     input, navigation_test_click_threshold);
}

} // namespace elf3d::navigation::test_support
