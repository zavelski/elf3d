#include "capture_internal.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

namespace elf3d::capture_tool {

[[nodiscard]] double luminance(const unsigned char* pixel) noexcept
{
    return (0.2126 * static_cast<double>(pixel[0]) + 0.7152 * static_cast<double>(pixel[1]) +
            0.0722 * static_cast<double>(pixel[2])) /
           255.0;
}

[[nodiscard]] std::vector<double> region_luminance(const std::vector<unsigned char>& pixels,
                                                   elf3d::Extent2D extent, int center_x,
                                                   int center_y, int radius)
{
    std::vector<double> values;
    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            const int dx = x - center_x;
            const int dy = y - center_y;
            if (x < 0 || y < 0 || x >= static_cast<int>(extent.width) ||
                y >= static_cast<int>(extent.height) || dx * dx + dy * dy > radius * radius) {
                continue;
            }
            const std::size_t offset =
                (static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x)) * 4U;
            values.push_back(luminance(pixels.data() + offset));
        }
    }

    std::sort(values.begin(), values.end());
    return values;
}

[[nodiscard]] double percentile(const std::vector<double>& sorted, double fraction) noexcept
{
    if (sorted.empty()) {
        return 0.0;
    }

    const std::size_t index =
        static_cast<std::size_t>(std::round(fraction * static_cast<double>(sorted.size() - 1U)));
    return sorted[std::min(index, sorted.size() - 1U)];
}

[[nodiscard]] double fraction_above(const std::vector<double>& values, double threshold) noexcept
{
    return values.empty() ? 0.0
                          : static_cast<double>(std::count_if(
                                values.begin(), values.end(),
                                [threshold](double value) { return value > threshold; })) /
                                static_cast<double>(values.size());
}

[[nodiscard]] double region_mean_absolute_rgb_difference(const std::vector<unsigned char>& first,
                                                         const std::vector<unsigned char>& second,
                                                         elf3d::Extent2D extent, int center_x,
                                                         int center_y, int radius) noexcept
{
    std::uint64_t accumulated = 0;
    std::uint64_t channel_count = 0;
    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            const int dx = x - center_x;
            const int dy = y - center_y;
            if (x < 0 || y < 0 || x >= static_cast<int>(extent.width) ||
                y >= static_cast<int>(extent.height) || dx * dx + dy * dy > radius * radius) {
                continue;
            }
            const std::size_t offset =
                (static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x)) * 4U;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                const int difference = static_cast<int>(first[offset + channel]) -
                                       static_cast<int>(second[offset + channel]);
                accumulated += static_cast<std::uint64_t>(std::abs(difference));
                ++channel_count;
            }
        }
    }
    return channel_count == 0U
               ? 0.0
               : static_cast<double>(accumulated) / (static_cast<double>(channel_count) * 255.0);
}

[[nodiscard]] elf3d::Result<void> validate_material_capture(const Options& options,
                                                            const MaterialCaptureImages& images)
{
    if (path_to_utf8(options.model) != procedural_model ||
        options.camera_mode != CameraMode::fit_front || options.extent.width != 1280U ||
        options.extent.height != 720U || options.shading != elf3d::RenderShadingMode::standard) {
        return elf3d::Error{
            elf3d::ErrorCode::invalid_argument,
            "Material validation requires procedural-material fit-front at 1280x720"};
    }
    const auto& front_pixels = images.frames[0];
    const double white_motion = region_mean_absolute_rgb_difference(
        images.frames[1], images.frames[2], options.extent, 333, 360, 92);
    const double polished_motion = region_mean_absolute_rgb_difference(
        images.frames[1], images.frames[2], options.extent, 741, 360, 92);
    const double rough_motion = region_mean_absolute_rgb_difference(
        images.frames[1], images.frames[2], options.extent, 946, 360, 92);

    const auto white_front = region_luminance(front_pixels, options.extent, 333, 360, 92);
    const auto white_back = region_luminance(images.frames[3], options.extent, 946, 360, 92);
    const auto polished = region_luminance(front_pixels, options.extent, 741, 360, 92);
    const auto rough = region_luminance(front_pixels, options.extent, 946, 360, 92);
    const double white_front_median = percentile(white_front, 0.5);
    const double white_back_median = percentile(white_back, 0.5);
    const double white_p99 = percentile(white_front, 0.99);
    const double polished_median = percentile(polished, 0.5);
    const double polished_p99 = percentile(polished, 0.99);
    const double rough_median = percentile(rough, 0.5);
    const double rough_p99 = percentile(rough, 0.99);
    const bool passes =
        white_front_median >= 0.72 && white_front_median <= 0.85 && white_back_median >= 0.77 &&
        white_back_median <= 0.90 && white_front_median / white_back_median >= 0.85 &&
        white_front_median / white_back_median <= 1.0 && white_p99 >= 0.85 && white_p99 < 0.95 &&
        polished_median >= 0.53 && polished_median <= 0.70 &&
        polished_p99 - polished_median >= 0.38 && polished_p99 - polished_median <= 0.50 &&
        fraction_above(polished, 0.75) >= 0.04 &&
        rough_p99 - rough_median < polished_p99 - polished_median &&
        rough_p99 - rough_median >= 0.10 && rough_p99 - rough_median <= 0.25 &&
        fraction_above(rough, 0.55) > fraction_above(polished, 0.55) && polished_motion >= 0.018 &&
        polished_motion >= 1.5 * white_motion && rough_motion >= 0.013 &&
        rough_motion < polished_motion;
    std::cout << std::fixed << std::setprecision(6)
              << "material_metrics white_front_median=" << white_front_median
              << " white_back_median=" << white_back_median
              << " front_back_ratio=" << white_front_median / white_back_median
              << " white_p99=" << white_p99 << " polished_median=" << polished_median
              << " polished_contrast=" << polished_p99 - polished_median
              << " polished_fraction_above_075=" << fraction_above(polished, 0.75)
              << " rough_contrast=" << rough_p99 - rough_median
              << " white_rotation_motion=" << white_motion
              << " polished_rotation_motion=" << polished_motion
              << " rough_rotation_motion=" << rough_motion << '\n';
    return passes ? elf3d::Result<void>{}
                  : elf3d::Result<void>{elf3d::Error{
                        elf3d::ErrorCode::draw_submission_failed,
                        "Procedural material capture did not satisfy the high-contrast studio "
                        "gates"}};
}

} // namespace elf3d::capture_tool
