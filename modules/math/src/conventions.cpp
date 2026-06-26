module;

#include <elf3d/math/detail/glm_helpers.h>

#include <cmath>

module elf.math;

namespace elf3d::math {
namespace {

float clamp_channel(float value, float fallback) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

Color4 clamp_color(Color4 value) noexcept {
    return Color4{clamp_channel(value.red, 0.0F), clamp_channel(value.green, 0.0F),
                  clamp_channel(value.blue, 0.0F), clamp_channel(value.alpha, 1.0F)};
}

bool is_finite(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool is_finite(Quaternion value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
           std::isfinite(value.w);
}

bool is_valid_transform(const Transform &transform) noexcept {
    if (!is_finite(transform.translation) || !is_finite(transform.rotation) ||
        !is_finite(transform.scale)) {
        return false;
    }

    constexpr float minimum_length = 0.000001F;
    const Rotation rotation = to_rotation(transform.rotation);
    return glm::length(rotation) > minimum_length && std::abs(transform.scale.x) > minimum_length &&
           std::abs(transform.scale.y) > minimum_length &&
           std::abs(transform.scale.z) > minimum_length;
}

bool is_valid_affine_matrix(const Float4x4 &matrix) noexcept {
    for (const float value : matrix.elements) {
        if (!std::isfinite(value)) {
            return false;
        }
    }

    const Matrix4 native = to_matrix(matrix);
    constexpr float tolerance = 0.00001F;
    if (std::abs(native[0][3]) > tolerance || std::abs(native[1][3]) > tolerance ||
        std::abs(native[2][3]) > tolerance || std::abs(native[3][3] - 1.0F) > tolerance) {
        return false;
    }

    const float determinant = glm::determinant(Matrix3{native});
    return std::isfinite(determinant) && std::abs(determinant) > 0.000001F;
}

Transform normalized_transform(const Transform &transform) noexcept {
    Transform result = transform;
    result.rotation = to_quaternion(glm::normalize(to_rotation(transform.rotation)));
    return result;
}

Float4x4 compose_world(const Float4x4 &parent_world, const Float4x4 &local) noexcept {
    return to_float4x4(to_matrix(parent_world) * to_matrix(local));
}

Float4x4 transform_matrix(const Transform &transform) noexcept {
    const Matrix4 translation = glm::translate(Matrix4{1.0F}, to_vector(transform.translation));
    const Matrix4 rotation = glm::mat4_cast(glm::normalize(to_rotation(transform.rotation)));
    const Matrix4 scale = glm::scale(Matrix4{1.0F}, to_vector(transform.scale));
    return to_float4x4(translation * rotation * scale);
}

Result<Float4x4> camera_view_matrix(const Float4x4 &camera_world) noexcept {
    const Matrix4 native_camera_world = to_matrix(camera_world);
    const Vector3 position = Vector3{native_camera_world[3]};
    Vector3 right = Vector3{native_camera_world[0]};
    Vector3 up = Vector3{native_camera_world[1]};

    constexpr float minimum_length = 0.000001F;
    const auto finite_vector = [](const Vector3 &value) noexcept {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite_vector(position) || !finite_vector(right) || !finite_vector(up) ||
        glm::length(right) <= minimum_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "The camera world transform cannot produce a finite rigid view matrix"};
    }

    right = glm::normalize(right);
    up -= right * glm::dot(right, up);
    if (glm::length(up) <= minimum_length) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "The camera world transform has degenerate orientation axes"};
    }
    up = glm::normalize(up);
    const Vector3 backward = glm::normalize(glm::cross(right, up));
    up = glm::normalize(glm::cross(backward, right));

    Matrix4 rigid_world{1.0F};
    rigid_world[0] = Vector4{right, 0.0F};
    rigid_world[1] = Vector4{up, 0.0F};
    rigid_world[2] = Vector4{backward, 0.0F};
    rigid_world[3] = Vector4{position, 1.0F};
    return to_float4x4(glm::inverse(rigid_world));
}

Result<Float4x4> perspective_matrix(float vertical_field_of_view_radians, float aspect_ratio,
                                    float near_plane, float far_plane) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    if (!std::isfinite(vertical_field_of_view_radians) || !std::isfinite(aspect_ratio) ||
        !std::isfinite(near_plane) || !std::isfinite(far_plane) ||
        vertical_field_of_view_radians <= 0.0F || vertical_field_of_view_radians >= pi ||
        aspect_ratio <= 0.0F || near_plane <= 0.0F || far_plane <= near_plane) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Perspective projection requires finite field of view, aspect, and planes"};
    }
    return to_float4x4(glm::perspectiveRH_NO(vertical_field_of_view_radians, aspect_ratio,
                                             near_plane, far_plane));
}

Result<Matrix3x3> normal_matrix(const Float4x4 &model) noexcept {
    const Matrix3 linear{to_matrix(model)};
    const float determinant = glm::determinant(linear);
    if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
        return Error{ErrorCode::invalid_argument,
                     "A model transform must be invertible to transform normals"};
    }
    Matrix3x3 result{};
    const Matrix3 native = glm::transpose(glm::inverse(linear));
    std::copy_n(glm::value_ptr(native), result.size(), result.begin());
    return result;
}

Float3 transform_point(const Float4x4 &matrix, Float3 point) noexcept {
    const Vector4 transformed = to_matrix(matrix) * Vector4{point.x, point.y, point.z, 1.0F};
    return Float3{transformed.x, transformed.y, transformed.z};
}

} // namespace elf3d::math
