#include <elf3d/embed/runtime.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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

namespace {

constexpr std::string_view procedural_model = "procedural-material";

enum class CameraMode { authored_first, authored_opposite, fit_front, fit_back, matrix };

struct Options final {
    std::filesystem::path model;
    elf3d::Extent2D extent;
    CameraMode camera_mode = CameraMode::authored_first;
    std::string camera_name;
    std::optional<elf3d::Float4x4> camera_matrix;
    std::filesystem::path output;
    std::filesystem::path metadata;
    elf3d::RenderShadingMode shading = elf3d::RenderShadingMode::standard;
    elf3d::BasicLighting lighting;
    elf3d::EnvironmentLighting environment;
    elf3d::DisplayTransform display;
    bool validate_material = false;
};

struct Presence final {
    bool model = false;
    bool extent = false;
    bool camera = false;
    bool output = false;
    bool metadata = false;
};

class GlfwRuntime final {
  public:
    ~GlfwRuntime() {
        if (initialized_) {
            glfwTerminate();
        }
    }
    [[nodiscard]] bool initialize() noexcept {
        initialized_ = glfwInit() == GLFW_TRUE;
        return initialized_;
    }

  private:
    bool initialized_ = false;
};

class Window final {
  public:
    explicit Window(GLFWwindow* value) noexcept : value_(value) {}
    ~Window() {
        if (value_ != nullptr) {
            glfwDestroyWindow(value_);
        }
    }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    [[nodiscard]] GLFWwindow* get() const noexcept {
        return value_;
    }

