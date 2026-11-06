#include "rotate_document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <optional>

namespace elf3d::cmd {
namespace {

using DoubleMatrix = std::array<double, 16>;
using DoublePoint = std::array<double, 3>;

struct WorldBounds {
    DoublePoint minimum;
    DoublePoint maximum;
};

[[nodiscard]] DoubleMatrix matrix_values(const Float4x4& matrix) noexcept
{
    DoubleMatrix result{};
    std::copy(matrix.elements.begin(), matrix.elements.end(), result.begin());
    return result;
}

[[nodiscard]] bool affine(const Float4x4& matrix) noexcept
{
    return matrix.elements[3] == 0.0F && matrix.elements[7] == 0.0F &&
           matrix.elements[11] == 0.0F && matrix.elements[15] == 1.0F;
}

[[nodiscard]] Result<DoubleMatrix> multiply(const DoubleMatrix& left,
                                            const DoubleMatrix& right) noexcept
{
    DoubleMatrix result{};
    for (std::size_t column = 0; column < 4U; ++column) {
        for (std::size_t row = 0; row < 4U; ++row) {
            double value = 0.0;
            for (std::size_t index = 0; index < 4U; ++index) {
                value += left[index * 4U + row] * right[column * 4U + index];
            }
            if (!std::isfinite(value)) {
                return Error{ErrorCode::invalid_transform_matrix,
                             "Rotation produced a non-finite matrix"};
            }
            result[column * 4U + row] = value;
        }
    }
    return result;
}

[[nodiscard]] Result<DoubleMatrix> world_matrix(const Document& document, NodeId id) noexcept
{
    auto result = matrix_values(Float4x4{});
    std::optional<NodeId> current{id};
    std::size_t depth = 0;
    while (current.has_value()) {
        if (++depth > document.node_count()) {
            return Error{ErrorCode::hierarchy_cycle, "Cannot rotate a cyclic node hierarchy"};
        }
        const auto node = document.node(*current);
        if (!node) {
            return node.error();
        }
        if (!affine(node.value().local_matrix)) {
            return Error{ErrorCode::invalid_transform_matrix,
                         "Model-center rotation requires affine node matrices"};
        }
        const auto combined = multiply(matrix_values(node.value().local_matrix), result);
        if (!combined) {
            return combined.error();
        }
        result = combined.value();
        current = node.value().parent;
    }
    return result;
}

[[nodiscard]] Result<void> include_position(std::optional<WorldBounds>& bounds,
                                            const DoubleMatrix& world, Float3 position) noexcept
{
    const std::array coordinates{double{position.x}, double{position.y}, double{position.z}, 1.0};
    DoublePoint point{};
    for (std::size_t row = 0; row < 3U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            point[row] += world[column * 4U + row] * coordinates[column];
        }
        if (!std::isfinite(point[row])) {
            return Error{ErrorCode::invalid_transform_matrix,
                         "Cannot compute finite world-space geometry bounds"};
        }
    }
    if (!bounds.has_value()) {
        bounds = WorldBounds{point, point};
    }
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        bounds->minimum[axis] = std::min(bounds->minimum[axis], point[axis]);
        bounds->maximum[axis] = std::max(bounds->maximum[axis], point[axis]);
    }
    return {};
}

