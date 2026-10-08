#include "capture_internal.h"

#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace elf3d::capture_tool {

constexpr elf3d::Color4 default_viewer_clear_color{213.0F / 255.0F, 227.0F / 255.0F,
                                                   240.0F / 255.0F, 1.0F};

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

[[nodiscard]] std::string bytes_to_hex(std::span<const unsigned char> bytes)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const unsigned char byte : bytes) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

[[nodiscard]] std::optional<std::string> sha256_file(const std::filesystem::path& path)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_bytes = 0;
    DWORD hash_bytes = 0;
    DWORD copied = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_bytes),
                          sizeof(object_bytes), &copied, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_bytes),
                          sizeof(hash_bytes), &copied, 0) < 0) {
        if (algorithm != nullptr) {
            BCryptCloseAlgorithmProvider(algorithm, 0);
        }
        return std::nullopt;
    }

    std::vector<unsigned char> object(object_bytes);
    std::vector<unsigned char> digest(hash_bytes);
    if (BCryptCreateHash(algorithm, &hash, object.data(), object_bytes, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return std::nullopt;
    }

    std::ifstream stream{path, std::ios::binary};
    std::vector<unsigned char> buffer(1024U * 1024U);
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) {
            stream.setstate(std::ios::badbit);
        }
    }

    const bool success = stream.eof() && BCryptFinishHash(hash, digest.data(), hash_bytes, 0) >= 0;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return success ? std::optional<std::string>{bytes_to_hex(digest)} : std::nullopt;
}

struct SphereMesh final {
    std::vector<elf3d::VertexPositionNormal> vertices;
    std::vector<std::uint32_t> indices;
};

[[nodiscard]] SphereMesh make_sphere()
{
    constexpr std::uint32_t longitude_count = 48;
    constexpr std::uint32_t latitude_count = 24;
    constexpr float pi = 3.14159265359F;
    SphereMesh mesh;
    mesh.vertices.reserve((longitude_count + 1) * (latitude_count + 1));
    for (std::uint32_t latitude = 0; latitude <= latitude_count; ++latitude) {
        const float polar = pi * static_cast<float>(latitude) / static_cast<float>(latitude_count);
        const float y = std::cos(polar);
        const float radius = std::sin(polar);
        for (std::uint32_t longitude = 0; longitude <= longitude_count; ++longitude) {
            const float azimuth =
                2.0F * pi * static_cast<float>(longitude) / static_cast<float>(longitude_count);
            const elf3d::Float3 normal{radius * std::cos(azimuth), y, radius * std::sin(azimuth)};
            mesh.vertices.push_back({normal, normal});
        }
    }
    for (std::uint32_t latitude = 0; latitude < latitude_count; ++latitude) {
        for (std::uint32_t longitude = 0; longitude < longitude_count; ++longitude) {
            const std::uint32_t row = longitude_count + 1;
            const std::uint32_t top_left = latitude * row + longitude;
            const std::uint32_t bottom_left = top_left + row;
            mesh.indices.insert(mesh.indices.end(), {top_left, bottom_left, top_left + 1,
                                                     top_left + 1, bottom_left, bottom_left + 1});
        }
    }
    return mesh;
}

