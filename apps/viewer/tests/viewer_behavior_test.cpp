#include "viewer_input_math.hpp"
#include "viewer_internal.hpp"

#include <cmath>
#include <limits>
#include <optional>

namespace {

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 1.0e-6F;
}

[[nodiscard]] int verify_dpi_and_pointer_precision() {
    using elf3d::Extent2D;
    using elf3d::Float2;
    using elf3d::viewer::content_extent_in_pixels;
    using elf3d::viewer::pointer_delta_in_target_pixels;

    if (content_extent_in_pixels({800.0F, 600.0F}, {1.0F, 1.0F}) != Extent2D{800, 600} ||
        content_extent_in_pixels({800.0F, 600.0F}, {1.5F, 1.5F}) != Extent2D{1200, 900} ||
        content_extent_in_pixels({800.0F, 600.0F}, {2.0F, 2.0F}) != Extent2D{1600, 1200}) {
        return 1;
    }
    const Float2 scaled =
        pointer_delta_in_target_pixels({0.25F, -0.125F}, {800.0F, 600.0F}, {1600, 1200});
    if (!nearly_equal(scaled.x, 0.5F) || !nearly_equal(scaled.y, -0.25F)) {
        return 2;
    }
    const Float2 fractional =
        pointer_delta_in_target_pixels({0.1F, 0.2F}, {1000.0F, 500.0F}, {1500, 750});
    if (!nearly_equal(fractional.x, 0.15F) || !nearly_equal(fractional.y, 0.3F)) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int verify_wheel_accumulation() {
    float accumulated = 0.0F;
    for (double delta : {0.125, 0.25, -0.0625}) {
        const std::optional<float> next =
            elf3d::viewer::accumulated_wheel_delta(accumulated, delta);
        if (!next.has_value()) {
            return 4;
        }
        accumulated = *next;
    }
    if (!nearly_equal(accumulated, 0.3125F) ||
        elf3d::viewer::accumulated_wheel_delta(accumulated, 0.0).has_value() ||
        elf3d::viewer::accumulated_wheel_delta(accumulated, std::numeric_limits<double>::infinity())
            .has_value()) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int verify_retained_frame_invalidation() {
    using elf3d::viewer::RetainedViewportFrameKey;
    using elf3d::viewer::viewport_frame_render_required;
    RetainedViewportFrameKey key;
    key.scene_revision = 7;
    const std::optional<RetainedViewportFrameKey> missing;
    if (!viewport_frame_render_required(missing, key, true, false) ||
        !viewport_frame_render_required(key, key, false, false) ||
        !viewport_frame_render_required(key, key, true, true) ||
        viewport_frame_render_required(key, key, true, false)) {
        return 6;
    }
    RetainedViewportFrameKey changed = key;
    ++changed.scene_revision;
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 7;
    }
    changed = key;
    changed.target_extent = {1920, 1080};
    if (!viewport_frame_render_required(key, changed, true, false)) {
        return 8;
    }
    changed = key;
    changed.diagnostic_render_scale_percent = 50;
    return viewport_frame_render_required(key, changed, true, false) ? 0 : 9;
}

} // namespace

int main() {
    const int dpi = verify_dpi_and_pointer_precision();
    if (dpi != 0) {
        return dpi;
    }
    const int wheel = verify_wheel_accumulation();
    if (wheel != 0) {
        return wheel;
    }
    return verify_retained_frame_invalidation();
}
