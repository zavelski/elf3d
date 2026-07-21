#include <elf3d/model.h>
#include <elf3d/model/detail/document_builder.h>

#include <utility>

namespace elf3d {

DocumentView::DocumentView(const Document* document) noexcept : document_(document) {}

DocumentStatistics DocumentView::statistics() const noexcept {
    return document_ != nullptr ? document_->statistics() : DocumentStatistics{};
}

std::optional<DocumentSceneId> DocumentView::default_scene() const noexcept {
    return document_ != nullptr ? document_->default_scene() : std::nullopt;
}

ModelJsonMetadataView DocumentView::root_metadata() const noexcept {
    return document_ != nullptr ? document_->root_metadata() : ModelJsonMetadataView{};
}

ModelJsonMetadataView DocumentView::asset_metadata() const noexcept {
    return document_ != nullptr ? document_->asset_metadata() : ModelJsonMetadataView{};
}

bool DocumentView::preserved_metadata_stale() const noexcept {
    return document_ != nullptr && document_->preserved_metadata_stale();
}

std::size_t DocumentView::scene_count() const noexcept {
    return document_ != nullptr ? document_->scene_count() : 0;
}

std::size_t DocumentView::node_count() const noexcept {
    return document_ != nullptr ? document_->node_count() : 0;
}

std::size_t DocumentView::mesh_count() const noexcept {
    return document_ != nullptr ? document_->mesh_count() : 0;
}

std::size_t DocumentView::primitive_count() const noexcept {
    return document_ != nullptr ? document_->primitive_count() : 0;
}

std::size_t DocumentView::material_count() const noexcept {
    return document_ != nullptr ? document_->material_count() : 0;
}

std::size_t DocumentView::image_count() const noexcept {
    return document_ != nullptr ? document_->image_count() : 0;
}

std::size_t DocumentView::texture_count() const noexcept {
    return document_ != nullptr ? document_->texture_count() : 0;
}

std::size_t DocumentView::sampler_count() const noexcept {
    return document_ != nullptr ? document_->sampler_count() : 0;
}

Result<DocumentSceneView> DocumentView::scene_at(std::size_t index) const noexcept {
    return document_ != nullptr ? document_->scene_at(index)
                                : Result<DocumentSceneView>{Error{
                                      ErrorCode::invalid_document_scene_id, "The view is empty"}};
}

Result<NodeView> DocumentView::node_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->node_at(index)
               : Result<NodeView>{Error{ErrorCode::invalid_node_id, "The view is empty"}};
}

Result<MeshView> DocumentView::mesh_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->mesh_at(index)
               : Result<MeshView>{Error{ErrorCode::invalid_mesh_id, "The view is empty"}};
}

Result<PrimitiveView> DocumentView::primitive_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->primitive_at(index)
               : Result<PrimitiveView>{Error{ErrorCode::invalid_primitive_id, "The view is empty"}};
}

Result<MaterialView> DocumentView::material_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->material_at(index)
               : Result<MaterialView>{Error{ErrorCode::invalid_material_id, "The view is empty"}};
}

Result<ImageView> DocumentView::image_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->image_at(index)
               : Result<ImageView>{Error{ErrorCode::invalid_image_id, "The view is empty"}};
}

Result<TextureView> DocumentView::texture_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->texture_at(index)
               : Result<TextureView>{Error{ErrorCode::invalid_texture_id, "The view is empty"}};
}

Result<SamplerView> DocumentView::sampler_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->sampler_at(index)
               : Result<SamplerView>{Error{ErrorCode::invalid_sampler_id, "The view is empty"}};
}

Result<DocumentSceneView> DocumentView::scene(DocumentSceneId scene_id) const noexcept {
    return document_ != nullptr ? document_->scene(scene_id)
                                : Result<DocumentSceneView>{Error{
                                      ErrorCode::invalid_document_scene_id, "The view is empty"}};
}

Result<NodeView> DocumentView::node(NodeId node_id) const noexcept {
    return document_ != nullptr
               ? document_->node(node_id)
               : Result<NodeView>{Error{ErrorCode::invalid_node_id, "The view is empty"}};
}

Result<MeshView> DocumentView::mesh(MeshId mesh_id) const noexcept {
    return document_ != nullptr
               ? document_->mesh(mesh_id)
               : Result<MeshView>{Error{ErrorCode::invalid_mesh_id, "The view is empty"}};
}

