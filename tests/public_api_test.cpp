#include <elf3d/elf3d.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace {

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

void verify_compile_time_contracts() noexcept {
    static_assert(sizeof(elf3d::Error) <= 256);
    static_assert(noexcept(elf3d::Engine::create(elf3d::EngineConfiguration{})));
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
    static_assert(std::is_standard_layout_v<elf3d::ModelLoadOptions>);
    static_assert(std::is_standard_layout_v<elf3d::PerspectiveCameraDescription>);
    static_assert(std::is_standard_layout_v<elf3d::SamplerDescription>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormal>);
    static_assert(std::is_standard_layout_v<elf3d::VertexPositionNormalTexCoord>);
    static_assert(std::is_standard_layout_v<elf3d::Extent2D>);
    static_assert(!std::is_move_constructible_v<elf3d::Viewport>);
    static_assert(!std::is_move_assignable_v<elf3d::Viewport>);
    static_assert(!std::is_copy_constructible_v<elf3d::Viewport>);
    static_assert(std::is_move_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_copy_constructible_v<elf3d::SceneHierarchySnapshot>);
    static_assert(!std::is_convertible_v<elf3d::ImageHandle, elf3d::TextureAssetHandle>);

    static_assert(!std::is_move_constructible_v<elf3d::Engine>);
    static_assert(!std::is_move_assignable_v<elf3d::Engine>);
    static_assert(!std::is_move_constructible_v<elf3d::Scene>);
}

[[nodiscard]] bool has_inactive_pointer_input(const elf3d::ViewportInput& input) noexcept {
    return !input.is_hovered && !input.left_button_down && input.wheel_delta == 0.0F &&
           !input.space_down;
}

[[nodiscard]] bool has_inactive_motion_input(const elf3d::ViewportInput& input) noexcept {
    return !input.w_pressed && !input.s_pressed && !input.a_pressed && !input.d_pressed &&
           !input.q_pressed && !input.e_pressed;
}

