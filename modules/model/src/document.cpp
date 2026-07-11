#include <elf3d/model.h>
#include <elf3d/model/detail/imported_metadata.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace elf3d {
namespace {

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] bool finite(Float2 value) noexcept {
    return finite(value.x) && finite(value.y);
}

[[nodiscard]] bool finite(Float3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] bool finite(Color4 value) noexcept {
    return finite(value.red) && finite(value.green) && finite(value.blue) && finite(value.alpha);
}

[[nodiscard]] bool finite(const Float4x4& value) noexcept {
    return std::all_of(value.elements.begin(), value.elements.end(),
                       [](float element) noexcept { return finite(element); });
}

[[nodiscard]] bool valid_alpha(ModelAlphaMode mode) noexcept {
    return mode == ModelAlphaMode::opaque || mode == ModelAlphaMode::mask ||
           mode == ModelAlphaMode::blend;
}

[[nodiscard]] bool valid_wrap(ModelTextureWrap wrap) noexcept {
    return wrap == ModelTextureWrap::repeat || wrap == ModelTextureWrap::mirrored_repeat ||
           wrap == ModelTextureWrap::clamp_to_edge;
}

[[nodiscard]] bool valid_filter(ModelTextureFilter filter) noexcept {
    return filter == ModelTextureFilter::nearest || filter == ModelTextureFilter::linear ||
           filter == ModelTextureFilter::nearest_mipmap_nearest ||
           filter == ModelTextureFilter::linear_mipmap_nearest ||
           filter == ModelTextureFilter::nearest_mipmap_linear ||
           filter == ModelTextureFilter::linear_mipmap_linear;
}

[[nodiscard]] bool valid_mag_filter(ModelTextureFilter filter) noexcept {
    return filter == ModelTextureFilter::nearest || filter == ModelTextureFilter::linear;
}

[[nodiscard]] bool
valid_perspective_camera(const ModelPerspectiveCameraDescription& description) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    return finite(description.vertical_field_of_view_radians) && finite(description.near_plane) &&
           finite(description.far_plane) && description.vertical_field_of_view_radians > 0.0F &&
           description.vertical_field_of_view_radians < pi && description.near_plane > 0.0F &&
           description.far_plane > description.near_plane;
}

[[nodiscard]] bool valid_mapping(ModelTextureMapping mapping) noexcept {
    return mapping.texcoord_set < model_maximum_texture_coordinate_sets &&
           finite(mapping.transform.offset) && finite(mapping.transform.scale) &&
           finite(mapping.transform.rotation_radians);
}

[[nodiscard]] bool
valid_material_color_factors(const ModelMaterialDescription& description) noexcept {
    return finite(description.base_color) && finite(description.emissive_factor) &&
           finite(description.specular_color_factor);
}

[[nodiscard]] bool
valid_material_scalar_factors(const ModelMaterialDescription& description) noexcept {
    return finite(description.metallic_factor) && finite(description.roughness_factor) &&
           finite(description.alpha_cutoff) && finite(description.emissive_strength) &&
           finite(description.normal_scale) && finite(description.occlusion_strength) &&
           finite(description.ior) && finite(description.specular_factor);
}

[[nodiscard]] bool valid_material_factors(const ModelMaterialDescription& description) noexcept {
    return valid_material_color_factors(description) &&
           valid_material_scalar_factors(description) && valid_alpha(description.alpha_mode);
}

[[nodiscard]] bool valid_material_mappings(const ModelMaterialDescription& description) noexcept {
    return valid_mapping(description.base_color_texture_mapping) &&
           valid_mapping(description.metallic_roughness_texture_mapping) &&
           valid_mapping(description.normal_texture_mapping) &&
           valid_mapping(description.occlusion_texture_mapping) &&
           valid_mapping(description.emissive_texture_mapping);
}

inline constexpr std::size_t model_maximum_image_bytes = 256ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t model_maximum_source_image_bytes = 64ULL * 1024ULL * 1024ULL;

[[nodiscard]] bool source_bytes_match(ModelImageMimeType mime,
                                      std::span<const std::byte> bytes) noexcept {
    if (mime == ModelImageMimeType::none) {
        return bytes.empty();
    }
    if (bytes.empty() || bytes.size() > model_maximum_source_image_bytes) {
        return false;
    }
    if (mime == ModelImageMimeType::png) {
        constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47,
                                                        0x0d, 0x0a, 0x1a, 0x0a};
        if (bytes.size() < signature.size()) {
            return false;
        }
        for (std::size_t index = 0; index < signature.size(); ++index) {
            if (std::to_integer<std::uint8_t>(bytes[index]) != signature[index]) {
                return false;
            }
        }
        return true;
    }
    return mime == ModelImageMimeType::jpeg && bytes.size() >= 3U &&
           std::to_integer<std::uint8_t>(bytes[0]) == 0xffU &&
           std::to_integer<std::uint8_t>(bytes[1]) == 0xd8U &&
           std::to_integer<std::uint8_t>(bytes[2]) == 0xffU;
}

[[nodiscard]] Result<std::size_t> expected_image_bytes(std::uint32_t image_width,
                                                       std::uint32_t image_height,
                                                       ModelPixelFormat format) noexcept {
    if (image_width == 0 || image_height == 0) {
        return Error{ErrorCode::zero_image_dimensions, "Images require nonzero dimensions"};
    }
    if (format != ModelPixelFormat::rgba8_unorm) {
        return Error{ErrorCode::unsupported_texture_format,
                     "Document images currently support only RGBA8 UNORM pixels"};
    }
    const std::size_t width = image_width;
    const std::size_t height = image_height;
    if (width > model_maximum_image_bytes / 4 || height > model_maximum_image_bytes / (width * 4)) {
        return Error{ErrorCode::decoded_image_size_overflow,
                     "Image dimensions overflow the decoded byte limit"};
    }
    return width * height * 4U;
}

