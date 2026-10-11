#include "diagnostics.h"

#include <elf3d/core/diagnostics.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace elf3d::cmd {
namespace {

class ConsoleSink final : public LogSink {
  public:
    void write(const LogRecord& record) noexcept override
    {
        const auto severity = record.level == LogLevel::error     ? "error"
                              : record.level == LogLevel::warning ? "warning"
                                                                  : "info";
        std::cerr << severity;
        if (record.code.has_value()) {
            // ErrorCode uses the default int underlying representation.
            std::cerr << " [code=" << static_cast<int>(*record.code) << ']';
        }
        std::cerr << ": " << record.message << '\n';
    }
};

void print_diagnostic(std::string_view stage, LogLevel level, std::string_view code,
                      std::string_view message, std::optional<std::string_view> context)
{
    std::string text{stage};
    text += " [";
    text += code;
    text += "]: ";
    text += message;
    if (context.has_value()) {
        text += " (";
        text += *context;
        text += ')';
    }
    ConsoleSink sink;
    sink.write(LogRecord{level, std::nullopt, text});
}

[[nodiscard]] std::string_view code_name(ModelLoadDiagnosticCode code) noexcept
{
    // This closed public enum uses sequential values from zero.
    constexpr std::array<std::string_view, 17> names{"generated_normals",
                                                     "degenerate_geometry",
                                                     "missing_texture_coordinates",
                                                     "unsupported_optional_extension",
                                                     "material_fallback",
                                                     "normal_map_fallback",
                                                     "camera_fallback",
                                                     "ignored_lights",
                                                     "ignored_animation",
                                                     "ignored_skin",
                                                     "ignored_morph_targets",
                                                     "ignored_instancing",
                                                     "skipped_invalid_transform",
                                                     "texture_fallback",
                                                     "skipped_unsupported_primitive",
                                                     "metadata_not_preserved",
                                                     "repaired_signed_buffer_layout"};
    static_assert(static_cast<std::size_t>(ModelLoadDiagnosticCode::repaired_signed_buffer_layout) +
                      1U ==
                  names.size());
    const auto index = static_cast<std::size_t>(code);
    return index < names.size() ? names[index] : "unknown";
}

[[nodiscard]] std::string_view code_name(DocumentDiagnosticCode code) noexcept
{
    switch (code) {
    case DocumentDiagnosticCode::invalid_reference:
        return "invalid_reference";
    case DocumentDiagnosticCode::invalid_geometry:
        return "invalid_geometry";
    case DocumentDiagnosticCode::invalid_asset:
        return "invalid_asset";
    case DocumentDiagnosticCode::invalid_transform:
        return "invalid_transform";
    case DocumentDiagnosticCode::hierarchy_cycle:
        return "hierarchy_cycle";
    case DocumentDiagnosticCode::stale_preserved_metadata:
        return "stale_preserved_metadata";
    }
    return "unknown";
}

[[nodiscard]] std::string_view code_name(ModelWriteDiagnosticCode code) noexcept
{
    switch (code) {
    case ModelWriteDiagnosticCode::image_reencoded_as_png:
        return "image_reencoded_as_png";
    case ModelWriteDiagnosticCode::preserved_metadata_dropped_after_mutation:
        return "preserved_metadata_dropped_after_mutation";
    case ModelWriteDiagnosticCode::matrix_rotation_cleaned:
        return "matrix_rotation_cleaned";
    }
    return "unknown";
}

} // namespace

void print_error(std::string_view stage, const Error& error)
{
    std::string text{stage};
    text += ": ";
    text += error.message();
    ConsoleSink sink;
    sink.write(LogRecord{LogLevel::error, error.code(), text});
}

void print_report(const ModelLoadReport& report)
{
    for (const auto& diagnostic : report.diagnostics) {
        const auto level = diagnostic.severity == ModelLoadDiagnosticSeverity::warning
                               ? LogLevel::warning
                               : LogLevel::info;
        const auto context = diagnostic.source_context.has_value()
                                 ? std::optional<std::string_view>{*diagnostic.source_context}
                                 : std::nullopt;
        print_diagnostic("load", level, code_name(diagnostic.code), diagnostic.message, context);
    }
}

void print_report(const DocumentValidationReport& report)
{
    for (const auto& diagnostic : report.diagnostics) {
        const auto level =
            diagnostic.severity == DocumentDiagnosticSeverity::error     ? LogLevel::error
            : diagnostic.severity == DocumentDiagnosticSeverity::warning ? LogLevel::warning
                                                                         : LogLevel::info;
        print_diagnostic("validate", level, code_name(diagnostic.code), diagnostic.message,
                         std::nullopt);
    }
}

void print_report(const ModelWriteReport& report)
{
    for (const auto& diagnostic : report.diagnostics) {
        const auto level = diagnostic.severity == ModelWriteDiagnosticSeverity::warning
                               ? LogLevel::warning
                               : LogLevel::info;
        const auto context = diagnostic.source_context.has_value()
                                 ? std::optional<std::string_view>{*diagnostic.source_context}
                                 : std::nullopt;
        print_diagnostic("save", level, code_name(diagnostic.code), diagnostic.message, context);
    }
}

void print_usage()
{
    std::cout << "Usage: elf3d_cmd --input <file.gltf|file.glb> --output <file.gltf|file.glb>\n"
                 "                 --rotate <x> <y> <z>\n"
                 "       elf3d_cmd --help\n"
                 "Angles are degrees, X then Y then Z, about the model bounds center. glTF writes "
                 "sidecars.\n"
                 "Existing output artifacts are replaced. Rotation preserves metadata on supported "
                 "Document objects.\n";
}

} // namespace elf3d::cmd
