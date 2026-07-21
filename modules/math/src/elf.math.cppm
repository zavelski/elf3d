module;

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>

#include <array>
#include <cstddef>

export module elf.math;

import elf.core;

export namespace elf3d::math {

using Matrix3x3 = std::array<float, 9>;

struct ViewportProjection {
    Float2 position_pixels;
    float depth = 0.0F;
    bool is_in_front = false;
    bool is_inside_viewport = false;
};

[[nodiscard]] Color4 clamp_color(Color4 value) noexcept;
[[nodiscard]] bool is_finite(Float3 value) noexcept;
[[nodiscard]] bool is_finite(Quaternion value) noexcept;
[[nodiscard]] Float3 add(Float3 left, Float3 right) noexcept;
[[nodiscard]] Float3 subtract(Float3 left, Float3 right) noexcept;
[[nodiscard]] Float3 scale(Float3 value, float factor) noexcept;
[[nodiscard]] Float3 negate(Float3 value) noexcept;
[[nodiscard]] float dot(Float3 left, Float3 right) noexcept;
[[nodiscard]] Float3 cross(Float3 left, Float3 right) noexcept;
[[nodiscard]] float vector_length(Float3 value) noexcept;
[[nodiscard]] Float3 normalized(Float3 value) noexcept;
[[nodiscard]] Quaternion normalized(Quaternion value) noexcept;
[[nodiscard]] Quaternion rotation_from_axis_angle(float angle_radians, Float3 axis) noexcept;
[[nodiscard]] Quaternion compose_rotations(Quaternion left, Quaternion right) noexcept;
[[nodiscard]] Float3 rotate_vector(Quaternion rotation, Float3 value) noexcept;
[[nodiscard]] Quaternion rotation_from_basis(Float3 right, Float3 up, Float3 backward) noexcept;
[[nodiscard]] Float3 matrix_column(const Float4x4& matrix, std::size_t column) noexcept;
[[nodiscard]] bool is_valid_transform(const Transform& transform) noexcept;
[[nodiscard]] bool is_valid_affine_matrix(const Float4x4& matrix) noexcept;
[[nodiscard]] Transform normalized_transform(const Transform& transform) noexcept;
[[nodiscard]] Float4x4 compose_world(const Float4x4& parent_world, const Float4x4& local) noexcept;
[[nodiscard]] Result<Float4x4> inverse_affine_matrix(const Float4x4& matrix) noexcept;
[[nodiscard]] Float4x4 transform_matrix(const Transform& transform) noexcept;
[[nodiscard]] Result<Float4x4> camera_view_matrix(const Float4x4& camera_world) noexcept;
[[nodiscard]] Result<Float4x4> perspective_matrix(float vertical_field_of_view_radians,
                                                  float aspect_ratio, float near_plane,
                                                  float far_plane) noexcept;
[[nodiscard]] Result<Matrix3x3> normal_matrix(const Float4x4& model) noexcept;
[[nodiscard]] Result<bool> orientation_reversed(const Float4x4& model) noexcept;
[[nodiscard]] Float3 transform_point(const Float4x4& matrix, Float3 point) noexcept;
[[nodiscard]] Float3 transform_direction(const Float4x4& matrix, Float3 direction) noexcept;
[[nodiscard]] Result<ViewportProjection>
project_world_to_viewport_point(const Float4x4& view, const Float4x4& projection, Extent2D extent,
                                Float3 world_position) noexcept;
[[nodiscard]] Result<Float3> unproject_viewport_point(const Float4x4& view,
                                                      const Float4x4& projection, Extent2D extent,
                                                      Float2 position_pixels, float depth) noexcept;
[[nodiscard]] float distance(Float3 left, Float3 right) noexcept;

} // namespace elf3d::math