[[nodiscard]] Result<void>
validate_image_description(const ModelImageDescription& description) noexcept {
    const Result<std::size_t> expected_bytes =
        expected_image_bytes(description.width, description.height, description.format);
    if (!expected_bytes) {
        return expected_bytes.error();
    }
    if (description.pixels.size() != expected_bytes.value()) {
        return Error{ErrorCode::invalid_argument, "Image pixels must be tightly packed RGBA8 rows"};
    }
    if (!source_bytes_match(description.source_mime_type, description.source_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Image source bytes must be an optional matching PNG or JPEG stream"};
    }
    return {};
}

[[nodiscard]] Bounds3 merge(Bounds3 left, Bounds3 right) noexcept {
    return Bounds3{
        Float3{std::min(left.minimum.x, right.minimum.x), std::min(left.minimum.y, right.minimum.y),
               std::min(left.minimum.z, right.minimum.z)},
        Float3{std::max(left.maximum.x, right.maximum.x), std::max(left.maximum.y, right.maximum.y),
               std::max(left.maximum.z, right.maximum.z)},
    };
}

[[nodiscard]] Result<Bounds3> primitive_bounds(const PrimitiveDataView& data) noexcept {
    if (data.positions.empty()) {
        return Error{ErrorCode::invalid_mesh_data, "A model primitive requires positions"};
    }
    Bounds3 bounds{data.positions.front(), data.positions.front()};
    for (const Float3 position : data.positions) {
        if (!finite(position)) {
            return Error{ErrorCode::non_finite_position,
                         "Primitive positions must contain only finite values"};
        }
        bounds.minimum.x = std::min(bounds.minimum.x, position.x);
        bounds.minimum.y = std::min(bounds.minimum.y, position.y);
        bounds.minimum.z = std::min(bounds.minimum.z, position.z);
        bounds.maximum.x = std::max(bounds.maximum.x, position.x);
        bounds.maximum.y = std::max(bounds.maximum.y, position.y);
        bounds.maximum.z = std::max(bounds.maximum.z, position.z);
    }
    return bounds;
}

[[nodiscard]] bool matching_attribute_count(std::size_t count, std::size_t positions) noexcept {
    return count == 0 || count == positions;
}

[[nodiscard]] Result<void> validate_attribute_counts(const PrimitiveDataView& data) noexcept {
    const std::size_t positions = data.positions.size();
    if (matching_attribute_count(data.normals.size(), positions) &&
        matching_attribute_count(data.texcoord0.size(), positions) &&
        matching_attribute_count(data.texcoord1.size(), positions) &&
        matching_attribute_count(data.colors.size(), positions)) {
        return {};
    }
    return Error{ErrorCode::invalid_mesh_data,
                 "Primitive attribute arrays must be empty or match POSITION count"};
}

[[nodiscard]] Result<void> validate_normals(std::span<const Float3> normals) noexcept {
    for (const Float3 normal : normals) {
        if (!finite(normal)) {
            return Error{ErrorCode::invalid_accessor, "Primitive normals must be finite"};
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_texcoords(std::span<const Float2> texcoords,
                                              std::string_view semantic) noexcept {
    for (const Float2 texcoord : texcoords) {
        if (!finite(texcoord)) {
            return Error{ErrorCode::invalid_texcoord, semantic};
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_colors(std::span<const Color4> colors) noexcept {
    for (const Color4 color : colors) {
        if (!finite(color)) {
            return Error{ErrorCode::invalid_accessor, "Primitive colors must be finite"};
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_indices(std::span<const std::uint32_t> indices,
                                            std::size_t vertex_count) noexcept {
    for (const std::uint32_t index : indices) {
        if (static_cast<std::size_t>(index) >= vertex_count) {
            return Error{ErrorCode::mesh_index_out_of_range,
                         "Primitive index is outside the position array"};
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_primitive_data(const PrimitiveDataView& data) noexcept {
    if (data.positions.empty()) {
        return Error{ErrorCode::invalid_mesh_data, "A model primitive requires positions"};
    }
    if (data.indices.empty() || data.indices.size() % 3 != 0) {
        return Error{ErrorCode::invalid_mesh_data,
                     "A model primitive requires triangle-list indices"};
    }

    if (const Result<void> result = validate_attribute_counts(data); !result) {
        return result.error();
    }
    if (const Result<void> result = validate_normals(data.normals); !result) {
        return result.error();
    }
    if (const Result<void> result =
            validate_texcoords(data.texcoord0, "Primitive TEXCOORD_0 values must be finite");
        !result) {
        return result.error();
    }
    if (const Result<void> result =
            validate_texcoords(data.texcoord1, "Primitive TEXCOORD_1 values must be finite");
        !result) {
        return result.error();
    }
    if (const Result<void> result = validate_colors(data.colors); !result) {
        return result.error();
    }
    return validate_indices(data.indices, data.positions.size());
}

[[nodiscard]] PrimitiveData copy_primitive_data(const PrimitiveDataView& view) {
    PrimitiveData result;
    result.positions.assign(view.positions.begin(), view.positions.end());
    result.normals.assign(view.normals.begin(), view.normals.end());
    result.texcoord0.assign(view.texcoord0.begin(), view.texcoord0.end());
    result.texcoord1.assign(view.texcoord1.begin(), view.texcoord1.end());
    result.colors.assign(view.colors.begin(), view.colors.end());
    result.indices.assign(view.indices.begin(), view.indices.end());
    return result;
}

[[nodiscard]] ModelJsonMetadataView metadata_view(const ModelJsonMetadata& metadata) noexcept {
    return ModelJsonMetadataView{
        metadata.extras_json.has_value()
            ? std::optional<std::string_view>{std::string_view{*metadata.extras_json}}
            : std::nullopt,
        metadata.extensions};
}

[[nodiscard]] bool has_metadata(const ModelJsonMetadata& metadata) noexcept {
    return metadata.extras_json.has_value() || !metadata.extensions.empty();
}

inline constexpr std::size_t model_maximum_json_block_bytes = 1024ULL * 1024ULL;
inline constexpr std::size_t model_maximum_json_metadata_bytes = 64ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t model_maximum_extension_name_bytes = 256ULL;

[[nodiscard]] bool valid_metadata(const ModelJsonMetadata& metadata,
                                  std::size_t& total_bytes) noexcept {
    const auto add_size = [&total_bytes](std::size_t size) noexcept {
        if (size > model_maximum_json_metadata_bytes - total_bytes) {
            return false;
        }
        total_bytes += size;
        return true;
    };
    if (metadata.extras_json.has_value() &&
        (metadata.extras_json->empty() ||
         metadata.extras_json->size() > model_maximum_json_block_bytes ||
         !add_size(metadata.extras_json->size()))) {
        return false;
    }
    for (std::size_t index = 0; index < metadata.extensions.size(); ++index) {
        const ModelJsonExtension& extension = metadata.extensions[index];
        if (extension.name.empty() || extension.name.size() > model_maximum_extension_name_bytes ||
            extension.data.empty() || extension.data.size() > model_maximum_json_block_bytes ||
            !add_size(extension.name.size()) || !add_size(extension.data.size())) {
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (metadata.extensions[previous].name == extension.name) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

class Document::Storage final {
  public:
    struct SceneRecord {
        DocumentSceneId id;
        std::string name;
        std::vector<NodeId> roots;
        ModelJsonMetadata metadata;
    };

    struct NodeRecord {
        NodeId id;
        std::string name;
        std::optional<NodeId> parent;
        std::vector<NodeId> children;
        Float4x4 local_matrix;
        std::optional<MeshId> mesh;
        std::optional<ModelPerspectiveCameraDescription> perspective_camera;
        ModelJsonMetadata metadata;
    };

    struct MeshRecord {
        MeshId id;
        std::string name;
        std::vector<PrimitiveId> primitives;
        std::optional<Bounds3> bounds;
        ModelJsonMetadata metadata;
    };

    struct PrimitiveRecord {
        PrimitiveId id;
        MeshId mesh;
        MaterialId material;
        Bounds3 bounds;
        PrimitiveData data;
        ModelJsonMetadata metadata;
    };

    struct MaterialRecord {
        MaterialId id;
        ModelMaterialDescription description;
        ModelJsonMetadata metadata;
    };

    struct ImageRecord {
        ImageId id;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        ModelPixelFormat format = ModelPixelFormat::rgba8_unorm;
        std::vector<std::byte> pixels;
        ModelImageMimeType source_mime_type = ModelImageMimeType::none;
        std::vector<std::byte> source_bytes;
        ModelJsonMetadata metadata;
    };

    struct TextureRecord {
        TextureId id;
        ModelTextureDescription description;
        ModelJsonMetadata metadata;
    };

    struct SamplerRecord {
        SamplerId id;
        ModelSamplerDescription description;
        ModelJsonMetadata metadata;
    };

    [[nodiscard]] std::uintptr_t token() const noexcept {
        return reinterpret_cast<std::uintptr_t>(this);
    }

    [[nodiscard]] bool owns(DocumentSceneId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(NodeId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(MeshId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(PrimitiveId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(MaterialId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(ImageId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(TextureId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(SamplerId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] Result<SceneRecord*> mutable_scene(DocumentSceneId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_argument,
                         "The scene identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= scenes.size()) {
            return Error{ErrorCode::invalid_argument, "The scene identifier is stale"};
        }
        return &scenes[index];
    }

    [[nodiscard]] Result<const SceneRecord*> scene(DocumentSceneId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_argument,
                         "The scene identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= scenes.size()) {
            return Error{ErrorCode::invalid_argument, "The scene identifier is stale"};
        }
        return &scenes[index];
    }

    [[nodiscard]] Result<NodeRecord*> mutable_node(NodeId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_entity,
                         "The node identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= nodes.size()) {
            return Error{ErrorCode::invalid_entity, "The node identifier is stale"};
        }
        return &nodes[index];
    }

    [[nodiscard]] Result<const NodeRecord*> node(NodeId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_entity,
                         "The node identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= nodes.size()) {
            return Error{ErrorCode::invalid_entity, "The node identifier is stale"};
        }
        return &nodes[index];
    }

    [[nodiscard]] Result<MeshRecord*> mutable_mesh(MeshId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The mesh identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= meshes.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The mesh identifier is stale"};
        }
        return &meshes[index];
    }

    [[nodiscard]] Result<const MeshRecord*> mesh(MeshId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The mesh identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= meshes.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The mesh identifier is stale"};
        }
        return &meshes[index];
    }

    [[nodiscard]] Result<PrimitiveRecord*> mutable_primitive(PrimitiveId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The primitive identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= primitives.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The primitive identifier is stale"};
        }
        return &primitives[index];
    }

    [[nodiscard]] Result<const PrimitiveRecord*> primitive(PrimitiveId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The primitive identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= primitives.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The primitive identifier is stale"};
        }
        return &primitives[index];
    }

    [[nodiscard]] Result<const MaterialRecord*> material(MaterialId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_material_handle,
                         "The material identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= materials.size()) {
            return Error{ErrorCode::invalid_material_handle, "The material identifier is stale"};
        }
        return &materials[index];
    }

    [[nodiscard]] Result<const ImageRecord*> image(ImageId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_image_handle,
                         "The image identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= images.size()) {
            return Error{ErrorCode::invalid_image_handle, "The image identifier is stale"};
        }
        return &images[index];
    }

    [[nodiscard]] Result<const TextureRecord*> texture(TextureId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_texture_asset_handle,
                         "The texture identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= textures.size()) {
            return Error{ErrorCode::invalid_texture_asset_handle,
                         "The texture identifier is stale"};
        }
        return &textures[index];
    }

    [[nodiscard]] Result<const SamplerRecord*> sampler(SamplerId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_sampler_description,
                         "The sampler identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= samplers.size()) {
            return Error{ErrorCode::invalid_sampler_description, "The sampler identifier is stale"};
        }
        return &samplers[index];
    }

    [[nodiscard]] bool
    valid_material_textures(const ModelMaterialDescription& description) const noexcept {
        return valid_texture_or_empty(description.base_color_texture) &&
               valid_texture_or_empty(description.metallic_roughness_texture) &&
               valid_texture_or_empty(description.normal_texture) &&
               valid_texture_or_empty(description.occlusion_texture) &&
               valid_texture_or_empty(description.emissive_texture);
    }

    [[nodiscard]] Result<void> reject_parent_cycle(NodeId child_id,
                                                   NodeId parent_id) const noexcept {
        std::optional<NodeId> ancestor = parent_id;
        while (ancestor.has_value()) {
            if (*ancestor == child_id) {
                return Error{ErrorCode::hierarchy_cycle,
                             "The parent assignment would create a document node cycle"};
            }
            const Result<const NodeRecord*> ancestor_record = node(*ancestor);
            if (!ancestor_record) {
                return ancestor_record.error();
            }
            ancestor = ancestor_record.value()->parent;
        }
        return {};
    }

    void detach_from_existing_parent(NodeRecord& child, NodeId child_id) noexcept {
        if (!child.parent.has_value()) {
            return;
        }
        Result<NodeRecord*> old_parent = mutable_node(*child.parent);
        if (!old_parent) {
            return;
        }
        auto& siblings = old_parent.value()->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child_id), siblings.end());
    }

    void remove_scene_root_entries(NodeId node_id) noexcept {
        for (SceneRecord& scene_record : scenes) {
            scene_record.roots.erase(
                std::remove(scene_record.roots.begin(), scene_record.roots.end(), node_id),
                scene_record.roots.end());
        }
    }

    void update_mesh_bounds(MeshRecord& mesh) noexcept {
        mesh.bounds.reset();
        for (const PrimitiveId primitive_id : mesh.primitives) {
            const Result<const PrimitiveRecord*> primitive_result = primitive(primitive_id);
            if (!primitive_result) {
                continue;
            }
            mesh.bounds = mesh.bounds.has_value()
                              ? merge(*mesh.bounds, primitive_result.value()->bounds)
                              : primitive_result.value()->bounds;
        }
    }

    void note_mutation() noexcept {
        preserved_metadata_stale = preserved_metadata_stale || has_preserved_metadata;
    }

    ModelJsonMetadata root_metadata;
    ModelJsonMetadata asset_metadata;
    std::vector<SceneRecord> scenes;
    std::optional<DocumentSceneId> default_scene;
    std::vector<NodeRecord> nodes;
    std::vector<MeshRecord> meshes;
    std::vector<PrimitiveRecord> primitives;
    std::vector<MaterialRecord> materials;
    std::vector<ImageRecord> images;
    std::vector<TextureRecord> textures;
    std::vector<SamplerRecord> samplers;
    bool has_preserved_metadata = false;
    bool preserved_metadata_stale = false;

  private:
    [[nodiscard]] bool valid_texture_or_empty(TextureId texture_id) const noexcept {
        return !texture_id.is_valid() || static_cast<bool>(texture(texture_id));
    }
};

bool DocumentValidationReport::has_errors() const noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const DocumentDiagnostic& diagnostic) noexcept {
                           return diagnostic.severity == DocumentDiagnosticSeverity::error;
                       });
}

Document::Document() : storage_(std::make_unique<Storage>()) {}

Document::~Document() noexcept = default;

Document::Document(Document&&) noexcept = default;

Document& Document::operator=(Document&&) noexcept = default;

DocumentView Document::view() const noexcept {
    return DocumentView{this};
}

DocumentStatistics Document::statistics() const noexcept {
    DocumentStatistics result;
    if (storage_ == nullptr) {
        return result;
    }
    result.scenes = static_cast<std::uint64_t>(storage_->scenes.size());
    result.nodes = static_cast<std::uint64_t>(storage_->nodes.size());
    result.meshes = static_cast<std::uint64_t>(storage_->meshes.size());
    result.primitives = static_cast<std::uint64_t>(storage_->primitives.size());
    result.materials = static_cast<std::uint64_t>(storage_->materials.size());
    result.images = static_cast<std::uint64_t>(storage_->images.size());
    result.textures = static_cast<std::uint64_t>(storage_->textures.size());
    result.samplers = static_cast<std::uint64_t>(storage_->samplers.size());
    for (const Storage::NodeRecord& node : storage_->nodes) {
        if (node.perspective_camera.has_value()) {
            ++result.perspective_cameras;
        }
    }
    for (const Storage::PrimitiveRecord& primitive : storage_->primitives) {
        result.vertices += static_cast<std::uint64_t>(primitive.data.positions.size());
        result.indices += static_cast<std::uint64_t>(primitive.data.indices.size());
        result.triangles += static_cast<std::uint64_t>(primitive.data.indices.size() / 3);
    }
    for (const Storage::ImageRecord& image : storage_->images) {
        result.decoded_image_bytes += static_cast<std::uint64_t>(image.pixels.size());
    }
    for (const Storage::MaterialRecord& material : storage_->materials) {
        result.materials_with_base_color_textures +=
            material.description.base_color_texture.is_valid() ? 1U : 0U;
        result.materials_with_metallic_roughness_textures +=
            material.description.metallic_roughness_texture.is_valid() ? 1U : 0U;
        result.materials_with_normal_textures +=
            material.description.normal_texture.is_valid() ? 1U : 0U;
        result.materials_with_occlusion_textures +=
            material.description.occlusion_texture.is_valid() ? 1U : 0U;
        result.materials_with_emissive_textures +=
            material.description.emissive_texture.is_valid() ? 1U : 0U;
    }
    return result;
}

std::optional<DocumentSceneId> Document::default_scene() const noexcept {
    return storage_ != nullptr ? storage_->default_scene : std::nullopt;
}

ModelJsonMetadataView Document::root_metadata() const noexcept {
    return storage_ != nullptr ? metadata_view(storage_->root_metadata) : ModelJsonMetadataView{};
}

ModelJsonMetadataView Document::asset_metadata() const noexcept {
    return storage_ != nullptr ? metadata_view(storage_->asset_metadata) : ModelJsonMetadataView{};
}

bool Document::preserved_metadata_stale() const noexcept {
    return storage_ != nullptr && storage_->preserved_metadata_stale;
}

std::size_t Document::scene_count() const noexcept {
    return storage_ != nullptr ? storage_->scenes.size() : 0;
}

std::size_t Document::node_count() const noexcept {
    return storage_ != nullptr ? storage_->nodes.size() : 0;
}

std::size_t Document::mesh_count() const noexcept {
    return storage_ != nullptr ? storage_->meshes.size() : 0;
}

std::size_t Document::primitive_count() const noexcept {
    return storage_ != nullptr ? storage_->primitives.size() : 0;
}

std::size_t Document::material_count() const noexcept {
    return storage_ != nullptr ? storage_->materials.size() : 0;
}

std::size_t Document::image_count() const noexcept {
    return storage_ != nullptr ? storage_->images.size() : 0;
}

std::size_t Document::texture_count() const noexcept {
    return storage_ != nullptr ? storage_->textures.size() : 0;
}

std::size_t Document::sampler_count() const noexcept {
    return storage_ != nullptr ? storage_->samplers.size() : 0;
}

Result<DocumentSceneView> Document::scene_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->scenes.size()) {
        return Error{ErrorCode::invalid_argument,
                     "The document scene index is outside the document"};
    }
    const Storage::SceneRecord& record = storage_->scenes[index];
    return DocumentSceneView{record.id, record.name, record.roots, metadata_view(record.metadata)};
}

Result<NodeView> Document::node_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->nodes.size()) {
        return Error{ErrorCode::invalid_entity, "The document node index is outside the document"};
    }
    const Storage::NodeRecord& record = storage_->nodes[index];
    return NodeView{record.id,
                    record.name,
                    record.parent,
                    record.children,
                    record.local_matrix,
                    record.mesh,
                    record.perspective_camera,
                    metadata_view(record.metadata)};
}

Result<MeshView> Document::mesh_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->meshes.size()) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The document mesh index is outside the document"};
    }
    const Storage::MeshRecord& record = storage_->meshes[index];
    return MeshView{record.id, record.name, record.primitives, record.bounds,
                    metadata_view(record.metadata)};
}

