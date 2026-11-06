#include <elf3d/model.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace elf3d_examples {

// Capture alongside the ID and Document revision. This is a current source
// table position; a compact output file may use different indices.
[[nodiscard]] elf3d::Result<std::size_t> source_mesh_position(elf3d::DocumentView document,
                                                              elf3d::MeshId mesh) noexcept
{
    return document.index_of(mesh);
}

// Owns only names, typed IDs and relationships. A GUI retains this owner and
// calls advance(source, 128) once per service frame instead of looping here.
// The source is never borrowed across calls; mutation invalidates advancement.
[[nodiscard]] elf3d::Result<elf3d::DocumentHierarchySnapshot>
inspect_document_tables(const elf3d::Document& document) noexcept
{
    return document.hierarchy_snapshot();
}

// An editor also retains supported resources and nodes outside authored scenes.
[[nodiscard]] elf3d::Result<elf3d::LoadedDocument>
load_for_editing(std::string_view path_utf8) noexcept
{
    return elf3d::load_document(path_utf8,
                                {.preserve_unreferenced_data = true, .allow_empty_geometry = true});
}

// A fresh reconstruction can reuse an immutable image payload without borrowing
// the source's lifetime. The new image record has no inherited opaque metadata.
[[nodiscard]] elf3d::Result<elf3d::ImageId>
append_snapshot_image(elf3d::Document& target, elf3d::DocumentView source,
                      elf3d::ImageId source_image) noexcept
{
    return target.create_image_snapshot(source, source_image);
}

// The application has built all typed objects/resources and audited every opaque
// reference against these explicit maps. This call cannot reinterpret JSON
// indices or infer provenance for a generated/merged object.
[[nodiscard]] elf3d::Result<elf3d::Document>
finish_audited_reconstruction(elf3d::Document target, elf3d::DocumentView source,
                              const elf3d::ModelMetadataMapping& mapping) noexcept
{
    const auto attached = target.copy_verified_metadata(source, mapping);
    if (!attached) {
        return attached.error();
    }
    return target;
}

// A candidate owns its tables/geometry; unchanged images survive in either owner.
[[nodiscard]] elf3d::Result<elf3d::Document>
replace_snapshot_image(elf3d::DocumentView source, std::size_t image_index,
                       const elf3d::ModelImageDescription& replacement) noexcept
{
    auto result = elf3d::snapshot_document(source);
    if (!result)
        return result.error();
    auto candidate = std::move(result).value();
    const auto image = candidate.image_at(image_index);
    if (!image)
        return image.error();
    const auto changed = candidate.replace_image(image.value().id, replacement);
    if (!changed)
        return changed.error();
    return candidate;
}

[[nodiscard]] elf3d::Result<elf3d::ModelWriteReport>
validate_and_save_document(std::string_view input_path_utf8, std::string_view output_path_utf8,
                           const elf3d::ModelLoadOptions& load_options = {},
                           const elf3d::ModelWriteOptions& write_options = {}) noexcept
{
    elf3d::Result<elf3d::LoadedDocument> loaded_result =
        elf3d::load_document(input_path_utf8, load_options);
    if (!loaded_result) {
        return loaded_result.error();
    }

    elf3d::LoadedDocument loaded = std::move(loaded_result).value();
    // Validation is noexcept; allocation exhaustion follows the SDK fatal policy.
    const elf3d::DocumentValidationReport validation =
        elf3d::validate_document(loaded.document.view());
    if (validation.has_errors()) {
        return elf3d::Error{elf3d::ErrorCode::gltf_validation_failed,
                            "The loaded document failed validation"};
    }

    return elf3d::save_document(output_path_utf8, loaded.document.view(), write_options);
}

// Build an independent candidate using public Model only. The caller decides
// whether to commit it; failures leave the original Document unchanged.
[[nodiscard]] elf3d::Result<elf3d::Document>
copy_with_node_transform(elf3d::DocumentView source, std::size_t node_ordinal,
                         const elf3d::Float4x4& matrix) noexcept
{
    auto copied = elf3d::clone_document(source);
    if (!copied) {
        return copied.error();
    }
    auto candidate = std::move(copied).value();
    const auto node = candidate.node_at(node_ordinal);
    if (!node) {
        return node.error();
    }
    const auto changed = candidate.set_node_matrix(node.value().id, matrix);
    if (!changed) {
        return changed.error();
    }
    return candidate;
}

// Use only after an application-specific schema audit proves that all metadata
// references survive the geometry change. Object/resource tables are unchanged.
[[nodiscard]] elf3d::Result<void> replace_audited_geometry(elf3d::Document& candidate,
                                                           elf3d::PrimitiveId primitive,
                                                           elf3d::PrimitiveData geometry) noexcept
{
    return candidate.replace_primitive(
        primitive, std::move(geometry),
        elf3d::PrimitiveMetadataPolicy::preserve_verified_references);
}

} // namespace elf3d_examples
