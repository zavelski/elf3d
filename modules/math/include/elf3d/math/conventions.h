#ifndef ELF3D_MATH_CONVENTIONS_H
#define ELF3D_MATH_CONVENTIONS_H

#include <elf3d/core/result.h>
#include <elf3d/math/glm_config.h>
#include <elf3d/math/value_types.h>

namespace elf3d::math {

// Elf3D uses a right-handed world, column-major matrices, column vectors,
// matrix * vector transformation, and parent_world * local composition.
using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Vector4 = glm::vec4;
using Matrix4 = glm::mat4;
using Matrix3 = glm::mat3;
using Rotation = glm::quat;

[[nodiscard]] Vector2 to_vector(Float2 value) noexcept;
[[nodiscard]] Float2 to_float2(const Vector2 &value) noexcept;
[[nodiscard]] Vector3 to_vector(Float3 value) noexcept;
[[nodiscard]] Float3 to_float3(const Vector3 &value) noexcept;
[[nodiscard]] Rotation to_rotation(Quaternion value) noexcept;
[[nodiscard]] Quaternion to_quaternion(const Rotation &value) noexcept;
[[nodiscard]] Matrix4 to_matrix(const Float4x4 &value) noexcept;
[[nodiscard]] Float4x4 to_float4x4(const Matrix4 &value) noexcept;

[[nodiscard]] Vector4 to_vector(Color4 value) noexcept;
[[nodiscard]] Color4 to_color4(const Vector4 &value) noexcept;
[[nodiscard]] Color4 clamp_color(Color4 value) noexcept;

[[nodiscard]] Matrix4 compose_world(const Matrix4 &parent_world, const Matrix4 &local) noexcept;
[[nodiscard]] bool is_finite(Float3 value) noexcept;
[[nodiscard]] bool is_finite(Quaternion value) noexcept;
[[nodiscard]] bool is_valid_transform(const Transform &transform) noexcept;
[[nodiscard]] bool is_valid_affine_matrix(const Matrix4 &matrix) noexcept;
[[nodiscard]] Transform normalized_transform(const Transform &transform) noexcept;
[[nodiscard]] Matrix4 transform_matrix(const Transform &transform) noexcept;
[[nodiscard]] Result<Matrix4> camera_view_matrix(const Matrix4 &camera_world) noexcept;
[[nodiscard]] Result<Matrix4> perspective_matrix(float vertical_field_of_view_radians,
                                                 float aspect_ratio, float near_plane,
                                                 float far_plane) noexcept;
[[nodiscard]] Result<Matrix3> normal_matrix(const Matrix4 &model) noexcept;

} // namespace elf3d::math

#endif
