#ifndef ELF3D_MODEL_H
#define ELF3D_MODEL_H

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/model_ids.h>

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

inline constexpr std::uint32_t model_maximum_texture_coordinate_sets = 2;

enum class ModelAlphaMode {
    opaque,
    mask,
    blend,
};

enum class ModelPixelFormat {
    rgba8_unorm,
};

enum class ModelImageMimeType {
    none,
    png,
    jpeg,
};

struct ModelPerspectiveCameraDescription {
    float vertical_field_of_view_radians = 1.0471975512F;
    float near_plane = 0.1F;
    float far_plane = 1000.0F;

    bool operator==(const ModelPerspectiveCameraDescription&) const = default;
};

enum class ModelTextureWrap {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

enum class ModelTextureFilter {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};

struct ModelTextureTransform {
    Float2 offset;
    Float2 scale{1.0F, 1.0F};
    float rotation_radians = 0.0F;

    bool operator==(const ModelTextureTransform&) const = default;
};

struct ModelTextureMapping {
    std::uint32_t texcoord_set = 0;
    ModelTextureTransform transform;

    bool operator==(const ModelTextureMapping&) const = default;
};

struct ModelSamplerDescription {
    ModelTextureWrap wrap_u = ModelTextureWrap::repeat;
    ModelTextureWrap wrap_v = ModelTextureWrap::repeat;
    ModelTextureFilter min_filter = ModelTextureFilter::linear;
    ModelTextureFilter mag_filter = ModelTextureFilter::linear;

    bool operator==(const ModelSamplerDescription&) const = default;
};

struct ModelImageDescription {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ModelPixelFormat format = ModelPixelFormat::rgba8_unorm;
    std::span<const std::byte> pixels;
    // Optional original PNG/JPEG stream corresponding to the decoded pixels.
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

struct ModelTextureDescription {
    ImageId image;
    SamplerId sampler;

    bool operator==(const ModelTextureDescription&) const = default;
};

struct ModelMaterialDescription {
    Color4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
    bool double_sided = false;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    TextureId base_color_texture;
    TextureId metallic_roughness_texture;

    bool unlit = false;
    ModelAlphaMode alpha_mode = ModelAlphaMode::opaque;
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
    ModelTextureMapping base_color_texture_mapping;
    ModelTextureMapping metallic_roughness_texture_mapping;
    ModelTextureMapping normal_texture_mapping;
    ModelTextureMapping occlusion_texture_mapping;
    ModelTextureMapping emissive_texture_mapping;

    bool operator==(const ModelMaterialDescription&) const = default;
};

struct PrimitiveDataView {
    std::span<const Float3> positions;
    std::span<const Float3> normals;
    std::span<const Float2> texcoord0;
    std::span<const Float2> texcoord1;
    std::span<const Color4> colors;
    std::span<const std::uint32_t> indices;
};

struct PrimitiveData {
    std::vector<Float3> positions;
    std::vector<Float3> normals;
    std::vector<Float2> texcoord0;
    std::vector<Float2> texcoord1;
    std::vector<Color4> colors;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] PrimitiveDataView view() const noexcept {
        return PrimitiveDataView{positions, normals, texcoord0, texcoord1, colors, indices};
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
    ModelPixelFormat format = ModelPixelFormat::rgba8_unorm;
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
    ModelSamplerDescription description;
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
    std::optional<ModelPerspectiveCameraDescription> perspective_camera;
    ModelJsonMetadataView metadata;
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

struct ModelLoadOptions {
    bool generate_missing_normals = true;
    bool import_node_names = true;
};

enum class ModelImageWritePolicy {
    automatic,
    external,
    embedded,
};

struct ModelWriteOptions {
    // Automatic writes sidecars for .gltf and buffer-view images for .glb.
    ModelImageWritePolicy image_policy = ModelImageWritePolicy::automatic;
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

class Document final {
  public:
    Document();
    ~Document() noexcept;

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) noexcept;
    Document& operator=(Document&&) noexcept;

    [[nodiscard]] DocumentView view() const noexcept;
    [[nodiscard]] DocumentStatistics statistics() const noexcept;
    [[nodiscard]] std::optional<DocumentSceneId> default_scene() const noexcept;
    [[nodiscard]] ModelJsonMetadataView root_metadata() const noexcept;
    [[nodiscard]] ModelJsonMetadataView asset_metadata() const noexcept;
    [[nodiscard]] bool preserved_metadata_stale() const noexcept;
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t mesh_count() const noexcept;
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] std::size_t material_count() const noexcept;
    [[nodiscard]] std::size_t image_count() const noexcept;
    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t sampler_count() const noexcept;
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

    [[nodiscard]] Result<DocumentSceneId> create_scene(std::string_view name = {});
    [[nodiscard]] Result<NodeId> create_node(std::string_view name = {});
    [[nodiscard]] Result<MeshId> create_mesh(std::string_view name = {});
    [[nodiscard]] Result<MaterialId>
    create_material(const ModelMaterialDescription& description = {});
    [[nodiscard]] Result<ImageId> create_image(const ModelImageDescription& description);
    [[nodiscard]] Result<SamplerId> create_sampler(const ModelSamplerDescription& description = {});
    [[nodiscard]] Result<TextureId> create_texture(const ModelTextureDescription& description);
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       const PrimitiveDataView& data);
    [[nodiscard]] Result<PrimitiveId> create_primitive(MeshId mesh, MaterialId material,
                                                       PrimitiveData&& data);
    [[nodiscard]] Result<void> add_scene_root(DocumentSceneId scene, NodeId node);
    [[nodiscard]] Result<void> set_default_scene(DocumentSceneId scene);
    [[nodiscard]] Result<void> clear_default_scene() noexcept;
    [[nodiscard]] Result<void> set_parent(NodeId node, NodeId parent);
    [[nodiscard]] Result<void> clear_parent(NodeId node);
    [[nodiscard]] Result<void> set_node_mesh(NodeId node, MeshId mesh);
    [[nodiscard]] Result<void> clear_node_mesh(NodeId node);
    [[nodiscard]] Result<void> set_node_matrix(NodeId node, const Float4x4& matrix);
    [[nodiscard]] Result<void>
    set_node_perspective_camera(NodeId node, const ModelPerspectiveCameraDescription& description);
    [[nodiscard]] Result<void> clear_node_perspective_camera(NodeId node);
    [[nodiscard]] Result<void> replace_primitive(PrimitiveId primitive,
                                                 const PrimitiveDataView& data);
    [[nodiscard]] Result<void> replace_primitive(PrimitiveId primitive, PrimitiveData&& data);
    [[nodiscard]] Result<std::span<Float3>> mutable_positions(PrimitiveId primitive) noexcept;
    [[nodiscard]] Result<std::span<Float3>> mutable_normals(PrimitiveId primitive) noexcept;
    [[nodiscard]] Result<void> update_primitive_bounds(PrimitiveId primitive) noexcept;

