module;

#include <elf3d/math/detail/glm_helpers.h>

#include <cmath>
#include <optional>

module elf.math;

namespace elf3d::math {
namespace {

float clamp_channel(float value, float fallback) noexcept {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0F, 1.0F);
}

struct LinearInverse final {
    Matrix3 matrix;
    float determinant = 0.0F;
};

[[nodiscard]] std::optional<LinearInverse> invert_linear(const Matrix3& matrix) noexcept {
    constexpr std::size_t element_count = 9;
    float magnitude = 0.0F;
    const float* const elements = glm::value_ptr(matrix);
    for (std::size_t index = 0; index < element_count; ++index) {
        if (!std::isfinite(elements[index])) {
            return std::nullopt;
        }
        magnitude = std::max(magnitude, std::abs(elements[index]));
    }
    if (magnitude == 0.0F) {
        return std::nullopt;
    }

    // Keep determinant and inversion checks independent of uniform model scale.
    const Matrix3 normalized = matrix / magnitude;
    const float determinant = glm::determinant(normalized);
    if (!std::isfinite(determinant) || determinant == 0.0F) {
        return std::nullopt;
    }

    const Matrix3 inverse = glm::inverse(normalized) / magnitude;
    const float* const inverse_elements = glm::value_ptr(inverse);
    if (!std::all_of(inverse_elements, inverse_elements + element_count,
                     [](float value) noexcept { return std::isfinite(value); })) {
        return std::nullopt;
    }
    return LinearInverse{inverse, determinant};
}

constexpr float homogeneous_weight_epsilon = 0.000001F;

[[nodiscard]] bool valid_projection_input(Extent2D extent, Float3 world_position) noexcept {
    return extent.width != 0 && extent.height != 0 && is_finite(world_position);
}

[[nodiscard]] bool valid_unprojection_input(Extent2D extent, Float2 position_pixels,
                                            float depth) noexcept {
    return extent.width != 0 && extent.height != 0 && std::isfinite(position_pixels.x) &&
           std::isfinite(position_pixels.y) && std::isfinite(depth) && depth >= 0.0F &&
           depth <= 1.0F;
}

[[nodiscard]] bool valid_clip_point(const Vector4& point) noexcept {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
           std::isfinite(point.w) && std::abs(point.w) > homogeneous_weight_epsilon;
}

[[nodiscard]] bool valid_homogeneous_weight(const Vector4& point) noexcept {
    return std::isfinite(point.w) && std::abs(point.w) > homogeneous_weight_epsilon;
}

[[nodiscard]] bool inside_clip_volume(float ndc_x, float ndc_y, float ndc_z,
                                      bool in_front) noexcept {
    return in_front && ndc_x >= -1.0F && ndc_x <= 1.0F && ndc_y >= -1.0F && ndc_y <= 1.0F &&
           ndc_z >= -1.0F && ndc_z <= 1.0F;
}

[[nodiscard]] std::optional<Matrix4> inverse_view_projection(const Float4x4& view,
                                                             const Float4x4& projection) noexcept {
    const Matrix4 view_projection = to_matrix(projection) * to_matrix(view);
    const float determinant = glm::determinant(view_projection);
    if (!std::isfinite(determinant) || std::abs(determinant) <= homogeneous_weight_epsilon) {
        return std::nullopt;
    }
    return glm::inverse(view_projection);
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

bool is_valid_transform(const Transform& transform) noexcept {
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

bool is_valid_affine_matrix(const Float4x4& matrix) noexcept {
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

    return invert_linear(Matrix3{native}).has_value();
}

Transform normalized_transform(const Transform& transform) noexcept {
    Transform result = transform;
    result.rotation = to_quaternion(glm::normalize(to_rotation(transform.rotation)));
    return result;
}

Float4x4 compose_world(const Float4x4& parent_world, const Float4x4& local) noexcept {
    return to_float4x4(to_matrix(parent_world) * to_matrix(local));
}

Float4x4 transform_matrix(const Transform& transform) noexcept {
    const Matrix4 translation = glm::translate(Matrix4{1.0F}, to_vector(transform.translation));
    const Matrix4 rotation = glm::mat4_cast(glm::normalize(to_rotation(transform.rotation)));
    const Matrix4 scale = glm::scale(Matrix4{1.0F}, to_vector(transform.scale));
    return to_float4x4(translation * rotation * scale);
}

Result<Float4x4> camera_view_matrix(const Float4x4& camera_world) noexcept {
    const Matrix4 native_camera_world = to_matrix(camera_world);
    const Vector3 position = Vector3{native_camera_world[3]};
    Vector3 right = Vector3{native_camera_world[0]};
    Vector3 up = Vector3{native_camera_world[1]};

    constexpr float minimum_length = 0.000001F;
    const auto finite_vector = [](const Vector3& value) noexcept {
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
    return to_float4x4(
        glm::perspectiveRH_NO(vertical_field_of_view_radians, aspect_ratio, near_plane, far_plane));
}

Result<Matrix3x3> normal_matrix(const Float4x4& model) noexcept {
    const Matrix3 linear{to_matrix(model)};
    const std::optional<LinearInverse> inverse = invert_linear(linear);
    if (!inverse.has_value()) {
        return Error{ErrorCode::invalid_argument,
                     "A model transform must be invertible to transform normals"};
    }
    Matrix3x3 result{};
    const Matrix3 native = glm::transpose(inverse->matrix);
    std::copy_n(glm::value_ptr(native), result.size(), result.begin());
    return result;
}

Result<bool> orientation_reversed(const Float4x4& model) noexcept {
    const Matrix3 linear{to_matrix(model)};
    const std::optional<LinearInverse> inverse = invert_linear(linear);
    if (!inverse.has_value()) {
        return Error{ErrorCode::invalid_argument,
                     "A model transform must be invertible to determine orientation"};
    }
    return inverse->determinant < 0.0F;
}

Float3 transform_point(const Float4x4& matrix, Float3 point) noexcept {
    const Vector4 transformed = to_matrix(matrix) * Vector4{point.x, point.y, point.z, 1.0F};
    return Float3{transformed.x, transformed.y, transformed.z};
}

Result<ViewportProjection> project_world_to_viewport_point(const Float4x4& view,
                                                           const Float4x4& projection,
                                                           Extent2D extent,
                                                           Float3 world_position) noexcept {
    if (!valid_projection_input(extent, world_position)) {
        return Error{ErrorCode::projection_failed,
                     "Viewport projection requires finite coordinates and a nonzero extent"};
    }

    const Vector4 clip = to_matrix(projection) * to_matrix(view) *
                         Vector4{world_position.x, world_position.y, world_position.z, 1.0F};
    if (!valid_clip_point(clip)) {
        return Error{ErrorCode::projection_failed,
                     "The projected viewport point has invalid homogeneous weight"};
    }

    const float inverse_w = 1.0F / clip.w;
    const float ndc_x = clip.x * inverse_w;
    const float ndc_y = clip.y * inverse_w;
    const float ndc_z = clip.z * inverse_w;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) {
        return Error{ErrorCode::projection_failed,
                     "The projected viewport point has non-finite clip coordinates"};
    }

    ViewportProjection result;
    result.position_pixels = {(ndc_x * 0.5F + 0.5F) * static_cast<float>(extent.width) - 0.5F,
                              (1.0F - (ndc_y * 0.5F + 0.5F)) * static_cast<float>(extent.height) -
                                  0.5F};
    result.depth = ndc_z;
    result.is_in_front = clip.w > 0.0F;
    result.is_inside_viewport = inside_clip_volume(ndc_x, ndc_y, ndc_z, result.is_in_front);
    return result;
}

Result<Float3> unproject_viewport_point(const Float4x4& view, const Float4x4& projection,
                                        Extent2D extent, Float2 position_pixels,
                                        float depth) noexcept {
    if (!valid_unprojection_input(extent, position_pixels, depth)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Viewport unprojection requires finite coordinates inside a nonzero extent"};
    }

    const std::optional<Matrix4> inverse = inverse_view_projection(view, projection);
    if (!inverse.has_value()) {
        return Error{ErrorCode::projection_failed, "The view-projection matrix is not invertible"};
    }

    const float ndc_x = 2.0F * (position_pixels.x + 0.5F) / static_cast<float>(extent.width) - 1.0F;
    const float ndc_y =
        1.0F - 2.0F * (position_pixels.y + 0.5F) / static_cast<float>(extent.height);
    const float ndc_z = depth * 2.0F - 1.0F;
    const Vector4 world = *inverse * Vector4{ndc_x, ndc_y, ndc_z, 1.0F};
    if (!valid_homogeneous_weight(world)) {
        return Error{ErrorCode::projection_failed,
                     "The unprojected viewport point has invalid homogeneous weight"};
    }

    const Float3 result{world.x / world.w, world.y / world.w, world.z / world.w};
    if (!is_finite(result)) {
        return Error{ErrorCode::projection_failed, "The unprojected viewport point is not finite"};
    }
    return result;
}

float distance(Float3 left, Float3 right) noexcept {
    const Vector3 delta{left.x - right.x, left.y - right.y, left.z - right.z};
    return glm::length(delta);
}

} // namespace elf3d::math
