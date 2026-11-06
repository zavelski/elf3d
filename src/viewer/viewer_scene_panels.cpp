#include "viewer_browser.hpp"
#include "viewer_components.hpp"

#include "viewer_ui.hpp"
#include "viewer_viewport.hpp"

#include <elf3d/core/assert.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::viewer {

void build_scene_hierarchy_panel(ViewerFrameContext& state, SceneSession& scene,
                                 elf3d::Viewport& engine_viewport, ToolCoordinator& tools,
                                 ViewerCommandDispatcher& commands)
{
    if (!state.shell.show_scene_hierarchy) {
        return;
    }
    set_default_dock(state.shell.dock_right_id, state.shell.apply_dock_layout);
    const bool open = begin_panel_window("Scene Browser", &state.shell.show_scene_hierarchy,
                                         state.presentation.panel_title_font);
    if (!open) {
        ImGui::End();
        return;
    }

    const ScopedFont panel_font{state.presentation.panel_content_font};
    build_document_browser(state, scene, engine_viewport, tools, commands);
    ImGui::End();
}

void draw_model_source_information(const SceneSession& scene)
{
    const std::string source =
        scene.is_imported() ? path_to_utf8(scene.source_path) : "Empty scene";
    const std::string extension = scene.source_path.extension().string();
    ImGui::TextWrapped("Source: %s", source.c_str());
    ImGui::Text("Format: %s",
                scene.is_imported() ? (extension == ".glb" ? "GLB" : "glTF") : "None");
    if (!scene.is_imported()) {
        ImGui::TextUnformatted("No model loaded");
    } else if (scene.source_statistics.triangles == 0) {
        ImGui::TextUnformatted("Loaded empty model: Save As is available.");
    }
    ImGui::Separator();
    ImGui::Text("Entities: %llu",
                static_cast<unsigned long long>(scene.source_statistics.entities));
    ImGui::Text("Model entities: %llu",
                static_cast<unsigned long long>(scene.source_statistics.model_entities));
    const auto document = scene.scene->document();
    ImGui::Text("Document nodes / meshes: %zu / %zu", document ? document->node_count() : 0,
                document ? document->mesh_count() : 0);
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
    ImGui::Text("Unique triangles: %llu",
                static_cast<unsigned long long>(scene.source_statistics.triangles));
    ImGui::Text("Document instance triangles: %llu",
                static_cast<unsigned long long>(scene.expanded_triangles));
    if (scene.source_bounds.has_value()) {
        ImGui::Text("Bounds min: %.4g, %.4g, %.4g", scene.source_bounds->minimum.x,
                    scene.source_bounds->minimum.y, scene.source_bounds->minimum.z);
        ImGui::Text("Bounds max: %.4g, %.4g, %.4g", scene.source_bounds->maximum.x,
                    scene.source_bounds->maximum.y, scene.source_bounds->maximum.z);
    } else {
        ImGui::TextUnformatted("Bounds: empty");
    }
}

void draw_model_render_statistics(const ViewerFrameContext& state)
{
    ImGui::Separator();
    ImGui::Text("Latest draw calls: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.draw_calls));
    ImGui::Text("Latest rendered triangles: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.triangles));
    ImGui::Text("Latest texture bindings: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.texture_bindings));
    ImGui::Text("Latest texture uploads: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.gpu_texture_uploads));
    ImGui::Text("Current GPU textures: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.unique_gpu_textures));
    ImGui::Text("Overlay lines: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.overlay_lines));
    ImGui::Text("Overlay markers: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.overlay_markers));
    ImGui::Text(
        "Candidate / visible / culled: %llu / %llu / %llu",
        static_cast<unsigned long long>(state.rendering.statistics.candidate_primitives),
        static_cast<unsigned long long>(state.rendering.statistics.visible_primitives),
        static_cast<unsigned long long>(state.rendering.statistics.frustum_culled_primitives));
    ImGui::Text(
        "Buffer uploads: %llu (%llu bytes)",
        static_cast<unsigned long long>(state.rendering.statistics.gpu_buffer_uploads),
        static_cast<unsigned long long>(state.rendering.statistics.gpu_buffer_uploaded_bytes));
    ImGui::Text("Draw packet rebuilds: %llu",
                static_cast<unsigned long long>(state.rendering.statistics.draw_packet_rebuilds));
    ImGui::Text("Resident geometry / textures: %llu / %llu bytes",
                static_cast<unsigned long long>(
                    state.rendering.statistics.estimated_resident_geometry_bytes),
                static_cast<unsigned long long>(
                    state.rendering.statistics.estimated_resident_texture_bytes));
    ImGui::Text("CPU list / resources / GL / total: %.3f / %.3f / %.3f / %.3f ms",
                state.rendering.statistics.cpu_render_list_milliseconds,
                state.rendering.statistics.cpu_resource_preparation_milliseconds,
                state.rendering.statistics.cpu_gl_submission_milliseconds,
                state.rendering.statistics.cpu_total_milliseconds);
    if (state.rendering.statistics.gpu_main_pass_timing_available) {
        ImGui::Text("Delayed GPU main: %.3f ms",
                    state.rendering.statistics.gpu_main_pass_milliseconds);
    } else {
        ImGui::TextUnformatted("Delayed GPU main: unavailable");
    }
    if (state.rendering.statistics.gpu_resolve_timing_available) {
        ImGui::Text("Delayed GPU resolve: %.3f ms",
                    state.rendering.statistics.gpu_resolve_milliseconds);
    } else {
        ImGui::TextUnformatted("Delayed GPU resolve: unavailable");
    }
    ImGui::TextUnformatted("Image formats: PNG, JPEG");
    ImGui::TextWrapped("PBR: one directional light with vertex color, UV0/UV1 texture "
                       "mapping, texture transforms, emissive, occlusion, unlit, alpha mask, "
                       "normal maps, and simple sorted alpha blending.");
}

void draw_import_diagnostics(const SceneSession& scene)
{
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

void build_model_information(ImGuiID dockspace_id, ViewerFrameContext& state,
                             const SceneSession& scene)
{
    if (!state.shell.show_model_information) {
        return;
    }
    set_default_dock(state.shell.dock_center_id != 0 ? state.shell.dock_center_id : dockspace_id,
                     state.shell.apply_dock_layout);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{1.0F, 1.0F, 1.0F, 1.0F});
    if (begin_panel_window("Model Information", &state.shell.show_model_information,
                           state.presentation.panel_title_font)) {
        const ScopedFont panel_font{state.presentation.panel_content_font};
        draw_model_source_information(scene);
        draw_model_render_statistics(state);
        draw_import_diagnostics(scene);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

} // namespace elf3d::viewer
