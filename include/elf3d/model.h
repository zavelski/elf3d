#ifndef ELF3D_MODEL_H
#define ELF3D_MODEL_H

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/model_ids.h>
#include <elf3d/model_types.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d {

namespace model::detail {
class DocumentWriteSource;
class DocumentBuilder;
} // namespace model::detail

enum class ModelImageMimeType {
    none,
    png,
    jpeg,
    // Encoded-only environment resource: no decoded pixels or dimensions.
    exr,
    tiff,
};

struct ModelImageDescription {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    std::span<const std::byte> pixels;
    // Optional original PNG/JPEG/TIFF stream, or encoded-only EXR with zero dimensions.
    ModelImageMimeType source_mime_type = ModelImageMimeType::none;
    std::span<const std::byte> source_bytes;
};

struct ModelJsonExtension {
    std::string name;
    std::string data;

    bool operator==(const ModelJsonExtension&) const = default;
};

struct ModelJsonMetadata {
    std::optional<std::string> extras_json;
    std::vector<ModelJsonExtension> extensions;

    bool operator==(const ModelJsonMetadata&) const = default;
};

struct ModelJsonMetadataView {
    std::optional<std::string_view> extras_json;
    std::span<const ModelJsonExtension> extensions;
};

enum class PrimitiveMetadataPolicy {
    invalidate,
    // Caller has checked every retained metadata scope against this operation.
    // No retained payload may refer to changed vertex/triangle identities.
    // Object/resource table identities remain unchanged. Existing stale state
    // is never cleared; this is not metadata validation or remapping.
    preserve_verified_references,
};

struct ModelCameraImageBasedLightingDescription {
    ImageId image;
    Float3 rotation;
    Float2 scale{1.0F, 1.0F};

    bool operator==(const ModelCameraImageBasedLightingDescription&) const = default;
};

struct ModelCameraPropertiesDescription {
    std::string name;
    std::optional<float> aspect_ratio;
    bool infinite_far_plane = false;
    // Equirectangular environment description; angles retain authored units.
    std::optional<ModelCameraImageBasedLightingDescription> image_based_lighting;

    bool operator==(const ModelCameraPropertiesDescription&) const = default;
};

struct ModelCameraPropertiesView {
    std::string_view name;
    std::optional<float> aspect_ratio;
    bool infinite_far_plane = false;
    std::optional<ModelCameraImageBasedLightingDescription> image_based_lighting;
    ModelJsonMetadataView metadata;
    ModelJsonMetadataView projection_metadata;
};

struct ModelTextureDescription {
    ImageId image;
    SamplerId sampler;

    bool operator==(const ModelTextureDescription&) const = default;
};

struct ModelVolumeDescription {
    float thickness_factor = 0.0F;
    TextureId thickness_texture;
    TextureMapping thickness_texture_mapping;
    // Absence represents the glTF default of infinite attenuation distance.
    std::optional<float> attenuation_distance;
    Float3 attenuation_color{1.0F, 1.0F, 1.0F};

    bool operator==(const ModelVolumeDescription&) const = default;
};

struct ModelTransmissionDescription {
    float factor = 0.0F;
    TextureId texture;
    TextureMapping texture_mapping;
    bool operator==(const ModelTransmissionDescription&) const = default;
};

struct ModelClearcoatDescription {
    float factor = 0.0F;
    float roughness_factor = 0.0F;
    TextureId texture;
    TextureId roughness_texture;
    TextureId normal_texture;
    float normal_scale = 1.0F;
    TextureMapping texture_mapping;
    TextureMapping roughness_texture_mapping;
    TextureMapping normal_texture_mapping;
    bool operator==(const ModelClearcoatDescription&) const = default;
};

struct ModelMaterialDescription {
    Color4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
    bool double_sided = false;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    TextureId base_color_texture;
    TextureId metallic_roughness_texture;