Result<PrimitiveView> DocumentView::primitive(PrimitiveId primitive_id) const noexcept {
    return document_ != nullptr
               ? document_->primitive(primitive_id)
               : Result<PrimitiveView>{Error{ErrorCode::invalid_primitive_id, "The view is empty"}};
}

Result<MaterialView> DocumentView::material(MaterialId material_id) const noexcept {
    return document_ != nullptr
               ? document_->material(material_id)
               : Result<MaterialView>{Error{ErrorCode::invalid_material_id, "The view is empty"}};
}

Result<ImageView> DocumentView::image(ImageId image_id) const noexcept {
    return document_ != nullptr
               ? document_->image(image_id)
               : Result<ImageView>{Error{ErrorCode::invalid_image_id, "The view is empty"}};
}

Result<TextureView> DocumentView::texture(TextureId texture_id) const noexcept {
    return document_ != nullptr
               ? document_->texture(texture_id)
               : Result<TextureView>{Error{ErrorCode::invalid_texture_id, "The view is empty"}};
}

Result<SamplerView> DocumentView::sampler(SamplerId sampler_id) const noexcept {
    return document_ != nullptr
               ? document_->sampler(sampler_id)
               : Result<SamplerView>{Error{ErrorCode::invalid_sampler_id, "The view is empty"}};
}

std::optional<Bounds3> DocumentView::bounds() const noexcept {
    return document_ != nullptr ? document_->bounds() : std::nullopt;
}

} // namespace elf3d

namespace elf3d::model::detail {

DocumentBuilder::DocumentBuilder() = default;

DocumentBuilder::~DocumentBuilder() noexcept = default;

DocumentBuilder::DocumentBuilder(DocumentBuilder&&) noexcept = default;

DocumentBuilder& DocumentBuilder::operator=(DocumentBuilder&&) noexcept = default;

Result<DocumentSceneId> DocumentBuilder::create_scene(std::string_view name) {
    return document_.create_scene(name);
}

Result<NodeId> DocumentBuilder::create_node(std::string_view name) {
    return document_.create_node(name);
}

Result<MeshId> DocumentBuilder::create_mesh(std::string_view name) {
    return document_.create_mesh(name);
}

Result<ImageId> DocumentBuilder::create_image(const ModelImageDescription& description) {
    return document_.create_image(description);
}

Result<SamplerId> DocumentBuilder::create_sampler(const SamplerDescription& description) {
    return document_.create_sampler(description);
}

Result<TextureId> DocumentBuilder::create_texture(const ModelTextureDescription& description) {
    return document_.create_texture(description);
}

Result<MaterialId> DocumentBuilder::create_material(const ModelMaterialDescription& description) {
    return document_.create_material(description);
}

Result<PrimitiveId> DocumentBuilder::create_primitive(MeshId mesh_id, MaterialId material_id,
                                                      const PrimitiveDataView& data) {
    return document_.create_primitive(mesh_id, material_id, data);
}

Result<PrimitiveId> DocumentBuilder::create_primitive(MeshId mesh_id, MaterialId material_id,
                                                      PrimitiveData&& data) {
    return document_.create_primitive(mesh_id, material_id, std::move(data));
}

Result<void> DocumentBuilder::add_scene_root(DocumentSceneId scene_id, NodeId node_id) {
    return document_.add_scene_root(scene_id, node_id);
}

Result<void> DocumentBuilder::set_default_scene(DocumentSceneId scene_id) {
    return document_.set_default_scene(scene_id);
}

Result<void> DocumentBuilder::clear_default_scene() noexcept {
    return document_.clear_default_scene();
}

Result<void> DocumentBuilder::set_parent(NodeId node_id, NodeId parent_id) {
    return document_.set_parent(node_id, parent_id);
}

Result<void> DocumentBuilder::set_node_mesh(NodeId node_id, MeshId mesh_id) {
    return document_.set_node_mesh(node_id, mesh_id);
}

Result<void> DocumentBuilder::set_node_matrix(NodeId node_id, const Float4x4& matrix) {
    return document_.set_node_matrix(node_id, matrix);
}

Result<void>
DocumentBuilder::set_node_perspective_camera(NodeId node_id,
                                             const PerspectiveCameraDescription& description) {
    return document_.set_node_perspective_camera(node_id, description);
}

Result<Document> DocumentBuilder::finish() {
    DocumentValidationReport report = validate_document(document_.view());
    if (report.has_errors()) {
        return Error{ErrorCode::invalid_argument, "Document validation failed"};
    }
    return std::move(document_);
}

} // namespace elf3d::model::detail