Result<PrimitiveView> Document::primitive_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->primitives.size()) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The document primitive index is outside the document"};
    }
    const Storage::PrimitiveRecord& record = storage_->primitives[index];
    return PrimitiveView{record.id,     record.mesh,        record.material,
                         record.bounds, record.data.view(), metadata_view(record.metadata)};
}

Result<MaterialView> Document::material_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->materials.size()) {
        return Error{ErrorCode::invalid_material_handle,
                     "The document material index is outside the document"};
    }
    const Storage::MaterialRecord& record = storage_->materials[index];
    return MaterialView{record.id, record.description, metadata_view(record.metadata)};
}

Result<ImageView> Document::image_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->images.size()) {
        return Error{ErrorCode::invalid_image_handle,
                     "The document image index is outside the document"};
    }
    const Storage::ImageRecord& record = storage_->images[index];
    return ImageView{record.id,           record.width,
                     record.height,       record.format,
                     record.pixels,       record.source_mime_type,
                     record.source_bytes, metadata_view(record.metadata)};
}

Result<TextureView> Document::texture_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->textures.size()) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "The document texture index is outside the document"};
    }
    const Storage::TextureRecord& record = storage_->textures[index];
    return TextureView{record.id, record.description, metadata_view(record.metadata)};
}

Result<SamplerView> Document::sampler_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->samplers.size()) {
        return Error{ErrorCode::invalid_sampler_description,
                     "The document sampler index is outside the document"};
    }
    const Storage::SamplerRecord& record = storage_->samplers[index];
    return SamplerView{record.id, record.description, metadata_view(record.metadata)};
}

