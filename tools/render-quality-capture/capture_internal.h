#pragma once

#include <array>
#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::capture_tool {

inline constexpr std::string_view procedural_model = "procedural-material";

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

struct CaptureScene final {
    std::unique_ptr<elf3d::Scene> scene;
    std::unique_ptr<elf3d::Viewport> viewport;
    elf3d::EntityId camera;
    elf3d::PerspectiveCameraDescription projection;
    elf3d::Float4x4 camera_matrix;
    std::string model_alias;
    std::string model_hash;
};

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
[[nodiscard]] std::optional<Options> parse_options(int count, char** values);
void print_usage();
[[nodiscard]] elf3d::Float4x4
fitted_camera_matrix(const elf3d::Bounds3& bounds, elf3d::Extent2D extent,
                     const elf3d::PerspectiveCameraDescription& projection, bool opposite) noexcept;
struct MaterialCaptureImages final {
    std::array<std::vector<unsigned char>, 4> frames;
};

[[nodiscard]] elf3d::Result<void> validate_material_capture(const Options& options,
                                                            const MaterialCaptureImages& images);

} // namespace elf3d::capture_tool
