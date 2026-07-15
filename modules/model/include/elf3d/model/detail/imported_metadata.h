#ifndef ELF3D_MODEL_DETAIL_IMPORTED_METADATA_H
#define ELF3D_MODEL_DETAIL_IMPORTED_METADATA_H

#include <elf3d/model.h>

#include <utility>
#include <vector>

namespace elf3d::model::detail {

struct ImportedDocumentMetadata {
    ModelJsonMetadata root;
    ModelJsonMetadata asset;
    std::vector<std::pair<DocumentSceneId, ModelJsonMetadata>> scenes;
    std::vector<std::pair<NodeId, ModelJsonMetadata>> nodes;
    std::vector<std::pair<MeshId, ModelJsonMetadata>> meshes;
    std::vector<std::pair<PrimitiveId, ModelJsonMetadata>> primitives;
    std::vector<std::pair<MaterialId, ModelJsonMetadata>> materials;
    std::vector<std::pair<ImageId, ModelJsonMetadata>> images;
    std::vector<std::pair<TextureId, ModelJsonMetadata>> textures;
    std::vector<std::pair<SamplerId, ModelJsonMetadata>> samplers;
};

class DocumentMetadataAccess final {
  public:
    [[nodiscard]] static Result<void> attach_import_metadata(Document& document,
                                                             ImportedDocumentMetadata&& metadata);

  private:
    [[nodiscard]] static bool target_metadata_valid(bool target_exists,
                                                    const ModelJsonMetadata& metadata,
                                                    std::size_t& total_bytes) noexcept;
    [[nodiscard]] static Result<void>
    validate_document_metadata(const ImportedDocumentMetadata& metadata,
                               std::size_t& total_bytes) noexcept;
    [[nodiscard]] static Result<void>
    validate_structural_targets(const Document::Storage& storage,
                                const ImportedDocumentMetadata& metadata,
                                std::size_t& total_bytes) noexcept;
    [[nodiscard]] static Result<void>
    validate_resource_targets(const Document::Storage& storage,
                              const ImportedDocumentMetadata& metadata,
                              std::size_t& total_bytes) noexcept;
    static void attach_structural_metadata(Document::Storage& storage,
                                           ImportedDocumentMetadata& metadata,
                                           bool& any_metadata) noexcept;
    static void attach_resource_metadata(Document::Storage& storage,
                                         ImportedDocumentMetadata& metadata,
                                         bool& any_metadata) noexcept;
};

} // namespace elf3d::model::detail

#endif