Result<DocumentSceneView> Document::scene(DocumentSceneId scene_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument, "The document is empty"};
    }
    const Result<const Storage::SceneRecord*> record = storage_->scene(scene_id);
    if (!record) {
        return record.error();
    }
    return DocumentSceneView{record.value()->id, record.value()->name, record.value()->roots,
                             metadata_view(record.value()->metadata)};
}

Result<NodeView> Document::node(NodeId node_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    const Result<const Storage::NodeRecord*> record = storage_->node(node_id);
    if (!record) {
        return record.error();
    }
    return NodeView{record.value()->id,
                    record.value()->name,
                    record.value()->parent,
                    record.value()->children,
                    record.value()->local_matrix,
                    record.value()->mesh,
                    record.value()->perspective_camera,
                    metadata_view(record.value()->metadata)};
}

Result<MeshView> Document::mesh(MeshId mesh_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    const Result<const Storage::MeshRecord*> record = storage_->mesh(mesh_id);
    if (!record) {
        return record.error();
    }
    return MeshView{record.value()->id, record.value()->name, record.value()->primitives,
                    record.value()->bounds, metadata_view(record.value()->metadata)};
}

Result<PrimitiveView> Document::primitive(PrimitiveId primitive_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    const Result<const Storage::PrimitiveRecord*> record = storage_->primitive(primitive_id);
    if (!record) {
        return record.error();
    }
    return PrimitiveView{record.value()->id,          record.value()->mesh,
                         record.value()->material,    record.value()->bounds,
                         record.value()->data.view(), metadata_view(record.value()->metadata)};
}

