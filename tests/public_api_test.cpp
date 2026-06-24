#include <elf3d/elf3d.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <type_traits>
#include <utility>

int main() {
    static_assert(std::is_standard_layout_v<elf3d::Float2>);
    static_assert(std::is_standard_layout_v<elf3d::Color4>);
    static_assert(std::is_standard_layout_v<elf3d::Float3>);
    static_assert(std::is_standard_layout_v<elf3d::Quaternion>);
    static_assert(std::is_standard_layout_v<elf3d::Transform>);
    static_assert(std::is_standard_layout_v<elf3d::Float4x4>);
    static_assert(std::is_standard_layout_v<elf3d::ViewportInput>);
    static_assert(std::is_standard_layout_v<elf3d::OrbitNavigationSettings>);
    static_assert(std::is_standard_layout_v<elf3d::NavigationSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::Ray3>);
    static_assert(std::is_standard_layout_v<elf3d::PickOptions>);
    static_assert(std::is_standard_layout_v<elf3d::PickHit>);
    static_assert(std::is_standard_layout_v<elf3d::PickingStatistics>);
    static_assert(std::is_standard_layout_v<elf3d::SectionPlane>);
    static_assert(std::is_standard_layout_v<elf3d::ClippingBox>);
    static_assert(std::is_standard_layout_v<elf3d::ClippingHelperSettings>);
    static_assert(std::is_standard_layout_v<elf3d::ClippingSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::SelectionSettings>);
    static_assert(std::is_standard_layout_v<elf3d::SelectionSnapshot>);
    static_assert(std::is_standard_layout_v<elf3d::MeasurementPoint>);
    static_assert(std::is_standard_layout_v<elf3d::DistanceMeasurementSettings>);
    static_assert(std::is_standard_layout_v<elf3d::MeasurementStatistics>);
    static_assert(std::is_standard_layout_v<elf3d::OverlayLineSegment>);
    static_assert(std::is_standard_layout_v<elf3d::OverlayPointMarker>);
    static_assert(std::is_standard_layout_v<elf3d::ProjectedViewportPoint>);
    static_assert(std::is_standard_layout_v<elf3d::EntityInfo>);
    static_assert(std::is_standard_layout_v<elf3d::SceneHierarchyItem>);
    static_assert(std::is_standard_layout_v<elf3d::SceneHierarchyStatistics>);
    static_assert(std::is_standard_layout_v<elf3d::EntityHighlight>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormal>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormalTexCoord>);
    static_assert(std::is_standard_layout_v<elf3d::Extent2D>);
    static_assert(std::is_move_constructible_v<elf3d::Viewport>);
    static_assert(std::is_move_assignable_v<elf3d::Viewport>);
    static_assert(!std::is_copy_constructible_v<elf3d::Viewport>);
    static_assert(std::is_move_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_copy_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_convertible_v<elf3d::ImageHandle, elf3d::TextureAssetHandle>);

    const elf3d::Version current = elf3d::version();
    if (current.major != 0 || current.minor != 2 || current.patch != 0) {
        return 1;
    }
    if (std::strcmp(elf3d::version_string(), "0.2.0") != 0) {
        return 2;
    }

    elf3d::Engine engine;
    elf3d::Engine moved_engine{std::move(engine)};
    elf3d::Engine assigned_engine;
    assigned_engine = std::move(moved_engine);

    const elf3d::Float2 position{4.0F, 8.0F};
    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 1.0F};
    const elf3d::Extent2D extent{800, 600};
    if (position != elf3d::Float2{4.0F, 8.0F} || color != elf3d::Color4{0.1F, 0.2F, 0.3F, 1.0F} ||
        extent != elf3d::Extent2D{800, 600}) {
        return 3;
    }

    const elf3d::TextureHandle texture;
    const elf3d::NativeTextureView native_view;
    const elf3d::ViewportInput inactive_input;
    const elf3d::Ray3 default_ray;
    const elf3d::SelectionSettings selection_settings;
    const elf3d::SectionPlane section_plane;
    const elf3d::ClippingBox clipping_box;
    const elf3d::ClippingSnapshot clipping_snapshot;
    const elf3d::DistanceMeasurementSettings measurement_settings;
    const elf3d::ProjectedViewportPoint projected_point;
    if (texture.is_valid() || native_view.is_valid() || inactive_input.is_hovered ||
        inactive_input.left_button_down || inactive_input.wheel_delta != 0.0F ||
        default_ray.direction != elf3d::Float3{0.0F, 0.0F, -1.0F} ||
        selection_settings.click_drag_threshold_pixels <= 0.0F ||
        section_plane.enabled ||
        section_plane.retained_half_space != elf3d::PlaneHalfSpace::positive ||
        clipping_box.minimum != elf3d::Float3{-0.5F, -0.5F, -0.5F} ||
        clipping_box.maximum != elf3d::Float3{0.5F, 0.5F, 0.5F} ||
        !clipping_box.enabled || clipping_snapshot.box_count != 0 ||
        !clipping_snapshot.helpers.visible ||
        measurement_settings.display_unit != elf3d::LengthDisplayUnit::automatic_metric ||
        measurement_settings.depth_mode != elf3d::OverlayDepthMode::always_visible ||
        measurement_settings.line_thickness_pixels <= 0.0F ||
        measurement_settings.marker_radius_pixels <= 0.0F || projected_point.is_in_front ||
        projected_point.is_inside_viewport) {
        return 4;
    }

    const elf3d::EngineConfiguration missing_loader;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> create_result =
        elf3d::Engine::create(missing_loader);
    if (create_result ||
        create_result.error().code() != elf3d::ErrorCode::missing_graphics_procedure_loader) {
        return 5;
    }

    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = assigned_engine.create_scene();
    if (!scene_result) {
        return 6;
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = scene->create_mesh({vertices, indices});
    const std::array<std::byte, 4> image_pixel{
        {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
    const auto image = scene->create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, image_pixel});
    const auto asset_texture = scene->create_texture({image.value(), {}});
    elf3d::MaterialDescription public_material;
    public_material.base_color = {0.1F, 0.2F, 0.3F, 1.0F};
    public_material.base_color_texture = asset_texture.value();
    const auto material = scene->create_material(public_material);
    if (!mesh || !image || !asset_texture || !material) {
        return 7;
    }
    const auto model = scene->create_model(mesh.value(), material.value());
    const auto camera = scene->create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera || !scene->set_local_transform(model.value(), elf3d::Transform{})) {
        return 8;
    }
    elf3d::PerspectiveCameraDescription camera_description =
        scene->perspective_camera(camera.value()).value();
    camera_description.near_plane = 0.05F;
    if (!scene->set_perspective_camera(camera.value(), camera_description) ||
        scene->perspective_camera(camera.value()).value().near_plane != 0.05F) {
        return 8;
    }

    elf3d::Float4x4 matrix;
    matrix.elements[12] = 2.0F;
    if (!scene->set_local_matrix(model.value(), matrix) ||
        scene->local_matrix(model.value()).value() != matrix ||
        !scene->set_entity_name(model.value(), "Public model") ||
        scene->entity_name(model.value()).value() != "Public model" ||
        !scene->world_bounds().is_valid || scene->statistics().model_entities != 1) {
        return 9;
    }
    const elf3d::Result<elf3d::EntityInfo> info = scene->entity_info(model.value());
    elf3d::Result<elf3d::SceneHierarchySnapshot> snapshot = scene->hierarchy_snapshot();
    if (!info || !info.value().renderable || !info.value().local_visible ||
        !info.value().effective_visible || !snapshot || snapshot.value().size() != 2 ||
        snapshot.value().hierarchy_revision() != scene->hierarchy_revision() ||
        snapshot.value().visibility_revision() != scene->visibility_revision()) {
        return 91;
    }
    bool found_public_model = false;
    for (std::size_t index = 0; index < snapshot.value().size(); ++index) {
        const elf3d::Result<elf3d::SceneHierarchyItem> item = snapshot.value().item(index);
        const elf3d::Result<std::string_view> name = snapshot.value().name(index);
        if (!item || !name) {
            return 92;
        }
        if (item.value().entity == model.value() && name.value() == "Public model" &&
            item.value().renderable) {
            found_public_model = true;
        }
    }
    if (!found_public_model || snapshot.value().item(snapshot.value().size()).error().code() !=
                                   elf3d::ErrorCode::invalid_hierarchy_snapshot_index) {
        return 93;
    }
    const std::uint64_t visibility_revision = scene->visibility_revision();
    if (!scene->set_entity_visible(model.value(), false) ||
        scene->entity_local_visibility(model.value()).value() ||
        scene->entity_effective_visibility(model.value()).value() ||
        scene->visible_bounds().is_valid || !scene->set_entity_visible(model.value(), false) ||
        scene->visibility_revision() == visibility_revision || !scene->show_all_entities() ||
        !scene->visible_bounds().is_valid) {
        return 94;
    }

    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "elf3d_public_load_test";
    std::error_code filesystem_error;
    std::filesystem::remove_all(directory, filesystem_error);
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        return 10;
    }
    const std::array<float, 9> imported_positions{0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                                  0.0F, 0.0F, 1.0F, 0.0F};
    {
        std::ofstream buffer{directory / "triangle.bin", std::ios::binary};
        buffer.write(reinterpret_cast<const char *>(imported_positions.data()),
                     static_cast<std::streamsize>(sizeof(imported_positions)));
        std::ofstream gltf{directory / "triangle.gltf", std::ios::binary};
        constexpr char json[] =
            R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"triangle.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"name":"Triangle","mesh":0}],"scenes":[{"nodes":[0]}]})json";
        gltf.write(json, static_cast<std::streamsize>(sizeof(json) - 1));
        if (!buffer || !gltf) {
            return 11;
        }
    }

    elf3d::Result<std::unique_ptr<elf3d::Scene>> loaded_result =
        assigned_engine.load_scene(directory / "triangle.gltf");
    if (!loaded_result ||
        loaded_result.value()->statistics() != elf3d::SceneStatistics{1, 1, 1, 1, 1, 3, 3, 1} ||
        !loaded_result.value()->world_bounds().is_valid) {
        return 12;
    }
    std::unique_ptr<elf3d::Scene> loaded = std::move(loaded_result).value();
    if (assigned_engine.load_scene(directory / "missing.gltf").error().code() !=
            elf3d::ErrorCode::source_file_not_found ||
        loaded->statistics().model_entities != 1) {
        return 13;
    }
    loaded.reset();
    std::filesystem::remove_all(directory, filesystem_error);

    std::unique_ptr<elf3d::Engine> lifetime_engine = std::make_unique<elf3d::Engine>();
    elf3d::Result<std::unique_ptr<elf3d::Scene>> late_scene_result =
        lifetime_engine->create_scene();
    if (!late_scene_result) {
        return 14;
    }
    std::unique_ptr<elf3d::Scene> late_scene = std::move(late_scene_result).value();
    lifetime_engine.reset();
    late_scene.reset();

    return 0;
}