  private:
    friend class DocumentView;
    friend class model::detail::DocumentMetadataAccess;
    friend class model::detail::DocumentValidation;
    friend DocumentValidationReport validate_document(DocumentView document);

    class Storage;
    std::unique_ptr<Storage> storage_;
};

class DocumentView final {
  public:
    DocumentView() noexcept = default;

    [[nodiscard]] DocumentStatistics statistics() const noexcept;
    [[nodiscard]] std::optional<DocumentSceneId> default_scene() const noexcept;
    [[nodiscard]] ModelJsonMetadataView root_metadata() const noexcept;
    [[nodiscard]] ModelJsonMetadataView asset_metadata() const noexcept;
    [[nodiscard]] bool preserved_metadata_stale() const noexcept;
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] std::size_t node_count() const noexcept;
    [[nodiscard]] std::size_t mesh_count() const noexcept;
    [[nodiscard]] std::size_t primitive_count() const noexcept;
    [[nodiscard]] std::size_t material_count() const noexcept;
    [[nodiscard]] std::size_t image_count() const noexcept;
    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t sampler_count() const noexcept;
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
    friend class model::detail::DocumentValidation;
    friend DocumentValidationReport validate_document(DocumentView document);

    explicit DocumentView(const Document* document) noexcept;

    const Document* document_ = nullptr;
};

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
    [[nodiscard]] Result<SamplerId> create_sampler(const ModelSamplerDescription& description = {});
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
    set_node_perspective_camera(NodeId node, const ModelPerspectiveCameraDescription& description);
    [[nodiscard]] Result<Document> finish();

  private:
    Document document_;
};

struct LoadedDocument {
    Document document;
    DocumentSceneId default_scene;
    ModelLoadReport report;
};

[[nodiscard]] DocumentValidationReport validate_document(DocumentView document);
[[nodiscard]] Result<LoadedDocument> load_document(std::string_view path_utf8,
                                                   const ModelLoadOptions& options = {}) noexcept;
[[nodiscard]] Result<ModelWriteReport>
save_document(std::string_view path_utf8, DocumentView document,
              const ModelWriteOptions& options = {}) noexcept;

namespace model::detail {

class DocumentHandleAccess final {
  public:
    [[nodiscard]] static constexpr DocumentSceneId create_scene(std::uintptr_t document,
                                                                std::uint64_t value) noexcept {
        return DocumentSceneId{document, value};
    }

    [[nodiscard]] static constexpr NodeId create_node(std::uintptr_t document,
                                                      std::uint64_t value) noexcept {
        return NodeId{document, value};
    }

    [[nodiscard]] static constexpr MeshId create_mesh(std::uintptr_t document,
                                                      std::uint64_t value) noexcept {
        return MeshId{document, value};
    }

    [[nodiscard]] static constexpr PrimitiveId create_primitive(std::uintptr_t document,
                                                                std::uint64_t value) noexcept {
        return PrimitiveId{document, value};
    }

    [[nodiscard]] static constexpr MaterialId create_material(std::uintptr_t document,
                                                              std::uint64_t value) noexcept {
        return MaterialId{document, value};
    }

    [[nodiscard]] static constexpr ImageId create_image(std::uintptr_t document,
                                                        std::uint64_t value) noexcept {
        return ImageId{document, value};
    }

    [[nodiscard]] static constexpr TextureId create_texture(std::uintptr_t document,
                                                            std::uint64_t value) noexcept {
        return TextureId{document, value};
    }

    [[nodiscard]] static constexpr SamplerId create_sampler(std::uintptr_t document,
                                                            std::uint64_t value) noexcept {
        return SamplerId{document, value};
    }

    [[nodiscard]] static constexpr std::uintptr_t document(DocumentSceneId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(NodeId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(MeshId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(PrimitiveId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(MaterialId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(ImageId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(TextureId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uintptr_t document(SamplerId id) noexcept {
        return id.document_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(DocumentSceneId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(NodeId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MeshId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(PrimitiveId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MaterialId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(ImageId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(TextureId id) noexcept {
        return id.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(SamplerId id) noexcept {
        return id.value_;
    }
};

} // namespace model::detail

} // namespace elf3d

#endif