Result<MaterialView> Document::material(MaterialId material_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_material_handle, "The document is empty"};
    }
    const Result<const Storage::MaterialRecord*> record = storage_->material(material_id);
    if (!record) {
        return record.error();
    }
    return MaterialView{record.value()->id, record.value()->description,
                        metadata_view(record.value()->metadata)};
}

Result<ImageView> Document::image(ImageId image_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_image_handle, "The document is empty"};
    }
    const Result<const Storage::ImageRecord*> record = storage_->image(image_id);
    if (!record) {
        return record.error();
    }
    return ImageView{record.value()->id,
                     record.value()->width,
                     record.value()->height,
                     record.value()->format,
                     std::span<const std::byte>{record.value()->pixels},
                     record.value()->source_mime_type,
                     std::span<const std::byte>{record.value()->source_bytes},
                     metadata_view(record.value()->metadata)};
}

Result<TextureView> Document::texture(TextureId texture_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_texture_asset_handle, "The document is empty"};
    }
    const Result<const Storage::TextureRecord*> record = storage_->texture(texture_id);
    if (!record) {
        return record.error();
    }
    return TextureView{record.value()->id, record.value()->description,
                       metadata_view(record.value()->metadata)};
}

Result<SamplerView> Document::sampler(SamplerId sampler_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_sampler_description, "The document is empty"};
    }
    const Result<const Storage::SamplerRecord*> record = storage_->sampler(sampler_id);
    if (!record) {
        return record.error();
    }
    return SamplerView{record.value()->id, record.value()->description,
                       metadata_view(record.value()->metadata)};
}

