#include "exporter_file_writer.h"

#include <utility>

namespace elf3d::gltf::exporter_detail {
namespace {
void clear_node_metadata(NodeView& node) noexcept
{
    node.metadata = {};
    node.camera_properties.metadata = {};
    node.camera_properties.projection_metadata = {};
}

void clear_preserved_metadata(ExportData& result) noexcept
{
    result.root_metadata = {};
    result.asset_metadata = {};
    for (DocumentSceneView& scene : result.scenes) {
        scene.metadata = {};
    }
    for (NodeView& node : result.nodes) {
        clear_node_metadata(node);
    }
    for (MeshView& mesh : result.meshes) {
        mesh.metadata = {};
    }
    for (PrimitiveView& primitive : result.primitives) {
        primitive.metadata = {};
    }
    for (MaterialView& material : result.materials) {
        material.metadata = {};
    }
    for (ImageView& image : result.images) {
        image.metadata = {};
    }
    for (TextureView& texture : result.textures) {
        texture.metadata = {};
    }
    for (SamplerView& sampler : result.samplers) {
        sampler.metadata = {};
    }
}

} // namespace

void apply_export_metadata_policy(ExportData& document) noexcept
{
    if (!document.preserved_metadata_dropped)
        return;
    clear_preserved_metadata(document);
}

[[nodiscard]] ModelWriteReport build_write_report(std::span<const EncodedImageInfo> encoded_images,
                                                  bool preserved_metadata_dropped,
                                                  std::size_t cleaned_matrices)
{
    ModelWriteReport report;
    if (cleaned_matrices != 0U) {
        report.diagnostics.push_back(ModelWriteDiagnostic{
            ModelWriteDiagnosticSeverity::information, ModelWriteDiagnosticCategory::output,
            ModelWriteDiagnosticCode::matrix_rotation_cleaned,
            "Orthonormal matrix bases were cleaned within a 1e-7 absolute tolerance; translation "
            "was unchanged",
            std::to_string(cleaned_matrices) + " matrices"});
    }
    for (std::size_t index = 0; index < encoded_images.size(); ++index) {
        if (!encoded_images[index].reencoded) {
            continue;
        }
        report.diagnostics.push_back(ModelWriteDiagnostic{
            ModelWriteDiagnosticSeverity::information, ModelWriteDiagnosticCategory::image,
            ModelWriteDiagnosticCode::image_reencoded_as_png,
            "Document image was encoded as PNG for glTF output", "image " + std::to_string(index)});
    }
    if (preserved_metadata_dropped) {
        report.diagnostics.push_back(ModelWriteDiagnostic{
            ModelWriteDiagnosticSeverity::warning, ModelWriteDiagnosticCategory::metadata,
            ModelWriteDiagnosticCode::preserved_metadata_dropped_after_mutation,
            "Preserved glTF extras and unknown extensions were omitted after document mutation",
            std::nullopt});
    }
    return report;
}

} // namespace elf3d::gltf::exporter_detail
