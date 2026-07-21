#ifndef ELF3D_SCENE_LOAD_H
#define ELF3D_SCENE_LOAD_H

#include <elf3d/core/api.h>
#include <elf3d/core/result.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

namespace elf3d {

enum class SceneLoadDiagnosticSeverity {
    information,
    warning,
};

enum class SceneLoadDiagnosticCategory {
    geometry,
    material,
    texture,
    extension,
    camera,
    light,
    animation,
    metadata,
    scene,
};

enum class SceneLoadDiagnosticCode {
    generated_normals,
    degenerate_geometry,
    missing_texture_coordinates,
    unsupported_optional_extension,
    material_fallback,
    normal_map_fallback,
    camera_fallback,
    ignored_lights,
    ignored_animation,
    ignored_skin,
    ignored_morph_targets,
    ignored_instancing,
    skipped_invalid_transform,
    texture_fallback,
    skipped_unsupported_primitive,
    metadata_not_preserved,
};

struct SceneLoadDiagnosticView {
    SceneLoadDiagnosticSeverity severity = SceneLoadDiagnosticSeverity::warning;
    SceneLoadDiagnosticCategory category = SceneLoadDiagnosticCategory::geometry;
    SceneLoadDiagnosticCode code = SceneLoadDiagnosticCode::material_fallback;
    std::string_view message;
    std::optional<std::string_view> source_context;
};

#if defined(_MSC_VER)
#pragma warning(push)
// The exported special members keep unique_ptr operations inside the DLL.
#pragma warning(disable : 4251)
#endif
class ELF3D_API SceneLoadReport final {
  public:
    SceneLoadReport() noexcept;
    ~SceneLoadReport() noexcept;

    SceneLoadReport(const SceneLoadReport&) = delete;
    SceneLoadReport& operator=(const SceneLoadReport&) = delete;
    SceneLoadReport(SceneLoadReport&&) noexcept;
    SceneLoadReport& operator=(SceneLoadReport&&) noexcept;

    [[nodiscard]] std::size_t diagnostic_count() const noexcept;
    [[nodiscard]] Result<SceneLoadDiagnosticView> diagnostic(std::size_t index) const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;

  private:
    friend class Engine;

    class Impl;
    explicit SceneLoadReport(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