std::optional<Bounds3> Document::bounds() const noexcept {
    if (storage_ == nullptr) {
        return std::nullopt;
    }
    std::optional<Bounds3> result;
    for (const Storage::MeshRecord& mesh_record : storage_->meshes) {
        if (!mesh_record.bounds.has_value()) {
            continue;
        }
        result = result.has_value() ? merge(*result, *mesh_record.bounds) : *mesh_record.bounds;
    }
    return result;
}

Result<DocumentSceneId> Document::create_scene(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const DocumentSceneId id = model::detail::DocumentHandleAccess::create_scene(
        storage_->token(), static_cast<std::uint64_t>(storage_->scenes.size() + 1U));
    Storage::SceneRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->scenes.push_back(std::move(record));
    if (!storage_->default_scene.has_value()) {
        storage_->default_scene = id;
    }
    storage_->note_mutation();
    return id;
}

Result<NodeId> Document::create_node(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const NodeId id = model::detail::DocumentHandleAccess::create_node(
        storage_->token(), static_cast<std::uint64_t>(storage_->nodes.size() + 1U));
    Storage::NodeRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->nodes.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<MeshId> Document::create_mesh(std::string_view name) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const MeshId id = model::detail::DocumentHandleAccess::create_mesh(
        storage_->token(), static_cast<std::uint64_t>(storage_->meshes.size() + 1U));
    Storage::MeshRecord record;
    record.id = id;
    record.name.assign(name);
    storage_->meshes.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<MaterialId> Document::create_material(const ModelMaterialDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!storage_->valid_material_textures(description)) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "Material texture identifiers must belong to the same document"};
    }
    if (!valid_material_factors(description) || !valid_material_mappings(description)) {
        return Error{ErrorCode::invalid_material_handle,
                     "Material factors and texture mappings must be finite and valid"};
    }
    const MaterialId id = model::detail::DocumentHandleAccess::create_material(
        storage_->token(), static_cast<std::uint64_t>(storage_->materials.size() + 1U));
    storage_->materials.push_back(Storage::MaterialRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<ImageId> Document::create_image(const ModelImageDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    const Result<void> validation = validate_image_description(description);
    if (!validation) {
        return validation.error();
    }
    const ImageId id = model::detail::DocumentHandleAccess::create_image(
        storage_->token(), static_cast<std::uint64_t>(storage_->images.size() + 1U));
    Storage::ImageRecord record;
    record.id = id;
    record.width = description.width;
    record.height = description.height;
    record.format = description.format;
    record.pixels.assign(description.pixels.begin(), description.pixels.end());
    record.source_mime_type = description.source_mime_type;
    record.source_bytes.assign(description.source_bytes.begin(), description.source_bytes.end());
    storage_->images.push_back(std::move(record));
    storage_->note_mutation();
    return id;
}

Result<SamplerId> Document::create_sampler(const ModelSamplerDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!valid_wrap(description.wrap_u) || !valid_wrap(description.wrap_v) ||
        !valid_filter(description.min_filter) || !valid_mag_filter(description.mag_filter)) {
        return Error{ErrorCode::invalid_sampler_description,
                     "Sampler wrap and filter values must be valid"};
    }
    const SamplerId id = model::detail::DocumentHandleAccess::create_sampler(
        storage_->token(), static_cast<std::uint64_t>(storage_->samplers.size() + 1U));
    storage_->samplers.push_back(Storage::SamplerRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<TextureId> Document::create_texture(const ModelTextureDescription& description) {
    if (storage_ == nullptr) {
        storage_ = std::make_unique<Storage>();
    }
    if (!storage_->image(description.image)) {
        return Error{ErrorCode::invalid_image_handle,
                     "Texture image must belong to the same document"};
    }
    if (!storage_->sampler(description.sampler)) {
        return Error{ErrorCode::invalid_sampler_description,
                     "Texture sampler must belong to the same document"};
    }
    const TextureId id = model::detail::DocumentHandleAccess::create_texture(
        storage_->token(), static_cast<std::uint64_t>(storage_->textures.size() + 1U));
    storage_->textures.push_back(Storage::TextureRecord{id, description});
    storage_->note_mutation();
    return id;
}

Result<PrimitiveId> Document::create_primitive(MeshId mesh_id, MaterialId material_id,
                                               const PrimitiveDataView& data) {
    const Result<void> validation = validate_primitive_data(data);
    if (!validation) {
        return validation.error();
    }
    return create_primitive(mesh_id, material_id, copy_primitive_data(data));
}

Result<PrimitiveId> Document::create_primitive(MeshId mesh_id, MaterialId material_id,
                                               PrimitiveData&& data) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "Primitive creation requires a live document"};
    }
    Result<Storage::MeshRecord*> mesh_record = storage_->mutable_mesh(mesh_id);
    if (!mesh_record) {
        return mesh_record.error();
    }
    if (!storage_->material(material_id)) {
        return Error{ErrorCode::invalid_material_handle,
                     "Primitive material must belong to the same document"};
    }
    const Result<void> validation = validate_primitive_data(data.view());
    if (!validation) {
        return validation.error();
    }
    const Result<Bounds3> bounds = primitive_bounds(data.view());
    if (!bounds) {
        return bounds.error();
    }
    const PrimitiveId id = model::detail::DocumentHandleAccess::create_primitive(
        storage_->token(), static_cast<std::uint64_t>(storage_->primitives.size() + 1U));
    Storage::PrimitiveRecord record;
    record.id = id;
    record.mesh = mesh_id;
    record.material = material_id;
    record.bounds = bounds.value();
    record.data = std::move(data);
    storage_->primitives.push_back(std::move(record));
    mesh_record.value()->primitives.push_back(id);
    storage_->update_mesh_bounds(*mesh_record.value());
    storage_->note_mutation();
    return id;
}

Result<void> Document::add_scene_root(DocumentSceneId scene_id, NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument, "The document is empty"};
    }
    Result<Storage::SceneRecord*> scene_record = storage_->mutable_scene(scene_id);
    if (!scene_record) {
        return scene_record.error();
    }
    const Result<const Storage::NodeRecord*> node_record = storage_->node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->parent.has_value()) {
        return Error{ErrorCode::invalid_parent_assignment,
                     "A scene root node cannot already have a parent"};
    }
    if (std::find(scene_record.value()->roots.begin(), scene_record.value()->roots.end(),
                  node_id) == scene_record.value()->roots.end()) {
        scene_record.value()->roots.push_back(node_id);
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::set_default_scene(DocumentSceneId scene_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument, "The document is empty"};
    }
    if (!storage_->scene(scene_id)) {
        return Error{ErrorCode::invalid_argument,
                     "The default scene must belong to the same document"};
    }
    if (storage_->default_scene != scene_id) {
        storage_->default_scene = scene_id;
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::clear_default_scene() noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument, "The document is empty"};
    }
    if (storage_->default_scene.has_value()) {
        storage_->default_scene.reset();
        storage_->note_mutation();
    }
    return {};
}

