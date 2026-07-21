#include <elf3d/model/detail/document_storage.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace elf3d::model::detail {
namespace {

[[nodiscard]] std::uint64_t allocate_document_owner_token() noexcept {
    static std::atomic<std::uint64_t> next_token{1};
    const std::uint64_t token = next_token.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        fatal_error("Elf3D exhausted document owner identities");
    }
    return token;
}

} // namespace

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

[[nodiscard]] bool valid_alpha(AlphaMode mode) noexcept {
    return mode == AlphaMode::opaque || mode == AlphaMode::mask || mode == AlphaMode::blend;
}

[[nodiscard]] bool valid_wrap(TextureWrap wrap) noexcept {
    return wrap == TextureWrap::repeat || wrap == TextureWrap::mirrored_repeat ||
           wrap == TextureWrap::clamp_to_edge;
}

[[nodiscard]] bool valid_filter(TextureFilter filter) noexcept {
    return filter == TextureFilter::nearest || filter == TextureFilter::linear ||
           filter == TextureFilter::nearest_mipmap_nearest ||
           filter == TextureFilter::linear_mipmap_nearest ||
           filter == TextureFilter::nearest_mipmap_linear ||
           filter == TextureFilter::linear_mipmap_linear;
}

[[nodiscard]] bool valid_mag_filter(TextureFilter filter) noexcept {
    return filter == TextureFilter::nearest || filter == TextureFilter::linear;
}

[[nodiscard]] bool
valid_perspective_camera(const PerspectiveCameraDescription& description) noexcept {
    constexpr float pi = 3.14159265358979323846F;
    return finite(description.vertical_field_of_view_radians) && finite(description.near_plane) &&
           finite(description.far_plane) && description.vertical_field_of_view_radians > 0.0F &&
           description.vertical_field_of_view_radians < pi && description.near_plane > 0.0F &&
           description.far_plane > description.near_plane;
}

