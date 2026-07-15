#include <elf3d/model/detail/document_storage.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d {
namespace model::detail {

class DocumentValidation final {
  public:
    [[nodiscard]] static DocumentValidationReport validate(DocumentView document) {
        DocumentValidationReport report;
        const Document* owner = document.document_;
        if (owner == nullptr || owner->storage_ == nullptr) {
            add_error(report, DocumentDiagnosticCode::invalid_reference, "Document view is empty");
            return report;
        }

        const Document::Storage& storage = *owner->storage_;
        if (storage.preserved_metadata_stale) {
            report.diagnostics.push_back(DocumentDiagnostic{
                DocumentDiagnosticSeverity::warning,
                DocumentDiagnosticCode::stale_preserved_metadata,
                "Preserved glTF extras and unknown extensions are stale after document mutation"});
        }
        validate_default_scene(storage, report);
        validate_scene_roots(storage, report);
        validate_nodes(storage, report);
        validate_mesh_primitives(storage, report);
        validate_primitives(storage, report);
        validate_materials(storage, report);
        validate_images(storage, report);
        validate_textures(storage, report);
        validate_samplers(storage, report);
        return report;
    }

  private:
    static void add_error(DocumentValidationReport& report, DocumentDiagnosticCode code,
                          std::string_view message) {
        report.diagnostics.push_back(
            DocumentDiagnostic{DocumentDiagnosticSeverity::error, code, std::string{message}});
    }

    static void validate_default_scene(const Document::Storage& storage,
                                       DocumentValidationReport& report) {
        if (storage.default_scene.has_value() && !storage.scene(*storage.default_scene)) {
            add_error(report, DocumentDiagnosticCode::invalid_reference,
                      "Document default scene is invalid");
        }
    }

    static void validate_scene_roots(const Document::Storage& storage,
                                     DocumentValidationReport& report) {
        for (const Document::Storage::SceneRecord& scene_record : storage.scenes) {
            for (const NodeId root : scene_record.roots) {
                const Result<const Document::Storage::NodeRecord*> root_record = storage.node(root);
                if (!root_record) {
                    add_error(report, DocumentDiagnosticCode::invalid_reference,
                              "Scene root references an invalid node");
                    continue;
                }
                if (root_record.value()->parent.has_value()) {
                    add_error(report, DocumentDiagnosticCode::invalid_reference,
                              "Scene root node has a parent");
                }
            }
        }
    }

    static void validate_node_hierarchy(const Document::Storage& storage,
                                        DocumentValidationReport& report) {
        std::vector<std::uint8_t> states(storage.nodes.size(), 0U);
        std::vector<std::size_t> path;
        path.reserve(storage.nodes.size());
        for (std::size_t start = 0; start < storage.nodes.size(); ++start) {
            if (states[start] == 2U) {
                continue;
            }
            path.clear();
            std::size_t current = start;
            while (states[current] == 0U) {
                states[current] = 1U;
                path.push_back(current);
                const std::optional<NodeId> parent = storage.nodes[current].parent;
                if (!parent.has_value()) {
                    break;
                }
                const Result<const Document::Storage::NodeRecord*> parent_record =
                    storage.node(*parent);
                if (!parent_record) {
                    break;
                }
                current = static_cast<std::size_t>(
                    model::detail::DocumentHandleAccess::value(*parent) - 1U);
                if (states[current] == 1U) {
                    add_error(report, DocumentDiagnosticCode::hierarchy_cycle,
                              "Document node hierarchy contains a cycle");
                    break;
                }
                if (states[current] == 2U) {
                    break;
                }
            }
            for (const std::size_t index : path) {
                states[index] = 2U;
            }
        }
    }

    static void validate_nodes(const Document::Storage& storage, DocumentValidationReport& report) {
        for (const Document::Storage::NodeRecord& node_record : storage.nodes) {
            if (!finite(node_record.local_matrix)) {
                add_error(report, DocumentDiagnosticCode::invalid_transform,
                          "Node local matrix contains non-finite values");
            }
            if (node_record.mesh.has_value() && !storage.mesh(*node_record.mesh)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Node references an invalid mesh");
            }
            if (node_record.parent.has_value() && !storage.node(*node_record.parent)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Node references an invalid parent");
            }
            if (node_record.perspective_camera.has_value() &&
                !valid_perspective_camera(*node_record.perspective_camera)) {
                add_error(report, DocumentDiagnosticCode::invalid_asset,
                          "Node contains an invalid perspective camera");
            }
        }
        validate_node_hierarchy(storage, report);
    }

