#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>
#include <elf3d/navigation.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

import elf.assets;
import elf.math;
import elf.navigation;
import elf.scene;

namespace {

struct SceneFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
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

[[nodiscard]] elf3d::Float3 add(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] elf3d::Float3 multiply(elf3d::Float3 value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float dot(elf3d::Float3 left, elf3d::Float3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
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
    return SceneFixture{std::move(scene), model, camera, second_camera};
}

[[nodiscard]] elf3d::Float3 camera_position(const elf3d::scene::Storage& scene,
                                            elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    return {world[3].x, world[3].y, world[3].z};
}

[[nodiscard]] elf3d::Float3 camera_forward(const elf3d::scene::Storage& scene,
                                           elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    elf3d::math::Vector3 right{world[0]};
    elf3d::math::Vector3 up{world[1]};
    right = glm::normalize(right);
    up = glm::normalize(up - right * glm::dot(right, up));
    const elf3d::math::Vector3 backward = glm::normalize(glm::cross(right, up));
    return elf3d::math::to_float3(-backward);
}

[[nodiscard]] elf3d::Float3 camera_right(const elf3d::scene::Storage& scene,
                                         elf3d::EntityId camera) {
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    return elf3d::math::to_float3(glm::normalize(elf3d::math::Vector3{world[0]}));
}

[[nodiscard]] bool camera_looks_at(const elf3d::scene::Storage& scene, elf3d::EntityId camera,
                                   elf3d::Float3 pivot) {
    const elf3d::Float3 position = camera_position(scene, camera);
    const elf3d::math::Vector3 to_pivot =
        glm::normalize(elf3d::math::to_vector(subtract(pivot, position)));
    const elf3d::math::Vector3 forward = elf3d::math::to_vector(camera_forward(scene, camera));
    return glm::dot(to_pivot, forward) > 0.999F;
}

void set_camera_position(elf3d::scene::Storage& scene, elf3d::EntityId camera,
                         elf3d::Float3 position) {
    elf3d::math::Matrix4 world = elf3d::math::to_matrix(scene.world_matrix(camera).value());
    world[3] = elf3d::math::Vector4{position.x, position.y, position.z, 1.0F};
    static_cast<void>(scene.set_local_matrix(camera, elf3d::math::to_float4x4(world)));
}

[[nodiscard]] float signed_camera_distance_to(const elf3d::scene::Storage& scene,
                                              elf3d::EntityId camera,
                                              elf3d::Float3 world_position) {
    return dot(subtract(world_position, camera_position(scene, camera)),
               camera_forward(scene, camera));
}

[[nodiscard]] bool depth_ratio_within_limit(const elf3d::scene::Storage& scene,
                                            elf3d::EntityId camera) {
    const elf3d::PerspectiveCameraDescription description =
        scene.perspective_camera(camera).value();
    return description.near_plane > 0.0F && description.far_plane > description.near_plane &&
           description.far_plane / description.near_plane <= 10000.5F;
}

[[nodiscard]] elf3d::Float2 project_to_ndc(const elf3d::scene::Storage& scene,
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

[[nodiscard]] bool bounds_visible(const elf3d::scene::Storage& scene, elf3d::EntityId camera,
                                  elf3d::Extent2D extent) {
    const std::optional<elf3d::Bounds3> bounds_result = scene.world_bounds();
    if (!bounds_result.has_value()) {
        return false;
    }
    const elf3d::Bounds3 bounds = bounds_result.value();
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

[[nodiscard]] float maximum_projected_bounds_extent(const elf3d::scene::Storage& scene,
                                                    elf3d::EntityId camera,
                                                    elf3d::Extent2D extent) {
    const std::optional<elf3d::Bounds3> bounds_result = scene.world_bounds();
    if (!bounds_result.has_value()) {
        return 0.0F;
    }
    const elf3d::Bounds3 bounds = bounds_result.value();
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
    if (!nearly_equal(reset.pivot, expected_center) ||
        !camera_looks_at(fixture.scene, fixture.camera, reset.pivot) ||
        !bounds_visible(fixture.scene, fixture.camera, {800, 600})) {
        return 2;
    }
    const elf3d::Float3 pivot_target{2.0F, 1.0F, 0.0F};
    const elf3d::Float3 position_before_pivot = camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_pivot = camera_forward(fixture.scene, fixture.camera);
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, pivot_target)) {
        return 28;
    }
    const elf3d::NavigationSnapshot anchored = navigation.snapshot();
    if (!nearly_equal(anchored.pivot, reset.pivot) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_pivot) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_pivot) ||
        !(anchored.distance > 0.0F)) {
        return 29;
    }
    elf3d::ViewportInput pivot_input = hovered_input();
    pivot_input.left_button_down = true;
    pivot_input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input, click_threshold));
    pivot_input.pointer_position_pixels = {20.0F, 10.0F};
    pivot_input.pointer_delta_pixels = {10.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input,
                           click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_pivot) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_pivot)) {
        return 30;
    }
    pivot_input.left_button_down = false;
    pivot_input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, pivot_input, click_threshold));
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 31;
    }
    if (!navigation.fit_to_scene(fixture.scene, fixture.camera, {360, 800}) ||
        !bounds_visible(fixture.scene, fixture.camera, {360, 800})) {
        return 3;
    }
    const float fitted_bounds_extent =
        maximum_projected_bounds_extent(fixture.scene, fixture.camera, {360, 800});
    if (fitted_bounds_extent < 0.85F || fitted_bounds_extent > 1.001F) {
        return 96;
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
        !(yawed.yaw_radians < first_drag.yaw_radians) ||
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
    if (!(pitched.pitch_radians > yawed.pitch_radians) ||
        pitched.pitch_radians < navigation.settings().minimum_pitch_radians ||
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
        return 104;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {10.0F, 10.0F};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_position_pixels = {40.0F, 10.0F};
    input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 105;
    }
    input.right_button_down = true;
    input.pointer_delta_pixels = {0.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 106;
    }
    input.left_button_down = false;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan) {
        return 107;
    }
    const elf3d::Float3 position_before_handoff_pan =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_handoff_pan = camera_forward(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera), position_before_handoff_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera), forward_before_handoff_pan) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan) {
        return 108;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 109;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 110;
    }
    input = hovered_input();
    input.right_button_down = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan ||
        !navigation.snapshot().is_pointer_captured) {
        return 111;
    }
    input.left_button_down = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::pan ||
        !navigation.snapshot().is_pointer_captured) {
        return 112;
    }
    input.right_button_down = false;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> orbit_handoff =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!orbit_handoff ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit ||
        !navigation.snapshot().is_pointer_captured ||
        !orbit_handoff.value().orbit_start_position_pixels.has_value()) {
        return 113;
    }
    const float yaw_before_handoff_orbit = navigation.snapshot().yaw_radians;
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(navigation.snapshot().yaw_radians, yaw_before_handoff_orbit) ||
        navigation.snapshot().interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 114;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 115;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 11;
    }
    const elf3d::NavigationSnapshot before_pan = navigation.snapshot();
    const elf3d::Float3 before_forward = camera_forward(fixture.scene, fixture.camera);
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
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
    input.x_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    const float distance_before_zoom = navigation.snapshot().distance;
    input = hovered_input();
    input.left_button_down = true;
    input.z_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {0.0F, -80.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < distance_before_zoom)) {
        return 32;
    }
    input.left_button_down = false;
    input.z_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    const float distance_before_wheel_zoom = navigation.snapshot().distance;
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
    if (!(distance_after_zoom_in < distance_before_wheel_zoom) ||
        !(distance_after_zoom_out > distance_after_zoom_in) ||
        !nearly_equal(navigation.snapshot().distance, distance_after_zoom_out)) {
        return 15;
    }
    input = hovered_input();
    input.is_focused = false;
    input.wheel_delta = 1.0F;
    const float distance_before_hover_wheel = navigation.snapshot().distance;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < distance_before_hover_wheel)) {
        return 33;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 68;
    }
    const float distance_before_scaled_wheel = navigation.snapshot().distance;
    input = hovered_input();
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 69;
    }
    const float scaled_wheel_step = distance_before_scaled_wheel - navigation.snapshot().distance;
    const float full_wheel_step =
        distance_before_scaled_wheel * (1.0F - std::exp(-navigation.settings().zoom_sensitivity));
    if (!nearly_equal(scaled_wheel_step, full_wheel_step * 0.5F)) {
        return 70;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 75;
    }
    const elf3d::NavigationSnapshot before_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 position_before_keyboard_forward =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_keyboard_forward =
        camera_forward(fixture.scene, fixture.camera);
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera,
                                      before_keyboard_forward.pivot)) {
        return 130;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_forward =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_forward || !keyboard_forward.value().orbit_start_position_pixels.has_value() ||
        navigation.has_screen_anchor()) {
        return 76;
    }
    const elf3d::NavigationSnapshot after_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 keyboard_forward_offset =
        subtract(camera_position(fixture.scene, fixture.camera), position_before_keyboard_forward);
    const float keyboard_forward_step =
        before_keyboard_forward.distance - after_keyboard_forward.distance;
    if (!nearly_equal(after_keyboard_forward.pivot, before_keyboard_forward.pivot) ||
        !nearly_equal(keyboard_forward_step, scaled_wheel_step * 0.025F) ||
        !nearly_equal(keyboard_forward_offset,
                      multiply(forward_before_keyboard_forward, keyboard_forward_step))) {
        return 77;
    }
    input.w_pressed = false;
    input.s_pressed = true;
    const elf3d::Float3 position_before_keyboard_backward =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 forward_before_keyboard_backward =
        camera_forward(fixture.scene, fixture.camera);
    const float distance_before_keyboard_backward = navigation.snapshot().distance;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance > distance_before_keyboard_backward) ||
        !(dot(subtract(camera_position(fixture.scene, fixture.camera),
                       position_before_keyboard_backward),
              forward_before_keyboard_backward) < 0.0F)) {
        return 78;
    }
    input.left_button_down = false;
    input.s_pressed = false;
    const elf3d::Float3 position_before_keyboard_release =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_release =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_release || keyboard_release.value().click_position_pixels.has_value() ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_keyboard_release)) {
        return 86;
    }
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> keyboard_reentry =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    if (!keyboard_reentry || !keyboard_reentry.value().orbit_start_position_pixels.has_value()) {
        return 131;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 138;
    }
    const elf3d::NavigationSnapshot before_local_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 local_keyboard_position_before =
        camera_position(fixture.scene, fixture.camera);
    const elf3d::Float3 far_anchor = add(
        local_keyboard_position_before, multiply(camera_forward(fixture.scene, fixture.camera),
                                                 before_local_keyboard_forward.distance * 1000.0F));
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, far_anchor)) {
        return 139;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.w_pressed = true;
    const elf3d::Result<elf3d::navigation::NavigationUpdate> local_keyboard_forward =
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold);
    const float keyboard_base_multiplier = std::exp(-navigation.settings().zoom_sensitivity);
    const float keyboard_reference_multiplier =
        1.0F + (keyboard_base_multiplier - 1.0F) * 0.5F * 0.025F;
    const float expected_local_keyboard_step =
        before_local_keyboard_forward.distance * (1.0F - keyboard_reference_multiplier);
    const float local_keyboard_step =
        before_local_keyboard_forward.distance - navigation.snapshot().distance;
    if (!local_keyboard_forward ||
        !local_keyboard_forward.value().orbit_start_position_pixels.has_value() ||
        navigation.has_screen_anchor() ||
        !nearly_equal(local_keyboard_step, expected_local_keyboard_step, 0.0005F)) {
        return 140;
    }
    input.left_button_down = false;
    input.w_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 99;
    }
    input = hovered_input();
    input.right_button_down = true;
    input.w_pressed = true;
    const elf3d::NavigationSnapshot before_right_keyboard_forward = navigation.snapshot();
    const elf3d::Float3 position_before_right_keyboard_forward =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(navigation.snapshot().distance < before_right_keyboard_forward.distance) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_right_keyboard_forward)) {
        return 100;
    }
    input.w_pressed = false;
    input.a_pressed = true;
    const elf3d::Float3 position_before_right_keyboard_pan =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_right_keyboard_pan)) {
        return 101;
    }
    input.a_pressed = false;
    input.e_pressed = true;
    const elf3d::Float3 position_before_right_keyboard_up =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 102;
    }
    const elf3d::Float3 position_after_right_keyboard_up =
        camera_position(fixture.scene, fixture.camera);
    if (!(position_after_right_keyboard_up.y > position_before_right_keyboard_up.y) ||
        !nearly_equal(position_after_right_keyboard_up.x, position_before_right_keyboard_up.x) ||
        !nearly_equal(position_after_right_keyboard_up.z, position_before_right_keyboard_up.z)) {
        return 103;
    }
    input.right_button_down = false;
    input.e_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 71;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 72;
    }
    const elf3d::Float3 mouse_pan_delta_position = camera_position(fixture.scene, fixture.camera);

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 73;
    }
    input = hovered_input();
    input.right_button_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {80.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera), mouse_pan_delta_position)) {
        return 74;
    }
    input.right_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    constexpr float keyboard_pan_test_step = 800.0F / 400.0F;
    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 79;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.x_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));
    input.pointer_delta_pixels = {keyboard_pan_test_step, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 80;
    }
    const elf3d::Float3 keyboard_horizontal_pan_delta_position =
        camera_position(fixture.scene, fixture.camera);

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 87;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.a_pressed = true;
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      keyboard_horizontal_pan_delta_position)) {
        return 88;
    }
    input.left_button_down = false;
    input.a_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 81;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.e_pressed = true;
    const elf3d::Float3 before_keyboard_up_pan = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold)) {
        return 82;
    }
    const elf3d::Float3 after_keyboard_up_pan = camera_position(fixture.scene, fixture.camera);
    if (!(after_keyboard_up_pan.y > before_keyboard_up_pan.y) ||
        !nearly_equal(after_keyboard_up_pan.x, before_keyboard_up_pan.x) ||
        !nearly_equal(after_keyboard_up_pan.z, before_keyboard_up_pan.z)) {
        return 84;
    }
    input.e_pressed = false;
    input.q_pressed = true;
    const elf3d::Float3 before_keyboard_down_pan = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold) ||
        !(camera_position(fixture.scene, fixture.camera).y < before_keyboard_down_pan.y) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera).x,
                      before_keyboard_down_pan.x) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera).z,
                      before_keyboard_down_pan.z)) {
        return 85;
    }
    input.left_button_down = false;
    input.q_pressed = false;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, {800, 600}, input, click_threshold));

    SceneFixture forward_turn_fixture = make_scene(4, {99'000'000.0F, -1'000'000.0F, -1'000'000.0F},
                                                   {101'000'000.0F, 1'000'000.0F, 1'000'000.0F});
    elf3d::navigation::OrbitNavigationController forward_turn_navigation;
    if (!forward_turn_navigation.reset_view(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                            {800, 600})) {
        return 116;
    }
    elf3d::ViewportInput forward_turn_input = hovered_input();
    forward_turn_input.left_button_down = true;
    forward_turn_input.pointer_position_pixels = {10.0F, 10.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 117;
    }
    forward_turn_input.pointer_position_pixels = {80.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {70.0F, 0.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold) ||
        !forward_turn_navigation.set_screen_anchor(forward_turn_fixture.scene,
                                                   forward_turn_fixture.camera,
                                                   forward_turn_navigation.snapshot().pivot)) {
        return 118;
    }
    const elf3d::NavigationSnapshot before_first_forward_turn = forward_turn_navigation.snapshot();
    const elf3d::Float3 fixed_dynamic_center = before_first_forward_turn.pivot;
    forward_turn_input.pointer_position_pixels = {120.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {40.0F, 0.0F};
    forward_turn_input.w_pressed = true;
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 119;
    }
    const elf3d::NavigationSnapshot after_first_forward_turn = forward_turn_navigation.snapshot();
    if (nearly_equal(after_first_forward_turn.yaw_radians, before_first_forward_turn.yaw_radians) ||
        !(after_first_forward_turn.distance < before_first_forward_turn.distance) ||
        length(subtract(after_first_forward_turn.pivot, fixed_dynamic_center)) > 32.0F ||
        !forward_turn_navigation.has_screen_anchor() ||
        !camera_looks_at(forward_turn_fixture.scene, forward_turn_fixture.camera,
                         fixed_dynamic_center)) {
        return 120;
    }

    const float yaw_before_second_forward_turn = after_first_forward_turn.yaw_radians;
    const float distance_before_second_forward_turn = after_first_forward_turn.distance;
    forward_turn_input.pointer_position_pixels = {150.0F, 10.0F};
    forward_turn_input.pointer_delta_pixels = {30.0F, 0.0F};
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold)) {
        return 121;
    }
    const elf3d::NavigationSnapshot after_second_forward_turn = forward_turn_navigation.snapshot();
    if (nearly_equal(after_second_forward_turn.yaw_radians, yaw_before_second_forward_turn) ||
        !(after_second_forward_turn.distance < distance_before_second_forward_turn) ||
        length(subtract(after_second_forward_turn.pivot, fixed_dynamic_center)) > 32.0F ||
        !forward_turn_navigation.has_screen_anchor() ||
        !camera_looks_at(forward_turn_fixture.scene, forward_turn_fixture.camera,
                         fixed_dynamic_center)) {
        return 122;
    }
    forward_turn_input.w_pressed = false;
    forward_turn_input.pointer_delta_pixels = {};
    const elf3d::Float3 position_before_forward_release =
        camera_position(forward_turn_fixture.scene, forward_turn_fixture.camera);
    if (!forward_turn_navigation.update(forward_turn_fixture.scene, forward_turn_fixture.camera,
                                        {800, 600}, forward_turn_input, click_threshold) ||
        !nearly_equal(camera_position(forward_turn_fixture.scene, forward_turn_fixture.camera),
                      position_before_forward_release)) {
        return 123;
    }
    forward_turn_input.left_button_down = false;
    static_cast<void>(forward_turn_navigation.update(forward_turn_fixture.scene,
                                                     forward_turn_fixture.camera, {800, 600},
                                                     forward_turn_input, click_threshold));

    SceneFixture low_fps_fixture = make_scene(5, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    SceneFixture high_fps_fixture = make_scene(8, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController low_fps_navigation;
    elf3d::navigation::OrbitNavigationController high_fps_navigation;
    if (!low_fps_navigation.reset_view(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600}) ||
        !high_fps_navigation.reset_view(high_fps_fixture.scene, high_fps_fixture.camera,
                                        {800, 600})) {
        return 132;
    }
    elf3d::ViewportInput low_fps_input = hovered_input();
    low_fps_input.left_button_down = true;
    low_fps_input.pointer_position_pixels = {10.0F, 10.0F};
    elf3d::ViewportInput high_fps_input = low_fps_input;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 133;
    }
    low_fps_input.pointer_position_pixels = {80.0F, 10.0F};
    low_fps_input.pointer_delta_pixels = {70.0F, 0.0F};
    high_fps_input = low_fps_input;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold) ||
        !low_fps_navigation.set_screen_anchor(low_fps_fixture.scene, low_fps_fixture.camera,
                                              low_fps_navigation.snapshot().pivot) ||
        !high_fps_navigation.set_screen_anchor(high_fps_fixture.scene, high_fps_fixture.camera,
                                               high_fps_navigation.snapshot().pivot)) {
        return 134;
    }
    low_fps_input.frame_delta_seconds = 1.0F / 30.0F;
    low_fps_input.pointer_position_pixels = {120.0F, 10.0F};
    low_fps_input.pointer_delta_pixels = {40.0F, 0.0F};
    low_fps_input.w_pressed = true;
    high_fps_input.frame_delta_seconds = 1.0F / 60.0F;
    high_fps_input.pointer_position_pixels = {100.0F, 10.0F};
    high_fps_input.pointer_delta_pixels = {20.0F, 0.0F};
    high_fps_input.w_pressed = true;
    if (!low_fps_navigation.update(low_fps_fixture.scene, low_fps_fixture.camera, {800, 600},
                                   low_fps_input, click_threshold) ||
        !high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 135;
    }
    high_fps_input.pointer_position_pixels = {120.0F, 10.0F};
    if (!high_fps_navigation.update(high_fps_fixture.scene, high_fps_fixture.camera, {800, 600},
                                    high_fps_input, click_threshold)) {
        return 136;
    }
    const elf3d::NavigationSnapshot low_fps_snapshot = low_fps_navigation.snapshot();
    const elf3d::NavigationSnapshot high_fps_snapshot = high_fps_navigation.snapshot();
    if (!nearly_equal(low_fps_snapshot.yaw_radians, high_fps_snapshot.yaw_radians) ||
        !nearly_equal(low_fps_snapshot.pitch_radians, high_fps_snapshot.pitch_radians) ||
        !nearly_equal(low_fps_snapshot.distance, high_fps_snapshot.distance, 0.002F) ||
        !nearly_equal(low_fps_snapshot.pivot, high_fps_snapshot.pivot, 0.002F) ||
        !nearly_equal(camera_position(low_fps_fixture.scene, low_fps_fixture.camera),
                      camera_position(high_fps_fixture.scene, high_fps_fixture.camera), 0.002F) ||
        !low_fps_navigation.has_screen_anchor() || !high_fps_navigation.has_screen_anchor()) {
        return 137;
    }

    SceneFixture combined_pan_fixture = make_scene(6, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    SceneFixture mouse_pan_fixture = make_scene(7, {-1.0F, -2.0F, -3.0F}, {4.0F, 5.0F, 6.0F});
    elf3d::navigation::OrbitNavigationController combined_pan_navigation;
    elf3d::navigation::OrbitNavigationController mouse_pan_navigation;
    if (!combined_pan_navigation.reset_view(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                            {800, 600}) ||
        !mouse_pan_navigation.reset_view(mouse_pan_fixture.scene, mouse_pan_fixture.camera,
                                         {800, 600})) {
        return 124;
    }
    elf3d::ViewportInput combined_pan_input = hovered_input();
    combined_pan_input.right_button_down = true;
    elf3d::ViewportInput mouse_pan_input = combined_pan_input;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold)) {
        return 125;
    }
    combined_pan_input.pointer_delta_pixels = {80.0F, 40.0F};
    combined_pan_input.w_pressed = true;
    mouse_pan_input = combined_pan_input;
    mouse_pan_input.w_pressed = false;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold)) {
        return 126;
    }
    const elf3d::Float3 combined_pan_offset =
        subtract(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                 camera_position(mouse_pan_fixture.scene, mouse_pan_fixture.camera));
    const elf3d::Float3 current_pan_forward =
        camera_forward(mouse_pan_fixture.scene, mouse_pan_fixture.camera);
    const float combined_pan_forward_distance = dot(combined_pan_offset, current_pan_forward);
    if (!(combined_pan_forward_distance > 0.0F) ||
        !nearly_equal(combined_pan_offset,
                      multiply(current_pan_forward, combined_pan_forward_distance)) ||
        !(combined_pan_navigation.snapshot().distance < mouse_pan_navigation.snapshot().distance)) {
        return 127;
    }
    combined_pan_input.w_pressed = false;
    combined_pan_input.pointer_delta_pixels = {};
    mouse_pan_input = combined_pan_input;
    const elf3d::Float3 position_before_pan_key_release =
        camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera);
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold) ||
        !nearly_equal(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                      position_before_pan_key_release)) {
        return 128;
    }
    const elf3d::Float3 position_before_mouse_only_pan =
        camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera);
    combined_pan_input.pointer_delta_pixels = {20.0F, 10.0F};
    mouse_pan_input = combined_pan_input;
    if (!combined_pan_navigation.update(combined_pan_fixture.scene, combined_pan_fixture.camera,
                                        {800, 600}, combined_pan_input, click_threshold) ||
        !mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera, {800, 600},
                                     mouse_pan_input, click_threshold) ||
        nearly_equal(camera_position(combined_pan_fixture.scene, combined_pan_fixture.camera),
                     position_before_mouse_only_pan)) {
        return 129;
    }
    combined_pan_input.right_button_down = false;
    combined_pan_input.pointer_delta_pixels = {};
    mouse_pan_input = combined_pan_input;
    static_cast<void>(combined_pan_navigation.update(combined_pan_fixture.scene,
                                                     combined_pan_fixture.camera, {800, 600},
                                                     combined_pan_input, click_threshold));
    static_cast<void>(mouse_pan_navigation.update(mouse_pan_fixture.scene, mouse_pan_fixture.camera,
                                                  {800, 600}, mouse_pan_input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, {800, 600})) {
        return 34;
    }
    constexpr elf3d::Extent2D viewport_extent{800, 600};
    const elf3d::Float3 forward_before_click_pivot = camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot = camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot{
        position_before_click_pivot.x + forward_before_click_pivot.x * click_pivot_distance +
            right_before_click_pivot.x * 2.0F,
        position_before_click_pivot.y + forward_before_click_pivot.y * click_pivot_distance +
            right_before_click_pivot.y * 2.0F,
        position_before_click_pivot.z + forward_before_click_pivot.z * click_pivot_distance +
            right_before_click_pivot.z * 2.0F,
    };
    const elf3d::Float2 projected_click_pivot_before =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot);
    if (!std::isfinite(projected_click_pivot_before.x) ||
        !std::isfinite(projected_click_pivot_before.y) ||
        std::abs(projected_click_pivot_before.x) >= 1.0F ||
        std::abs(projected_click_pivot_before.y) >= 1.0F) {
        return 35;
    }

    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {
        (projected_click_pivot_before.x + 1.0F) * 0.5F * static_cast<float>(viewport_extent.width),
        (1.0F - projected_click_pivot_before.y) * 0.5F * static_cast<float>(viewport_extent.height),
    };
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 36;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 37;
    }
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot)) {
        return 38;
    }
    const float distance_before_click_pivot_wheel =
        length(subtract(off_axis_click_pivot, position_before_click_pivot));
    const std::uint64_t scene_revision_before_model_update = fixture.scene.revision();
    if (!fixture.scene.set_local_transform(fixture.model, elf3d::Transform{}) ||
        fixture.scene.revision() == scene_revision_before_model_update) {
        return 39;
    }

    input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot)) {
        return 40;
    }
    input.pointer_delta_pixels = {};
    input.wheel_delta = 1.0F;
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 41;
    }
    const elf3d::Float3 forward_after_click_pivot_wheel =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float2 projected_click_pivot_after =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot);
    if (!nearly_equal(forward_after_click_pivot_wheel, forward_before_click_pivot) ||
        !(navigation.snapshot().distance < distance_before_click_pivot_wheel) ||
        !nearly_equal(projected_click_pivot_after.x, projected_click_pivot_before.x, 0.002F) ||
        !nearly_equal(projected_click_pivot_after.y, projected_click_pivot_before.y, 0.002F)) {
        return 42;
    }
    input = hovered_input();
    input.middle_button_down = true;
    const elf3d::Float3 before_post_dolly_pan_start =
        camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      before_post_dolly_pan_start) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_after_click_pivot_wheel)) {
        return 65;
    }
    const elf3d::Float3 before_post_dolly_pan = camera_position(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {1.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 66;
    }
    const float post_dolly_pan_step =
        length(subtract(camera_position(fixture.scene, fixture.camera), before_post_dolly_pan));
    if (post_dolly_pan_step <= 0.0F || post_dolly_pan_step > 0.25F ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_after_click_pivot_wheel)) {
        return 67;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 43;
    }
    const elf3d::Float3 forward_before_click_pivot_pan =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot_pan = camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot_pan =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_pan_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot_pan{
        position_before_click_pivot_pan.x +
            forward_before_click_pivot_pan.x * click_pivot_pan_distance +
            right_before_click_pivot_pan.x * 2.0F,
        position_before_click_pivot_pan.y +
            forward_before_click_pivot_pan.y * click_pivot_pan_distance +
            right_before_click_pivot_pan.y * 2.0F,
        position_before_click_pivot_pan.z +
            forward_before_click_pivot_pan.z * click_pivot_pan_distance +
            right_before_click_pivot_pan.z * 2.0F,
    };
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_pan)) {
        return 44;
    }
    input = hovered_input();
    input.pointer_position_pixels = {400.0F, 300.0F};
    input.pointer_delta_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan)) {
        return 45;
    }
    input.middle_button_down = true;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_pan) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan)) {
        return 46;
    }
    input.pointer_position_pixels = {480.0F, 340.0F};
    input.pointer_delta_pixels = {80.0F, 40.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        !navigation.snapshot().is_pointer_captured ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_pan) ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_click_pivot_pan)) {
        return 47;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 48;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 49;
    }
    const elf3d::Float3 forward_before_click_pivot_orbit =
        camera_forward(fixture.scene, fixture.camera);
    const elf3d::Float3 right_before_click_pivot_orbit =
        camera_right(fixture.scene, fixture.camera);
    const elf3d::Float3 position_before_click_pivot_orbit =
        camera_position(fixture.scene, fixture.camera);
    const float click_pivot_orbit_distance = navigation.snapshot().distance;
    const elf3d::Float3 off_axis_click_pivot_orbit{
        position_before_click_pivot_orbit.x +
            forward_before_click_pivot_orbit.x * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.x * 2.0F,
        position_before_click_pivot_orbit.y +
            forward_before_click_pivot_orbit.y * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.y * 2.0F,
        position_before_click_pivot_orbit.z +
            forward_before_click_pivot_orbit.z * click_pivot_orbit_distance +
            right_before_click_pivot_orbit.z * 2.0F,
    };
    const elf3d::Float2 projected_click_pivot_orbit_before =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot_orbit);
    if (!std::isfinite(projected_click_pivot_orbit_before.x) ||
        !std::isfinite(projected_click_pivot_orbit_before.y) ||
        std::abs(projected_click_pivot_orbit_before.x) >= 1.0F ||
        std::abs(projected_click_pivot_orbit_before.y) >= 1.0F) {
        return 50;
    }
    if (!navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_orbit)) {
        return 51;
    }
    input = hovered_input();
    input.left_button_down = true;
    input.pointer_position_pixels = {400.0F, 300.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 52;
    }
    input.pointer_position_pixels = {460.0F, 330.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    const elf3d::Result<elf3d::navigation::NavigationUpdate> click_pivot_orbit_start =
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold);
    if (!click_pivot_orbit_start ||
        !click_pivot_orbit_start.value().orbit_start_position_pixels.has_value() ||
        !nearly_equal(camera_position(fixture.scene, fixture.camera),
                      position_before_click_pivot_orbit) ||
        !nearly_equal(camera_forward(fixture.scene, fixture.camera),
                      forward_before_click_pivot_orbit) ||
        !navigation.set_screen_anchor(fixture.scene, fixture.camera, off_axis_click_pivot_orbit)) {
        return 53;
    }
    input.pointer_position_pixels = {520.0F, 360.0F};
    input.pointer_delta_pixels = {60.0F, 30.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 54;
    }
    const elf3d::Float2 projected_click_pivot_orbit_after =
        project_to_ndc(fixture.scene, fixture.camera, viewport_extent, off_axis_click_pivot_orbit);
    if (!navigation.snapshot().is_pointer_captured ||
        nearly_equal(camera_position(fixture.scene, fixture.camera),
                     position_before_click_pivot_orbit) ||
        nearly_equal(camera_forward(fixture.scene, fixture.camera),
                     forward_before_click_pivot_orbit) ||
        camera_looks_at(fixture.scene, fixture.camera, off_axis_click_pivot_orbit) ||
        !nearly_equal(projected_click_pivot_orbit_after.x, projected_click_pivot_orbit_before.x,
                      0.002F) ||
        !nearly_equal(projected_click_pivot_orbit_after.y, projected_click_pivot_orbit_before.y,
                      0.002F)) {
        return 55;
    }
    input.left_button_down = false;
    input.pointer_delta_pixels = {};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        navigation.snapshot().is_pointer_captured) {
        return 56;
    }

    if (!navigation.reset_view(fixture.scene, fixture.camera, viewport_extent)) {
        return 57;
    }
    const elf3d::NavigationSnapshot crossing_start = navigation.snapshot();
    const elf3d::Float3 crossing_forward = camera_forward(fixture.scene, fixture.camera);
    const float scaled_wheel_step_ratio =
        (1.0F - std::exp(-navigation.settings().zoom_sensitivity)) * 0.5F;
    const std::optional<elf3d::Bounds3> crossing_bounds = fixture.scene.world_bounds();
    if (!crossing_bounds.has_value()) {
        return 141;
    }
    const elf3d::Float3 crossing_bounds_center =
        multiply(add(crossing_bounds->minimum, crossing_bounds->maximum), 0.5F);
    const float crossing_reference =
        length(subtract(crossing_bounds->maximum, crossing_bounds_center));
    const float minimum_motion = crossing_reference * navigation.settings().minimum_motion_scale;
    const float expected_crossing_step = minimum_motion * scaled_wheel_step_ratio;
    const float starting_signed_distance = expected_crossing_step * 1.5F;
    set_camera_position(
        fixture.scene, fixture.camera,
        add(crossing_start.pivot, multiply(crossing_forward, -starting_signed_distance)));
    if (!navigation.synchronize(fixture.scene, fixture.camera) ||
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <=
            expected_crossing_step) {
        return 58;
    }
    input = hovered_input();
    input.wheel_delta = 1.0F;
    const elf3d::Float3 before_crossing_step = camera_position(fixture.scene, fixture.camera);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 59;
    }
    const elf3d::Float3 before_crossing = camera_position(fixture.scene, fixture.camera);
    const float initial_crossing_step = length(subtract(before_crossing, before_crossing_step));
    if (signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <= 0.0F) {
        return 60;
    }
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold)) {
        return 61;
    }
    const elf3d::Float3 after_crossing = camera_position(fixture.scene, fixture.camera);
    const float crossing_step = length(subtract(after_crossing, before_crossing));
    const float step_ratio = crossing_step / initial_crossing_step;
    const float local_reference_step_ratio = crossing_step / expected_crossing_step;
    if (signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) >= 0.0F ||
        step_ratio < 0.45F || step_ratio > 1.05F || local_reference_step_ratio < 0.45F ||
        local_reference_step_ratio > 1.05F ||
        !depth_ratio_within_limit(fixture.scene, fixture.camera)) {
        return 62;
    }
    input.wheel_delta = -1.0F;
    const float signed_after_crossing =
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot);
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        signed_camera_distance_to(fixture.scene, fixture.camera, crossing_start.pivot) <=
            signed_after_crossing) {
        return 63;
    }
    input = hovered_input();
    input.middle_button_down = true;
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));
    const elf3d::Float3 before_post_crossing_pan = camera_position(fixture.scene, fixture.camera);
    input.pointer_delta_pixels = {40.0F, 0.0F};
    if (!navigation.update(fixture.scene, fixture.camera, viewport_extent, input,
                           click_threshold) ||
        length(subtract(camera_position(fixture.scene, fixture.camera),
                        before_post_crossing_pan)) <= minimum_motion * 0.01F) {
        return 64;
    }
    input.middle_button_down = false;
    input.pointer_delta_pixels = {};
    static_cast<void>(
        navigation.update(fixture.scene, fixture.camera, viewport_extent, input, click_threshold));

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
    const elf3d::Float4x4 empty_camera_matrix = empty.local_matrix(empty_camera).value();
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
