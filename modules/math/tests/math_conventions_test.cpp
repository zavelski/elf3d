#include <elf3d/math/conventions.h>

#include <cmath>

namespace {

bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 0.0001F;
}

} // namespace

int main() {
    const elf3d::Float2 public_value{2.0F, -3.5F};
    const elf3d::math::Vector2 vector = elf3d::math::to_vector(public_value);
    if (vector.x != 2.0F || vector.y != -3.5F || elf3d::math::to_float2(vector) != public_value) {
        return 1;
    }

    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 0.4F};
    if (elf3d::math::to_color4(elf3d::math::to_vector(color)) != color) {
        return 2;
    }

    const elf3d::math::Vector3 right =
        glm::cross(elf3d::math::Vector3{1.0F, 0.0F, 0.0F}, elf3d::math::Vector3{0.0F, 1.0F, 0.0F});
    if (right != elf3d::math::Vector3{0.0F, 0.0F, 1.0F}) {
        return 3;
    }

    elf3d::math::Matrix4 column_major{1.0F};
    column_major[0][1] = 2.0F;
    column_major[1][0] = 3.0F;
    const float *storage = glm::value_ptr(column_major);
    if (storage[1] != 2.0F || storage[4] != 3.0F) {
        return 4;
    }

    const elf3d::math::Matrix4 parent =
        glm::translate(elf3d::math::Matrix4{1.0F}, elf3d::math::Vector3{10.0F, 0.0F, 0.0F});
    const elf3d::math::Matrix4 local =
        glm::translate(elf3d::math::Matrix4{1.0F}, elf3d::math::Vector3{0.0F, 2.0F, 0.0F});
    const elf3d::math::Matrix4 world = elf3d::math::compose_world(parent, local);
    const elf3d::math::Vector4 transformed = world * elf3d::math::Vector4{0.0F, 0.0F, 0.0F, 1.0F};
    if (!nearly_equal(transformed.x, 10.0F) || !nearly_equal(transformed.y, 2.0F) ||
        !nearly_equal(transformed.z, 0.0F) || !nearly_equal(transformed.w, 1.0F)) {
        return 5;
    }

    return 0;
}
