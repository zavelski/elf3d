import elf.math;

#include <elf3d/core/result.h>
#include <elf3d/math/detail/glm_helpers.h>

#include <cmath>

namespace {

bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 0.0001F;
}

} // namespace

int main() {
    const elf3d::Float2 public_value{2.0F, -3.5F};
    const elf3d::math::Vector2 vector = elf3d::math::to_vector(public_value);
    if (vector.x != 2.0F || vector.y != -3.5F || elf3d::math::to_float2(vector) != public_value) {
        return 1;
    }

    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 0.4F};
    if (elf3d::math::to_color4(elf3d::math::to_vector(color)) != color) {
        return 2;
    }

    const elf3d::math::Vector3 right =
        glm::cross(elf3d::math::Vector3{1.0F, 0.0F, 0.0F}, elf3d::math::Vector3{0.0F, 1.0F, 0.0F});
    if (right != elf3d::math::Vector3{0.0F, 0.0F, 1.0F}) {
        return 3;
    }

    elf3d::math::Matrix4 column_major{1.0F};
    column_major[0][1] = 2.0F;
    column_major[1][0] = 3.0F;
    const float *storage = glm::value_ptr(column_major);
    if (storage[1] != 2.0F || storage[4] != 3.0F) {
        return 4;
    }

    const elf3d::math::Matrix4 parent =
        glm::translate(elf3d::math::Matrix4{1.0F}, elf3d::math::Vector3{10.0F, 0.0F, 0.0F});
    const elf3d::math::Matrix4 local =
        glm::translate(elf3d::math::Matrix4{1.0F}, elf3d::math::Vector3{0.0F, 2.0F, 0.0F});
    const elf3d::math::Matrix4 world = elf3d::math::to_matrix(elf3d::math::compose_world(
        elf3d::math::to_float4x4(parent), elf3d::math::to_float4x4(local)));
    const elf3d::math::Vector4 transformed = world * elf3d::math::Vector4{0.0F, 0.0F, 0.0F, 1.0F};
    if (!nearly_equal(transformed.x, 10.0F) || !nearly_equal(transformed.y, 2.0F) ||
        !nearly_equal(transformed.z, 0.0F) || !nearly_equal(transformed.w, 1.0F)) {
        return 5;
    }

    elf3d::Transform rotated_transform;
    rotated_transform.rotation =
        elf3d::Quaternion{0.0F, std::sin(0.7853981634F), 0.0F, std::cos(0.7853981634F)};
    const elf3d::math::Vector4 rotated =
        elf3d::math::to_matrix(elf3d::math::transform_matrix(rotated_transform)) *
        elf3d::math::Vector4{0.0F, 0.0F, -1.0F, 0.0F};
    if (!nearly_equal(rotated.x, -1.0F) || !nearly_equal(rotated.y, 0.0F) ||
        !nearly_equal(rotated.z, 0.0F)) {
        return 6;
    }

    elf3d::Transform camera_transform;
    camera_transform.translation = {0.0F, 0.0F, 3.0F};
    camera_transform.scale = {2.0F, 3.0F, 4.0F};
    const elf3d::Result<elf3d::Float4x4> view =
        elf3d::math::camera_view_matrix(elf3d::math::transform_matrix(camera_transform));
    if (!view) {
        return 7;
    }
    const elf3d::math::Matrix4 native_view = elf3d::math::to_matrix(view.value());
    const elf3d::math::Vector4 camera_space_origin =
        native_view * elf3d::math::Vector4{0.0F, 0.0F, 0.0F, 1.0F};
    if (!nearly_equal(camera_space_origin.x, 0.0F) || !nearly_equal(camera_space_origin.y, 0.0F) ||
        !nearly_equal(camera_space_origin.z, -3.0F)) {
        return 8;
    }

    const elf3d::Result<elf3d::Float4x4> projection =
        elf3d::math::perspective_matrix(1.5707963268F, 1.0F, 1.0F, 10.0F);
    if (!projection) {
        return 9;
    }
    const elf3d::math::Matrix4 native_projection = elf3d::math::to_matrix(projection.value());
    if (!nearly_equal(native_projection[0][0], 1.0F) ||
        !nearly_equal(native_projection[1][1], 1.0F) ||
        !nearly_equal(native_projection[2][3], -1.0F)) {
        return 9;
    }
    const elf3d::math::Vector4 near_clip =
        native_projection * elf3d::math::Vector4{0.0F, 0.0F, -1.0F, 1.0F};
    const elf3d::math::Vector4 far_clip =
        native_projection * elf3d::math::Vector4{0.0F, 0.0F, -10.0F, 1.0F};
    if (!nearly_equal(near_clip.z / near_clip.w, -1.0F) ||
        !nearly_equal(far_clip.z / far_clip.w, 1.0F)) {
        return 10;
    }

    elf3d::Transform scaled_transform;
    scaled_transform.scale = {2.0F, 4.0F, 5.0F};
    const elf3d::Result<elf3d::math::Matrix3x3> normals =
        elf3d::math::normal_matrix(elf3d::math::transform_matrix(scaled_transform));
    if (!normals || !nearly_equal(normals.value()[0], 0.5F) ||
        !nearly_equal(normals.value()[4], 0.25F) || !nearly_equal(normals.value()[8], 0.2F)) {
        return 11;
    }

    return 0;
}