    bool unlit = false;
    AlphaMode alpha_mode = AlphaMode::opaque;
    float alpha_cutoff = 0.5F;
    Float3 emissive_factor;
    float emissive_strength = 1.0F;
    float normal_scale = 1.0F;
    float occlusion_strength = 1.0F;
    float ior = 1.5F;
    float specular_factor = 1.0F;
    Float3 specular_color_factor{1.0F, 1.0F, 1.0F};
    TextureId normal_texture;
    TextureId occlusion_texture;
    TextureId emissive_texture;
    TextureMapping base_color_texture_mapping;
    TextureMapping metallic_roughness_texture_mapping;
    TextureMapping normal_texture_mapping;
    TextureMapping occlusion_texture_mapping;
    TextureMapping emissive_texture_mapping;
    std::optional<ModelVolumeDescription> volume;
    // Copied into Document storage by create_material; borrowed in returned views.
    std::string_view name;

    TextureId specular_texture;
    TextureId specular_color_texture;
    TextureMapping specular_texture_mapping;
    TextureMapping specular_color_texture_mapping;
    std::optional<ModelTransmissionDescription> transmission;
    std::optional<ModelClearcoatDescription> clearcoat;

    bool operator==(const ModelMaterialDescription&) const = default;
};

struct PrimitiveDataView {
    std::span<const Float3> positions;
    std::span<const Float3> normals;
    std::span<const Float2> texcoord0;
    std::span<const Float2> texcoord1;
    std::span<const Color4> colors;
    std::span<const std::uint32_t> indices;
    std::span<const Float4> tangents;
};

struct PrimitiveData {
    std::vector<Float3> positions;
    std::vector<Float3> normals;
    std::vector<Float2> texcoord0;
    std::vector<Float2> texcoord1;
    std::vector<Color4> colors;
    std::vector<std::uint32_t> indices;
    std::vector<Float4> tangents;

    [[nodiscard]] PrimitiveDataView view() const noexcept
    {
        return PrimitiveDataView{positions, normals, texcoord0, texcoord1,
                                 colors,    indices, tangents};
    }
};

struct PrimitiveView {
    PrimitiveId id;
    MeshId mesh;
    MaterialId material;
    Bounds3 bounds;
    PrimitiveDataView data;
    ModelJsonMetadataView metadata;
};

struct MaterialView {
    MaterialId id;
    ModelMaterialDescription description;
    ModelJsonMetadataView metadata;
};

struct ImageView {
    ImageId id;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    std::span<const std::byte> pixels;
    ModelImageMimeType source_mime_type = ModelImageMimeType::none;
    std::span<const std::byte> source_bytes;
    ModelJsonMetadataView metadata;
};

struct TextureView {
    TextureId id;
    ModelTextureDescription description;
    ModelJsonMetadataView metadata;
};

struct SamplerView {
    SamplerId id;
    SamplerDescription description;
    ModelJsonMetadataView metadata;
};

struct MeshView {
    MeshId id;
    std::string_view name;
    std::span<const PrimitiveId> primitives;
    std::optional<Bounds3> bounds;
    ModelJsonMetadataView metadata;
};

struct NodeView {
    NodeId id;
    std::string_view name;
    std::optional<NodeId> parent;
    std::span<const NodeId> children;
    Float4x4 local_matrix;
    std::optional<MeshId> mesh;
    std::optional<PerspectiveCameraDescription> perspective_camera;
    ModelJsonMetadataView metadata;
    ModelCameraPropertiesView camera_properties;
};

struct DocumentSceneView {
    DocumentSceneId id;
    std::string_view name;
    std::span<const NodeId> roots;
    ModelJsonMetadataView metadata;
};

struct DocumentStatistics {
    std::uint64_t scenes = 0;
    std::uint64_t nodes = 0;
    std::uint64_t meshes = 0;
    std::uint64_t primitives = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    std::uint64_t triangles = 0;
    std::uint64_t materials = 0;
    std::uint64_t images = 0;
    std::uint64_t textures = 0;
    std::uint64_t samplers = 0;
    std::uint64_t perspective_cameras = 0;
    std::uint64_t decoded_image_bytes = 0;
    std::uint64_t materials_with_base_color_textures = 0;
    std::uint64_t materials_with_metallic_roughness_textures = 0;
    std::uint64_t materials_with_normal_textures = 0;
    std::uint64_t materials_with_occlusion_textures = 0;
    std::uint64_t materials_with_emissive_textures = 0;

