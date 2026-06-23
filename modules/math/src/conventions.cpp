#include <elf3d/math/conventions.h>

#include <algorithm>
#include <cmath>

namespace elf3d::math {
namespace {

float clamp_channel(float value, float fallback) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

} // namespace

Vector2 to_vector(Float2 value) noexcept {
    return Vector2{value.x, value.y};
}

Float2 to_float2(const Vector2 &value) noexcept {
    return Float2{value.x, value.y};
}

Vector3 to_vector(Float3 value) noexcept {
    return Vector3{value.x, value.y, value.z};
}

Float3 to_float3(const Vector3 &value) noexcept {
    return Float3{value.x, value.y, value.z};
}

Rotation to_rotation(Quaternion value) noexcept {
    return Rotation{value.w, value.x, value.y, value.z};
}

Quaternion to_quaternion(const Rotation &value) noexcept {
    return Quaternion{value.x, value.y, value.z, value.w};
}

Matrix4 to_matrix(const Float4x4 &value) noexcept {
    return glm::make_mat4(value.elements.data());
}

Float4x4 to_float4x4(const Matrix4 &value) noexcept {
    Float4x4 result;
    std::copy_n(glm::value_ptr(value), result.elements.size(), result.elements.begin());
    return result;
}

Vector4 to_vector(Color4 value) noexcept {
    return Vector4{value.red, value.green, value.blue, value.alpha};
}

Color4 to_color4(const Vector4 &value) noexcept {
    return Color4{value.r, value.g, value.b, value.a};
}

Color4 clamp_color(Color4 value) noexcept {
    return Color4{clamp_channel(value.red, 0.0F), clamp_channel(value.green, 0.0F),
                  clamp_channel(value.blue, 0.0F), clamp_channel(value.alpha, 1.0F)};
}

Matrix4 compose_world(const Matrix4 &parent_world, const Matrix4 &local) noexcept {
    return parent_world * local;
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

bool is_valid_affine_matrix(const Matrix4 &matrix) noexcept {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }

    constexpr float tolerance = 0.00001F;
    if (std::abs(matrix[0][3]) > tolerance || std::abs(matrix[1][3]) > tolerance ||
        std::abs(matrix[2][3]) > tolerance || std::abs(matrix[3][3] - 1.0F) > tolerance) {
        return false;
    }

    const float determinant = glm::determinant(Matrix3{matrix});
    return std::isfinite(determinant) && std::abs(determinant) > 0.000001F;
}

Transform normalized_transform(const Transform &transform) noexcept {
    Transform result = transform;
    result.rotation = to_quaternion(glm::normalize(to_rotation(transform.rotation)));
    return result;
}

Matrix4 transform_matrix(const Transform &transform) noexcept {
    const Matrix4 translation = glm::translate(Matrix4{1.0F}, to_vector(transform.translation));
    const Matrix4 rotation = glm::mat4_cast(glm::normalize(to_rotation(transform.rotation)));
    const Matrix4 scale = glm::scale(Matrix4{1.0F}, to_vector(transform.scale));
    return translation * rotation * scale;
}

Result<Matrix4> camera_view_matrix(const Matrix4 &camera_world) noexcept {
    const Vector3 position = Vector3{camera_world[3]};
    Vector3 right = Vector3{camera_world[0]};
    Vector3 up = Vector3{camera_world[1]};

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
    return glm::inverse(rigid_world);
}

Result<Matrix4> perspective_matrix(float vertical_field_of_view_radians, float aspect_ratio,
                                   float near_plane, float far_plane) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    if (!std::isfinite(vertical_field_of_view_radians) || !std::isfinite(aspect_ratio) ||
        !std::isfinite(near_plane) || !std::isfinite(far_plane) ||
        vertical_field_of_view_radians <= 0.0F || vertical_field_of_view_radians >= pi ||
        aspect_ratio <= 0.0F || near_plane <= 0.0F || far_plane <= near_plane) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Perspective projection requires finite field of view, aspect, and planes"};
    }
    return glm::perspectiveRH_NO(vertical_field_of_view_radians, aspect_ratio, near_plane,
                                 far_plane);
}

Result<Matrix3> normal_matrix(const Matrix4 &model) noexcept {
    const Matrix3 linear{model};
    const float determinant = glm::determinant(linear);
    if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001F) {
        return Error{ErrorCode::invalid_argument,
                     "A model transform must be invertible to transform normals"};
    }
    return glm::transpose(glm::inverse(linear));
}

} // namespace elf3d::math
