#ifndef ELF3D_MODEL_DETAIL_DOCUMENT_BUILDER_H
#define ELF3D_MODEL_DETAIL_DOCUMENT_BUILDER_H

#include <elf3d/model.h>

namespace elf3d::model::detail {

class DocumentBuilder final {
  public:
    DocumentBuilder();
    ~DocumentBuilder() noexcept;

    DocumentBuilder(const DocumentBuilder&) = delete;
    DocumentBuilder& operator=(const DocumentBuilder&) = delete;
    DocumentBuilder(DocumentBuilder&&) noexcept;
    DocumentBuilder& operator=(DocumentBuilder&&) noexcept;

    [[nodiscard]] Result<DocumentSceneId> create_scene(std::string_view name = {});
    [[nodiscard]] Result<NodeId> create_node(std::string_view name = {});
    [[nodiscard]] Result<MeshId> create_mesh(std::string_view name = {});
    [[nodiscard]] Result<ImageId> create_image(const ModelImageDescription& description);
    [[nodiscard]] Result<SamplerId> create_sampler(const SamplerDescription& description = {});
    [[nodiscard]] Result<TextureId> create_texture(const ModelTextureDescription& description);
    [[nodiscard]] Result<MaterialId>
    create_material(const ModelMaterialDescription& description = {});
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       const PrimitiveDataView& data);
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       PrimitiveData&& data);
    [[nodiscard]] Result<void> add_scene_root(DocumentSceneId scene, NodeId node);
    [[nodiscard]] Result<void> set_default_scene(DocumentSceneId scene);
    [[nodiscard]] Result<void> clear_default_scene() noexcept;
    [[nodiscard]] Result<void> set_parent(NodeId node, NodeId parent);
    [[nodiscard]] Result<void> set_node_mesh(NodeId node, MeshId mesh);
    [[nodiscard]] Result<void> set_node_matrix(NodeId node, const Float4x4& matrix);
    [[nodiscard]] Result<void>
    set_node_perspective_camera(NodeId node, const PerspectiveCameraDescription& description);
    [[nodiscard]] Result<Document> finish();

  private:
    Document document_;
};

} // namespace elf3d::model::detail

#endif
