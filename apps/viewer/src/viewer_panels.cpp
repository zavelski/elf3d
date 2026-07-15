#include "viewer_internal.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace elf3d::viewer {

void draw_model_source_information(const ViewerScene& scene) {
    const std::string source =
        scene.is_imported() ? path_to_utf8(scene.source_path) : "Procedural cube demo";
    const std::string extension = scene.source_path.extension().string();
    ImGui::TextWrapped("Source: %s", source.c_str());
    ImGui::Text("Format: %s",
                scene.is_imported() ? (extension == ".glb" ? "GLB" : "glTF") : "Procedural");
    ImGui::Separator();
    ImGui::Text("Entities: %llu",
                static_cast<unsigned long long>(scene.source_statistics.entities));
    ImGui::Text("Model entities: %llu",
                static_cast<unsigned long long>(scene.source_statistics.model_entities));
    ImGui::Text("Mesh assets: %llu",
                static_cast<unsigned long long>(scene.source_statistics.mesh_assets));
    ImGui::Text("Materials: %llu",
                static_cast<unsigned long long>(scene.source_statistics.material_assets));
    ImGui::Text("Images: %llu",
                static_cast<unsigned long long>(scene.source_statistics.image_assets));
    ImGui::Text("Textures: %llu",
                static_cast<unsigned long long>(scene.source_statistics.texture_assets));
    ImGui::Text("Sampler descriptions: %llu",
                static_cast<unsigned long long>(scene.source_statistics.sampler_descriptions));
    ImGui::Text("Decoded image memory: %llu bytes",
                static_cast<unsigned long long>(scene.source_statistics.decoded_image_bytes));
    ImGui::Text("Base-color textured materials: %llu",
                static_cast<unsigned long long>(
                    scene.source_statistics.materials_with_base_color_textures));
    ImGui::Text("Metallic-roughness textured materials: %llu",
                static_cast<unsigned long long>(
                    scene.source_statistics.materials_with_metallic_roughness_textures));
    ImGui::Text(
        "Normal-textured materials: %llu",
        static_cast<unsigned long long>(scene.source_statistics.materials_with_normal_textures));
    ImGui::Text(
        "Occlusion-textured materials: %llu",
        static_cast<unsigned long long>(scene.source_statistics.materials_with_occlusion_textures));
    ImGui::Text(
        "Emissive-textured materials: %llu",
        static_cast<unsigned long long>(scene.source_statistics.materials_with_emissive_textures));
    ImGui::Text("Primitives: %llu",
                static_cast<unsigned long long>(scene.source_statistics.primitives));
    ImGui::Text("Vertices: %llu",
                static_cast<unsigned long long>(scene.source_statistics.vertices));
    ImGui::Text("Indices: %llu", static_cast<unsigned long long>(scene.source_statistics.indices));
    ImGui::Text("Triangles: %llu",
                static_cast<unsigned long long>(scene.source_statistics.triangles));
    if (scene.source_bounds.has_value()) {
        ImGui::Text("Bounds min: %.4g, %.4g, %.4g", scene.source_bounds->minimum.x,
                    scene.source_bounds->minimum.y, scene.source_bounds->minimum.z);
        ImGui::Text("Bounds max: %.4g, %.4g, %.4g", scene.source_bounds->maximum.x,
                    scene.source_bounds->maximum.y, scene.source_bounds->maximum.z);
    } else {
        ImGui::TextUnformatted("Bounds: empty");
    }
}

void draw_model_render_statistics(const ViewerState& state) {
    ImGui::Separator();
    ImGui::Text("Latest draw calls: %llu",
                static_cast<unsigned long long>(state.statistics.draw_calls));
    ImGui::Text("Latest rendered triangles: %llu",
                static_cast<unsigned long long>(state.statistics.triangles));
    ImGui::Text("Latest texture bindings: %llu",
                static_cast<unsigned long long>(state.statistics.texture_bindings));
    ImGui::Text("Latest texture uploads: %llu",
                static_cast<unsigned long long>(state.statistics.gpu_texture_uploads));
    ImGui::Text("Current GPU textures: %llu",
                static_cast<unsigned long long>(state.statistics.unique_gpu_textures));
    ImGui::Text("Overlay lines: %llu",
                static_cast<unsigned long long>(state.statistics.overlay_lines));
    ImGui::Text("Overlay markers: %llu",
                static_cast<unsigned long long>(state.statistics.overlay_markers));
    ImGui::Text("Clipping bounds tested: %llu",
                static_cast<unsigned long long>(state.statistics.clipping_bounds_tested));
    ImGui::Text("Clipping bounds rejected: %llu",
                static_cast<unsigned long long>(state.statistics.clipping_bounds_rejected));
    ImGui::Text("Clipping bounds intersecting: %llu",
                static_cast<unsigned long long>(state.statistics.clipping_bounds_intersecting));
    ImGui::TextUnformatted("Image formats: PNG, JPEG");
    ImGui::TextWrapped("PBR: one directional light with vertex color, UV0/UV1 texture "
                       "mapping, texture transforms, emissive, occlusion, unlit, alpha mask, "
                       "and simple sorted alpha blending. Normal maps are preserved but use a "
                       "documented fallback until tangent-space rendering is available.");
}

