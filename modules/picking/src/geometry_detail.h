#pragma once

#include <elf3d/math/value_types.h>

#include <algorithm>
#include <cmath>

namespace elf3d::picking::geometry_detail {

struct Double3 final {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    [[nodiscard]] double operator[](int axis) const noexcept {
        if (axis == 0) {
            return x;
        }
        return axis == 1 ? y : z;
    }
};

[[nodiscard]] inline Double3 to_double3(Float3 value) noexcept {
    return Double3{static_cast<double>(value.x), static_cast<double>(value.y),
                   static_cast<double>(value.z)};
}

[[nodiscard]] inline Float3 to_float3_checked(Double3 value) noexcept {
    return Float3{static_cast<float>(value.x), static_cast<float>(value.y),
                  static_cast<float>(value.z)};
}

[[nodiscard]] inline bool finite_double3(Double3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

[[nodiscard]] inline Double3 add(Double3 left, Double3 right) noexcept {
    return Double3{left.x + right.x, left.y + right.y, left.z + right.z};
}

[[nodiscard]] inline Double3 subtract(Double3 left, Double3 right) noexcept {
    return Double3{left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] inline Double3 scale(Double3 value, double factor) noexcept {
    return Double3{value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] inline Double3 component_min(Double3 left, Double3 right) noexcept {
    return Double3{std::min(left.x, right.x), std::min(left.y, right.y),
                   std::min(left.z, right.z)};
}

[[nodiscard]] inline Double3 component_max(Double3 left, Double3 right) noexcept {
    return Double3{std::max(left.x, right.x), std::max(left.y, right.y),
                   std::max(left.z, right.z)};
}

[[nodiscard]] inline double dot(Double3 left, Double3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] inline Double3 cross(Double3 left, Double3 right) noexcept {
    return Double3{left.y * right.z - left.z * right.y,
                   left.z * right.x - left.x * right.z,
                   left.x * right.y - left.y * right.x};
}

[[nodiscard]] inline double length(Double3 value) noexcept {
    return std::sqrt(dot(value, value));
}

} // namespace elf3d::picking::geometry_detail
