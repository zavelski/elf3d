module;

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>

#include <array>

export module elf.math;

export namespace elf3d::math {

using Matrix3x3 = std::array<float, 9>;

[[nodiscard]] Color4 clamp_color(Color4 value) noexcept;
[[nodiscard]] bool is_finite(Float3 value) noexcept;
[[nodiscard]] bool is_finite(Quaternion value) noexcept;
[[nodiscard]] bool is_valid_transform(const Transform &transform) noexcept;
[[nodiscard]] bool is_valid_affine_matrix(const Float4x4 &matrix) noexcept;
[[nodiscard]] Transform normalized_transform(const Transform &transform) noexcept;
[[nodiscard]] Float4x4 compose_world(const Float4x4 &parent_world,
                                     const Float4x4 &local) noexcept;
[[nodiscard]] Float4x4 transform_matrix(const Transform &transform) noexcept;
[[nodiscard]] Result<Float4x4> camera_view_matrix(const Float4x4 &camera_world) noexcept;
[[nodiscard]] Result<Float4x4> perspective_matrix(float vertical_field_of_view_radians,
                                                  float aspect_ratio, float near_plane,
                                                  float far_plane) noexcept;
[[nodiscard]] Result<Matrix3x3> normal_matrix(const Float4x4 &model) noexcept;
[[nodiscard]] Float3 transform_point(const Float4x4 &matrix, Float3 point) noexcept;

} // namespace elf3d::math