[[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::Scene>>
create_material_scene(elf3d::Engine& engine, elf3d::EntityId& camera)
{
    auto scene_result = engine.create_scene();
    if (!scene_result) {
        return scene_result.error();
    }

    std::unique_ptr<elf3d::Scene> scene = std::move(scene_result).value();
    const SphereMesh sphere = make_sphere();
    const auto mesh = scene->create_mesh({sphere.vertices, sphere.indices});
    if (!mesh) {
        return mesh.error();
    }
    constexpr std::array<float, 4> x_positions{{-3.0F, -1.0F, 1.0F, 3.0F}};
    constexpr std::array<float, 4> metallic{{0.0F, 0.0F, 1.0F, 1.0F}};
    constexpr std::array<float, 4> roughness{{0.8F, 0.25F, 0.08F, 0.65F}};
    constexpr std::array<elf3d::Color4, 4> colors{{
        {1.0F, 1.0F, 1.0F, 1.0F},
        {0.55F, 0.55F, 0.55F, 1.0F},
        {0.85F, 0.58F, 0.22F, 1.0F},
        {0.72F, 0.76F, 0.8F, 1.0F},
    }};
    for (std::size_t index = 0; index < x_positions.size(); ++index) {
        elf3d::MaterialDescription description;
        description.base_color = colors[index];
        description.metallic_factor = metallic[index];
        description.roughness_factor = roughness[index];
        const auto material = scene->create_material(description);
        if (!material) {
            return material.error();
        }

        const auto model = scene->create_model_entity(mesh.value(), material.value());
        if (!model) {
            return model.error();
        }
        elf3d::Transform transform;
        transform.translation = {x_positions[index], 0.0F, 0.0F};
        const auto positioned = scene->set_local_transform(model.value(), transform);
        if (!positioned) {
            return positioned.error();
        }
    }

    const auto camera_result = scene->create_perspective_camera_entity({});
    if (!camera_result) {
        return camera_result.error();
    }
    camera = camera_result.value();
    return scene;
}

[[nodiscard]] std::optional<elf3d::EntityId> first_authored_camera(elf3d::Scene& scene)
{
    auto snapshot = scene.hierarchy_snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < snapshot.value().size(); ++index) {
        const auto item = snapshot.value().item_at(index);
        if (item && item.value().has_camera) {
            return item.value().entity;
        }
    }
    return std::nullopt;
}

[[nodiscard]] elf3d::Float4x4
fitted_camera_matrix(const elf3d::Bounds3& bounds, const elf3d::Extent2D extent,
                     const elf3d::PerspectiveCameraDescription& camera, bool back) noexcept
{
    const elf3d::Float3 center{(bounds.minimum.x + bounds.maximum.x) * 0.5F,
                               (bounds.minimum.y + bounds.maximum.y) * 0.5F,
                               (bounds.minimum.z + bounds.maximum.z) * 0.5F};
    const float width = bounds.maximum.x - bounds.minimum.x;
    const float height = bounds.maximum.y - bounds.minimum.y;
    const float depth = bounds.maximum.z - bounds.minimum.z;
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const float half_tangent = std::tan(camera.vertical_field_of_view_radians * 0.5F);
    const float distance =
        std::max(height * 0.5F / half_tangent, width * 0.5F / (half_tangent * aspect)) * 1.1F +
        depth * 0.5F;
    elf3d::Float4x4 matrix;
    if (back) {
        matrix.elements[0] = -1.0F;
        matrix.elements[10] = -1.0F;
    }
    matrix.elements[12] = center.x;
    matrix.elements[13] = center.y;
    matrix.elements[14] = back ? bounds.minimum.z - distance : bounds.maximum.z + distance;
    return matrix;
}

[[nodiscard]] elf3d::Result<CaptureScene> create_capture_scene(const Options& options,
                                                               elf3d::Engine& engine)
{
    CaptureScene capture;
    const bool procedural = path_to_utf8(options.model) == procedural_model;
    if (procedural) {
        auto scene_result = create_material_scene(engine, capture.camera);
        if (!scene_result) {
            return scene_result.error();
        }
        capture.scene = std::move(scene_result).value();
        capture.model_alias = std::string{procedural_model};
        capture.model_hash = "procedural-material-v1-standard";
    } else {
        auto loaded = engine.load_scene(path_to_utf8(options.model));
        if (!loaded) {
            return loaded.error();
        }
        capture.scene = std::move(loaded).value().scene;
        capture.model_alias = path_to_utf8(options.model.filename());
        const auto hash = sha256_file(options.model);
        if (!hash) {
            return elf3d::Error{elf3d::ErrorCode::source_file_read_failed,
                                "Could not compute the model SHA-256"};
        }
        capture.model_hash = *hash;
    }
    auto viewport = engine.create_viewport(options.extent);
    if (!viewport) {
        return viewport.error();
    }
    capture.viewport = std::move(viewport).value();
    if ((options.camera_mode == CameraMode::authored_first ||
         options.camera_mode == CameraMode::authored_opposite) &&
        !procedural) {
        const auto authored = first_authored_camera(*capture.scene);
        if (!authored) {
            return elf3d::Error{elf3d::ErrorCode::entity_has_no_camera,
                                "The scene has no authored perspective camera"};
        }
        capture.camera = *authored;
    } else if (!capture.camera.is_valid()) {
        const auto camera = capture.scene->create_perspective_camera_entity({});
        if (!camera) {
            return camera.error();
        }
        capture.camera = camera.value();
    }
    auto projection = capture.scene->perspective_camera_description(capture.camera);
    if (!projection) {
        return projection.error();
    }
    capture.projection = projection.value();
    if (options.camera_mode == CameraMode::authored_opposite) {
        elf3d::OrbitNavigationSettings settings;
        settings.focus_depth_anchor_enabled = false;
        const auto settings_result = capture.viewport->set_navigation_settings(settings);
        const auto synchronized =
            capture.viewport->synchronize_navigation(*capture.scene, capture.camera);
        if (!settings_result) {
            return settings_result.error();
        }
        if (!synchronized) {
            return synchronized.error();
        }
        elf3d::NavigationInput input;
        input.pointer_hovered = true;
        input.region_focused = true;
        input.orbit_down = true;
        input.eye_orbit_modifier_down = true;
        const auto pressed =
            capture.viewport->update_navigation(*capture.scene, capture.camera, input);
        if (!pressed) {
            return pressed.error();
        }
        input.pointer_position_pixels = {628.318542F, 0.0F};
        input.pointer_delta_pixels = {628.318542F, 0.0F};
        const auto orbited =
            capture.viewport->update_navigation(*capture.scene, capture.camera, input);
        if (!orbited) {
            return orbited.error();
        }
        input.pointer_position_pixels = {1256.63708F, 0.0F};
        const auto moved =
            capture.viewport->update_navigation(*capture.scene, capture.camera, input);
        if (!moved) {
            return moved.error();
        }

        const auto matrix = capture.scene->local_matrix(capture.camera);
        if (!matrix) {
            return matrix.error();
        }
        capture.camera_matrix = matrix.value();
    } else if (options.camera_mode == CameraMode::fit_front ||
               options.camera_mode == CameraMode::fit_back || procedural) {
        const auto bounds = capture.scene->visible_bounds();
        if (!bounds) {
            return elf3d::Error{elf3d::ErrorCode::empty_scene_geometry,
                                "Cannot fit a camera to an empty scene"};
        }
        capture.camera_matrix = fitted_camera_matrix(*bounds, options.extent, capture.projection,
                                                     options.camera_mode == CameraMode::fit_back);
        const auto positioned =
            capture.scene->set_local_matrix(capture.camera, capture.camera_matrix);
        if (!positioned) {
            return positioned.error();
        }
    } else if (options.camera_mode == CameraMode::matrix) {
        capture.camera_matrix = *options.camera_matrix;
        const auto positioned =
            capture.scene->set_local_matrix(capture.camera, capture.camera_matrix);
        if (!positioned) {
            return positioned.error();
        }
    } else {
        const auto matrix = capture.scene->local_matrix(capture.camera);
        if (!matrix) {
            return matrix.error();
        }
        capture.camera_matrix = matrix.value();
    }
    return capture;
}

[[nodiscard]] bool write_png(const std::filesystem::path& path, elf3d::Extent2D extent,
                             const std::vector<unsigned char>& pixels)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"wb") != 0 || file == nullptr) {
        return false;
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = extent.width;
    image.height = extent.height;
    image.format = PNG_FORMAT_RGBA;
    const int written = png_image_write_to_stdio(&image, file, 0, pixels.data(), 0, nullptr);
    const int closed = std::fclose(file);
    return written != 0 && closed == 0;
}

