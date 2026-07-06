#ifndef ELF3D_SCENE_LOAD_H
#define ELF3D_SCENE_LOAD_H

#include <optional>
#include <string>
#include <vector>

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
};

struct SceneLoadDiagnostic {
    SceneLoadDiagnosticSeverity severity = SceneLoadDiagnosticSeverity::warning;
    SceneLoadDiagnosticCategory category = SceneLoadDiagnosticCategory::geometry;
    SceneLoadDiagnosticCode code = SceneLoadDiagnosticCode::material_fallback;
    std::string message;
    std::optional<std::string> source_context;
};

struct SceneLoadReport {
    std::vector<SceneLoadDiagnostic> diagnostics;

    [[nodiscard]] bool has_warnings() const noexcept {
        for (const SceneLoadDiagnostic& diagnostic : diagnostics) {
            if (diagnostic.severity == SceneLoadDiagnosticSeverity::warning) {
                return true;
            }
        }
        return false;
    }
};

} // namespace elf3d

#endif