    bool operator==(const DocumentStatistics&) const = default;
};

enum class DocumentDiagnosticSeverity {
    information,
    warning,
    error,
};

enum class DocumentDiagnosticCode {
    invalid_reference,
    invalid_geometry,
    invalid_asset,
    invalid_transform,
    hierarchy_cycle,
    stale_preserved_metadata,
};

struct DocumentDiagnostic {
    DocumentDiagnosticSeverity severity = DocumentDiagnosticSeverity::error;
    DocumentDiagnosticCode code = DocumentDiagnosticCode::invalid_reference;
    std::string message;
};

struct DocumentValidationReport {
    std::vector<DocumentDiagnostic> diagnostics;

    [[nodiscard]] bool has_errors() const noexcept;
};

enum class ModelImageWritePolicy {
    automatic,
    external,
    embedded,
};

enum class ModelNumericWritePolicy {
    round_trip,
    clean_rotation,
};

struct ModelWriteOptions {
    // Automatic writes sidecars for .gltf and buffer-view images for .glb.
    ModelImageWritePolicy image_policy = ModelImageWritePolicy::automatic;
    // Clean only orthonormal matrix bases within a 1e-7 absolute tolerance.
    ModelNumericWritePolicy numeric_policy = ModelNumericWritePolicy::round_trip;
};

enum class ModelWriteDiagnosticSeverity {
    information,
    warning,
};

enum class ModelWriteDiagnosticCategory {
    image,
    material,
    metadata,
    output,
};

enum class ModelWriteDiagnosticCode {
    image_reencoded_as_png,
    preserved_metadata_dropped_after_mutation,
    matrix_rotation_cleaned,
};

struct ModelWriteDiagnostic {
    ModelWriteDiagnosticSeverity severity = ModelWriteDiagnosticSeverity::warning;
    ModelWriteDiagnosticCategory category = ModelWriteDiagnosticCategory::output;
    ModelWriteDiagnosticCode code = ModelWriteDiagnosticCode::image_reencoded_as_png;
    std::string message;
    std::optional<std::string> source_context;
};

struct ModelWriteReport {
    std::vector<ModelWriteDiagnostic> diagnostics;
};

enum class ModelLoadDiagnosticSeverity {
    information,
    warning,
};

enum class ModelLoadDiagnosticCategory {
    geometry,
    material,
    texture,
    extension,
    camera,
    light,
    animation,
    metadata,
    scene,
};

enum class ModelLoadDiagnosticCode {
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
    metadata_not_preserved,
    repaired_signed_buffer_layout,
    ignored_vertex_attribute,
};

struct ModelLoadDiagnostic {
    ModelLoadDiagnosticSeverity severity = ModelLoadDiagnosticSeverity::warning;
    ModelLoadDiagnosticCategory category = ModelLoadDiagnosticCategory::geometry;
    ModelLoadDiagnosticCode code = ModelLoadDiagnosticCode::material_fallback;
    std::string message;
    std::optional<std::string> source_context;
};

struct ModelLoadReport {
    std::vector<ModelLoadDiagnostic> diagnostics;
};

class DocumentView;
class DocumentHierarchySnapshot;