[[nodiscard]] std::string json_escape(std::string_view value)
{
    std::string result;
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    return result;
}

[[nodiscard]] bool write_metadata(const Options& options, const CaptureScene& capture,
                                  const elf3d::GraphicsContextSnapshot& graphics)
{
    std::error_code error;
    std::filesystem::create_directories(options.metadata.parent_path(), error);
    if (error) {
        return false;
    }

    std::ofstream stream{options.metadata, std::ios::trunc};
    if (!stream) {
        return false;
    }

    stream << std::fixed << std::setprecision(6) << "{\n  \"schema\": 1,\n  \"model_alias\": \""
           << json_escape(capture.model_alias) << "\",\n  \"sha256\": \"" << capture.model_hash
           << "\",\n  \"camera_mode\": \"" << options.camera_name
           << "\",\n  \"camera_matrix_column_major\": [";
    for (std::size_t index = 0; index < capture.camera_matrix.elements.size(); ++index) {
        stream << (index == 0 ? "" : ", ") << capture.camera_matrix.elements[index];
    }
    stream << "],\n  \"projection\": {\"vertical_fov_radians\": "
           << capture.projection.vertical_field_of_view_radians
           << ", \"near\": " << capture.projection.near_plane
           << ", \"far\": " << capture.projection.far_plane
           << "},\n  \"viewport\": {\"width\": " << options.extent.width
           << ", \"height\": " << options.extent.height << ", \"clear_color\": ["
           << default_viewer_clear_color.red << ", " << default_viewer_clear_color.green << ", "
           << default_viewer_clear_color.blue << ", " << default_viewer_clear_color.alpha
           << "]},\n  \"lighting\": {"
           << "\"direction\": [" << options.lighting.direction.x << ", "
           << options.lighting.direction.y << ", " << options.lighting.direction.z << "], "
           << "\"directional_intensity\": " << options.lighting.diffuse_intensity << ", "
           << "\"legacy_ambient\": " << options.lighting.ambient_intensity << ", "
           << "\"environment_intensity\": " << options.environment.intensity << ", "
           << "\"environment_rotation_radians\": " << options.environment.rotation_radians
           << "},\n  \"display\": {\"exposure_ev\": " << options.display.exposure_ev
           << ", \"tone_mapping\": \""
           << (options.display.tone_mapping == elf3d::ToneMappingMode::none          ? "none"
               : options.display.tone_mapping == elf3d::ToneMappingMode::pbr_neutral ? "pbr_neutral"
                                                                                     : "standard")
           << "\"},\n  \"shading\": \""
           << (options.shading == elf3d::RenderShadingMode::unlit ? "unlit" : "standard")
           << "\",\n  \"renderer\": {\"vendor\": \"" << json_escape(graphics.vendor_name)
           << "\", \"device\": \"" << json_escape(graphics.device_name) << "\", \"opengl\": \""
           << json_escape(graphics.api_version) << "\"},\n  \"build_revision\": \""
           << ELF3D_CAPTURE_BUILD_REVISION << "\"\n}\n";
    return static_cast<bool>(stream);
}

