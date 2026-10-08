#pragma once

#include "viewer_tools.hpp"

#include <algorithm>
#include <cmath>

namespace elf3d::viewer::tool_detail {

[[nodiscard]] inline bool finite_color(Color4 color) noexcept
{
    return std::isfinite(color.red) && std::isfinite(color.green) && std::isfinite(color.blue) &&
           std::isfinite(color.alpha);
}

[[nodiscard]] inline bool valid_depth_mode(OverlayDepthMode mode) noexcept
{
    return mode == OverlayDepthMode::depth_tested || mode == OverlayDepthMode::always_visible;
}

[[nodiscard]] inline Color4 sanitized_color(Color4 color) noexcept
{
    const auto channel = [](float value, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 1.0F), channel(color.blue, 1.0F),
                  channel(color.alpha, 1.0F)};
}

} // namespace elf3d::viewer::tool_detail