// Borrowed source/destination ID pairs for one verified reconstruction. Multiple
// destinations may inherit one source; each destination may occur only once.
// Unlisted objects receive no metadata. Opaque references are caller-verified,
// not rewritten by this operation. Names and typed camera/resource fields are
// ordinary model data and must already be built by the caller.
struct ModelMetadataMapping {
    std::span<const std::pair<DocumentSceneId, DocumentSceneId>> scenes;
    std::span<const std::pair<NodeId, NodeId>> nodes;
    std::span<const std::pair<MeshId, MeshId>> meshes;
    std::span<const std::pair<PrimitiveId, PrimitiveId>> primitives;
    std::span<const std::pair<MaterialId, MaterialId>> materials;
    std::span<const std::pair<ImageId, ImageId>> images;
    std::span<const std::pair<TextureId, TextureId>> textures;
    std::span<const std::pair<SamplerId, SamplerId>> samplers;
};

// Owned metadata for a completed, unannotated Document. Every target ID belongs
// to that Document. The caller has verified JSON syntax and retained opaque
// references against its current topology; no source Document is required.
struct ModelDocumentMetadata {
    ModelJsonMetadata root;
    ModelJsonMetadata asset;
    std::vector<std::string> required_extensions;
    std::vector<std::pair<DocumentSceneId, ModelJsonMetadata>> scenes;
    std::vector<std::pair<NodeId, ModelJsonMetadata>> nodes;
    std::vector<std::pair<NodeId, ModelJsonMetadata>> cameras;
    std::vector<std::pair<NodeId, ModelJsonMetadata>> camera_projections;
    std::vector<std::pair<MeshId, ModelJsonMetadata>> meshes;
    std::vector<std::pair<PrimitiveId, ModelJsonMetadata>> primitives;
    std::vector<std::pair<MaterialId, ModelJsonMetadata>> materials;
    std::vector<std::pair<ImageId, ModelJsonMetadata>> images;
    std::vector<std::pair<TextureId, ModelJsonMetadata>> textures;
    std::vector<std::pair<SamplerId, ModelJsonMetadata>> samplers;
};

