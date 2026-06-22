#ifndef ELF3D_MATH_CONVENTIONS_H
#define ELF3D_MATH_CONVENTIONS_H

#include <elf3d/math/glm_config.h>
#include <elf3d/math/value_types.h>

namespace elf3d::math {

// Elf3D uses a right-handed world, column-major matrices, column vectors,
// matrix * vector transformation, and parent_world * local composition.
using Vector2 = glm::vec2;
using Vector3 = glm::vec3;
using Vector4 = glm::vec4;
using Matrix4 = glm::mat4;

[[nodiscard]] Vector2 to_vector(Float2 value) noexcept;
[[nodiscard]] Float2 to_float2(const Vector2 &value) noexcept;

[[nodiscard]] Vector4 to_vector(Color4 value) noexcept;
[[nodiscard]] Color4 to_color4(const Vector4 &value) noexcept;
[[nodiscard]] Color4 clamp_color(Color4 value) noexcept;

[[nodiscard]] Matrix4 compose_world(const Matrix4 &parent_world, const Matrix4 &local) noexcept;

} // namespace elf3d::math

#endif
