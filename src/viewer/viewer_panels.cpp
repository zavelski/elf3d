#include "viewer_components.hpp"

#include "viewer_browser.hpp"
#include "viewer_rendering_controls.hpp"
#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace elf3d::viewer {
namespace {

void draw_shading_controls(ViewerFrameContext& state, ViewerCommandDispatcher& commands)
{
    const auto& capabilities = commands.capabilities();
    constexpr std::array<const char*, 2> shading_modes{{"Standard PBR", "Unlit"}};
    int shading = state.rendering.shading_mode == RenderShadingMode::unlit ? 1 : 0;
    ImGui::BeginDisabled(!viewer_command_enabled(SetShadingModeCommand{}, capabilities));
    if (ImGui::Combo("Shading", &shading, shading_modes.data(),
                     static_cast<int>(shading_modes.size()))) {
        commands.emit(SetShadingModeCommand{shading == 1 ? RenderShadingMode::unlit
                                                         : RenderShadingMode::standard});
    }
    ImGui::EndDisabled();
    if (!viewer_command_enabled(SetShadingModeCommand{}, capabilities))
        tooltip(viewer_command_disabled_reason(SetShadingModeCommand{}, capabilities));
}
void draw_view_modes(ViewerFrameContext& state, ViewerCommandDispatcher& commands)
{
    ImGui::Checkbox("VSync", &state.rendering.vsync_enabled);
    draw_shading_controls(state, commands);
    bool culling = state.rendering.backface_culling_enabled;
    const auto culling_command = SetBackfaceCullingCommand{!culling};
    const bool culling_available = viewer_command_enabled(culling_command, commands.capabilities());
    ImGui::BeginDisabled(!culling_available);
    if (ImGui::Checkbox("Backface Culling", &culling))
        commands.emit(SetBackfaceCullingCommand{culling});
    ImGui::EndDisabled();
    tooltip(culling_available
                ? "Respect material sidedness. Disable to display and pick both faces in this view."
                : viewer_command_disabled_reason(culling_command, commands.capabilities()));
    constexpr std::array<const char*, 3> scales{{"100%", "50%", "25%"}};
    int scale = state.rendering.diagnostic_render_scale_percent == 50
                    ? 1
                    : (state.rendering.diagnostic_render_scale_percent == 25 ? 2 : 0);
    if (ImGui::Combo("Render scale", &scale, scales.data(), static_cast<int>(scales.size()))) {
        state.rendering.diagnostic_render_scale_percent = scale == 1 ? 50 : (scale == 2 ? 25 : 100);
    }
}

} // namespace

