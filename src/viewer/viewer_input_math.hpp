#pragma once

#include <elf3d/math/value_types.h>

#include <optional>

namespace elf3d::viewer {

[[nodiscard]] std::uint32_t to_pixel_dimension(float logical_size,
                                               float framebuffer_scale) noexcept;
[[nodiscard]] Extent2D content_extent_in_pixels(Float2 logical_size,
                                                Float2 framebuffer_scale) noexcept;
[[nodiscard]] Float2 pointer_delta_in_target_pixels(Float2 logical_delta, Float2 logical_size,
                                                    Extent2D target_extent) noexcept;
[[nodiscard]] std::optional<float> accumulated_wheel_delta(float current_delta,
                                                           double additional_delta) noexcept;

} // namespace elf3d::viewer