    static void validate_mesh_primitives(const Document::Storage& storage,
                                         DocumentValidationReport& report) {
        for (const Document::Storage::MeshRecord& mesh_record : storage.meshes) {
            for (const PrimitiveId primitive_id : mesh_record.primitives) {
                const Result<const Document::Storage::PrimitiveRecord*> primitive_record =
                    storage.primitive(primitive_id);
                if (!primitive_record) {
                    add_error(report, DocumentDiagnosticCode::invalid_reference,
                              "Mesh references an invalid primitive");
                    continue;
                }
                if (primitive_record.value()->mesh != mesh_record.id) {
                    add_error(report, DocumentDiagnosticCode::invalid_reference,
                              "Primitive owner mesh does not match its containing mesh");
                }
            }
        }
    }

    static void validate_primitives(const Document::Storage& storage,
                                    DocumentValidationReport& report) {
        for (const Document::Storage::PrimitiveRecord& primitive_record : storage.primitives) {
            if (!storage.mesh(primitive_record.mesh)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Primitive references an invalid mesh");
            }
            if (!storage.material(primitive_record.material)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Primitive references an invalid material");
            }
            const Result<void> data_result = validate_primitive_data(primitive_record.data.view());
            if (!data_result) {
                add_error(report, DocumentDiagnosticCode::invalid_geometry,
                          data_result.error().message());
            }
        }
    }

    static void validate_materials(const Document::Storage& storage,
                                   DocumentValidationReport& report) {
        for (const Document::Storage::MaterialRecord& material_record : storage.materials) {
            const ModelMaterialDescription& description = material_record.description;
            if (!storage.valid_material_textures(description)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Material references an invalid texture");
            }
            if (!valid_material_mappings(description)) {
                add_error(report, DocumentDiagnosticCode::invalid_asset,
                          "Material contains an invalid texture mapping");
            }
            if (!valid_material_factors(description)) {
                add_error(report, DocumentDiagnosticCode::invalid_asset,
                          "Material contains invalid factors or colors");
            }
        }
    }

    static void validate_images(const Document::Storage& storage,
                                DocumentValidationReport& report) {
        for (const Document::Storage::ImageRecord& image_record : storage.images) {
            const ModelImageDescription description{
                image_record.width,
                image_record.height,
                image_record.format,
                std::span<const std::byte>{image_record.pixels},
                image_record.source_mime_type,
                std::span<const std::byte>{image_record.source_bytes}};
            const Result<void> image_result = validate_image_description(description);
            if (!image_result) {
                add_error(report, DocumentDiagnosticCode::invalid_asset,
                          image_result.error().message());
            }
        }
    }

    static void validate_textures(const Document::Storage& storage,
                                  DocumentValidationReport& report) {
        for (const Document::Storage::TextureRecord& texture_record : storage.textures) {
            if (!storage.image(texture_record.description.image) ||
                !storage.sampler(texture_record.description.sampler)) {
                add_error(report, DocumentDiagnosticCode::invalid_reference,
                          "Texture references an invalid image or sampler");
            }
        }
    }

    static void validate_samplers(const Document::Storage& storage,
                                  DocumentValidationReport& report) {
        for (const Document::Storage::SamplerRecord& sampler_record : storage.samplers) {
            const ModelSamplerDescription& description = sampler_record.description;
            if (!valid_wrap(description.wrap_u) || !valid_wrap(description.wrap_v) ||
                !valid_filter(description.min_filter) ||
                !valid_mag_filter(description.mag_filter)) {
                add_error(report, DocumentDiagnosticCode::invalid_asset,
                          "Sampler contains an invalid wrap or filter value");
            }
        }
    }
};

} // namespace model::detail

DocumentValidationReport validate_document(DocumentView document) {
    return model::detail::DocumentValidation::validate(document);
}

} // namespace elf3d