class CaptureApplication final : public elf3d::Application {
  public:
    explicit CaptureApplication(Options options) : options_(std::move(options))
    {
    }

    elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override
    {
        try {
            auto created = create_capture_scene(options_, context.engine());
            if (!created) {
                return created.error();
            }
            scene_ = std::move(created).value();
            // Snapshot strings are borrowed only for this run_application call.
            graphics_ = context.graphics_context();
            scene_.viewport->set_clear_color(default_viewer_clear_color);
            scene_.viewport->set_basic_lighting(options_.lighting);
            scene_.viewport->set_environment_lighting(options_.environment);
            scene_.viewport->set_display_transform(options_.display);
            scene_.viewport->set_render_shading_mode(options_.shading);
            return {};
        } catch (...) {
            elf3d::fatal_error("Capture application startup failed unexpectedly");
        }
    }

    elf3d::Result<void> update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        try {
            if (!context.previous_frame_statistics()) {
                return {};
            }
            auto& pixels = images_.frames[stage_];
            pixels.resize(static_cast<std::size_t>(options_.extent.width) * options_.extent.height *
                          4U);
            const auto readback = scene_.viewport->read_color_pixels(pixels);
            if (!readback) {
                return readback.error();
            }
            if (!options_.validate_material || stage_ == 3) {
                if (options_.validate_material) {
                    const auto validated = validate_material_capture(options_, images_);
                    if (!validated) {
                        return validated.error();
                    }
                }
                if (!write_png(options_.output, options_.extent, images_.frames[0]) ||
                    !write_metadata(options_, scene_, graphics_)) {
                    return elf3d::Error{elf3d::ErrorCode::source_file_write_failed,
                                        "Could not write capture output"};
                }
                std::cout << "Wrote " << path_to_utf8(options_.output) << " and "
                          << path_to_utf8(options_.metadata) << '\n';
                context.request_exit();
                return {};
            }
            ++stage_;
            return configure_stage();
        } catch (...) {
            elf3d::fatal_error("Capture application update failed unexpectedly");
        }
    }

    elf3d::Result<void> build_ui(elf3d::ApplicationUiContext& context) noexcept override
    {
        elf3d::ViewportRenderOptions options;
        options.shading_mode = options_.shading;
        return context.queue_viewport_render(*scene_.viewport, *scene_.scene, scene_.camera,
                                             options);
    }
    void stop(elf3d::ApplicationContext&) noexcept override
    {
        scene_.viewport.reset();
        scene_.scene.reset();
        graphics_ = {};
    }

  private:
    elf3d::Result<void> configure_stage() noexcept
    {
        if (stage_ < 3) {
            auto lighting = options_.lighting;
            lighting.ambient_intensity = 0.0F;
            lighting.diffuse_intensity = 0.0F;
            auto environment = options_.environment;
            environment.rotation_radians = stage_ == 1 ? 0.0F : 0.174532925F;
            scene_.viewport->set_basic_lighting(lighting);
            scene_.viewport->set_environment_lighting(environment);
            return {};
        }
        scene_.viewport->set_basic_lighting(options_.lighting);
        scene_.viewport->set_environment_lighting(options_.environment);
        const auto bounds = scene_.scene->visible_bounds();
        if (!bounds) {
            return elf3d::Error{elf3d::ErrorCode::empty_scene_geometry,
                                "Capture scene has no bounds"};
        }
        return scene_.scene->set_local_matrix(
            scene_.camera, fitted_camera_matrix(*bounds, options_.extent, scene_.projection, true));
    }

    Options options_;
    CaptureScene scene_;
    MaterialCaptureImages images_;
    elf3d::GraphicsContextSnapshot graphics_;
    std::size_t stage_ = 0;
};

[[nodiscard]] int capture(const Options& options)
{
    if (path_to_utf8(options.model) != procedural_model &&
        !std::filesystem::is_regular_file(options.model)) {
        std::cerr << "Model does not exist\n";
        return 3;
    }
    CaptureApplication application{options};
    elf3d::ApplicationOptions application_options;
    application_options.title = "Elf3D render quality capture";
    application_options.initial_window_extent = options.extent;
    application_options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    application_options.presentation_mode = elf3d::PresentationMode::immediate;
    const auto result = elf3d::run_application(application_options, application);
    if (!result) {
        const auto code = result.error().code();
        if (code == elf3d::ErrorCode::graphics_initialization_failed ||
            code == elf3d::ErrorCode::graphics_context_unavailable ||
            code == elf3d::ErrorCode::unsupported_graphics_version) {
            std::cerr << "Hidden OpenGL 4.1 context initialization failed\n";
            return 77;
        }
        std::cerr << result.error().message() << '\n';
        return 7;
    }
    return result.value();
}

} // namespace elf3d::capture_tool

int main(int count, char** values)
{
    using namespace elf3d::capture_tool;
    const auto options = parse_options(count, values);
    if (!options) {
        print_usage();
        return 2;
    }
    return capture(*options);
}
