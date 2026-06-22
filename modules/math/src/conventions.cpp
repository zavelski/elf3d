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

} // namespace elf3d::math
