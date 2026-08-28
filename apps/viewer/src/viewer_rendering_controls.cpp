#include "viewer_rendering_controls.hpp"

#include <imgui.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace elf3d::viewer {
namespace {

[[nodiscard]] const char* tone_mapping_name(ToneMappingMode mode) noexcept {
    if (mode == ToneMappingMode::none) {
        return "none";
    }
    return mode == ToneMappingMode::pbr_neutral ? "pbr_neutral" : "standard";
}

void build_tone_mapping_control(DisplayTransform& display) {
    constexpr std::array<const char*, 3> modes{{"Standard", "PBR Neutral", "None (diagnostic)"}};
    int selected = 0;
    if (display.tone_mapping == ToneMappingMode::pbr_neutral) {
        selected = 1;
    } else if (display.tone_mapping == ToneMappingMode::none) {
        selected = 2;
    }
    if (!ImGui::Combo("Tone mapping", &selected, modes.data(), static_cast<int>(modes.size()))) {
        return;
    }
    if (selected == 1) {
        display.tone_mapping = ToneMappingMode::pbr_neutral;
    } else if (selected == 2) {
        display.tone_mapping = ToneMappingMode::none;
    } else {
        display.tone_mapping = ToneMappingMode::standard;
    }
}

[[nodiscard]] std::string camera_evidence_text(const ViewerFrameContext& state,
                                               const SceneSession& session,
                                               const Viewport& viewport) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6);
    const Result<Float4x4> matrix = session.scene->local_matrix(session.camera);
    const Result<PerspectiveCameraDescription> camera =
        session.scene->perspective_camera_description(session.camera);
    stream << "camera_local_matrix_column_major=";
    if (matrix) {
        for (std::size_t index = 0; index < matrix.value().elements.size(); ++index) {
            stream << (index == 0 ? "[" : ",") << matrix.value().elements[index];
        }
        stream << "]\n";
    } else {
        stream << "unavailable\n";
    }
    if (camera) {
        stream << "vertical_fov_radians=" << camera.value().vertical_field_of_view_radians << '\n'
               << "near_plane=" << camera.value().near_plane << '\n'
               << "far_plane=" << camera.value().far_plane << '\n';
    }
    const Extent2D extent = viewport.extent();
    stream << "viewport_extent=" << extent.width << 'x' << extent.height << '\n'
           << "light_direction=" << state.rendering.lighting.direction.x << ','
           << state.rendering.lighting.direction.y << ',' << state.rendering.lighting.direction.z
           << '\n'
           << "directional_intensity=" << state.rendering.lighting.diffuse_intensity << '\n'
           << "legacy_ambient=" << state.rendering.lighting.ambient_intensity << '\n'
           << "environment_intensity=" << state.rendering.environment_lighting.intensity << '\n'
           << "environment_rotation_radians="
           << state.rendering.environment_lighting.rotation_radians << '\n'
           << "exposure_ev=" << state.rendering.display_transform.exposure_ev << '\n'
           << "tone_mapping=" << tone_mapping_name(state.rendering.display_transform.tone_mapping);
    return stream.str();
}

} // namespace

void build_lighting_controls(ViewerFrameContext& state) {
    if (!ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    std::array<float, 3> direction{state.rendering.lighting.direction.x,
                                   state.rendering.lighting.direction.y,
                                   state.rendering.lighting.direction.z};
    if (ImGui::DragFloat3("Light direction", direction.data(), 0.01F, -1.0F, 1.0F)) {
        state.rendering.lighting.direction = {direction[0], direction[1], direction[2]};
    }
    ImGui::SliderFloat("Light intensity", &state.rendering.lighting.diffuse_intensity, 0.0F, 10.0F,
                       "%.2f");
    ImGui::SliderFloat("Legacy ambient", &state.rendering.lighting.ambient_intensity, 0.0F, 2.0F,
                       "%.2f");
    ImGui::SliderFloat("Environment intensity", &state.rendering.environment_lighting.intensity,
                       0.0F, 4.0F, "%.2f");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        ImGui::SetTooltip("Use environment intensity as the normal fill and reflection control.");
    }
    float rotation_degrees = state.rendering.environment_lighting.rotation_radians * 57.2957795131F;
    if (ImGui::SliderFloat("Environment rotation", &rotation_degrees, -180.0F, 180.0F,
                           "%.1f deg")) {
        state.rendering.environment_lighting.rotation_radians = rotation_degrees * 0.01745329252F;
    }
    ImGui::SliderFloat("Exposure", &state.rendering.display_transform.exposure_ev, -4.0F, 4.0F,
                       "%.2f EV");
    build_tone_mapping_control(state.rendering.display_transform);
    if (ImGui::Button("Reset Lighting")) {
        state.rendering.lighting = BasicLighting{};
        state.rendering.environment_lighting = EnvironmentLighting{};
        state.rendering.display_transform = DisplayTransform{};
    }
}

void build_camera_evidence(const ViewerFrameContext& state, const SceneSession& scene,
                           const Viewport& viewport) {
    if (!ImGui::CollapsingHeader("Camera Evidence")) {
        return;
    }
    const std::string evidence = camera_evidence_text(state, scene, viewport);
    ImGui::TextWrapped("%s", evidence.c_str());
    if (ImGui::Button("Copy Camera Evidence")) {
        ImGui::SetClipboardText(evidence.c_str());
    }
}

} // namespace elf3d::viewer
