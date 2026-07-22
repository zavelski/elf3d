module;

#include <elf3d/viewport.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

module elf.viewport;

import elf.math;

namespace elf3d::viewport {

void OffscreenViewport::set_clear_color(Color4 color) noexcept {
    const Color4 clamped = math::clamp_color(color);
    if (clamped != clear_color_) {
        clear_color_ = clamped;
        ++render_revision_;
    }
}

Color4 OffscreenViewport::clear_color() const noexcept {
    return clear_color_;
}

void OffscreenViewport::set_basic_lighting(const BasicLighting& lighting) noexcept {
    const BasicLighting previous = lighting_;
    const float direction_length_squared = lighting.direction.x * lighting.direction.x +
                                           lighting.direction.y * lighting.direction.y +
                                           lighting.direction.z * lighting.direction.z;
    const Float3 direction = math::is_finite(lighting.direction) &&
                                     std::isfinite(direction_length_squared) &&
                                     direction_length_squared > 0.000001F
                                 ? lighting.direction
                                 : Float3{-0.5F, -1.0F, -0.3F};
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y +
                                   direction.z * direction.z);
    lighting_.direction = {direction.x / length, direction.y / length, direction.z / length};
    lighting_.color = math::clamp_color(lighting.color);
    lighting_.ambient_intensity = std::isfinite(lighting.ambient_intensity)
                                      ? std::clamp(lighting.ambient_intensity, 0.0F, 2.0F)
                                      : 0.08F;
    lighting_.diffuse_intensity = std::isfinite(lighting.diffuse_intensity)
                                      ? std::clamp(lighting.diffuse_intensity, 0.0F, 10.0F)
                                      : 3.0F;
    if (previous.direction != lighting_.direction || previous.color != lighting_.color ||
        previous.ambient_intensity != lighting_.ambient_intensity ||
        previous.diffuse_intensity != lighting_.diffuse_intensity) {
        ++render_revision_;
    }
}

BasicLighting OffscreenViewport::basic_lighting() const noexcept {
    return lighting_;
}

void OffscreenViewport::set_render_shading_mode(RenderShadingMode mode) noexcept {
    if (mode != RenderShadingMode::standard && mode != RenderShadingMode::unlit) {
        mode = RenderShadingMode::standard;
    }
    if (shading_mode_ != mode) {
        shading_mode_ = mode;
        ++render_revision_;
    }
}

RenderShadingMode OffscreenViewport::render_shading_mode() const noexcept {
    return shading_mode_;
}

std::uint64_t OffscreenViewport::render_revision() const noexcept {
    return render_revision_;
}

} // namespace elf3d::viewport
