module;

#include <algorithm>
#include <cmath>

module elf.graphics;

namespace elf3d::graphics {
namespace {

[[nodiscard]] float sanitize(float value) noexcept {
    return std::isfinite(value) && value > 0.0F ? value : 0.0F;
}

[[nodiscard]] float linear_to_srgb(float value) noexcept {
    return value < 0.0031308F ? 12.92F * value : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] Float3 pbr_neutral(Float3 color) noexcept {
    constexpr float start_compression = 0.76F;
    constexpr float desaturation = 0.15F;
    const float darkest = std::min(color.x, std::min(color.y, color.z));
    const float offset = darkest < 0.08F ? darkest - 6.25F * darkest * darkest : 0.04F;
    color = {color.x - offset, color.y - offset, color.z - offset};
    const float peak = std::max(color.x, std::max(color.y, color.z));
    if (peak < start_compression) {
        return color;
    }
    constexpr float distance_to_white = 1.0F - start_compression;
    const float compressed_peak = 1.0F - distance_to_white * distance_to_white /
                                             (peak + distance_to_white - start_compression);
    const float scale = compressed_peak / peak;
    color = {color.x * scale, color.y * scale, color.z * scale};
    const float weight = 1.0F - 1.0F / (desaturation * (peak - compressed_peak) + 1.0F);
    return {color.x + (compressed_peak - color.x) * weight,
            color.y + (compressed_peak - color.y) * weight,
            color.z + (compressed_peak - color.z) * weight};
}

[[nodiscard]] Float3 standard_tone_mapping(Float3 color) noexcept {
    constexpr float calibration = 1.590579F;
    return {1.0F - std::exp2(-calibration * color.x), 1.0F - std::exp2(-calibration * color.y),
            1.0F - std::exp2(-calibration * color.z)};
}

} // namespace

Color4 resolve_display_color(Color4 linear_color, const DisplayTransform& transform) noexcept {
    const float exposure_ev = std::isfinite(transform.exposure_ev)
                                  ? std::clamp(transform.exposure_ev, -8.0F, 8.0F)
                                  : 0.0F;
    const float multiplier = std::exp2(exposure_ev);
    Float3 color{sanitize(linear_color.red) * multiplier, sanitize(linear_color.green) * multiplier,
                 sanitize(linear_color.blue) * multiplier};
    const ToneMappingMode mode = transform.tone_mapping == ToneMappingMode::none ||
                                         transform.tone_mapping == ToneMappingMode::pbr_neutral ||
                                         transform.tone_mapping == ToneMappingMode::standard
                                     ? transform.tone_mapping
                                     : ToneMappingMode::standard;
    if (mode == ToneMappingMode::pbr_neutral) {
        color = pbr_neutral(color);
    } else if (mode == ToneMappingMode::standard) {
        color = standard_tone_mapping(color);
    }
    return {linear_to_srgb(color.x), linear_to_srgb(color.y), linear_to_srgb(color.z),
            std::isfinite(linear_color.alpha) ? std::clamp(linear_color.alpha, 0.0F, 1.0F) : 0.0F};
}

StaticMesh::~StaticMesh() noexcept = default;

Texture2D::~Texture2D() noexcept = default;

TextureCube::~TextureCube() noexcept = default;

GraphicsPipeline::~GraphicsPipeline() noexcept = default;

RenderTarget::~RenderTarget() noexcept = default;

PickingTarget::~PickingTarget() noexcept = default;

Device::~Device() noexcept = default;

} // namespace elf3d::graphics
