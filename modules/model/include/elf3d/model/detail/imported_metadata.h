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
};

} // namespace elf3d::model::detail

#endif
