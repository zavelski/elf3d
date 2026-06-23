#include <elf3d/assets/handle_access.h>
#include <elf3d/navigation/orbit_navigation.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

struct SceneFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId camera;
    elf3d::EntityId second_camera;
};

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0005F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] float length(elf3d::Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] elf3d::Float3 subtract(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.0005F) noexcept {
    return length(subtract(left, right)) <= tolerance;
}

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(19, value);
}

[[nodiscard]] SceneFixture make_scene(std::uint64_t id_value, elf3d::Float3 minimum,
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
    return SceneFixture{std::move(scene), camera, second_camera};
}

[[nodiscard]] elf3d::Float3 camera_position(const elf3d::scene::Storage &scene,
                                            elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = scene.world_matrix(camera).value();
    return {world[3].x, world[3].y, world[3].z};
}

[[nodiscard]] elf3d::Float3 camera_forward(const elf3d::scene::Storage &scene,
                                           elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = scene.world_matrix(camera).value();
    elf3d::math::Vector3 right{world[0]};
    elf3d::math::Vector3 up{world[1]};
    right = glm::normalize(right);
    up = glm::normalize(up - right * glm::dot(right, up));
    const elf3d::math::Vector3 backward = glm::normalize(glm::cross(right, up));
    return elf3d::math::to_float3(-backward);
}

[[nodiscard]] bool camera_looks_at(const elf3d::scene::Storage &scene, elf3d::EntityId camera,
                                   elf3d::Float3 pivot) {
    const elf3d::Float3 position = camera_position(scene, camera);
    const elf3d::math::Vector3 to_pivot =
        glm::normalize(elf3d::math::to_vector(subtract(pivot, position)));
    const elf3d::math::Vector3 forward = elf3d::math::to_vector(camera_forward(scene, camera));
    return glm::dot(to_pivot, forward) > 0.999F;
}

[[nodiscard]] bool bounds_visible(const elf3d::scene::Storage &scene, elf3d::EntityId camera,
                                  elf3d::Extent2D extent) {
    const elf3d::Bounds3 bounds = scene.world_bounds();
    const elf3d::math::Matrix4 camera_world = scene.world_matrix(camera).value();
    const elf3d::math::Matrix4 view = elf3d::math::camera_view_matrix(camera_world).value();
    const elf3d::PerspectiveCameraDescription description =
        scene.perspective_camera(camera).value();
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const elf3d::math::Matrix4 projection =
        elf3d::math::perspective_matrix(description.vertical_field_of_view_radians, aspect,
                                        description.near_plane, description.far_plane)
            .value();
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

[[nodiscard]] elf3d::ViewportInput hovered_input() noexcept {
    elf3d::ViewportInput input;
    input.is_hovered = true;
    input.is_focused = true;
    return input;
}

} // namespace

