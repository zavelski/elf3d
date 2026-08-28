#include <elf3d/rendering.h>

#include <cmath>
#include <limits>

import elf.graphics;

namespace {

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool neutral_invariants() noexcept {
    const elf3d::DisplayTransform neutral;
    const elf3d::Color4 black =
        elf3d::graphics::resolve_display_color({0.0F, 0.0F, 0.0F, 1.0F}, neutral);
    const elf3d::Color4 gray =
        elf3d::graphics::resolve_display_color({0.5F, 0.5F, 0.5F, 1.0F}, neutral);
    const elf3d::Color4 white =
        elf3d::graphics::resolve_display_color({1.0F, 1.0F, 1.0F, 1.0F}, neutral);
    const elf3d::Color4 hdr =
        elf3d::graphics::resolve_display_color({100.0F, 100.0F, 100.0F, 1.0F}, neutral);
    return black == elf3d::Color4{0.0F, 0.0F, 0.0F, 1.0F} && nearly_equal(gray.red, gray.green) &&
           nearly_equal(gray.green, gray.blue) && gray.red < white.red && white.red < hdr.red &&
           hdr.red < 1.0F && std::isfinite(hdr.red);
}

[[nodiscard]] bool exposure_and_transfer_invariants() noexcept {
    const elf3d::DisplayTransform neutral;
    const elf3d::Color4 gray =
        elf3d::graphics::resolve_display_color({0.5F, 0.5F, 0.5F, 1.0F}, neutral);
    elf3d::DisplayTransform plus_one = neutral;
    plus_one.exposure_ev = 1.0F;
    const elf3d::Color4 exposed =
        elf3d::graphics::resolve_display_color({0.25F, 0.25F, 0.25F, 1.0F}, plus_one);
    elf3d::DisplayTransform none;
    none.tone_mapping = elf3d::ToneMappingMode::none;
    const elf3d::Color4 encoded_once =
        elf3d::graphics::resolve_display_color({0.5F, 0.5F, 0.5F, 1.0F}, none);
    return nearly_equal(exposed.red, gray.red) && nearly_equal(exposed.green, gray.green) &&
           nearly_equal(encoded_once.red, 0.735357F, 0.0001F);
}

[[nodiscard]] bool standard_preserves_dark_material_detail() noexcept {
    const elf3d::Color4 dark_linear{0.02F, 0.02F, 0.02F, 1.0F};
    const elf3d::Color4 standard =
        elf3d::graphics::resolve_display_color(dark_linear, elf3d::DisplayTransform{});
    elf3d::DisplayTransform neutral;
    neutral.tone_mapping = elf3d::ToneMappingMode::pbr_neutral;
    const elf3d::Color4 pbr_neutral = elf3d::graphics::resolve_display_color(dark_linear, neutral);
    return standard.red > pbr_neutral.red * 3.0F && standard.red < 0.2F;
}

[[nodiscard]] bool sanitization_invariants() noexcept {
    const elf3d::Color4 invalid = elf3d::graphics::resolve_display_color(
        {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0F,
         std::numeric_limits<float>::quiet_NaN()},
        elf3d::DisplayTransform{});
    return invalid == elf3d::Color4{0.0F, 0.0F, 0.0F, 0.0F};
}

} // namespace

int elf3d_display_transform_test() {
    return neutral_invariants() && exposure_and_transfer_invariants() &&
                   standard_preserves_dark_material_detail() && sanitization_invariants()
               ? 0
               : 1;
}
