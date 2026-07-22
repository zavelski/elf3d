#include "viewer_input_math.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace elf3d::viewer {

std::uint32_t to_pixel_dimension(float logical_size, float framebuffer_scale) noexcept {
    if (!std::isfinite(logical_size) || !std::isfinite(framebuffer_scale) || logical_size <= 0.0F ||
        framebuffer_scale <= 0.0F) {
        return 0;
    }
    const double pixel_size =
        static_cast<double>(logical_size) * static_cast<double>(framebuffer_scale);
    const double maximum = static_cast<double>(std::numeric_limits<std::uint32_t>::max());
    if (pixel_size >= maximum) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    return static_cast<std::uint32_t>(std::floor(pixel_size + 0.5));
}

Extent2D content_extent_in_pixels(Float2 logical_size, Float2 framebuffer_scale) noexcept {
    return Extent2D{to_pixel_dimension(logical_size.x, framebuffer_scale.x),
                    to_pixel_dimension(logical_size.y, framebuffer_scale.y)};
}

Float2 pointer_delta_in_target_pixels(Float2 logical_delta, Float2 logical_size,
                                      Extent2D target_extent) noexcept {
    const float x_scale =
        logical_size.x > 0.0F ? static_cast<float>(target_extent.width) / logical_size.x : 0.0F;
    const float y_scale =
        logical_size.y > 0.0F ? static_cast<float>(target_extent.height) / logical_size.y : 0.0F;
    return Float2{logical_delta.x * x_scale, logical_delta.y * y_scale};
}

std::optional<float> accumulated_wheel_delta(float current_delta,
                                             double additional_delta) noexcept {
    const float additional = static_cast<float>(additional_delta);
    const float accumulated = current_delta + additional;
    if (!std::isfinite(current_delta) || !std::isfinite(additional) || additional == 0.0F ||
        !std::isfinite(accumulated)) {
        return std::nullopt;
    }
    return accumulated;
}

} // namespace elf3d::viewer
