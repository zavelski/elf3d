#include "viewer_viewport.hpp"

namespace elf3d::viewer {

RetainedViewportFrameKey viewport_frame_key(const ViewerFrameContext& state,
                                            const SceneSession& scene, const Viewport& viewport,
                                            const ToolCoordinator& tools) {
    const DistanceMeasurementSnapshot measurement = tools.measurement().snapshot(
        *scene.scene, viewport, tools.active_tool() == ViewerTool::distance_measurement);
    RetainedViewportFrameKey key;
    key.scene = scene.scene->id();
    key.camera = scene.camera;
    key.target_extent = state.rendering.render_target_dimensions;
    key.scene_revision = scene.scene->revision();
    key.viewport_revision = viewport.render_revision();
    key.diagnostic_render_scale_percent = state.rendering.diagnostic_render_scale_percent;
    key.shading_mode = state.rendering.shading_mode;
    key.selection = viewport.selection_snapshot();
    key.selection_settings = tools.selection().settings();
    key.isolated_entity = viewport.isolated_entity();
    key.clipping_revision = viewport.clipping_snapshot().revision;
    key.clipping_settings = tools.clipping().settings();
    key.measurement_state = measurement.state;
    key.first_measurement_point = measurement.first_point;
    key.second_measurement_point = measurement.second_point;
    key.preview_measurement_point = measurement.preview_point;
    key.measurement_settings = tools.measurement().settings();
    return key;
}

bool viewport_frame_render_required(const ViewerFrameContext& state,
                                    const RetainedViewportFrameKey& key,
                                    const Viewport& viewport) noexcept {
    const std::optional<NavigationSnapshot> navigation = viewport.navigation_snapshot();
    const bool navigating = navigation.has_value() && navigation->is_pointer_captured;
    return viewport_frame_render_required(state.rendering.retained_viewport_frame, key,
                                          state.rendering.framebuffer_valid, navigating);
}

bool viewport_frame_render_required(const std::optional<RetainedViewportFrameKey>& previous_frame,
                                    const RetainedViewportFrameKey& key, bool framebuffer_valid,
                                    bool pointer_navigation_captured) noexcept {
    return !framebuffer_valid || !previous_frame.has_value() || *previous_frame != key ||
           pointer_navigation_captured;
}

} // namespace elf3d::viewer
