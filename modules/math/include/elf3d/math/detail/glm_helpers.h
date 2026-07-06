#ifndef ELF3D_MATH_DETAIL_GLM_HELPERS_H
#define ELF3D_MATH_DETAIL_GLM_HELPERS_H

#include <elf3d/math/glm_config.h>
#include <elf3d/math/value_types.h>

#include <algorithm>

namespace elf3d::math {

// GLM is an implementation detail. Do not expose these aliases from a module
// interface or public Elf3D header.
using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Vector4 = glm::vec4;
using Matrix4 = glm::mat4;
using Matrix3 = glm::mat3;
using Rotation = glm::quat;

[[nodiscard]] inline Vector2 to_vector(Float2 value) noexcept {
    return Vector2{value.x, value.y};
}

[[nodiscard]] inline Float2 to_float2(const Vector2 &value) noexcept {
    return Float2{value.x, value.y};
}

[[nodiscard]] inline Vector3 to_vector(Float3 value) noexcept {
    return Vector3{value.x, value.y, value.z};
}

[[nodiscard]] inline Float3 to_float3(const Vector3 &value) noexcept {
    return Float3{value.x, value.y, value.z};
}

[[nodiscard]] inline Rotation to_rotation(Quaternion value) noexcept {
    return Rotation{value.w, value.x, value.y, value.z};
}

[[nodiscard]] inline Quaternion to_quaternion(const Rotation &value) noexcept {
    return Quaternion{value.x, value.y, value.z, value.w};
}

[[nodiscard]] inline Matrix4 to_matrix(const Float4x4 &value) noexcept {
    return glm::make_mat4(value.elements.data());
}

[[nodiscard]] inline Float4x4 to_float4x4(const Matrix4 &value) noexcept {
    Float4x4 result;
    std::copy_n(glm::value_ptr(value), result.elements.size(), result.elements.begin());
    return result;
}

[[nodiscard]] inline Vector4 to_vector(Color4 value) noexcept {
    return Vector4{value.red, value.green, value.blue, value.alpha};
}

[[nodiscard]] inline Color4 to_color4(const Vector4 &value) noexcept {
    return Color4{value.r, value.g, value.b, value.a};
}

} // namespace elf3d::math

#endif