class Document final {
  public:
    Document() noexcept;
    ~Document() noexcept;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) noexcept;
    Document& operator=(Document&&) noexcept;

    [[nodiscard]] DocumentView view() const noexcept;
    // Persistent data revision; separate from Scene/view revisions. Zero for a
    // moved-from owner. Revisions are comparable only within the same owner.
    [[nodiscard]] std::uint64_t revision() const noexcept;
    // Synchronous wrapper around the bounded snapshot builder below.
    [[nodiscard]] Result<DocumentHierarchySnapshot> hierarchy_snapshot() const noexcept;
    [[nodiscard]] DocumentStatistics statistics() const noexcept;
    [[nodiscard]] std::optional<DocumentSceneId> default_scene() const noexcept;
    [[nodiscard]] ModelJsonMetadataView root_metadata() const noexcept;
    [[nodiscard]] ModelJsonMetadataView asset_metadata() const noexcept;
    [[nodiscard]] std::span<const std::string> required_extensions() const noexcept;
    [[nodiscard]] bool preserved_metadata_stale() const noexcept;
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t mesh_count() const noexcept;
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] std::size_t material_count() const noexcept;
    [[nodiscard]] std::size_t image_count() const noexcept;
    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t sampler_count() const noexcept;
    // Constant-sized owner/liveness checks. Zero-based positions in this
    // Document's current tables, never persistent identities or output-file
    // indices. Keep IDs and revision when capturing positions for later work.
    [[nodiscard]] Result<std::size_t> index_of(NodeId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(MeshId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(PrimitiveId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(MaterialId id) const noexcept;
    [[nodiscard]] Result<DocumentSceneView> scene_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<NodeView> node_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<MeshView> mesh_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<PrimitiveView> primitive_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<MaterialView> material_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<ImageView> image_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<TextureView> texture_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<SamplerView> sampler_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentSceneView> scene(DocumentSceneId scene) const noexcept;
    [[nodiscard]] Result<NodeView> node(NodeId node) const noexcept;
    [[nodiscard]] Result<MeshView> mesh(MeshId mesh) const noexcept;
    [[nodiscard]] Result<PrimitiveView> primitive(PrimitiveId primitive) const noexcept;
    [[nodiscard]] Result<MaterialView> material(MaterialId material) const noexcept;
    [[nodiscard]] Result<ImageView> image(ImageId image) const noexcept;
    [[nodiscard]] Result<TextureView> texture(TextureId texture) const noexcept;
    [[nodiscard]] Result<SamplerView> sampler(SamplerId sampler) const noexcept;
    [[nodiscard]] std::optional<Bounds3> bounds() const noexcept;

    [[nodiscard]] Result<DocumentSceneId> create_scene(std::string_view name = {}) noexcept;
    [[nodiscard]] Result<NodeId> create_node(std::string_view name = {}) noexcept;
    [[nodiscard]] Result<MeshId> create_mesh(std::string_view name = {}) noexcept;
    [[nodiscard]] Result<MaterialId>
    create_material(const ModelMaterialDescription& description = {}) noexcept;
    [[nodiscard]] Result<ImageId> create_image(const ModelImageDescription& description) noexcept;
    // Fresh ID and independent record; shares only immutable image payload.
    // Does not copy opaque metadata. Source must outlive this synchronous call,
    // but may be destroyed immediately afterward. Invalid IDs leave target intact.
    [[nodiscard]] Result<ImageId> create_image_snapshot(DocumentView source,
                                                        ImageId image) noexcept;
    // Attach root/asset/required declarations and mapped object/camera metadata
    // after building a complete fresh document. Reject stale source metadata,
    // an already annotated target, foreign IDs, duplicate destinations or
    // incompatible camera presence. Failure leaves target untouched. The caller
    // has verified retained opaque references against the resulting topology.
    [[nodiscard]] Result<void> copy_verified_metadata(DocumentView source,
                                                      const ModelMetadataMapping& mapping) noexcept;
    // Consume caller-verified JSON into a valid, completely built, unannotated
    // target. Checks byte/name budgets, owner-scoped IDs, duplicate targets and
    // camera presence before any mutation. Does not parse/remap opaque JSON or
    // clear stale metadata. Failure leaves the target unchanged; later structural
    // edits follow the ordinary metadata invalidation rules.
    [[nodiscard]] Result<void> attach_verified_metadata(ModelDocumentMetadata metadata) noexcept;
    // Copies new bytes before replacing this owner's image; snapshots keep their
    // old payload. Retains the ID and references, invalidates opaque metadata.
    // Invalid descriptions/foreign IDs leave the document unchanged. EXR cannot
    // replace an image referenced by a material texture. Image views expire.
    [[nodiscard]] Result<void> replace_image(ImageId image,
                                             const ModelImageDescription& description) noexcept;
    [[nodiscard]] Result<SamplerId>
    create_sampler(const SamplerDescription& description = {}) noexcept;
    [[nodiscard]] Result<TextureId>
    create_texture(const ModelTextureDescription& description) noexcept;
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       const PrimitiveDataView& data) noexcept;
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       PrimitiveData&& data) noexcept;
    [[nodiscard]] Result<void> add_scene_root(DocumentSceneId scene, NodeId node) noexcept;
    [[nodiscard]] Result<void> set_default_scene(DocumentSceneId scene) noexcept;
    [[nodiscard]] Result<void> clear_default_scene() noexcept;
    [[nodiscard]] Result<void> set_parent(NodeId node, NodeId parent) noexcept;
    [[nodiscard]] Result<void> clear_parent(NodeId node) noexcept;
    [[nodiscard]] Result<void> set_node_mesh(NodeId node, MeshId mesh) noexcept;
    [[nodiscard]] Result<void> clear_node_mesh(NodeId node) noexcept;
    [[nodiscard]] Result<void> set_node_matrix(NodeId node, const Float4x4& matrix) noexcept;
    [[nodiscard]] Result<void>
    set_node_perspective_camera(NodeId node,
                                const PerspectiveCameraDescription& description) noexcept;
    [[nodiscard]] Result<void>
    set_node_camera_properties(NodeId node,
                               const ModelCameraPropertiesDescription& description) noexcept;
    [[nodiscard]] Result<void> clear_node_perspective_camera(NodeId node) noexcept;
    [[nodiscard]] Result<void> replace_primitive(
        PrimitiveId primitive, const PrimitiveDataView& data,
        PrimitiveMetadataPolicy metadata = PrimitiveMetadataPolicy::invalidate) noexcept;
    [[nodiscard]] Result<void> replace_primitive(
        PrimitiveId primitive, PrimitiveData&& data,
        PrimitiveMetadataPolicy metadata = PrimitiveMetadataPolicy::invalidate) noexcept;

  private:
    friend class DocumentView;
    friend class DocumentHierarchySnapshot;
    friend class model::detail::DocumentMetadataAccess;
    friend class model::detail::DocumentValidation;
    friend class model::detail::DocumentWriteSource;
    friend class model::detail::DocumentBuilder;
    friend DocumentValidationReport validate_document(DocumentView document) noexcept;
    friend Result<Document> clone_document(DocumentView document) noexcept;
    friend Result<Document> snapshot_document(DocumentView document) noexcept;

    class Storage;
    std::unique_ptr<Storage> storage_;
};

