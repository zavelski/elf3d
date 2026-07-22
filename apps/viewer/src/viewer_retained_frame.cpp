#include "viewer_internal.hpp"

namespace elf3d::viewer {

RetainedViewportFrameKey viewport_frame_key(const ViewerState& state, const ViewerScene& scene,
                                            const Viewport& viewport) {
    const DistanceMeasurementSnapshot measurement =
        viewport.distance_measurement_snapshot(*scene.scene);
    RetainedViewportFrameKey key;
    key.scene = scene.scene->id();
    key.camera = scene.camera;
    key.target_extent = state.render_target_dimensions;
    key.scene_revision = scene.scene->revision();
    key.viewport_revision = viewport.render_revision();
    key.diagnostic_render_scale_percent = state.diagnostic_render_scale_percent;
    key.shading_mode = state.shading_mode;
    key.selection = viewport.selection_snapshot();
    key.selection_settings = viewport.selection_settings();
    key.isolated_entity = viewport.isolated_entity();
    key.clipping_revision = viewport.clipping_snapshot().revision;
    key.measurement_state = measurement.state;
    key.first_measurement_point = measurement.first_point;
    key.second_measurement_point = measurement.second_point;
    key.preview_measurement_point = measurement.preview_point;
    key.measurement_settings = viewport.measurement_settings();
    return key;
}

bool viewport_frame_render_required(const ViewerState& state, const RetainedViewportFrameKey& key,
                                    const Viewport& viewport) noexcept {
    const std::optional<NavigationSnapshot> navigation = viewport.navigation_snapshot();
    const bool navigating = navigation.has_value() && navigation->is_pointer_captured;
    return viewport_frame_render_required(state.retained_viewport_frame, key,
                                          state.framebuffer_valid, navigating);
}

bool viewport_frame_render_required(const std::optional<RetainedViewportFrameKey>& previous_frame,
                                    const RetainedViewportFrameKey& key, bool framebuffer_valid,
                                    bool pointer_navigation_captured) noexcept {
    return !framebuffer_valid || !previous_frame.has_value() || *previous_frame != key ||
           pointer_navigation_captured;
}

} // namespace elf3d::viewer