void build_rendering_panel(ImGuiID dockspace_id, ViewerFrameContext& state, SceneSession& scene,
                           const Viewport& viewport, ViewerCommandDispatcher& commands)
{
    if (!state.shell.show_rendering_panel) {
        return;
    }
    set_default_dock(state.shell.dock_right_bottom_id != 0 ? state.shell.dock_right_bottom_id
                                                           : dockspace_id,
                     state.shell.apply_dock_layout);
    if (begin_panel_window("Rendering", &state.shell.show_rendering_panel,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        ImGui::TextUnformatted("Viewport");
        color_control("Clear color", state.rendering.clear_color);
        build_lighting_controls(state);
        build_camera_evidence(state, scene, viewport);
        draw_view_modes(state, commands);
    }
    ImGui::End();
}

[[nodiscard]] float radians_to_degrees(float radians) noexcept
{
    return radians * 57.2957795131F;
}

[[nodiscard]] const char* interaction_mode_name(elf3d::NavigationInteractionMode mode) noexcept
{
    switch (mode) {
    case elf3d::NavigationInteractionMode::none:
        return "None";
    case elf3d::NavigationInteractionMode::orbit:
        return "Orbit";
    case elf3d::NavigationInteractionMode::pan:
        return "Pan";
    case elf3d::NavigationInteractionMode::zoom:
        return "Zoom";
    }
    return "None";
}

void build_navigation_settings_window(ImGuiID dockspace_id, ViewerFrameContext& state,
                                      elf3d::Viewport& engine_viewport)
{
    if (!state.shell.show_navigation_settings) {
        return;
    }
    set_default_dock(state.shell.dock_right_bottom_id != 0 ? state.shell.dock_right_bottom_id
                                                           : dockspace_id,
                     state.shell.apply_dock_layout);
    if (begin_panel_window("Navigation Settings", &state.shell.show_navigation_settings,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        bool enabled = engine_viewport.navigation_enabled();
        if (ImGui::Checkbox("Enable Navigation", &enabled)) {
            engine_viewport.set_navigation_enabled(enabled);
        }

        elf3d::OrbitNavigationSettings settings = engine_viewport.navigation_settings();
        bool settings_changed = false;
        settings_changed |= ImGui::DragFloat("Orbit sensitivity", &settings.orbit_sensitivity,
                                             0.0001F, 0.0F, 0.05F, "%.4f");
        settings_changed |= ImGui::DragFloat("Pan sensitivity", &settings.pan_sensitivity, 0.01F,
                                             0.0F, 10.0F, "%.2f");
        settings_changed |= ImGui::DragFloat("Zoom sensitivity", &settings.zoom_sensitivity, 0.005F,
                                             0.0F, 1.0F, "%.3f");
        settings_changed |=
            ImGui::Checkbox("Invert vertical orbit", &settings.invert_vertical_orbit);
        settings_changed |=
            ImGui::Checkbox("Focus-depth orbit anchor", &settings.focus_depth_anchor_enabled);
        if (settings_changed) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_navigation_settings(settings);
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }
        if (ImGui::Button("Reset Navigation Settings")) {
            const elf3d::Result<void> settings_result =
                engine_viewport.set_navigation_settings(elf3d::OrbitNavigationSettings{});
            if (!settings_result) {
                set_viewport_error(state, settings_result.error());
            }
        }

        ImGui::Separator();
        const std::optional<elf3d::NavigationSnapshot> snapshot =
            engine_viewport.navigation_snapshot();
        if (!snapshot.has_value()) {
            ImGui::TextUnformatted("Navigation state unavailable");
        } else {
            ImGui::Text("Pivot: %.4g, %.4g, %.4g", snapshot->pivot.x, snapshot->pivot.y,
                        snapshot->pivot.z);
            ImGui::Text("Distance: %.4g", snapshot->distance);
            ImGui::Text("Yaw: %.2f deg", radians_to_degrees(snapshot->yaw_radians));
            ImGui::Text("Pitch: %.2f deg", radians_to_degrees(snapshot->pitch_radians));
            ImGui::Text("Interaction: %s", interaction_mode_name(snapshot->interaction_mode));
        }
    }
    ImGui::End();
}

[[nodiscard]] std::string entity_label(const SceneSession& scene, elf3d::EntityId entity)
{
    const elf3d::Result<std::string_view> name = scene.scene->entity_name(entity);
    if (name && !name.value().empty()) {
        const std::string_view text = name.value();
        return std::string{text};
    }
    return std::string{"Entity "} + std::to_string(entity.debug_value());
}

[[nodiscard]] std::string selected_entity_label(const SceneSession& scene,
                                                const elf3d::SelectionSnapshot& selection)
{
    return selection.entity.has_value() ? entity_label(scene, *selection.entity) : "none";
}

void build_selection_settings(ViewerFrameContext& state, elf3d::Viewport& viewport,
                              SelectionTool& selection)
{
    bool enabled = selection.enabled();
    if (ImGui::Checkbox("Enable Viewport Selection", &enabled)) {
        selection.set_enabled(enabled);
        if (!enabled) {
            selection.clear(viewport);
        }
    }
    SelectionToolSettings settings = selection.settings();
    bool changed = ImGui::DragFloat("Click threshold", &settings.click_drag_threshold_pixels, 0.1F,
                                    0.0F, 32.0F, "%.1f px");
    std::array<float, 4> color{settings.highlight_color.red, settings.highlight_color.green,
                               settings.highlight_color.blue, settings.highlight_color.alpha};
    if (ImGui::ColorEdit4("Highlight color", color.data(), ImGuiColorEditFlags_NoInputs)) {
        settings.highlight_color = {color[0], color[1], color[2], color[3]};
        changed = true;
    }
    changed |=
        ImGui::SliderFloat("Highlight strength", &settings.highlight_strength, 0.0F, 1.0F, "%.2f");
    if (!changed) {
        return;
    }

    const elf3d::Result<void> result = selection.set_settings(settings);
    if (!result) {
        set_viewport_error(state, result.error());
    }
}

void draw_selected_entity(const SceneSession& scene, const elf3d::SelectionSnapshot& selection)
{
    if (!selection.entity.has_value()) {
        ImGui::TextUnformatted("Selected: none");
        return;
    }

    const std::string label = selected_entity_label(scene, selection);
    ImGui::Text("Selected: %s", label.c_str());
    ImGui::Text("Entity ID: %llu",
                static_cast<unsigned long long>(selection.entity->debug_value()));
    if (!selection.pick_hit.has_value()) {
        ImGui::TextUnformatted("Pick hit: none");
        return;
    }

    const elf3d::PickHit& hit = *selection.pick_hit;
    ImGui::Text("Mesh ID: %llu", static_cast<unsigned long long>(hit.mesh.debug_value()));
    ImGui::Text("Primitive: %u", hit.primitive_index);
    ImGui::Text("Triangle: %u", hit.triangle_index);
    ImGui::Text("Hit position: %.4g, %.4g, %.4g", hit.world_position.x, hit.world_position.y,
                hit.world_position.z);
    ImGui::Text("Hit normal: %.4g, %.4g, %.4g", hit.world_normal.x, hit.world_normal.y,
                hit.world_normal.z);
    ImGui::Text("Barycentric: %.4g, %.4g, %.4g", hit.barycentric_coordinates.x,
                hit.barycentric_coordinates.y, hit.barycentric_coordinates.z);
    ImGui::Text("Distance: %.4g", hit.world_distance);
}

void draw_picking_statistics(ViewerFrameContext& state, elf3d::Viewport& viewport)
{
    const elf3d::Result<elf3d::PickingStatistics> result = viewport.picking_statistics();
    if (!result) {
        set_viewport_error(state, result.error());
        ImGui::TextUnformatted("Picking statistics unavailable");
        return;
    }

    const elf3d::PickingStatistics& picking = result.value();
    ImGui::Text("GPU pick requests: %llu",
                static_cast<unsigned long long>(picking.latest_gpu_requests));
    ImGui::Text("GPU pick hits / misses: %llu / %llu",
                static_cast<unsigned long long>(picking.latest_gpu_hits),
                static_cast<unsigned long long>(picking.latest_gpu_misses));
    ImGui::Text("GPU pick draw calls: %llu",
                static_cast<unsigned long long>(picking.latest_gpu_draw_calls));
    ImGui::Text("GPU pixels read: %llu",
                static_cast<unsigned long long>(picking.latest_gpu_pixels_read));
    ImGui::Text("CPU refinements / fallbacks: %llu / %llu",
                static_cast<unsigned long long>(picking.latest_cpu_refinements),
                static_cast<unsigned long long>(picking.latest_cpu_fallbacks));
    ImGui::Text("Pick pass / readback: %.3f / %.3f ms", picking.latest_pass_milliseconds,
                picking.latest_readback_milliseconds);
    ImGui::Text("Pick allocation / total CPU: %.3f / %.3f ms",
                picking.latest_allocation_milliseconds, picking.latest_cpu_milliseconds);
    ImGui::Text("Pick target allocations latest / lifetime: %llu / %llu",
                static_cast<unsigned long long>(picking.latest_target_allocations),
                static_cast<unsigned long long>(picking.lifetime_target_allocations));
    if (picking.latest_gpu_timing_available) {
        ImGui::Text("Delayed GPU pick: %.3f ms", picking.latest_gpu_milliseconds);
    } else {
        ImGui::TextUnformatted("Delayed GPU pick: unavailable");
    }
    ImGui::Separator();
    ImGui::Text("Instance bounds tests: %llu",
                static_cast<unsigned long long>(picking.latest_instance_bounds_tests));
    ImGui::Text("Mesh bounds tests: %llu",
                static_cast<unsigned long long>(picking.latest_mesh_bounds_tests));
    ImGui::Text("BVH node tests: %llu",
                static_cast<unsigned long long>(picking.latest_bvh_node_tests));
    ImGui::Text("Triangle tests: %llu",
                static_cast<unsigned long long>(picking.latest_triangle_tests));
    ImGui::Text("BVH builds this pick: %llu",
                static_cast<unsigned long long>(picking.latest_bvh_builds));
    ImGui::Text("Lifetime BVH builds: %llu",
                static_cast<unsigned long long>(picking.lifetime_bvh_builds));
    ImGui::Text("Cached mesh BVHs: %llu",
                static_cast<unsigned long long>(picking.cached_mesh_bvhs));
}

void draw_properties_selection(ViewerFrameContext& state, const SceneSession& scene,
                               Viewport& viewport, SelectionTool& selection)
{
    const bool document_properties = draw_browser_properties(state, scene, selection);
    if (!document_properties) {
        ImGui::Text("Selected objects: %zu", selection.selected_count());
        draw_selected_entity(scene, viewport.selection_snapshot());
    }
    if (document_properties && viewport.selected_entity() &&
        ImGui::CollapsingHeader("Viewport Highlight")) {
        draw_selected_entity(scene, viewport.selection_snapshot());
    }
}

void draw_properties_settings(ViewerFrameContext& state, Viewport& viewport,
                              SelectionTool& selection)
{
    if (state.shell.expand_selection_settings) {
        ImGui::SetNextItemOpen(true);
        state.shell.expand_selection_settings = false;
    }
    if (ImGui::CollapsingHeader("Selection Settings")) {
        build_selection_settings(state, viewport, selection);
    }
    if (ImGui::CollapsingHeader("Picking Diagnostics")) {
        draw_picking_statistics(state, viewport);
    }
}

void build_selection_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                           const SceneSession& scene, elf3d::Viewport& engine_viewport,
                           ToolCoordinator& tools)
{
    if (!state.shell.show_selection_panel) {
        return;
    }
    set_default_dock(state.shell.dock_right_bottom_id != 0 ? state.shell.dock_right_bottom_id
                                                           : dockspace_id,
                     state.shell.apply_dock_layout);
    if (begin_panel_window("Properties", &state.shell.show_selection_panel,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        draw_properties_selection(state, scene, engine_viewport, tools.selection());
        draw_properties_settings(state, engine_viewport, tools.selection());
    }
    ImGui::End();
}

void draw_measurement_point(const char* label, const SceneSession& scene,
                            const std::optional<MeasurementPoint>& point)
{
    if (!point.has_value()) {
        ImGui::Text("%s: none", label);
        return;
    }

    const std::string entity = entity_label(scene, point->entity);
    ImGui::Text("%s: %s", label, entity.c_str());
    ImGui::Text("  Entity ID: %llu", static_cast<unsigned long long>(point->entity.debug_value()));
    ImGui::Text("  Mesh ID: %llu", static_cast<unsigned long long>(point->mesh.debug_value()));
    ImGui::Text("  Position: %.4g, %.4g, %.4g", point->world_position.x, point->world_position.y,
                point->world_position.z);
    ImGui::Text("  Normal: %.4g, %.4g, %.4g", point->world_normal.x, point->world_normal.y,
                point->world_normal.z);
}

void build_measurement_tool_selector(ToolCoordinator& tools)
{
    const ViewerTool active_tool = tools.active_tool();
    ImGui::Text("Active tool: %s", tool_name(active_tool));
    if (ImGui::RadioButton("Select", active_tool == ViewerTool::selection)) {
        tools.activate(ViewerTool::selection);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Measure Distance", active_tool == ViewerTool::distance_measurement)) {
        tools.activate(ViewerTool::distance_measurement);
    }
}

[[nodiscard]] DistanceMeasurementSettings edit_measurement_settings(ViewerFrameContext& state,
                                                                    MeasurementTool& measurement)
{
    DistanceMeasurementSettings settings = measurement.settings();
    bool changed = false;
    const char* unit_names[] = {"Automatic metric", "Meters", "Centimeters",
                                "Millimeters",      "Feet",   "Inches"};
    int unit_index = static_cast<int>(settings.display_unit);
    if (ImGui::Combo("Display unit", &unit_index, unit_names,
                     static_cast<int>(std::size(unit_names)))) {
        settings.display_unit = static_cast<LengthDisplayUnit>(unit_index);
        changed = true;
    }

    const char* depth_names[] = {"Depth tested", "Always visible"};
    int depth_index = static_cast<int>(settings.depth_mode);
    if (ImGui::Combo("Overlay depth", &depth_index, depth_names,
                     static_cast<int>(std::size(depth_names)))) {
        settings.depth_mode = static_cast<elf3d::OverlayDepthMode>(depth_index);
        changed = true;
    }

    std::array<float, 4> line{settings.line_color.red, settings.line_color.green,
                              settings.line_color.blue, settings.line_color.alpha};
    if (ImGui::ColorEdit4("Line color", line.data(), ImGuiColorEditFlags_NoInputs)) {
        settings.line_color = {line[0], line[1], line[2], line[3]};
        changed = true;
    }

    std::array<float, 4> first{settings.first_point_color.red, settings.first_point_color.green,
                               settings.first_point_color.blue, settings.first_point_color.alpha};
    if (ImGui::ColorEdit4("First marker color", first.data(), ImGuiColorEditFlags_NoInputs)) {
        settings.first_point_color = {first[0], first[1], first[2], first[3]};
        changed = true;
    }

    std::array<float, 4> second{settings.second_point_color.red, settings.second_point_color.green,
                                settings.second_point_color.blue,
                                settings.second_point_color.alpha};
    if (ImGui::ColorEdit4("Second marker color", second.data(), ImGuiColorEditFlags_NoInputs)) {
        settings.second_point_color = {second[0], second[1], second[2], second[3]};
        changed = true;
    }
    changed |= ImGui::DragFloat("Line thickness", &settings.line_thickness_pixels, 0.1F, 0.5F,
                                16.0F, "%.1f px");
    changed |= ImGui::DragFloat("Marker radius", &settings.marker_radius_pixels, 0.1F, 1.0F, 32.0F,
                                "%.1f px");
    if (changed) {
        const elf3d::Result<void> result = measurement.set_settings(settings);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    return settings;
}

[[nodiscard]] bool measurement_has_points(const DistanceMeasurementSnapshot& measurement) noexcept
{
    return measurement.first_point.has_value() || measurement.second_point.has_value() ||
           measurement.preview_point.has_value();
}

void draw_measurement_snapshot(const SceneSession& scene, elf3d::Viewport& viewport,
                               ToolCoordinator& tools, LengthDisplayUnit unit)
{
    const DistanceMeasurementSnapshot measurement = tools.measurement().snapshot(
        *scene.scene, viewport, tools.active_tool() == ViewerTool::distance_measurement);
    ImGui::Text("State: %s", measurement_state_name(measurement.state));
    if (measurement.state == DistanceMeasurementState::empty ||
        measurement.state == DistanceMeasurementState::awaiting_first_point) {
        ImGui::TextUnformatted("Click a visible surface to set the first point.");
    }

    draw_measurement_point("First point", scene, measurement.first_point);
    draw_measurement_point("Second point", scene, measurement.second_point);
    draw_measurement_point("Preview point", scene, measurement.preview_point);
    if (measurement.state == DistanceMeasurementState::complete) {
        ImGui::Text("Distance: %.8g m", measurement.distance_meters);
        ImGui::Text("Display: %s", format_distance(measurement.distance_meters, unit).c_str());
    } else if (measurement.preview_point.has_value()) {
        ImGui::Text("Preview distance: %.8g m", measurement.preview_distance_meters);
        ImGui::Text("Display: %s",
                    format_distance(measurement.preview_distance_meters, unit).c_str());
    }
    ImGui::Text("Anchors visible: %s", measurement.anchors_currently_visible ? "yes" : "no");
    if (measurement.diagnostic.has_value()) {
        ImGui::TextWrapped("Diagnostic: %s", measurement.diagnostic->message());
    }

    const bool incomplete = measurement.state == DistanceMeasurementState::awaiting_second_point;
    const bool present = measurement_has_points(measurement);
    ImGui::BeginDisabled(!incomplete);
    if (ImGui::Button("Cancel Current")) {
        tools.measurement().cancel_incomplete();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!present);
    if (ImGui::Button("Clear Measurement")) {
        tools.measurement().clear();
    }
    ImGui::EndDisabled();
}

void draw_measurement_statistics(const MeasurementTool& measurement)
{
    const MeasurementStatistics stats = measurement.statistics();
    ImGui::Text("Committed points: %llu", static_cast<unsigned long long>(stats.committed_points));
    ImGui::Text("Preview picks: %llu", static_cast<unsigned long long>(stats.preview_picks));
    ImGui::Text("Anchor resolutions: %llu",
                static_cast<unsigned long long>(stats.anchor_resolutions));
    ImGui::Text("Overlay lines: %llu", static_cast<unsigned long long>(stats.overlay_lines));
    ImGui::Text("Overlay markers: %llu", static_cast<unsigned long long>(stats.overlay_markers));
}

void build_measurement_panel(ImGuiID dockspace_id, ViewerFrameContext& state,
                             const SceneSession& scene, elf3d::Viewport& engine_viewport,
                             ToolCoordinator& tools)
{
    if (!state.shell.show_measurement_panel) {
        return;
    }
    set_default_dock(state.shell.dock_right_bottom_id != 0 ? state.shell.dock_right_bottom_id
                                                           : dockspace_id,
                     state.shell.apply_dock_layout);
    if (begin_panel_window("Measurement", &state.shell.show_measurement_panel,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        build_measurement_tool_selector(tools);
        const DistanceMeasurementSettings settings =
            edit_measurement_settings(state, tools.measurement());
        ImGui::Separator();
        draw_measurement_snapshot(scene, engine_viewport, tools, settings.display_unit);

        ImGui::Separator();
        draw_measurement_statistics(tools.measurement());
    }
    ImGui::End();
}

} // namespace elf3d::viewer