[[nodiscard]] Result<void> include_mesh(const Document& document, MeshId id,
                                        const DoubleMatrix& world,
                                        std::optional<WorldBounds>& bounds) noexcept
{
    const auto mesh = document.mesh(id);
    if (!mesh) {
        return mesh.error();
    }
    for (const PrimitiveId primitive_id : mesh.value().primitives) {
        const auto primitive = document.primitive(primitive_id);
        if (!primitive) {
            return primitive.error();
        }
        for (const Float3 position : primitive.value().data.positions) {
            const auto included = include_position(bounds, world, position);
            if (!included) {
                return included.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<DoublePoint> model_center(const Document& document) noexcept
{
    std::optional<WorldBounds> bounds;
    for (std::size_t index = 0; index < document.node_count(); ++index) {
        const auto node = document.node_at(index);
        if (!node) {
            return node.error();
        }
        if (!node.value().mesh.has_value()) {
            continue;
        }
        const auto world = world_matrix(document, node.value().id);
        if (!world) {
            return world.error();
        }
        const auto included = include_mesh(document, *node.value().mesh, world.value(), bounds);
        if (!included) {
            return included.error();
        }
    }
    if (!bounds.has_value()) {
        return Error{ErrorCode::empty_scene_geometry,
                     "Model-center rotation requires geometry attached to nodes"};
    }
    DoublePoint center{};
    for (std::size_t axis = 0; axis < 3U; ++axis) {
        center[axis] = bounds->minimum[axis] * 0.5 + bounds->maximum[axis] * 0.5;
    }
    return center;
}

[[nodiscard]] Result<DoubleMatrix> rotation_matrix(Float3 degrees,
                                                   const DoublePoint& center) noexcept
{
    constexpr double radians_per_degree = std::numbers::pi_v<double> / 180.0;
    const double x = double{degrees.x} * radians_per_degree;
    const double y = double{degrees.y} * radians_per_degree;
    const double z = double{degrees.z} * radians_per_degree;
    const double cx = std::cos(x), sx = std::sin(x);
    const double cy = std::cos(y), sy = std::sin(y);
    const double cz = std::cos(z), sz = std::sin(z);
    const DoubleMatrix rx{1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
    const DoubleMatrix ry{cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
    const DoubleMatrix rz{cz, sz, 0, 0, -sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const auto yx = multiply(ry, rx);
    if (!yx) {
        return yx.error();
    }
    auto result = multiply(rz, yx.value());
    if (!result) {
        return result.error();
    }
    // T(center) * Rz * Ry * Rx * T(-center).
    for (std::size_t row = 0; row < 3U; ++row) {
        double rotated_center = 0.0;
        for (std::size_t column = 0; column < 3U; ++column) {
            rotated_center += result.value()[column * 4U + row] * center[column];
        }
        result.value()[12U + row] = center[row] - rotated_center;
    }
    return result;
}

[[nodiscard]] Result<Float4x4> rotated_matrix(const Float4x4& matrix,
                                              const DoubleMatrix& rotation) noexcept
{
    if (!affine(matrix)) {
        return Error{ErrorCode::invalid_transform_matrix,
                     "Model-center rotation requires affine node matrices"};
    }
    const auto combined = multiply(rotation, matrix_values(matrix));
    if (!combined) {
        return combined.error();
    }
    Float4x4 result;
    for (std::size_t index = 0; index < result.elements.size(); ++index) {
        const double value = combined.value()[index];
        if (std::abs(value) > double{std::numeric_limits<float>::max()}) {
            return Error{ErrorCode::invalid_transform_matrix,
                         "Rotation exceeds the finite matrix range"};
        }
        // Multiplication checked finiteness; the range check proves float representability.
        result.elements[index] = static_cast<float>(value);
    }
    return result;
}

[[nodiscard]] Result<void> apply_rotation(Document& document, const DoubleMatrix& rotation) noexcept
{
    for (std::size_t index = 0; index < document.node_count(); ++index) {
        NodeId id;
        Float4x4 matrix;
        {
            const auto node = document.node_at(index);
            if (!node) {
                return node.error();
            }
            if (node.value().parent.has_value()) {
                continue;
            }
            id = node.value().id;
            matrix = node.value().local_matrix;
        }
        const auto rotated = rotated_matrix(matrix, rotation);
        if (!rotated) {
            return rotated.error();
        }
        const auto changed = document.set_node_matrix(id, rotated.value());
        if (!changed) {
            return changed.error();
        }
    }
    return {};
}

} // namespace

Result<void> rotate_document(Document& document, Float3 degrees) noexcept
{
    if (!std::isfinite(degrees.x) || !std::isfinite(degrees.y) || !std::isfinite(degrees.z)) {
        return Error{ErrorCode::invalid_argument, "Rotation angles must be finite"};
    }
    // Reduce before trig evaluation; whole turns are exact no-ops, including metadata.
    degrees = {std::remainder(degrees.x, 360.0F), std::remainder(degrees.y, 360.0F),
               std::remainder(degrees.z, 360.0F)};
    if (degrees == Float3{}) {
        return {};
    }
    const auto center = model_center(document);
    if (!center) {
        return center.error();
    }
    const auto rotation = rotation_matrix(degrees, center.value());
    if (!rotation) {
        return rotation.error();
    }
    return apply_rotation(document, rotation.value());
}

} // namespace elf3d::cmd