[[nodiscard]] bool valid_mapping(TextureMapping mapping) noexcept {
    return mapping.texcoord_set < maximum_texture_coordinate_sets &&
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

[[nodiscard]] bool png_source_bytes_match(std::span<const std::byte> bytes) noexcept {
    constexpr std::array<std::uint8_t, 8> signature{0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
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

[[nodiscard]] bool jpeg_source_bytes_match(std::span<const std::byte> bytes) noexcept {
    return bytes.size() >= 3U && std::to_integer<std::uint8_t>(bytes[0]) == 0xffU &&
           std::to_integer<std::uint8_t>(bytes[1]) == 0xd8U &&
           std::to_integer<std::uint8_t>(bytes[2]) == 0xffU;
}

[[nodiscard]] bool source_bytes_match(ModelImageMimeType mime,
                                      std::span<const std::byte> bytes) noexcept {
    if (mime == ModelImageMimeType::none) {
        return bytes.empty();
    }
    if (bytes.empty() || bytes.size() > model_maximum_source_image_bytes) {
        return false;
    }
    if (mime == ModelImageMimeType::png) {
        return png_source_bytes_match(bytes);
    }
    return mime == ModelImageMimeType::jpeg && jpeg_source_bytes_match(bytes);
}

[[nodiscard]] Result<std::size_t> expected_image_bytes(std::uint32_t image_width,
                                                       std::uint32_t image_height,
                                                       PixelFormat format) noexcept {
    if (image_width == 0 || image_height == 0) {
        return Error{ErrorCode::zero_image_dimensions, "Images require nonzero dimensions"};
    }
    if (format != PixelFormat::rgba8_unorm) {
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

[[nodiscard]] bool add_metadata_size(std::size_t size, std::size_t& total_bytes) noexcept {
    if (size > model_maximum_json_metadata_bytes - total_bytes) {
        return false;
    }
    total_bytes += size;
    return true;
}

[[nodiscard]] bool valid_metadata_extras(const ModelJsonMetadata& metadata,
                                         std::size_t& total_bytes) noexcept {
    if (!metadata.extras_json.has_value()) {
        return true;
    }
    return !metadata.extras_json->empty() &&
           metadata.extras_json->size() <= model_maximum_json_block_bytes &&
           add_metadata_size(metadata.extras_json->size(), total_bytes);
}

[[nodiscard]] bool valid_metadata_extension(const ModelJsonExtension& extension,
                                            std::size_t& total_bytes) noexcept {
    return !extension.name.empty() && extension.name.size() <= model_maximum_extension_name_bytes &&
           !extension.data.empty() && extension.data.size() <= model_maximum_json_block_bytes &&
           add_metadata_size(extension.name.size(), total_bytes) &&
           add_metadata_size(extension.data.size(), total_bytes);
}

[[nodiscard]] bool unique_extension_name(const ModelJsonMetadata& metadata,
                                         std::size_t index) noexcept {
    for (std::size_t previous = 0; previous < index; ++previous) {
        if (metadata.extensions[previous].name == metadata.extensions[index].name) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_metadata(const ModelJsonMetadata& metadata,
                                  std::size_t& total_bytes) noexcept {
    if (!valid_metadata_extras(metadata, total_bytes)) {
        return false;
    }
    for (std::size_t index = 0; index < metadata.extensions.size(); ++index) {
        const ModelJsonExtension& extension = metadata.extensions[index];
        if (!valid_metadata_extension(extension, total_bytes) ||
            !unique_extension_name(metadata, index)) {
            return false;
        }
    }
    return true;
}

} // namespace elf3d::model::detail

namespace elf3d {

using model::detail::merge;
using model::detail::metadata_view;

Document::Storage::Storage() noexcept
    : owner_token_(model::detail::allocate_document_owner_token()) {}

std::uint64_t Document::Storage::token() const noexcept {
    return owner_token_;
}

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
    if (storage_ == nullptr) {
        return {};
    }
    return storage_->statistics();
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
        return Error{ErrorCode::invalid_document_scene_id,
                     "The document scene index is outside the document"};
    }
    const Storage::SceneRecord& record = storage_->scenes[index];
    return DocumentSceneView{record.id, record.name, record.roots, metadata_view(record.metadata)};
}

Result<NodeView> Document::node_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->nodes.size()) {
        return Error{ErrorCode::invalid_node_id, "The document node index is outside the document"};
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
        return Error{ErrorCode::invalid_mesh_id, "The document mesh index is outside the document"};
    }
    const Storage::MeshRecord& record = storage_->meshes[index];
    return MeshView{record.id, record.name, record.primitives, record.bounds,
                    metadata_view(record.metadata)};
}

Result<PrimitiveView> Document::primitive_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->primitives.size()) {
        return Error{ErrorCode::invalid_primitive_id,
                     "The document primitive index is outside the document"};
    }
    const Storage::PrimitiveRecord& record = storage_->primitives[index];
    return PrimitiveView{record.id,     record.mesh,        record.material,
                         record.bounds, record.data.view(), metadata_view(record.metadata)};
}

Result<MaterialView> Document::material_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->materials.size()) {
        return Error{ErrorCode::invalid_material_id,
                     "The document material index is outside the document"};
    }
    const Storage::MaterialRecord& record = storage_->materials[index];
    return MaterialView{record.id, record.description, metadata_view(record.metadata)};
}

Result<ImageView> Document::image_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->images.size()) {
        return Error{ErrorCode::invalid_image_id,
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
        return Error{ErrorCode::invalid_texture_id,
                     "The document texture index is outside the document"};
    }
    const Storage::TextureRecord& record = storage_->textures[index];
    return TextureView{record.id, record.description, metadata_view(record.metadata)};
}

Result<SamplerView> Document::sampler_at(std::size_t index) const noexcept {
    if (storage_ == nullptr || index >= storage_->samplers.size()) {
        return Error{ErrorCode::invalid_sampler_id,
                     "The document sampler index is outside the document"};
    }
    const Storage::SamplerRecord& record = storage_->samplers[index];
    return SamplerView{record.id, record.description, metadata_view(record.metadata)};
}

Result<DocumentSceneView> Document::scene(DocumentSceneId scene_id) const noexcept {
    if (storage_ == nullptr) {
        return Error{ErrorCode::invalid_document_scene_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_node_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_mesh_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_primitive_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_material_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_image_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_texture_id, "The document is empty"};
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
        return Error{ErrorCode::invalid_sampler_id, "The document is empty"};
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

} // namespace elf3d