[[nodiscard]] int verify_basic_values() {
    const elf3d::Float2 position{4.0F, 8.0F};
    const elf3d::Color4 color{0.1F, 0.2F, 0.3F, 1.0F};
    const elf3d::Extent2D extent{800, 600};
    if (position != elf3d::Float2{4.0F, 8.0F} || color != elf3d::Color4{0.1F, 0.2F, 0.3F, 1.0F} ||
        extent != elf3d::Extent2D{800, 600}) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int verify_default_input_values() {
    const elf3d::TextureHandle texture;
    const elf3d::NativeTextureView native_view;
    const elf3d::ViewportInput inactive_input;
    const elf3d::Ray3 default_ray;
    const elf3d::SelectionSettings selection_settings;
    const elf3d::SectionPlane section_plane;
    if (texture.is_valid() || native_view.is_valid() ||
        !has_inactive_pointer_input(inactive_input) || !has_inactive_motion_input(inactive_input) ||
        default_ray.direction != elf3d::Float3{0.0F, 0.0F, -1.0F} ||
        selection_settings.click_drag_threshold_pixels <= 0.0F || section_plane.enabled ||
        section_plane.retained_half_space != elf3d::PlaneHalfSpace::positive) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_default_clipping_values() {
    const elf3d::ClippingBox clipping_box;
    const elf3d::ClippingSnapshot clipping_snapshot;
    if (clipping_box.minimum != elf3d::Float3{-0.5F, -0.5F, -0.5F} ||
        clipping_box.maximum != elf3d::Float3{0.5F, 0.5F, 0.5F} || !clipping_box.enabled ||
        clipping_snapshot.box_count != 0 || !clipping_snapshot.helpers.visible) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_default_measurement_values() {
    const elf3d::DistanceMeasurementSettings settings;
    const elf3d::ProjectedViewportPoint projected_point;
    if (settings.display_unit != elf3d::LengthDisplayUnit::automatic_metric ||
        settings.depth_mode != elf3d::OverlayDepthMode::always_visible ||
        settings.line_thickness_pixels <= 0.0F || settings.marker_radius_pixels <= 0.0F ||
        projected_point.is_in_front || projected_point.is_inside_viewport) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_public_values() {
    const int basic = verify_basic_values();
    if (basic != 0) {
        return basic;
    }
    const int input = verify_default_input_values();
    if (input != 0) {
        return input;
    }
    const int clipping = verify_default_clipping_values();
    if (clipping != 0) {
        return clipping;
    }
    return verify_default_measurement_values();
}

[[nodiscard]] int verify_missing_loader() {
    const elf3d::EngineConfiguration missing_loader;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> create_result =
        elf3d::Engine::create(missing_loader);
    if (create_result ||
        create_result.error().code() != elf3d::ErrorCode::missing_graphics_procedure_loader) {
        return 5;
    }
    return 0;
}

struct PublicSceneFixture {
    std::unique_ptr<elf3d::Scene> scene;
    elf3d::EntityId model;
    elf3d::EntityId camera;
};

[[nodiscard]] int create_public_scene(elf3d::Engine& engine, PublicSceneFixture& fixture) {
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    if (!scene_result) {
        return 6;
    }
    fixture.scene = std::move(scene_result).value();
    return 0;
}

[[nodiscard]] int populate_public_scene(PublicSceneFixture& fixture) {
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = fixture.scene->create_mesh({vertices, indices});
    const std::array<std::byte, 4> image_pixel{
        {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
    const auto image =
        fixture.scene->create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, image_pixel});
    const auto asset_texture = fixture.scene->create_texture({image.value(), {}});
    elf3d::MaterialDescription description;
    description.base_color = {0.1F, 0.2F, 0.3F, 1.0F};
    description.base_color_texture = asset_texture.value();
    const auto material = fixture.scene->create_material(description);
    if (!mesh || !image || !asset_texture || !material) {
        return 7;
    }
    const auto model = fixture.scene->create_model_entity(mesh.value(), material.value());
    const auto camera =
        fixture.scene->create_perspective_camera_entity(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera ||
        !fixture.scene->set_local_transform(model.value(), elf3d::Transform{})) {
        return 8;
    }
    fixture.model = model.value();
    fixture.camera = camera.value();
    elf3d::PerspectiveCameraDescription camera_description =
        fixture.scene->perspective_camera_description(fixture.camera).value();
    camera_description.near_plane = 0.05F;
    if (!fixture.scene->set_perspective_camera_description(fixture.camera, camera_description) ||
        fixture.scene->perspective_camera_description(fixture.camera).value().near_plane != 0.05F) {
        return 8;
    }
    return 0;
}

[[nodiscard]] int verify_scene_transform(PublicSceneFixture& fixture) {
    elf3d::Float4x4 matrix;
    matrix.elements[12] = 2.0F;
    if (!fixture.scene->set_local_matrix(fixture.model, matrix) ||
        fixture.scene->local_matrix(fixture.model).value() != matrix ||
        !fixture.scene->set_entity_name(fixture.model, "Public model") ||
        fixture.scene->entity_name(fixture.model).value() != "Public model" ||
        !fixture.scene->world_bounds().has_value() ||
        fixture.scene->statistics().model_entities != 1) {
        return 9;
    }
    return 0;
}

[[nodiscard]] int verify_camera_role_errors(PublicSceneFixture& fixture) {
    const elf3d::Result<elf3d::PerspectiveCameraDescription> missing_camera =
        fixture.scene->perspective_camera_description(fixture.model);
    if (missing_camera || missing_camera.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return 95;
    }

    const elf3d::Result<void> set_missing_camera =
        fixture.scene->set_perspective_camera_description(fixture.model,
                                                          elf3d::PerspectiveCameraDescription{});
    if (set_missing_camera ||
        set_missing_camera.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return 96;
    }

    const elf3d::Result<elf3d::PerspectiveCameraDescription> invalid_entity =
        fixture.scene->perspective_camera_description(elf3d::EntityId{});
    if (invalid_entity || invalid_entity.error().code() != elf3d::ErrorCode::invalid_entity) {
        return 97;
    }
    return 0;
}

[[nodiscard]] bool has_expected_entity_info(const elf3d::Result<elf3d::EntityInfo>& info) {
    return info && info.value().renderable && info.value().local_visible &&
           info.value().effective_visible;
}

[[nodiscard]] bool
has_expected_snapshot_header(const elf3d::Result<elf3d::SceneHierarchySnapshot>& snapshot,
                             const elf3d::Scene& scene) {
    return snapshot && snapshot.value().size() == 2 &&
           snapshot.value().hierarchy_revision() == scene.hierarchy_revision() &&
           snapshot.value().visibility_revision() == scene.visibility_revision();
}

[[nodiscard]] int verify_snapshot_items(const elf3d::SceneHierarchySnapshot& snapshot,
                                        elf3d::EntityId model) {
    bool found_public_model = false;
    for (std::size_t index = 0; index < snapshot.size(); ++index) {
        const elf3d::Result<elf3d::SceneHierarchyItem> item = snapshot.item_at(index);
        const elf3d::Result<std::string_view> name = snapshot.name_at(index);
        if (!item || !name) {
            return 92;
        }
        if (item.value().entity == model && name.value() == "Public model" &&
            item.value().renderable) {
            found_public_model = true;
        }
    }
    if (!found_public_model || snapshot.item_at(snapshot.size()).error().code() !=
                                   elf3d::ErrorCode::invalid_hierarchy_snapshot_index) {
        return 93;
    }
    return 0;
}

[[nodiscard]] int verify_scene_hierarchy(const PublicSceneFixture& fixture) {
    const elf3d::Result<elf3d::EntityInfo> info = fixture.scene->entity_info(fixture.model);
    elf3d::Result<elf3d::SceneHierarchySnapshot> snapshot = fixture.scene->hierarchy_snapshot();
    if (!has_expected_entity_info(info) ||
        !has_expected_snapshot_header(snapshot, *fixture.scene)) {
        return 91;
    }
    return verify_snapshot_items(snapshot.value(), fixture.model);
}

[[nodiscard]] int verify_scene_visibility(PublicSceneFixture& fixture) {
    const std::uint64_t visibility_revision = fixture.scene->visibility_revision();
    if (!fixture.scene->set_entity_local_visibility(fixture.model, false) ||
        fixture.scene->entity_local_visibility(fixture.model).value() ||
        fixture.scene->entity_effective_visibility(fixture.model).value() ||
        fixture.scene->visible_bounds().has_value() ||
        !fixture.scene->set_entity_local_visibility(fixture.model, false) ||
        fixture.scene->visibility_revision() == visibility_revision ||
        !fixture.scene->show_all_entities() || !fixture.scene->visible_bounds().has_value()) {
        return 94;
    }
    return 0;
}

[[nodiscard]] int verify_scene_api(elf3d::Engine& engine) {
    PublicSceneFixture fixture;
    const int created = create_public_scene(engine, fixture);
    if (created != 0) {
        return created;
    }
    const int populated = populate_public_scene(fixture);
    if (populated != 0) {
        return populated;
    }
    const int transformed = verify_scene_transform(fixture);
    if (transformed != 0) {
        return transformed;
    }
    const int camera_roles = verify_camera_role_errors(fixture);
    if (camera_roles != 0) {
        return camera_roles;
    }
    const int hierarchy = verify_scene_hierarchy(fixture);
    if (hierarchy != 0) {
        return hierarchy;
    }
    return verify_scene_visibility(fixture);
}

[[nodiscard]] int create_import_files(const std::filesystem::path& directory) {
    std::error_code filesystem_error;
    std::filesystem::remove_all(directory, filesystem_error);
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        return 10;
    }
    const std::array<float, 9> positions{0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
    std::ofstream buffer{directory / "triangle.bin", std::ios::binary};
    buffer.write(reinterpret_cast<const char*>(positions.data()),
                 static_cast<std::streamsize>(sizeof(positions)));
    std::ofstream gltf{directory / "triangle.gltf", std::ios::binary};
    constexpr char json[] =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["KHR_lights_punctual"],"buffers":[{"uri":"triangle.bin","byteLength":36,"extras":{"tag":"buffer"}}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"name":"Triangle","mesh":0}],"scenes":[{"nodes":[0]}]})json";
    gltf.write(json, static_cast<std::streamsize>(sizeof(json) - 1));
    if (!buffer || !gltf) {
        return 11;
    }
    return 0;
}

[[nodiscard]] bool report_has_diagnostic(const elf3d::SceneLoadReport& report,
                                         elf3d::SceneLoadDiagnosticCategory category,
                                         elf3d::SceneLoadDiagnosticCode code) {
    for (std::size_t index = 0; index < report.diagnostic_count(); ++index) {
        const auto diagnostic = report.diagnostic(index);
        if (diagnostic && diagnostic.value().category == category &&
            diagnostic.value().code == code) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] int load_imported_scene(elf3d::Engine& engine, const std::filesystem::path& directory,
                                      std::unique_ptr<elf3d::Scene>& loaded) {
    elf3d::Result<elf3d::LoadedScene> loaded_result =
        engine.load_scene(path_to_utf8(directory / "triangle.gltf"));
    if (!loaded_result) {
        return 12;
    }
    const elf3d::SceneLoadReport& report = loaded_result.value().report;
    if (loaded_result.value().scene->statistics() !=
            elf3d::SceneStatistics{1, 1, 1, 1, 1, 3, 3, 1} ||
        !loaded_result.value().scene->world_bounds().has_value() || report.diagnostic_count() < 2 ||
        !report_has_diagnostic(report, elf3d::SceneLoadDiagnosticCategory::light,
                               elf3d::SceneLoadDiagnosticCode::ignored_lights) ||
        !report_has_diagnostic(report, elf3d::SceneLoadDiagnosticCategory::metadata,
                               elf3d::SceneLoadDiagnosticCode::metadata_not_preserved)) {
        return 12;
    }
    loaded = std::move(loaded_result).value().scene;
    return 0;
}

[[nodiscard]] int verify_load_failures(elf3d::Engine& engine,
                                       const std::filesystem::path& directory,
                                       const elf3d::Scene& loaded) {
    const elf3d::Result<elf3d::LoadedScene> scene_only_load =
        engine.load_scene(path_to_utf8(directory / "triangle.gltf"));
    if (!scene_only_load) {
        return 121;
    }
    if (engine.load_scene(path_to_utf8(directory / "missing.gltf")).error().code() !=
            elf3d::ErrorCode::source_file_not_found ||
        loaded.statistics().model_entities != 1) {
        return 13;
    }
    return 0;
}

[[nodiscard]] int verify_export_round_trip(elf3d::Engine& engine,
                                           const std::filesystem::path& directory,
                                           const elf3d::Scene& loaded) {
    const std::filesystem::path exported_path = directory / "saved.glb";
    if (!loaded.export_loaded_document(path_to_utf8(exported_path))) {
        return 131;
    }
    const elf3d::Result<elf3d::LoadedScene> saved_load =
        engine.load_scene(path_to_utf8(exported_path));
    if (!saved_load || saved_load.value().scene->statistics() != loaded.statistics()) {
        return 132;
    }
    return 0;
}

[[nodiscard]] int verify_file_round_trip(elf3d::Engine& engine) {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "elf3d_public_load_test";
    const int files = create_import_files(directory);
    if (files != 0) {
        return files;
    }
    std::unique_ptr<elf3d::Scene> loaded;
    const int imported = load_imported_scene(engine, directory, loaded);
    if (imported != 0) {
        return imported;
    }
    const int failures = verify_load_failures(engine, directory, *loaded);
    if (failures != 0) {
        return failures;
    }
    const int exported = verify_export_round_trip(engine, directory, *loaded);
    if (exported != 0) {
        return exported;
    }
    loaded.reset();
    std::error_code filesystem_error;
    std::filesystem::remove_all(directory, filesystem_error);
    return 0;
}

[[nodiscard]] int verify_late_scene_lifetime(const elf3d::EngineConfiguration& configuration) {
    elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
        elf3d::Engine::create(configuration);
    if (!engine_result) {
        return 141;
    }
    std::unique_ptr<elf3d::Engine> engine = std::move(engine_result).value();
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine->create_scene();
    if (!scene_result) {
        return 14;
    }
    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();
    engine.reset();
    scene.reset();
    return 0;
}

[[nodiscard]] int verify_engine_owner_identity(const elf3d::EngineConfiguration& configuration) {
    auto first_engine_result = elf3d::Engine::create(configuration);
    auto second_engine_result = elf3d::Engine::create(configuration);
    if (!first_engine_result || !second_engine_result) {
        return 151;
    }
    std::unique_ptr<elf3d::Engine> first_engine = std::move(first_engine_result).value();
    std::unique_ptr<elf3d::Engine> second_engine = std::move(second_engine_result).value();
    auto first_scene = first_engine->create_scene();
    auto second_scene = second_engine->create_scene();
    if (!first_scene || !second_scene || first_scene.value()->id() == second_scene.value()->id() ||
        first_scene.value()->id().debug_value() != second_scene.value()->id().debug_value()) {
        return 152;
    }
    return 0;
}

} // namespace

int main() {
    verify_compile_time_contracts();

    elf3d::EngineConfiguration cpu_configuration;
    cpu_configuration.graphics_backend = elf3d::GraphicsBackend::none;
    elf3d::Result<std::unique_ptr<elf3d::Engine>> cpu_engine_result =
        elf3d::Engine::create(cpu_configuration);
    if (!cpu_engine_result) {
        return 51;
    }
    std::unique_ptr<elf3d::Engine> engine_owner = std::move(cpu_engine_result).value();
    elf3d::Engine& engine = *engine_owner;
    const int values = verify_public_values();
    if (values != 0) {
        return values;
    }
    const int loader = verify_missing_loader();
    if (loader != 0) {
        return loader;
    }
    const int scene_api = verify_scene_api(engine);
    if (scene_api != 0) {
        return scene_api;
    }
    const int round_trip = verify_file_round_trip(engine);
    if (round_trip != 0) {
        return round_trip;
    }
    const int late_lifetime = verify_late_scene_lifetime(cpu_configuration);
    if (late_lifetime != 0) {
        return late_lifetime;
    }
    return verify_engine_owner_identity(cpu_configuration);
}