void draw_import_diagnostics(const ViewerScene& scene) {
    if (!scene.is_imported()) {
        return;
    }
    ImGui::Separator();
    ImGui::Text("Import diagnostics: %llu",
                static_cast<unsigned long long>(scene.load_report.diagnostic_count()));
    for (std::size_t index = 0; index < scene.load_report.diagnostic_count(); ++index) {
        const elf3d::Result<elf3d::SceneLoadDiagnosticView> result =
            scene.load_report.diagnostic(index);
        if (!result) {
            continue;
        }
        const elf3d::SceneLoadDiagnosticView diagnostic = result.value();
        ImGui::BulletText("%.*s", static_cast<int>(diagnostic.message.size()),
                          diagnostic.message.data());
        if (diagnostic.source_context.has_value()) {
            const std::string_view context = *diagnostic.source_context;
            ImGui::Indent();
            ImGui::TextWrapped("Context: %.*s", static_cast<int>(context.size()), context.data());
            ImGui::Unindent();
        }
    }
}

void build_model_information(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene) {
    if (!state.show_model_information) {
        return;
    }
    set_default_dock(state.dock_center_id != 0 ? state.dock_center_id : dockspace_id,
                     state.apply_dock_layout);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{1.0F, 1.0F, 1.0F, 1.0F});
    if (begin_panel_window("Model Information", &state.show_model_information,
                           state.panel_title_font)) {
        const ScopedFont panel_font{state.panel_content_font};
        draw_model_source_information(scene);
        draw_model_render_statistics(state);
        draw_import_diagnostics(scene);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void build_demo_cube_rendering_controls(ViewerState& state, ViewerScene& scene) {
    if (scene.is_imported() || !scene.cube.has_value()) {
        return;
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Demo Cube");
    ImGui::Checkbox("Rotate cube", &state.rotate_cube);
    const float speed_label_width =
        ImGui::CalcTextSize("Speed").x + ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::SetNextItemWidth(std::max(80.0F, ImGui::GetContentRegionAvail().x - speed_label_width));
    ImGui::SliderFloat("Speed##CubeSpeed", &state.rotation_speed, 0.0F, 3.0F, "%.2f rad/s");
    if (ImGui::Button("Reset transform")) {
        reset_demo_cube_transform(state, scene);
    }
    if (color_control("Cube base color", state.cube_color)) {
        apply_demo_cube_color(state, scene);
    }
}

void build_lighting_controls(ViewerState& state) {
    if (!ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    std::array<float, 3> direction{state.lighting.direction.x, state.lighting.direction.y,
                                   state.lighting.direction.z};
    if (ImGui::DragFloat3("Light direction", direction.data(), 0.01F, -1.0F, 1.0F)) {
        state.lighting.direction = {direction[0], direction[1], direction[2]};
    }
    ImGui::SliderFloat("Light intensity", &state.lighting.diffuse_intensity, 0.0F, 10.0F, "%.2f");
    ImGui::SliderFloat("Ambient intensity", &state.lighting.ambient_intensity, 0.0F, 2.0F, "%.2f");
    if (ImGui::Button("Reset Lighting")) {
        state.lighting = elf3d::BasicLighting{};
    }
}

void build_rendering_panel(ImGuiID dockspace_id, ViewerState& state, ViewerScene& scene) {
    if (!state.show_rendering_panel) {
        return;
    }
    set_default_dock(state.dock_right_bottom_id != 0 ? state.dock_right_bottom_id : dockspace_id,
                     state.apply_dock_layout);
    if (begin_panel_window("Rendering", &state.show_rendering_panel, state.panel_title_font)) {
        const ScopedFont panel_font{state.panel_content_font};
        ImGui::TextUnformatted("Viewport");
        color_control("Clear color", state.clear_color);
        build_demo_cube_rendering_controls(state, scene);
        build_lighting_controls(state);
    }
    ImGui::End();
}

[[nodiscard]] float radians_to_degrees(float radians) noexcept {
    return radians * 57.2957795131F;
}

[[nodiscard]] const char* interaction_mode_name(elf3d::NavigationInteractionMode mode) noexcept {
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

void build_navigation_settings_window(ImGuiID dockspace_id, ViewerState& state,
                                      elf3d::Viewport& engine_viewport) {
    if (!state.show_navigation_settings) {
        return;
    }
    set_default_dock(state.dock_right_bottom_id != 0 ? state.dock_right_bottom_id : dockspace_id,
                     state.apply_dock_layout);
    if (begin_panel_window("Navigation Settings", &state.show_navigation_settings,
                           state.panel_title_font)) {
        const ScopedFont panel_font{state.panel_content_font};
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

[[nodiscard]] std::string entity_label(const ViewerScene& scene, elf3d::EntityId entity) {
    const elf3d::Result<std::string_view> name = scene.scene->entity_name(entity);
    if (name && !name.value().empty()) {
        const std::string_view text = name.value();
        return std::string{text};
    }
    return std::string{"Entity "} + std::to_string(entity.debug_value());
}

[[nodiscard]] std::string selected_entity_label(const ViewerScene& scene,
                                                const elf3d::SelectionSnapshot& selection) {
    return selection.entity.has_value() ? entity_label(scene, *selection.entity) : "none";
}

void build_selection_settings(ViewerState& state, elf3d::Viewport& viewport) {
    bool enabled = viewport.selection_enabled();
    if (ImGui::Checkbox("Enable Selection", &enabled)) {
        viewport.set_selection_enabled(enabled);
    }
    elf3d::SelectionSettings settings = viewport.selection_settings();
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
    const elf3d::Result<void> result = viewport.set_selection_settings(settings);
    if (!result) {
        set_viewport_error(state, result.error());
    }
}

void draw_selected_entity(const ViewerScene& scene, const elf3d::SelectionSnapshot& selection) {
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

void draw_picking_statistics(ViewerState& state, elf3d::Viewport& viewport) {
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
    ImGui::Separator();
    ImGui::Text("Instance bounds tests: %llu",
                static_cast<unsigned long long>(picking.latest_instance_bounds_tests));
    ImGui::Text("Mesh bounds tests: %llu",
                static_cast<unsigned long long>(picking.latest_mesh_bounds_tests));
    ImGui::Text("BVH node tests: %llu",
                static_cast<unsigned long long>(picking.latest_bvh_node_tests));
    ImGui::Text("Triangle tests: %llu",
                static_cast<unsigned long long>(picking.latest_triangle_tests));
    ImGui::Text("Clipping bounds rejected: %llu",
                static_cast<unsigned long long>(picking.latest_clipping_bounds_rejected));
    ImGui::Text("Clipping hits rejected: %llu",
                static_cast<unsigned long long>(picking.latest_clipping_hits_rejected));
    ImGui::Text("Clipping hits accepted: %llu",
                static_cast<unsigned long long>(picking.latest_clipping_hits_accepted));
    ImGui::Text("BVH builds this pick: %llu",
                static_cast<unsigned long long>(picking.latest_bvh_builds));
    ImGui::Text("Lifetime BVH builds: %llu",
                static_cast<unsigned long long>(picking.lifetime_bvh_builds));
    ImGui::Text("Cached mesh BVHs: %llu",
                static_cast<unsigned long long>(picking.cached_mesh_bvhs));
}

void build_selection_panel(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene,
                           elf3d::Viewport& engine_viewport) {
    if (!state.show_selection_panel) {
        return;
    }
    set_default_dock(state.dock_right_id != 0 ? state.dock_right_id : dockspace_id,
                     state.apply_dock_layout);
    if (begin_panel_window("Selection", &state.show_selection_panel, state.panel_title_font)) {
        const ScopedFont panel_font{state.panel_content_font};
        build_selection_settings(state, engine_viewport);

        ImGui::Separator();
        const elf3d::SelectionSnapshot selection = engine_viewport.selection_snapshot();
        draw_selected_entity(scene, selection);

        ImGui::Separator();
        draw_picking_statistics(state, engine_viewport);
    }
    ImGui::End();
}

void draw_measurement_point(const char* label, const ViewerScene& scene,
                            const std::optional<elf3d::MeasurementPoint>& point) {
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

void build_measurement_tool_selector(ViewerState& state, elf3d::Viewport& viewport) {
    const elf3d::ViewportTool active_tool = viewport.active_tool();
    ImGui::Text("Active tool: %s", tool_name(active_tool));
    if (ImGui::RadioButton("Select", active_tool == elf3d::ViewportTool::selection)) {
        viewport.set_active_tool(elf3d::ViewportTool::selection);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Measure Distance",
                           active_tool == elf3d::ViewportTool::distance_measurement)) {
        const elf3d::Result<void> result = viewport.begin_distance_measurement();
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
}

[[nodiscard]] elf3d::DistanceMeasurementSettings
edit_measurement_settings(ViewerState& state, elf3d::Viewport& viewport) {
    elf3d::DistanceMeasurementSettings settings = viewport.measurement_settings();
    bool changed = false;
    const char* unit_names[] = {"Automatic metric", "Meters", "Centimeters",
                                "Millimeters",      "Feet",   "Inches"};
    int unit_index = static_cast<int>(settings.display_unit);
    if (ImGui::Combo("Display unit", &unit_index, unit_names,
                     static_cast<int>(std::size(unit_names)))) {
        settings.display_unit = static_cast<elf3d::LengthDisplayUnit>(unit_index);
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
        const elf3d::Result<void> result = viewport.set_measurement_settings(settings);
        if (!result) {
            set_viewport_error(state, result.error());
        }
    }
    return settings;
}

[[nodiscard]] bool
measurement_has_points(const elf3d::DistanceMeasurementSnapshot& measurement) noexcept {
    return measurement.first_point.has_value() || measurement.second_point.has_value() ||
           measurement.preview_point.has_value();
}

void draw_measurement_snapshot(const ViewerScene& scene, elf3d::Viewport& viewport,
                               elf3d::LengthDisplayUnit unit) {
    const elf3d::DistanceMeasurementSnapshot measurement =
        viewport.distance_measurement_snapshot(*scene.scene);
    ImGui::Text("State: %s", measurement_state_name(measurement.state));
    if (measurement.state == elf3d::DistanceMeasurementState::empty ||
        measurement.state == elf3d::DistanceMeasurementState::awaiting_first_point) {
        ImGui::TextUnformatted("Click a visible surface to set the first point.");
    }
    draw_measurement_point("First point", scene, measurement.first_point);
    draw_measurement_point("Second point", scene, measurement.second_point);
    draw_measurement_point("Preview point", scene, measurement.preview_point);
    if (measurement.state == elf3d::DistanceMeasurementState::complete) {
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
    const bool incomplete =
        measurement.state == elf3d::DistanceMeasurementState::awaiting_second_point;
    const bool present = measurement_has_points(measurement);
    ImGui::BeginDisabled(!incomplete);
    if (ImGui::Button("Cancel Current")) {
        viewport.cancel_distance_measurement();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!present);
    if (ImGui::Button("Clear Measurement")) {
        viewport.clear_distance_measurement();
    }
    ImGui::EndDisabled();
}

void draw_measurement_statistics(const elf3d::Viewport& viewport) {
    const elf3d::MeasurementStatistics stats = viewport.measurement_statistics();
    ImGui::Text("Committed points: %llu", static_cast<unsigned long long>(stats.committed_points));
    ImGui::Text("Preview picks: %llu", static_cast<unsigned long long>(stats.preview_picks));
    ImGui::Text("Anchor resolutions: %llu",
                static_cast<unsigned long long>(stats.anchor_resolutions));
    ImGui::Text("Overlay lines: %llu", static_cast<unsigned long long>(stats.overlay_lines));
    ImGui::Text("Overlay markers: %llu", static_cast<unsigned long long>(stats.overlay_markers));
}

void build_measurement_panel(ImGuiID dockspace_id, ViewerState& state, const ViewerScene& scene,
                             elf3d::Viewport& engine_viewport) {
    if (!state.show_measurement_panel) {
        return;
    }
    set_default_dock(state.dock_right_bottom_id != 0 ? state.dock_right_bottom_id : dockspace_id,
                     state.apply_dock_layout);
    if (begin_panel_window("Measurement", &state.show_measurement_panel, state.panel_title_font)) {
        const ScopedFont panel_font{state.panel_content_font};
        build_measurement_tool_selector(state, engine_viewport);
        const elf3d::DistanceMeasurementSettings settings =
            edit_measurement_settings(state, engine_viewport);
        ImGui::Separator();
        draw_measurement_snapshot(scene, engine_viewport, settings.display_unit);

        ImGui::Separator();
        draw_measurement_statistics(engine_viewport);
    }
    ImGui::End();
}

} // namespace elf3d::viewer