Result<void> Document::set_parent(NodeId node_id, NodeId parent_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    Result<Storage::NodeRecord*> child = storage_->mutable_node(node_id);
    if (!child) {
        return child.error();
    }
    Result<Storage::NodeRecord*> parent = storage_->mutable_node(parent_id);
    if (!parent) {
        return parent.error();
    }
    if (node_id == parent_id) {
        return Error{ErrorCode::invalid_parent_assignment, "A node cannot be its own parent"};
    }
    const Result<void> cycle_result = storage_->reject_parent_cycle(node_id, parent_id);
    if (!cycle_result) {
        return cycle_result.error();
    }
    if (child.value()->parent == parent_id) {
        return {};
    }
    storage_->detach_from_existing_parent(*child.value(), node_id);
    child.value()->parent = parent_id;
    parent.value()->children.push_back(node_id);
    storage_->remove_scene_root_entries(node_id);
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_parent(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->parent.has_value()) {
        return {};
    }
    Result<Storage::NodeRecord*> parent_record =
        storage_->mutable_node(*node_record.value()->parent);
    if (parent_record) {
        auto& siblings = parent_record.value()->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), node_id), siblings.end());
    }
    node_record.value()->parent.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::set_node_mesh(NodeId node_id, MeshId mesh_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!storage_->mesh(mesh_id)) {
        return Error{ErrorCode::invalid_mesh_handle, "Node mesh must belong to the same document"};
    }
    if (node_record.value()->mesh == mesh_id) {
        return {};
    }
    node_record.value()->mesh = mesh_id;
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_node_mesh(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->mesh.has_value()) {
        return {};
    }
    node_record.value()->mesh.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::set_node_matrix(NodeId node_id, const Float4x4& matrix) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    if (!finite(matrix)) {
        return Error{ErrorCode::invalid_transform_matrix, "Node matrices must be finite"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->local_matrix == matrix) {
        return {};
    }
    node_record.value()->local_matrix = matrix;
    storage_->note_mutation();
    return {};
}

Result<void>
Document::set_node_perspective_camera(NodeId node_id,
                                      const ModelPerspectiveCameraDescription& description) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    if (!valid_perspective_camera(description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "A perspective camera requires a field of view in (0, pi), positive near "
                     "plane, and farther far plane"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (node_record.value()->perspective_camera == description) {
        return {};
    }
    node_record.value()->perspective_camera = description;
    storage_->note_mutation();
    return {};
}

Result<void> Document::clear_node_perspective_camera(NodeId node_id) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_entity, "The document is empty"};
    }
    Result<Storage::NodeRecord*> node_record = storage_->mutable_node(node_id);
    if (!node_record) {
        return node_record.error();
    }
    if (!node_record.value()->perspective_camera.has_value()) {
        return {};
    }
    node_record.value()->perspective_camera.reset();
    storage_->note_mutation();
    return {};
}

Result<void> Document::replace_primitive(PrimitiveId primitive_id, const PrimitiveDataView& data) {
    const Result<void> validation = validate_primitive_data(data);
    if (!validation) {
        return validation.error();
    }
    return replace_primitive(primitive_id, copy_primitive_data(data));
}

Result<void> Document::replace_primitive(PrimitiveId primitive_id, PrimitiveData&& data) {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    Result<Storage::PrimitiveRecord*> primitive_record = storage_->mutable_primitive(primitive_id);
    if (!primitive_record) {
        return primitive_record.error();
    }
    const Result<void> validation = validate_primitive_data(data.view());
    if (!validation) {
        return validation.error();
    }
    const Result<Bounds3> bounds = primitive_bounds(data.view());
    if (!bounds) {
        return bounds.error();
    }
    primitive_record.value()->data = std::move(data);
    primitive_record.value()->bounds = bounds.value();
    Result<Storage::MeshRecord*> mesh_record =
        storage_->mutable_mesh(primitive_record.value()->mesh);
    if (mesh_record) {
        storage_->update_mesh_bounds(*mesh_record.value());
    }
    storage_->note_mutation();
    return {};
}

Result<std::span<Float3>> Document::mutable_positions(PrimitiveId primitive_id) noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    Result<Storage::PrimitiveRecord*> primitive_record = storage_->mutable_primitive(primitive_id);
    if (!primitive_record) {
        return primitive_record.error();
    }
    storage_->note_mutation();
    return std::span<Float3>{primitive_record.value()->data.positions};
}

Result<std::span<Float3>> Document::mutable_normals(PrimitiveId primitive_id) noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    Result<Storage::PrimitiveRecord*> primitive_record = storage_->mutable_primitive(primitive_id);
    if (!primitive_record) {
        return primitive_record.error();
    }
    storage_->note_mutation();
    return std::span<Float3>{primitive_record.value()->data.normals};
}

Result<void> Document::update_primitive_bounds(PrimitiveId primitive_id) noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_mesh_handle, "The document is empty"};
    }
    Result<Storage::PrimitiveRecord*> primitive_record = storage_->mutable_primitive(primitive_id);
    if (!primitive_record) {
        return primitive_record.error();
    }
    const Result<Bounds3> bounds = primitive_bounds(primitive_record.value()->data.view());
    if (!bounds) {
        return bounds.error();
    }
    primitive_record.value()->bounds = bounds.value();
    Result<Storage::MeshRecord*> mesh_record =
        storage_->mutable_mesh(primitive_record.value()->mesh);
    if (mesh_record) {
        storage_->update_mesh_bounds(*mesh_record.value());
    }
    storage_->note_mutation();
    return {};
}

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
    return document_ != nullptr
               ? document_->scene_at(index)
               : Result<DocumentSceneView>{Error{ErrorCode::invalid_argument, "The view is empty"}};
}