class DocumentView final {
  public:
    DocumentView() noexcept = default;

    [[nodiscard]] std::uint64_t revision() const noexcept;

    [[nodiscard]] DocumentStatistics statistics() const noexcept;
    [[nodiscard]] std::optional<DocumentSceneId> default_scene() const noexcept;
    [[nodiscard]] ModelJsonMetadataView root_metadata() const noexcept;
    [[nodiscard]] ModelJsonMetadataView asset_metadata() const noexcept;
    [[nodiscard]] std::span<const std::string> required_extensions() const noexcept;
    [[nodiscard]] bool preserved_metadata_stale() const noexcept;
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t mesh_count() const noexcept;
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] std::size_t material_count() const noexcept;
    [[nodiscard]] std::size_t image_count() const noexcept;
    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t sampler_count() const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(NodeId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(MeshId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(PrimitiveId id) const noexcept;
    [[nodiscard]] Result<std::size_t> index_of(MaterialId id) const noexcept;
    [[nodiscard]] Result<DocumentSceneView> scene_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<NodeView> node_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<MeshView> mesh_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<PrimitiveView> primitive_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<MaterialView> material_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<ImageView> image_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<TextureView> texture_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<SamplerView> sampler_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentSceneView> scene(DocumentSceneId scene) const noexcept;
    [[nodiscard]] Result<NodeView> node(NodeId node) const noexcept;
    [[nodiscard]] Result<MeshView> mesh(MeshId mesh) const noexcept;
    [[nodiscard]] Result<PrimitiveView> primitive(PrimitiveId primitive) const noexcept;
    [[nodiscard]] Result<MaterialView> material(MaterialId material) const noexcept;
    [[nodiscard]] Result<ImageView> image(ImageId image) const noexcept;
    [[nodiscard]] Result<TextureView> texture(TextureId texture) const noexcept;
    [[nodiscard]] Result<SamplerView> sampler(SamplerId sampler) const noexcept;
    [[nodiscard]] std::optional<Bounds3> bounds() const noexcept;

  private:
    friend class Document;
    friend class DocumentHierarchySnapshot;
    friend class model::detail::DocumentValidation;
    friend class model::detail::DocumentWriteSource;
    friend DocumentValidationReport validate_document(DocumentView document) noexcept;
    friend Result<Document> clone_document(DocumentView document) noexcept;
    friend Result<Document> snapshot_document(DocumentView document) noexcept;

    explicit DocumentView(const Document* document) noexcept;

    const Document* document_ = nullptr;
};

struct DocumentHierarchyScene {
    DocumentSceneId id;
    std::string_view name;
    std::span<const NodeId> roots;
};

struct DocumentHierarchyNode {
    NodeId id;
    std::string_view name;
    std::optional<NodeId> parent;
    std::span<const NodeId> children;
    std::optional<MeshId> mesh;
    bool has_camera = false;
    bool has_areal_light = false;
    bool in_authored_scene = false;
};

struct DocumentHierarchyMesh {
    MeshId id;
    std::string_view name;
    std::span<const PrimitiveId> primitives;
    // All authored users, including nodes outside scenes; empty means unused.
    std::span<const NodeId> users;
};

struct DocumentHierarchyPrimitive {
    PrimitiveId id;
    MeshId mesh;
    MaterialId material;
    std::uint64_t vertices = 0;
    std::uint64_t triangles = 0;
};

// Owns metadata only. No Document, geometry, image payload or borrowed view is
// retained. Completed views borrow this snapshot and survive source destruction.
// Scene roots plus canonical node links define occurrence identity: a GUI may
// present a node in multiple scenes without copying the node's metadata.
class DocumentHierarchySnapshot final {
  public:
    DocumentHierarchySnapshot() noexcept;
    ~DocumentHierarchySnapshot() noexcept;
    DocumentHierarchySnapshot(const DocumentHierarchySnapshot&) = delete;
    DocumentHierarchySnapshot& operator=(const DocumentHierarchySnapshot&) = delete;
    DocumentHierarchySnapshot(DocumentHierarchySnapshot&&) noexcept;
    DocumentHierarchySnapshot& operator=(DocumentHierarchySnapshot&&) noexcept;

    // Creation captures identity/revision and reserves metadata tables only.
    // Call advance on the source's owning thread, or on an exclusive CPU owner.
    [[nodiscard]] static Result<DocumentHierarchySnapshot> create(DocumentView source) noexcept;
    // Each work unit copies at most 4096 name bytes or 64 relationships, or
    // processes one small record. Allocator calls and per-node supported-light
    // inspection are indivisible. Zero budget and changed/foreign sources fail.
    // No partial views are exposed; true means both projections are complete.
    [[nodiscard]] Result<bool> advance(DocumentView source, std::size_t work_units = 128) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool matches_document(DocumentView source) const noexcept;
    [[nodiscard]] std::uint64_t document_revision() const noexcept;
    [[nodiscard]] std::optional<DocumentSceneId> default_scene() const noexcept;
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t mesh_count() const noexcept;
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] Result<DocumentHierarchyScene> scene_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyNode> node_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyMesh> mesh_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyPrimitive> primitive_at(std::size_t index) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyNode> node(NodeId id) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyMesh> mesh(MeshId id) const noexcept;
    [[nodiscard]] Result<DocumentHierarchyPrimitive> primitive(PrimitiveId id) const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

struct LoadedDocument {
    Document document;
    DocumentSceneId default_scene;
    ModelLoadReport report;
};

[[nodiscard]] DocumentValidationReport validate_document(DocumentView document) noexcept;
// Synchronous complete copy, with independent storage and fresh owner-scoped IDs.
// Preserves enumeration order and raw metadata, including its stale state; does
// not interpret opaque JSON references. The source must stay alive and unmodified
// during the call. Empty/moved-from views or validation errors return invalid_argument;
// a live empty Document is supported. Allocation failure follows the SDK fatal policy.
[[nodiscard]] Result<Document> clone_document(DocumentView document) noexcept;
// Same validation, fresh IDs and independent tables/geometry/metadata as clone,
// but shares immutable encoded/decoded image bytes. Either owner may outlive the
// other or move to a different thread after this synchronous call. No mutable
// data aliases; image replacement allocates a new block for that owner only.
// No GPU sharing or thread-safety for concurrent access to the SAME Document.
[[nodiscard]] Result<Document> snapshot_document(DocumentView document) noexcept;
[[nodiscard]] Result<LoadedDocument> load_document(std::string_view path_utf8,
                                                   const ModelLoadOptions& options = {}) noexcept;
[[nodiscard]] Result<ModelWriteReport>
save_document(std::string_view path_utf8, DocumentView document,
              const ModelWriteOptions& options = {}) noexcept;

} // namespace elf3d

#endif
