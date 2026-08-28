#include <elf3d/elf3d.h>

#include <string_view>
#include <type_traits>
#include <utility>

namespace {

void verify_compile_time_contracts() noexcept {
    static_assert(sizeof(elf3d::Error) <= 256);
    static_assert(noexcept(std::declval<elf3d::Engine&>().~Engine()));
    static_assert(noexcept(std::declval<elf3d::Engine&>().create_scene()));
    static_assert(noexcept(std::declval<elf3d::Engine&>().create_viewport(elf3d::Extent2D{})));
    static_assert(noexcept(std::declval<elf3d::Engine&>().load_scene(std::string_view{})));
    static_assert(noexcept(std::declval<elf3d::SceneLoadReport&>().diagnostic_count()));
    static_assert(noexcept(std::declval<elf3d::SceneLoadReport&>().diagnostic(0)));
    static_assert(noexcept(std::declval<elf3d::SceneLoadReport&>().has_warnings()));
    static_assert(noexcept(std::declval<elf3d::Result<elf3d::SceneStatistics>&>().value()));
    static_assert(noexcept(std::declval<elf3d::Result<elf3d::SceneStatistics>&>().error()));
    static_assert(
        noexcept(std::declval<const elf3d::Scene&>().export_loaded_document(std::string_view{})));
    static_assert(noexcept(std::declval<elf3d::Scene&>().create_model_entity(
        elf3d::MeshHandle{}, elf3d::MaterialHandle{})));
    static_assert(noexcept(std::declval<elf3d::Scene&>().create_perspective_camera_entity(
        elf3d::PerspectiveCameraDescription{})));
    static_assert(noexcept(std::declval<const elf3d::Viewport&>().render_statistics()));

    static_assert(std::is_standard_layout_v<elf3d::Float2>);
    static_assert(std::is_standard_layout_v<elf3d::Color4>);
    static_assert(std::is_standard_layout_v<elf3d::Float3>);
    static_assert(std::is_standard_layout_v<elf3d::Quaternion>);
    static_assert(std::is_standard_layout_v<elf3d::Transform>);
    static_assert(std::is_standard_layout_v<elf3d::Float4x4>);
    static_assert(std::is_standard_layout_v<elf3d::NavigationInput>);
    static_assert(std::is_standard_layout_v<elf3d::OrbitNavigationSettings>);
    static_assert(std::is_standard_layout_v<elf3d::NavigationSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::Ray3>);
    static_assert(std::is_standard_layout_v<elf3d::PickOptions>);
    static_assert(std::is_standard_layout_v<elf3d::PickHit>);
    static_assert(std::is_standard_layout_v<elf3d::PickingStatistics>);
    static_assert(std::is_standard_layout_v<elf3d::SectionPlane>);
    static_assert(std::is_standard_layout_v<elf3d::ClippingBox>);
    static_assert(std::is_standard_layout_v<elf3d::ClippingSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::SelectionSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::OverlayLineSegment>);
    static_assert(std::is_standard_layout_v<elf3d::OverlayPointMarker>);
    static_assert(std::is_standard_layout_v<elf3d::ProjectedViewportPoint>);
    static_assert(std::is_standard_layout_v<elf3d::EntityInfo>);
    static_assert(std::is_standard_layout_v<elf3d::SceneHierarchyItem>);
    static_assert(std::is_standard_layout_v<elf3d::SceneHierarchyStatistics>);
    static_assert(std::is_standard_layout_v<elf3d::EntityHighlight>);
    static_assert(std::is_standard_layout_v<elf3d::ModelLoadOptions>);
    static_assert(std::is_standard_layout_v<elf3d::PerspectiveCameraDescription>);
    static_assert(std::is_standard_layout_v<elf3d::EnvironmentLighting>);
    static_assert(std::is_standard_layout_v<elf3d::DisplayTransform>);
    static_assert(std::is_standard_layout_v<elf3d::SamplerDescription>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormal>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormalTexCoord>);
    static_assert(std::is_standard_layout_v<elf3d::Extent2D>);

    static_assert(!std::is_default_constructible_v<elf3d::Engine>);
    static_assert(!std::is_copy_constructible_v<elf3d::Engine>);
    static_assert(!std::is_move_constructible_v<elf3d::Engine>);
    static_assert(!std::is_move_assignable_v<elf3d::Engine>);
    static_assert(!std::is_copy_constructible_v<elf3d::Viewport>);
    static_assert(!std::is_move_constructible_v<elf3d::Viewport>);
    static_assert(!std::is_move_assignable_v<elf3d::Viewport>);
    static_assert(!std::is_move_constructible_v<elf3d::Scene>);
    static_assert(std::is_move_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_copy_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_convertible_v<elf3d::ImageHandle, elf3d::TextureAssetHandle>);
}

[[nodiscard]] bool has_expected_math_defaults() noexcept {
    const elf3d::Float2 position{4.0F, 8.0F};
    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 1.0F};
    const elf3d::Extent2D extent{800, 600};
    return position == elf3d::Float2{4.0F, 8.0F} &&
           color == elf3d::Color4{0.1F, 0.2F, 0.3F, 1.0F} && extent == elf3d::Extent2D{800, 600};
}

[[nodiscard]] bool has_expected_navigation_defaults() noexcept {
    const elf3d::TextureHandle texture;
    const elf3d::NavigationInput input;
    const elf3d::OrbitNavigationSettings settings;
    return !texture.is_valid() && !input.pointer_hovered && !input.region_focused &&
           !input.orbit_down && !input.pan_down && !input.zoom_down && input.wheel_delta == 0.0F &&
           settings.orbit_sensitivity == 0.0025F;
}

[[nodiscard]] bool has_expected_rendering_defaults() noexcept {
    const elf3d::BasicLighting lighting;
    const elf3d::EnvironmentLighting environment;
    const elf3d::DisplayTransform display;
    return lighting.direction == elf3d::Float3{-0.5F, -1.0F, -0.3F} &&
           lighting.diffuse_intensity == 2.0F && lighting.ambient_intensity == 0.0F &&
           environment.intensity == 2.0F && environment.rotation_radians == 0.0F &&
           display.exposure_ev == 0.0F && display.tone_mapping == elf3d::ToneMappingMode::standard;
}

[[nodiscard]] bool has_expected_scene_defaults() noexcept {
    const elf3d::Ray3 ray;
    const elf3d::SectionPlane section_plane;
    const elf3d::ClippingBox clipping_box;
    const elf3d::ClippingSnapshot clipping_snapshot;
    const elf3d::ProjectedViewportPoint projected_point;
    return ray.direction == elf3d::Float3{0.0F, 0.0F, -1.0F} && !section_plane.enabled &&
           section_plane.retained_half_space == elf3d::PlaneHalfSpace::positive &&
           clipping_box.minimum == elf3d::Float3{-0.5F, -0.5F, -0.5F} &&
           clipping_box.maximum == elf3d::Float3{0.5F, 0.5F, 0.5F} && clipping_box.enabled &&
           clipping_snapshot.box_count == 0 && !projected_point.is_in_front &&
           !projected_point.is_inside_viewport;
}

} // namespace

int main() {
    verify_compile_time_contracts();
    return has_expected_math_defaults() && has_expected_navigation_defaults() &&
                   has_expected_rendering_defaults() && has_expected_scene_defaults()
               ? 0
               : 1;
}