Result<NodeView> DocumentView::node_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->node_at(index)
               : Result<NodeView>{Error{ErrorCode::invalid_entity, "The view is empty"}};
}

Result<MeshView> DocumentView::mesh_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->mesh_at(index)
               : Result<MeshView>{Error{ErrorCode::invalid_mesh_handle, "The view is empty"}};
}

Result<PrimitiveView> DocumentView::primitive_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->primitive_at(index)
               : Result<PrimitiveView>{Error{ErrorCode::invalid_mesh_handle, "The view is empty"}};
}

Result<MaterialView> DocumentView::material_at(std::size_t index) const noexcept {
    return document_ != nullptr ? document_->material_at(index)
                                : Result<MaterialView>{Error{ErrorCode::invalid_material_handle,
                                                             "The view is empty"}};
}

Result<ImageView> DocumentView::image_at(std::size_t index) const noexcept {
    return document_ != nullptr
               ? document_->image_at(index)
               : Result<ImageView>{Error{ErrorCode::invalid_image_handle, "The view is empty"}};
}

Result<TextureView> DocumentView::texture_at(std::size_t index) const noexcept {
    return document_ != nullptr ? document_->texture_at(index)
                                : Result<TextureView>{Error{ErrorCode::invalid_texture_asset_handle,
                                                            "The view is empty"}};
}

Result<SamplerView> DocumentView::sampler_at(std::size_t index) const noexcept {
    return document_ != nullptr ? document_->sampler_at(index)
                                : Result<SamplerView>{Error{ErrorCode::invalid_sampler_description,
                                                            "The view is empty"}};
}

Result<DocumentSceneView> DocumentView::scene(DocumentSceneId scene_id) const noexcept {
    return document_ != nullptr
               ? document_->scene(scene_id)
               : Result<DocumentSceneView>{Error{ErrorCode::invalid_argument, "The view is empty"}};
}

Result<NodeView> DocumentView::node(NodeId node_id) const noexcept {
    return document_ != nullptr
               ? document_->node(node_id)
               : Result<NodeView>{Error{ErrorCode::invalid_entity, "The view is empty"}};
}

Result<MeshView> DocumentView::mesh(MeshId mesh_id) const noexcept {
    return document_ != nullptr
               ? document_->mesh(mesh_id)
               : Result<MeshView>{Error{ErrorCode::invalid_mesh_handle, "The view is empty"}};
}

Result<PrimitiveView> DocumentView::primitive(PrimitiveId primitive_id) const noexcept {
    return document_ != nullptr
               ? document_->primitive(primitive_id)
               : Result<PrimitiveView>{Error{ErrorCode::invalid_mesh_handle, "The view is empty"}};
}

Result<MaterialView> DocumentView::material(MaterialId material_id) const noexcept {
    return document_ != nullptr ? document_->material(material_id)
                                : Result<MaterialView>{Error{ErrorCode::invalid_material_handle,
                                                             "The view is empty"}};
}

Result<ImageView> DocumentView::image(ImageId image_id) const noexcept {
    return document_ != nullptr
               ? document_->image(image_id)
               : Result<ImageView>{Error{ErrorCode::invalid_image_handle, "The view is empty"}};
}

Result<TextureView> DocumentView::texture(TextureId texture_id) const noexcept {
    return document_ != nullptr ? document_->texture(texture_id)
                                : Result<TextureView>{Error{ErrorCode::invalid_texture_asset_handle,
                                                            "The view is empty"}};
}

Result<SamplerView> DocumentView::sampler(SamplerId sampler_id) const noexcept {
    return document_ != nullptr ? document_->sampler(sampler_id)
                                : Result<SamplerView>{Error{ErrorCode::invalid_sampler_description,
                                                            "The view is empty"}};
}

std::optional<Bounds3> DocumentView::bounds() const noexcept {
    return document_ != nullptr ? document_->bounds() : std::nullopt;
}

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

Result<SamplerId> DocumentBuilder::create_sampler(const ModelSamplerDescription& description) {
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
                                             const ModelPerspectiveCameraDescription& description) {
    return document_.set_node_perspective_camera(node_id, description);
}

Result<Document> DocumentBuilder::finish() {
    DocumentValidationReport report = validate_document(document_.view());
    if (report.has_errors()) {
        return Error{ErrorCode::invalid_argument, "Document validation failed"};
    }
    return std::move(document_);
}

namespace model::detail {

Result<void> DocumentMetadataAccess::attach_import_metadata(Document& document,
                                                            ImportedDocumentMetadata&& metadata) {
    if (document.storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "Imported metadata requires a live model document"};
    }
    Document::Storage& storage = *document.storage_;
    std::size_t total_bytes = 0;
    const auto validate_block = [&total_bytes](const ModelJsonMetadata& block) noexcept {
        return valid_metadata(block, total_bytes);
    };
    if (!validate_block(metadata.root) || !validate_block(metadata.asset)) {
        return Error{ErrorCode::invalid_argument,
                     "Imported JSON metadata is malformed or exceeds its resource budget"};
    }
    for (const auto& entry : metadata.scenes) {
        if (!storage.scene(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported scene metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.nodes) {
        if (!storage.node(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported node metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.meshes) {
        if (!storage.mesh(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported mesh metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.primitives) {
        if (!storage.primitive(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported primitive metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.materials) {
        if (!storage.material(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported material metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.images) {
        if (!storage.image(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported image metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.textures) {
        if (!storage.texture(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported texture metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.samplers) {
        if (!storage.sampler(entry.first) || !validate_block(entry.second)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported sampler metadata does not match the document"};
        }
    }

    bool any_metadata = has_metadata(metadata.root) || has_metadata(metadata.asset);
    storage.root_metadata = std::move(metadata.root);
    storage.asset_metadata = std::move(metadata.asset);
    for (auto& entry : metadata.scenes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.scenes[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.nodes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.nodes[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.meshes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.meshes[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.primitives) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.primitives[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.materials) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.materials[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.images) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.images[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.textures) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.textures[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.samplers) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.samplers[index].metadata = std::move(entry.second);
    }
    storage.has_preserved_metadata = any_metadata;
    storage.preserved_metadata_stale = false;
    return {};
}

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