int main() {
    constexpr float click_threshold = 4.0F;
    SceneFixture fixture = make_scene(1, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController navigation;
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 1;
    }
    const elf3d::NavigationSnapshot reset = navigation.snapshot();
    const elf3d::Float3 expected_center{1.5F, 1.5F, 1.5F};
    if (!reset.has_valid_state || !nearly_equal(reset.pivot, expected_center) ||
        !camera_looks_at(fixture.scene, fixture.camera, reset.pivot) ||
        !bounds_visible(fixture.scene, fixture.camera, {800, 600})) {
        return 2;
    }
    if (!navigation.fit_to_scene(fixture.scene, fixture.camera, {360, 800}) ||
        !bounds_visible(fixture.scene, fixture.camera, {360, 800})) {
        return 3;
    }

    const elf3d::NavigationSnapshot before_orbit = navigation.snapshot();
    elf3d::ViewportInput input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 4;
    }
    input.pointer_position_pixels = {300.0F, 300.0F};
    input.pointer_delta_pixels = {290.0F, 290.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 4;
    }
    const elf3d::NavigationSnapshot first_drag = navigation.snapshot();
    if (!first_drag.is_orbiting || !first_drag.is_pointer_captured ||
        !nearly_equal(first_drag.yaw_radians, before_orbit.yaw_radians) ||
        !nearly_equal(first_drag.pitch_radians, before_orbit.pitch_radians)) {
        return 5;
    }
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {100.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 6;
    }
    const elf3d::NavigationSnapshot yawed = navigation.snapshot();
    if (nearly_equal(yawed.yaw_radians, first_drag.yaw_radians) ||
        !nearly_equal(yawed.distance, first_drag.distance) ||
        !camera_looks_at(fixture.scene, fixture.camera, yawed.pivot)) {
        return 7;
    }
    input.pointer_position_pixels = {400.0F, -9700.0F};
    input.pointer_delta_pixels = {0.0F, -10000.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 8;
    }
    const elf3d::NavigationSnapshot pitched = navigation.snapshot();
    if (pitched.pitch_radians < navigation.settings().minimum_pitch_radians ||
        pitched.pitch_radians > navigation.settings().maximum_pitch_radians) {
        return 9;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (navigation.snapshot().is_pointer_captured) {
        return 10;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 11;
    }
    const elf3d::NavigationSnapshot before_pan = navigation.snapshot();
    const elf3d::Float3 before_forward = camera_forward(fixture.scene, fixture.camera);
    input = hovered_input();
    input.left_button_down = true;
    input.shift_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 12;
    }
    const elf3d::NavigationSnapshot after_pan = navigation.snapshot();
    if (nearly_equal(after_pan.pivot, before_pan.pivot) ||
        !nearly_equal(after_pan.distance, before_pan.distance) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), before_forward)) {
        return 13;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    const float distance_before_zoom = navigation.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 14;
    }
    const float distance_after_zoom_in = navigation.snapshot().distance;
    input.wheel_delta = -1.0F;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    const float distance_after_zoom_out = navigation.snapshot().distance;
    input.is_hovered = false;
    input.wheel_delta = 20.0F;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!(distance_after_zoom_in < distance_before_zoom) ||
        !(distance_after_zoom_out > distance_after_zoom_in) ||
        !nearly_equal(navigation.snapshot().distance, distance_after_zoom_out)) {
        return 15;
    }

    elf3d::OrbitNavigationSettings invalid_settings = navigation.settings();
    invalid_settings.maximum_distance = invalid_settings.minimum_distance;
    if (navigation.set_settings(invalid_settings).error().code() !=
        elf3d::ErrorCode::invalid_navigation_settings) {
        return 16;
    }
    elf3d::OrbitNavigationSettings inverted = navigation.settings();
    inverted.invert_vertical_orbit = true;
    if (!navigation.set_settings(inverted) ||
        !navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 17;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {10.0F, 60.0F};
    input.pointer_delta_pixels = {0.0F, 50.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {10.0F, 110.0F};
    input.pointer_delta_pixels = {0.0F, 50.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!(navigation.snapshot().pitch_radians > reset.pitch_radians)) {
        return 18;
    }

    SceneFixture tiny = make_scene(2, {0.0F, 0.0F, 0.0F}, {0.000001F, 0.000001F, 0.000001F});
    if (!navigation.reset_view(tiny.scene, tiny.camera, {800, 600}) ||
        !std::isfinite(navigation.snapshot().distance)) {
        return 19;
    }
    SceneFixture large = make_scene(3, {-1.0e6F, -1.0e6F, -1.0e6F}, {1.0e6F, 1.0e6F, 1.0e6F});
    if (!navigation.reset_view(large.scene, large.camera, {800, 600}) ||
        !std::isfinite(navigation.snapshot().distance)) {
        return 20;
    }

    elf3d::scene::Storage empty{scene_id(4)};
    const elf3d::EntityId empty_camera =
        empty.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    const elf3d::math::Matrix4 empty_camera_matrix = empty.local_matrix(empty_camera).value();
    const elf3d::Result<void> empty_fit = navigation.fit_to_scene(empty, empty_camera, {800, 600});
    if (empty_fit || empty_fit.error().code() != elf3d::ErrorCode::scene_has_no_bounds ||
        empty.local_matrix(empty_camera).value() != empty_camera_matrix) {
        return 21;
    }

    elf3d::navigation::OrbitNavigationController first_viewport;
    elf3d::navigation::OrbitNavigationController second_viewport;
    if (!first_viewport.reset_view(fixture.scene, fixture.camera, {800, 600}) ||
        !second_viewport.reset_view(fixture.scene, fixture.second_camera, {800, 600})) {
        return 22;
    }
    const float second_distance = second_viewport.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (nearly_equal(first_viewport.snapshot().distance, second_distance) ||
        !nearly_equal(second_viewport.snapshot().distance, second_distance)) {
        return 23;
    }
    input = hovered_input();
    input.left_button_down = true;
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!first_viewport.snapshot().is_pointer_captured ||
        second_viewport.snapshot().is_pointer_captured) {
        return 24;
    }
    first_viewport.cancel_interaction();
    if (first_viewport.snapshot().is_pointer_captured ||
        second_viewport.snapshot().is_pointer_captured) {
        return 25;
    }

    elf3d::Transform external;
    external.translation = {10.0F, 2.0F, 3.0F};
    if (!fixture.scene.set_local_transform(fixture.camera, external) ||
        !first_viewport.synchronize(fixture.scene, fixture.camera)) {
        return 26;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_delta_pixels = {500.0F, 500.0F};
    const elf3d::NavigationSnapshot before_external_drag = first_viewport.snapshot();
    static_cast<void>(
        first_viewport.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    if (!nearly_equal(first_viewport.snapshot().yaw_radians, before_external_drag.yaw_radians) ||
        !nearly_equal(first_viewport.snapshot().pitch_radians,
                      before_external_drag.pitch_radians)) {
        return 27;
    }

    return 0;
}
