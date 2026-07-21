#include <elf3d/elf3d.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace elf3d_examples {

[[nodiscard]] elf3d::Result<elf3d::LoadedScene>
load_and_validate_report(elf3d::Engine& engine, std::string_view path_utf8,
                         const elf3d::ModelLoadOptions& options = {}) noexcept {
    elf3d::Result<elf3d::LoadedScene> loaded_result = engine.load_scene(path_utf8, options);
    if (!loaded_result) {
        return loaded_result.error();
    }

    elf3d::LoadedScene loaded = std::move(loaded_result).value();
    for (std::size_t index = 0; index < loaded.report.diagnostic_count(); ++index) {
        const elf3d::Result<elf3d::SceneLoadDiagnosticView> diagnostic =
            loaded.report.diagnostic(index);
        if (!diagnostic) {
            return diagnostic.error();
        }
        const elf3d::SceneLoadDiagnosticCode code = diagnostic.value().code;
        (void)code;
    }
    return std::move(loaded);
}

} // namespace elf3d_examples