  private:
    GLFWwindow* value_ = nullptr;
};

struct CaptureScene final {
    std::unique_ptr<elf3d::EmbeddedRuntime> runtime;
    std::unique_ptr<elf3d::Scene> scene;
    std::unique_ptr<elf3d::Viewport> viewport;
    elf3d::EntityId camera;
    elf3d::PerspectiveCameraDescription projection;
    elf3d::Float4x4 camera_matrix;
    std::string model_alias;
    std::string model_hash;
};

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    const auto* begin = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

[[nodiscard]] std::optional<float> float_value(std::string_view value) noexcept {
    float parsed = 0.0F;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
                   std::isfinite(parsed)
               ? std::optional<float>{parsed}
               : std::nullopt;
}

[[nodiscard]] std::optional<std::uint32_t> unsigned_value(std::string_view value) noexcept {
    std::uint32_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size()
               ? std::optional<std::uint32_t>{parsed}
               : std::nullopt;
}

[[nodiscard]] std::optional<elf3d::Extent2D> extent_value(std::string_view value) noexcept {
    const std::size_t separator = value.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto width = unsigned_value(value.substr(0, separator));
    const auto height = unsigned_value(value.substr(separator + 1));
    return width && height && *width != 0 && *height != 0
               ? std::optional<elf3d::Extent2D>{{*width, *height}}
               : std::nullopt;
}

[[nodiscard]] std::optional<elf3d::Float4x4> matrix_value(std::string_view value) noexcept {
    elf3d::Float4x4 matrix;
    std::size_t begin = 0;
    for (std::size_t index = 0; index < matrix.elements.size(); ++index) {
        const std::size_t end = value.find(',', begin);
        const std::string_view token = value.substr(begin, end - begin);
        const auto parsed = float_value(token);
        if (!parsed || (index + 1 != matrix.elements.size() && end == std::string_view::npos) ||
            (index + 1 == matrix.elements.size() && end != std::string_view::npos)) {
            return std::nullopt;
        }
        matrix.elements[index] = *parsed;
        begin = end == std::string_view::npos ? value.size() : end + 1;
    }
    return matrix;
}

template <typename Value>
[[nodiscard]] bool set_once(Value& destination, bool& present, Value value) {
    if (present) {
        return false;
    }
    destination = std::move(value);
    present = true;
    return true;
}

[[nodiscard]] bool parse_required(std::string_view name, std::string_view value, Options& options,
                                  Presence& presence) {
    if (name == "--model") {
        return set_once(options.model, presence.model, path_from_utf8(value));
    }
    if (name == "--extent") {
        const auto extent = extent_value(value);
        return extent && set_once(options.extent, presence.extent, *extent);
    }
    if (name == "--output") {
        return set_once(options.output, presence.output, path_from_utf8(value));
    }
    if (name == "--metadata") {
        return set_once(options.metadata, presence.metadata, path_from_utf8(value));
    }
    if (name == "--camera" && !presence.camera) {
        if (value == "authored-first") {
            options.camera_mode = CameraMode::authored_first;
        } else if (value == "authored-opposite") {
            options.camera_mode = CameraMode::authored_opposite;
        } else if (value == "fit-front") {
            options.camera_mode = CameraMode::fit_front;
        } else if (value == "fit-back") {
            options.camera_mode = CameraMode::fit_back;
        } else if (value == "matrix") {
            options.camera_mode = CameraMode::matrix;
        } else {
            return false;
        }
        options.camera_name = value;
        presence.camera = true;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_render_setting(std::string_view name, std::string_view value,
                                        Options& options) {
    const auto number = float_value(value);
    if (name == "--matrix") {
        options.camera_matrix = matrix_value(value);
        return options.camera_matrix.has_value();
    }
    if (name == "--shading") {
        if (value == "standard") {
            options.shading = elf3d::RenderShadingMode::standard;
            return true;
        }
        if (value == "unlit") {
            options.shading = elf3d::RenderShadingMode::unlit;
            return true;
        }
        return false;
    }
    if (name == "--tone") {
        if (value == "pbr-neutral") {
            options.display.tone_mapping = elf3d::ToneMappingMode::pbr_neutral;
            return true;
        }
        if (value == "none") {
            options.display.tone_mapping = elf3d::ToneMappingMode::none;
            return true;
        }
        return false;
    }
    if (name == "--validate-material") {
        if (value == "true") {
            options.validate_material = true;
            return true;
        }
        if (value == "false") {
            options.validate_material = false;
            return true;
        }
        return false;
    }
    if (!number) {
        return false;
    }
    if (name == "--ambient") {
        options.lighting.ambient_intensity = *number;
    } else if (name == "--directional") {
        options.lighting.diffuse_intensity = *number;
    } else if (name == "--environment") {
        options.environment.intensity = *number;
    } else if (name == "--rotation") {
        options.environment.rotation_radians = *number;
    } else if (name == "--exposure") {
        options.display.exposure_ev = *number;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<Options> parse_options(int count, char** values) {
    Options options;
    Presence presence;
    for (int index = 1; index < count; index += 2) {
        if (index + 1 >= count || values[index] == nullptr || values[index + 1] == nullptr) {
            return std::nullopt;
        }
        const std::string_view name{values[index]};
        const std::string_view value{values[index + 1]};
        if (!parse_required(name, value, options, presence) &&
            !parse_render_setting(name, value, options)) {
            return std::nullopt;
        }
    }
    if (!presence.model || !presence.extent || !presence.camera || !presence.output ||
        !presence.metadata ||
        (options.camera_mode == CameraMode::matrix) != options.camera_matrix.has_value()) {
        return std::nullopt;
    }
    return options;
}

void print_usage() {
    std::cerr << "Usage: elf3d_render_quality_capture --model <path|procedural-material> "
                 "--extent <width>x<height> "
                 "--camera authored-first|authored-opposite|fit-front|fit-back|matrix "
                 "[--matrix <16-column-major-values>] --output <png> --metadata <json> "
                 "[--shading standard|unlit] [--ambient <value>] [--directional <value>] "
                 "[--environment <value>] [--rotation <radians>] [--exposure <ev>] "
                 "[--tone pbr-neutral|none] [--validate-material true|false]\n";
}

elf3d::EmbeddedGraphicsProcedure load_opengl_procedure(const char* name) noexcept {
    return glfwGetProcAddress(name);
}

[[nodiscard]] std::string bytes_to_hex(std::span<const unsigned char> bytes) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const unsigned char byte : bytes) {
        stream << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return stream.str();
}

[[nodiscard]] std::optional<std::string> sha256_file(const std::filesystem::path& path) {
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

[[nodiscard]] SphereMesh make_sphere() {
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
create_material_scene(elf3d::Engine& engine, elf3d::EntityId& camera) {
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

[[nodiscard]] std::optional<elf3d::EntityId> first_authored_camera(elf3d::Scene& scene) {
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
                     const elf3d::PerspectiveCameraDescription& camera, bool back) noexcept {
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

[[nodiscard]] elf3d::Result<CaptureScene> create_capture_scene(const Options& options) {
    auto runtime_result = elf3d::EmbeddedRuntime::create({load_opengl_procedure});
    if (!runtime_result) {
        return runtime_result.error();
    }
    CaptureScene capture;
    capture.runtime = std::move(runtime_result).value();
    elf3d::Engine& engine = capture.runtime->engine();
    const bool procedural = path_to_utf8(options.model) == procedural_model;
    if (procedural) {
        auto scene_result = create_material_scene(engine, capture.camera);
        if (!scene_result) {
            return scene_result.error();
        }
        capture.scene = std::move(scene_result).value();
        capture.model_alias = std::string{procedural_model};
        capture.model_hash = "procedural-material-v1";
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
                             const std::vector<unsigned char>& pixels) {
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

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::string result;
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    return result;
}

[[nodiscard]] bool write_metadata(const Options& options, const CaptureScene& capture) {
    std::error_code error;
    std::filesystem::create_directories(options.metadata.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream stream{options.metadata, std::ios::trunc};
    if (!stream) {
        return false;
    }
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
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
           << ", \"height\": " << options.extent.height << "},\n  \"lighting\": {"
           << "\"direction\": [" << options.lighting.direction.x << ", "
           << options.lighting.direction.y << ", " << options.lighting.direction.z << "], "
           << "\"directional_intensity\": " << options.lighting.diffuse_intensity << ", "
           << "\"legacy_ambient\": " << options.lighting.ambient_intensity << ", "
           << "\"environment_intensity\": " << options.environment.intensity << ", "
           << "\"environment_rotation_radians\": " << options.environment.rotation_radians
           << "},\n  \"display\": {\"exposure_ev\": " << options.display.exposure_ev
           << ", \"tone_mapping\": \""
           << (options.display.tone_mapping == elf3d::ToneMappingMode::none ? "none"
                                                                            : "pbr_neutral")
           << "\"},\n  \"shading\": \""
           << (options.shading == elf3d::RenderShadingMode::unlit ? "unlit" : "standard")
           << "\",\n  \"renderer\": {\"vendor\": \""
           << json_escape(vendor == nullptr ? "unavailable" : vendor) << "\", \"device\": \""
           << json_escape(renderer == nullptr ? "unavailable" : renderer) << "\", \"opengl\": \""
           << json_escape(version == nullptr ? "unavailable" : version)
           << "\"},\n  \"build_revision\": \"" << ELF3D_CAPTURE_BUILD_REVISION << "\"\n}\n";
    return static_cast<bool>(stream);
}

[[nodiscard]] int initialize_graphics(const Options& options, GlfwRuntime& glfw,
                                      std::unique_ptr<Window>& window) {
    if (!glfw.initialize()) {
        return 4;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = std::make_unique<Window>(glfwCreateWindow(
        static_cast<int>(options.extent.width), static_cast<int>(options.extent.height),
        "Elf3D render quality capture", nullptr, nullptr));
    if (window->get() == nullptr) {
        return 5;
    }
    glfwMakeContextCurrent(window->get());
    glfwSwapInterval(0);
    return gladLoadGL(load_opengl_procedure) != 0 && GLAD_GL_VERSION_4_1 != 0 ? 0 : 6;
}

[[nodiscard]] elf3d::Result<std::vector<unsigned char>> read_resolved_pixels(const Options& options,
                                                                             CaptureScene& scene) {
    const auto native = scene.runtime->native_texture_view(scene.viewport->color_texture());
    if (!native || native.value().api != elf3d::NativeGraphicsApi::opengl) {
        return native ? elf3d::Error{elf3d::ErrorCode::backend_mismatch,
                                     "Resolved texture is not OpenGL"}
                      : native.error();
    }
    glFinish();
    const std::size_t row_bytes = static_cast<std::size_t>(options.extent.width) * 4U;
    std::vector<unsigned char> bottom_up(row_bytes * options.extent.height);
    std::vector<unsigned char> top_down(bottom_up.size());
    GLint previous_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(native.value().value));
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bottom_up.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    if (glGetError() != GL_NO_ERROR) {
        return elf3d::Error{elf3d::ErrorCode::gpu_texture_upload_failed,
                            "Resolved texture readback failed"};
    }
    for (std::uint32_t row = 0; row < options.extent.height; ++row) {
        const std::size_t source =
            static_cast<std::size_t>(options.extent.height - 1U - row) * row_bytes;
        const std::size_t destination = static_cast<std::size_t>(row) * row_bytes;
        std::copy_n(bottom_up.begin() + static_cast<std::ptrdiff_t>(source), row_bytes,
                    top_down.begin() + static_cast<std::ptrdiff_t>(destination));
    }
    return top_down;
}

[[nodiscard]] double luminance(const unsigned char* pixel) noexcept {
    return (0.2126 * static_cast<double>(pixel[0]) + 0.7152 * static_cast<double>(pixel[1]) +
            0.0722 * static_cast<double>(pixel[2])) /
           255.0;
}

[[nodiscard]] std::vector<double> region_luminance(const std::vector<unsigned char>& pixels,
                                                   elf3d::Extent2D extent, int center_x,
                                                   int center_y, int radius) {
    std::vector<double> values;
    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            const int dx = x - center_x;
            const int dy = y - center_y;
            if (x < 0 || y < 0 || x >= static_cast<int>(extent.width) ||
                y >= static_cast<int>(extent.height) || dx * dx + dy * dy > radius * radius) {
                continue;
            }
            const std::size_t offset =
                (static_cast<std::size_t>(y) * extent.width + static_cast<std::size_t>(x)) * 4U;
            values.push_back(luminance(pixels.data() + offset));
        }
    }
    std::sort(values.begin(), values.end());
    return values;
}

[[nodiscard]] double percentile(const std::vector<double>& sorted, double fraction) noexcept {
    if (sorted.empty()) {
        return 0.0;
    }
    const std::size_t index =
        static_cast<std::size_t>(std::round(fraction * static_cast<double>(sorted.size() - 1U)));
    return sorted[std::min(index, sorted.size() - 1U)];
}

[[nodiscard]] double fraction_above(const std::vector<double>& values, double threshold) noexcept {
    return values.empty() ? 0.0
                          : static_cast<double>(std::count_if(
                                values.begin(), values.end(),
                                [threshold](double value) { return value > threshold; })) /
                                static_cast<double>(values.size());
}

[[nodiscard]] elf3d::Result<void>
validate_material_capture(const Options& options, CaptureScene& scene,
                          const std::vector<unsigned char>& front_pixels) {
    if (path_to_utf8(options.model) != procedural_model ||
        options.camera_mode != CameraMode::fit_front || options.extent.width != 1280U ||
        options.extent.height != 720U || options.shading != elf3d::RenderShadingMode::standard) {
        return elf3d::Error{
            elf3d::ErrorCode::invalid_argument,
            "Material validation requires procedural-material fit-front at 1280x720"};
    }
    const auto bounds = scene.scene->visible_bounds();
    if (!bounds) {
        return elf3d::Error{elf3d::ErrorCode::empty_scene_geometry,
                            "Material validation scene has no visible bounds"};
    }
    const elf3d::Float4x4 back_matrix =
        fitted_camera_matrix(*bounds, options.extent, scene.projection, true);
    const auto positioned = scene.scene->set_local_matrix(scene.camera, back_matrix);
    if (!positioned) {
        return positioned.error();
    }
    const auto rendered = scene.viewport->render(*scene.scene, scene.camera);
    if (!rendered) {
        return rendered.error();
    }
    auto back_pixels_result = read_resolved_pixels(options, scene);
    if (!back_pixels_result) {
        return back_pixels_result.error();
    }
    const auto white_front = region_luminance(front_pixels, options.extent, 333, 360, 92);
    const auto white_back =
        region_luminance(back_pixels_result.value(), options.extent, 946, 360, 92);
    const auto polished = region_luminance(front_pixels, options.extent, 741, 360, 92);
    const auto rough = region_luminance(front_pixels, options.extent, 946, 360, 92);
    const double white_front_median = percentile(white_front, 0.5);
    const double white_back_median = percentile(white_back, 0.5);
    const double white_p99 = percentile(white_front, 0.99);
    const double polished_median = percentile(polished, 0.5);
    const double polished_p99 = percentile(polished, 0.99);
    const double rough_median = percentile(rough, 0.5);
    const double rough_p99 = percentile(rough, 0.99);
    const bool passes = white_front_median >= 0.55 && white_front_median <= 0.90 &&
                        white_back_median >= 0.25 &&
                        white_front_median / white_back_median <= 2.5 && white_p99 < 0.995 &&
                        polished_median > 0.08 && polished_p99 - polished_median >= 0.25 &&
                        fraction_above(polished, 0.75) >= 0.01 &&
                        rough_p99 - rough_median < polished_p99 - polished_median &&
                        fraction_above(rough, 0.55) > fraction_above(polished, 0.55);
    std::cout << std::fixed << std::setprecision(6)
              << "material_metrics white_front_median=" << white_front_median
              << " white_back_median=" << white_back_median
              << " front_back_ratio=" << white_front_median / white_back_median
              << " white_p99=" << white_p99 << " polished_median=" << polished_median
              << " polished_contrast=" << polished_p99 - polished_median
              << " polished_fraction_above_075=" << fraction_above(polished, 0.75)
              << " rough_contrast=" << rough_p99 - rough_median << '\n';
    return passes ? elf3d::Result<void>{}
                  : elf3d::Result<void>{elf3d::Error{
                        elf3d::ErrorCode::draw_submission_failed,
                        "Procedural material capture did not satisfy the neutral studio gates"}};
}

[[nodiscard]] int capture(const Options& options) {
    const bool procedural = path_to_utf8(options.model) == procedural_model;
    if (!procedural && !std::filesystem::is_regular_file(options.model)) {
        std::cerr << "Model does not exist\n";
        return 3;
    }
    GlfwRuntime glfw;
    std::unique_ptr<Window> window;
    const int graphics = initialize_graphics(options, glfw, window);
    if (graphics != 0) {
        std::cerr << "Hidden OpenGL 4.1 context initialization failed\n";
        return graphics;
    }
    auto capture_result = create_capture_scene(options);
    if (!capture_result) {
        std::cerr << capture_result.error().message() << '\n';
        return 7;
    }
    CaptureScene scene = std::move(capture_result).value();
    scene.viewport->set_basic_lighting(options.lighting);
    scene.viewport->set_environment_lighting(options.environment);
    scene.viewport->set_display_transform(options.display);
    scene.viewport->set_render_shading_mode(options.shading);
    const auto rendered = scene.viewport->render(*scene.scene, scene.camera);
    if (!rendered) {
        std::cerr << rendered.error().message() << '\n';
        return 8;
    }
    auto pixels_result = read_resolved_pixels(options, scene);
    if (!pixels_result) {
        std::cerr << pixels_result.error().message() << '\n';
        return 9;
    }
    const std::vector<unsigned char>& top_down = pixels_result.value();
    if (options.validate_material) {
        const auto validated = validate_material_capture(options, scene, top_down);
        if (!validated) {
            std::cerr << validated.error().message() << '\n';
            return 10;
        }
    }
    if (!write_png(options.output, options.extent, top_down) || !write_metadata(options, scene)) {
        std::cerr << "Could not write capture output\n";
        return 10;
    }
    std::cout << "Wrote " << path_to_utf8(options.output) << " and "
              << path_to_utf8(options.metadata) << '\n';
    return 0;
}

} // namespace

int main(int count, char** values) {
    const auto options = parse_options(count, values);
    if (!options) {
        print_usage();
        return 2;
    }
    return capture(*options);
}
