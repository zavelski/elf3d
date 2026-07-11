module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <png.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module elf.gltf;

import elf.image;

namespace elf3d::gltf {
namespace {

struct ByteRange {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct Accessor {
    std::uint32_t view = 0;
    std::uint32_t component_type = 0;
    std::uint32_t count = 0;
    std::string_view type;
    std::optional<Bounds3> bounds;
};

struct PrimitiveOutput {
    std::uint32_t positions = 0;
    std::optional<std::uint32_t> normals;
    std::optional<std::uint32_t> texcoord0;
    std::optional<std::uint32_t> texcoord1;
    std::optional<std::uint32_t> colors;
    std::uint32_t indices = 0;
    std::uint32_t material = 0;
};

template <typename Id> struct IdIndex {
    Id id;
    std::uint32_t index = 0;
};

struct ExportData {
    ModelJsonMetadataView root_metadata;
    ModelJsonMetadataView asset_metadata;
    std::vector<DocumentSceneView> scenes;
    std::optional<DocumentSceneId> default_scene;
    std::vector<NodeView> nodes;
    std::vector<MeshView> meshes;
    std::vector<PrimitiveView> primitives;
    std::vector<MaterialView> materials;
    std::vector<ImageView> images;
    std::vector<TextureView> textures;
    std::vector<SamplerView> samplers;
    std::vector<IdIndex<NodeId>> node_indices;
    std::vector<IdIndex<MeshId>> mesh_indices;
    std::vector<IdIndex<PrimitiveId>> primitive_indices;
    std::vector<IdIndex<MaterialId>> material_indices;
    std::vector<IdIndex<ImageId>> image_indices;
    std::vector<IdIndex<TextureId>> texture_indices;
    std::vector<IdIndex<SamplerId>> sampler_indices;
    bool preserved_metadata_dropped = false;
};

struct EncodedImageOutput {
    std::vector<std::byte> bytes;
    ModelImageMimeType mime_type = ModelImageMimeType::png;
    bool reencoded = false;
};

class BinaryBuilder final {
  public:
    [[nodiscard]] Result<ByteRange> append_positions(std::span<const Float3> values) {
        const Result<ByteRange> range = reserve(values.size(), 12U);
        if (!range) {
            return range.error();
        }
        for (const Float3 value : values) {
            append_float(value.x);
            append_float(value.y);
            append_float(value.z);
        }
        return range.value();
    }

    [[nodiscard]] Result<ByteRange> append_texcoords(std::span<const Float2> values) {
        const Result<ByteRange> range = reserve(values.size(), 8U);
        if (!range) {
            return range.error();
        }
        for (const Float2 value : values) {
            append_float(value.x);
            append_float(value.y);
        }
        return range.value();
    }

    [[nodiscard]] Result<ByteRange> append_colors(std::span<const Color4> values) {
        const Result<ByteRange> range = reserve(values.size(), 16U);
        if (!range) {
            return range.error();
        }
        for (const Color4 value : values) {
            append_float(value.red);
            append_float(value.green);
            append_float(value.blue);
            append_float(value.alpha);
        }
        return range.value();
    }

    [[nodiscard]] Result<ByteRange> append_indices(std::span<const std::uint32_t> values) {
        const Result<ByteRange> range = reserve(values.size(), 4U);
        if (!range) {
            return range.error();
        }
        for (const std::uint32_t value : values) {
            append_uint32(value);
        }
        return range.value();
    }

    [[nodiscard]] Result<ByteRange> append_bytes(std::span<const std::byte> values) {
        const Result<ByteRange> range = reserve(values.size(), 1U);
        if (!range) {
            return range.error();
        }
        bytes_.insert(bytes_.end(), values.begin(), values.end());
        return range.value();
    }

    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept {
        return bytes_;
    }

  private:
    [[nodiscard]] Result<ByteRange> reserve(std::size_t count, std::size_t stride) {
        if (count > std::numeric_limits<std::uint32_t>::max() / stride) {
            return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit buffer limit"};
        }
        while (bytes_.size() % 4U != 0U) {
            bytes_.push_back(std::byte{0});
        }
        const std::size_t length = count * stride;
        if (bytes_.size() > std::numeric_limits<std::uint32_t>::max() - length) {
            return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit buffer limit"};
        }
        return ByteRange{static_cast<std::uint32_t>(bytes_.size()),
                         static_cast<std::uint32_t>(length)};
    }

    void append_uint32(std::uint32_t value) {
        bytes_.push_back(static_cast<std::byte>(value & 0xffU));
        bytes_.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
        bytes_.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
        bytes_.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
    }

    void append_float(float value) {
        append_uint32(std::bit_cast<std::uint32_t>(value));
    }

    std::vector<std::byte> bytes_;
};

class PngImage final {
  public:
    PngImage() noexcept {
        value_.version = PNG_IMAGE_VERSION;
    }

    ~PngImage() {
        png_image_free(&value_);
    }

    PngImage(const PngImage&) = delete;
    PngImage& operator=(const PngImage&) = delete;

    [[nodiscard]] png_image& value() noexcept {
        return value_;
    }

  private:
    png_image value_{};
};

[[noreturn]] void fatal_export_allocation_failure() noexcept {
    fatal_error("Elf3D glTF exporter memory allocation failed");
}

[[noreturn]] void fatal_unexpected_export_boundary_exception() noexcept {
    fatal_error("Elf3D glTF exporter encountered an unexpected exception");
}

[[nodiscard]] Result<std::uint32_t> checked_index(std::size_t value) noexcept {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit index limit"};
    }
    return static_cast<std::uint32_t>(value);
}

template <typename Id>
[[nodiscard]] std::optional<std::uint32_t> find_index(const std::vector<IdIndex<Id>>& indices,
                                                      Id id) noexcept {
    for (const IdIndex<Id>& entry : indices) {
        if (entry.id == id) {
            return entry.index;
        }
    }
    return std::nullopt;
}

template <typename T>
[[nodiscard]] Result<void> append_view(std::vector<T>& destination, Result<T> source) {
    if (!source) {
        return source.error();
    }
    destination.push_back(source.value());
    return {};
}

template <typename Id, typename T>
[[nodiscard]] Result<void> append_index(std::vector<IdIndex<Id>>& destination, const T& view,
                                        std::size_t index) {
    const Result<std::uint32_t> converted = checked_index(index);
    if (!converted) {
        return converted.error();
    }
    destination.push_back(IdIndex<Id>{view.id, converted.value()});
    return {};
}

[[nodiscard]] Result<ExportData> collect_document(DocumentView document) {
    if (validate_document(document).has_errors()) {
        return Error{ErrorCode::invalid_argument, "Cannot export an invalid model document"};
    }
    ExportData result;
    result.root_metadata = document.root_metadata();
    result.asset_metadata = document.asset_metadata();
    result.default_scene = document.default_scene();
    for (std::size_t index = 0; index < document.scene_count(); ++index) {
        if (const Result<void> appended = append_view(result.scenes, document.scene_at(index));
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.node_count(); ++index) {
        if (const Result<void> appended = append_view(result.nodes, document.node_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.node_indices, result.nodes.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.mesh_count(); ++index) {
        if (const Result<void> appended = append_view(result.meshes, document.mesh_at(index));
            !appended) {
            return appended.error();
        }
        if (result.meshes.back().primitives.empty()) {
            return Error{ErrorCode::invalid_mesh_data,
                         "glTF export requires every document mesh to contain a primitive"};
        }
        if (const Result<void> appended =
                append_index(result.mesh_indices, result.meshes.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.primitive_count(); ++index) {
        if (const Result<void> appended =
                append_view(result.primitives, document.primitive_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.primitive_indices, result.primitives.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.material_count(); ++index) {
        if (const Result<void> appended =
                append_view(result.materials, document.material_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.material_indices, result.materials.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.image_count(); ++index) {
        if (const Result<void> appended = append_view(result.images, document.image_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.image_indices, result.images.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.texture_count(); ++index) {
        if (const Result<void> appended = append_view(result.textures, document.texture_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.texture_indices, result.textures.back(), index);
            !appended) {
            return appended.error();
        }
    }
    for (std::size_t index = 0; index < document.sampler_count(); ++index) {
        if (const Result<void> appended = append_view(result.samplers, document.sampler_at(index));
            !appended) {
            return appended.error();
        }
        if (const Result<void> appended =
                append_index(result.sampler_indices, result.samplers.back(), index);
            !appended) {
            return appended.error();
        }
    }
    result.preserved_metadata_dropped = document.preserved_metadata_stale();
    if (result.preserved_metadata_dropped) {
        result.root_metadata = {};
        result.asset_metadata = {};
        for (DocumentSceneView& scene : result.scenes) {
            scene.metadata = {};
        }
        for (NodeView& node : result.nodes) {
            node.metadata = {};
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
    return result;
}

[[nodiscard]] Result<std::vector<std::byte>> encode_png(const ImageView& image) {
    if (image.format != ModelPixelFormat::rgba8_unorm) {
        return Error{ErrorCode::unsupported_texture_format,
                     "glTF export supports only RGBA8 document images"};
    }
    PngImage png;
    png_image& value = png.value();
    value.width = image.width;
    value.height = image.height;
    value.format = PNG_FORMAT_RGBA;
    png_alloc_size_t size = 0;
    if (png_image_write_to_memory(&value, nullptr, &size, 0, image.pixels.data(), 0, nullptr) ==
        0) {
        return Error{ErrorCode::image_encode_failed, value.message};
    }
    std::vector<std::byte> result(static_cast<std::size_t>(size));
    if (png_image_write_to_memory(&value, result.data(), &size, 0, image.pixels.data(), 0,
                                  nullptr) == 0) {
        return Error{ErrorCode::image_encode_failed, value.message};
    }
    result.resize(static_cast<std::size_t>(size));
    return result;
}

[[nodiscard]] Result<EncodedImageOutput> encoded_image(const ImageView& image) {
    if (image.source_mime_type != ModelImageMimeType::none) {
        const Result<image::DecodedImage> decoded = image::decode_png_or_jpeg(image.source_bytes);
        if (decoded && decoded.value().width == image.width &&
            decoded.value().height == image.height &&
            decoded.value().pixels.size() == image.pixels.size() &&
            std::equal(decoded.value().pixels.begin(), decoded.value().pixels.end(),
                       image.pixels.begin())) {
            return EncodedImageOutput{
                std::vector<std::byte>{image.source_bytes.begin(), image.source_bytes.end()},
                image.source_mime_type, false};
        }
    }
    Result<std::vector<std::byte>> encoded = encode_png(image);
    if (!encoded) {
        return encoded.error();
    }
    return EncodedImageOutput{std::move(encoded).value(), ModelImageMimeType::png, true};
}

[[nodiscard]] std::string_view image_mime_text(ModelImageMimeType mime_type) noexcept {
    return mime_type == ModelImageMimeType::jpeg ? "image/jpeg" : "image/png";
}

[[nodiscard]] std::string_view image_extension(ModelImageMimeType mime_type) noexcept {
    return mime_type == ModelImageMimeType::jpeg ? ".jpg" : ".png";
}

[[nodiscard]] Result<std::uint32_t> append_buffer_view(std::vector<ByteRange>& views,
                                                       ByteRange range) {
    const Result<std::uint32_t> index = checked_index(views.size());
    if (!index) {
        return index.error();
    }
    views.push_back(range);
    return index.value();
}

[[nodiscard]] Result<std::uint32_t> append_accessor(std::vector<Accessor>& accessors,
                                                    std::uint32_t view,
                                                    std::uint32_t component_type, std::size_t count,
                                                    std::string_view type,
                                                    std::optional<Bounds3> bounds = std::nullopt) {
    const Result<std::uint32_t> converted_count = checked_index(count);
    if (!converted_count) {
        return converted_count.error();
    }
    const Result<std::uint32_t> index = checked_index(accessors.size());
    if (!index) {
        return index.error();
    }
    accessors.push_back(Accessor{view, component_type, converted_count.value(), type, bounds});
    return index.value();
}

[[nodiscard]] Result<PrimitiveOutput>
append_primitive(const PrimitiveView& primitive, const ExportData& document, BinaryBuilder& binary,
                 std::vector<ByteRange>& views, std::vector<Accessor>& accessors) {
    const std::optional<std::uint32_t> material =
        find_index(document.material_indices, primitive.material);
    if (!material.has_value()) {
        return Error{ErrorCode::invalid_argument, "Primitive material is not in this document"};
    }
    const Result<ByteRange> positions = binary.append_positions(primitive.data.positions);
    if (!positions) {
        return positions.error();
    }
    const Result<std::uint32_t> position_view = append_buffer_view(views, positions.value());
    if (!position_view) {
        return position_view.error();
    }
    const Result<std::uint32_t> position_accessor =
        append_accessor(accessors, position_view.value(), 5126U, primitive.data.positions.size(),
                        "VEC3", primitive.bounds);
    if (!position_accessor) {
        return position_accessor.error();
    }
    PrimitiveOutput output;
    output.positions = position_accessor.value();
    output.material = *material;
    const auto append_vec3 = [&binary, &views,
                              &accessors](std::span<const Float3> values) -> Result<std::uint32_t> {
        const Result<ByteRange> bytes = binary.append_positions(values);
        if (!bytes) {
            return bytes.error();
        }
        const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
        if (!view) {
            return view.error();
        }
        return append_accessor(accessors, view.value(), 5126U, values.size(), "VEC3");
    };
    if (!primitive.data.normals.empty()) {
        const Result<std::uint32_t> normals = append_vec3(primitive.data.normals);
        if (!normals) {
            return normals.error();
        }
        output.normals = normals.value();
    }
    const auto append_vec2 = [&binary, &views,
                              &accessors](std::span<const Float2> values) -> Result<std::uint32_t> {
        const Result<ByteRange> bytes = binary.append_texcoords(values);
        if (!bytes) {
            return bytes.error();
        }
        const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
        if (!view) {
            return view.error();
        }
        return append_accessor(accessors, view.value(), 5126U, values.size(), "VEC2");
    };
    if (!primitive.data.texcoord0.empty()) {
        const Result<std::uint32_t> values = append_vec2(primitive.data.texcoord0);
        if (!values) {
            return values.error();
        }
        output.texcoord0 = values.value();
    }
    if (!primitive.data.texcoord1.empty()) {
        const Result<std::uint32_t> values = append_vec2(primitive.data.texcoord1);
        if (!values) {
            return values.error();
        }
        output.texcoord1 = values.value();
    }
    if (!primitive.data.colors.empty()) {
        const Result<ByteRange> bytes = binary.append_colors(primitive.data.colors);
        if (!bytes) {
            return bytes.error();
        }
        const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
        if (!view) {
            return view.error();
        }
        const Result<std::uint32_t> colors =
            append_accessor(accessors, view.value(), 5126U, primitive.data.colors.size(), "VEC4");
        if (!colors) {
            return colors.error();
        }
        output.colors = colors.value();
    }
    const Result<ByteRange> index_bytes = binary.append_indices(primitive.data.indices);
    if (!index_bytes) {
        return index_bytes.error();
    }
    const Result<std::uint32_t> index_view = append_buffer_view(views, index_bytes.value());
    if (!index_view) {
        return index_view.error();
    }
    const Result<std::uint32_t> index_accessor = append_accessor(
        accessors, index_view.value(), 5125U, primitive.data.indices.size(), "SCALAR");
    if (!index_accessor) {
        return index_accessor.error();
    }
    output.indices = index_accessor.value();
    return output;
}

void append_string(std::string& output, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char character : value) {
        switch (character) {
        case '"':
            output.append("\\\"");
            break;
        case '\\':
            output.append("\\\\");
            break;
        case '\b':
            output.append("\\b");
            break;
        case '\f':
            output.append("\\f");
            break;
        case '\n':
            output.append("\\n");
            break;
        case '\r':
            output.append("\\r");
            break;
        case '\t':
            output.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output.append("\\u00");
                output.push_back(hex[(static_cast<unsigned char>(character) >> 4U) & 0x0fU]);
                output.push_back(hex[static_cast<unsigned char>(character) & 0x0fU]);
            } else {
                output.push_back(character);
            }
            break;
        }
    }
    output.push_back('"');
}

[[nodiscard]] bool begin_extension_member(std::string& output, bool& first,
                                          std::vector<std::string_view>& emitted,
                                          std::string_view name) {
    if (std::find(emitted.begin(), emitted.end(), name) != emitted.end()) {
        return false;
    }
    if (!first) {
        output.push_back(',');
    }
    first = false;
    append_string(output, name);
    output.push_back(':');
    emitted.push_back(name);
    return true;
}

void append_preserved_extension_members(std::string& output, bool& first,
                                        std::vector<std::string_view>& emitted,
                                        std::span<const ModelJsonExtension> extensions) {
    for (const ModelJsonExtension& extension : extensions) {
        if (begin_extension_member(output, first, emitted, extension.name)) {
            output.append(extension.data);
        }
    }
}

void append_preserved_extras(std::string& output, ModelJsonMetadataView metadata,
                             bool has_member = true) {
    if (!metadata.extras_json.has_value()) {
        return;
    }
    if (has_member) {
        output.push_back(',');
    }
    output.append("\"extras\":");
    output.append(*metadata.extras_json);
}

void append_preserved_metadata(std::string& output, ModelJsonMetadataView metadata,
                               bool has_member = true) {
    if (!metadata.extensions.empty()) {
        if (has_member) {
            output.push_back(',');
        }
        output.append("\"extensions\":{");
        bool first = true;
        std::vector<std::string_view> emitted;
        emitted.reserve(metadata.extensions.size());
        append_preserved_extension_members(output, first, emitted, metadata.extensions);
        output.push_back('}');
        has_member = true;
    }
    append_preserved_extras(output, metadata, has_member);
}

void add_extension_used(std::vector<std::string_view>& extensions, std::string_view name) {
    if (std::find(extensions.begin(), extensions.end(), name) == extensions.end()) {
        extensions.push_back(name);
    }
}

void add_preserved_extensions_used(std::vector<std::string_view>& destination,
                                   ModelJsonMetadataView metadata) {
    for (const ModelJsonExtension& extension : metadata.extensions) {
        add_extension_used(destination, extension.name);
    }
}

void append_unsigned(std::string& output, std::uint32_t value) {
    char buffer[16]{};
    const std::to_chars_result result = std::to_chars(buffer, buffer + std::size(buffer), value);
    ELF3D_ASSERT(result.ec == std::errc{});
    output.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

void append_float(std::string& output, float value) {
    char buffer[64]{};
    const std::to_chars_result result =
        std::to_chars(buffer, buffer + std::size(buffer), value, std::chars_format::general,
                      std::numeric_limits<float>::max_digits10);
    ELF3D_ASSERT(result.ec == std::errc{});
    output.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

void append_float2(std::string& output, Float2 value) {
    output.push_back('[');
    append_float(output, value.x);
    output.push_back(',');
    append_float(output, value.y);
    output.push_back(']');
}

void append_float3(std::string& output, Float3 value) {
    output.push_back('[');
    append_float(output, value.x);
    output.push_back(',');
    append_float(output, value.y);
    output.push_back(',');
    append_float(output, value.z);
    output.push_back(']');
}

void append_float4(std::string& output, Color4 value) {
    output.push_back('[');
    append_float(output, value.red);
    output.push_back(',');
    append_float(output, value.green);
    output.push_back(',');
    append_float(output, value.blue);
    output.push_back(',');
    append_float(output, value.alpha);
    output.push_back(']');
}

[[nodiscard]] std::uint32_t wrap_value(ModelTextureWrap value) noexcept {
    switch (value) {
    case ModelTextureWrap::repeat:
        return 10497U;
    case ModelTextureWrap::mirrored_repeat:
        return 33648U;
    case ModelTextureWrap::clamp_to_edge:
        return 33071U;
    }
    ELF3D_ASSERT(false);
    return 10497U;
}

[[nodiscard]] std::uint32_t filter_value(ModelTextureFilter value) noexcept {
    switch (value) {
    case ModelTextureFilter::nearest:
        return 9728U;
    case ModelTextureFilter::linear:
        return 9729U;
    case ModelTextureFilter::nearest_mipmap_nearest:
        return 9984U;
    case ModelTextureFilter::linear_mipmap_nearest:
        return 9985U;
    case ModelTextureFilter::nearest_mipmap_linear:
        return 9986U;
    case ModelTextureFilter::linear_mipmap_linear:
        return 9987U;
    }
    ELF3D_ASSERT(false);
    return 9729U;
}

[[nodiscard]] bool has_nondefault_transform(ModelTextureMapping mapping) noexcept {
    return mapping.transform.offset != Float2{} || mapping.transform.scale != Float2{1.0F, 1.0F} ||
           mapping.transform.rotation_radians != 0.0F;
}

void append_texture_info(std::string& output, std::uint32_t texture, ModelTextureMapping mapping,
                         std::optional<std::pair<std::string_view, float>> scalar = std::nullopt) {
    output.append("{\"index\":");
    append_unsigned(output, texture);
    if (scalar.has_value() && scalar->second != 1.0F) {
        output.push_back(',');
        append_string(output, scalar->first);
        output.push_back(':');
        append_float(output, scalar->second);
    }
    if (mapping.texcoord_set != 0U) {
        output.append(",\"texCoord\":");
        append_unsigned(output, mapping.texcoord_set);
    }
    if (has_nondefault_transform(mapping)) {
        output.append(",\"extensions\":{\"KHR_texture_transform\":{");
        bool first = true;
        const auto property = [&output, &first](std::string_view name) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            append_string(output, name);
            output.push_back(':');
        };
        if (mapping.transform.offset != Float2{}) {
            property("offset");
            append_float2(output, mapping.transform.offset);
        }
        if (mapping.transform.rotation_radians != 0.0F) {
            property("rotation");
            append_float(output, mapping.transform.rotation_radians);
        }
        if (mapping.transform.scale != Float2{1.0F, 1.0F}) {
            property("scale");
            append_float2(output, mapping.transform.scale);
        }
        if (mapping.texcoord_set != 0U) {
            property("texCoord");
            append_unsigned(output, mapping.texcoord_set);
        }
        output.append("}}");
    }
    output.push_back('}');
}

[[nodiscard]] Result<void> write_file(const std::filesystem::path& path,
                                      std::span<const std::byte> bytes) {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    if (!stream) {
        return Error{ErrorCode::source_file_write_failed, "Could not open output file for writing"};
    }
    if (!bytes.empty()) {
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    return stream ? Result<void>{}
                  : Result<void>{
                        Error{ErrorCode::source_file_write_failed, "Could not write output file"}};
}

void append_uint32(std::vector<std::byte>& output, std::uint32_t value) {
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
}

[[nodiscard]] Result<std::vector<std::byte>> build_glb(std::string json,
                                                       const std::vector<std::byte>& binary) {
    while (json.size() % 4U != 0U) {
        json.push_back(' ');
    }
    std::vector<std::byte> binary_chunk = binary;
    while (binary_chunk.size() % 4U != 0U) {
        binary_chunk.push_back(std::byte{0});
    }
    const std::size_t total =
        20U + json.size() + (binary_chunk.empty() ? 0U : 8U + binary_chunk.size());
    if (total > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::size_overflow, "GLB output exceeds the 32-bit container limit"};
    }
    std::vector<std::byte> output;
    output.reserve(total);
    append_uint32(output, 0x46546c67U);
    append_uint32(output, 2U);
    append_uint32(output, static_cast<std::uint32_t>(total));
    append_uint32(output, static_cast<std::uint32_t>(json.size()));
    append_uint32(output, 0x4e4f534aU);
    for (const char value : json) {
        output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    if (!binary_chunk.empty()) {
        append_uint32(output, static_cast<std::uint32_t>(binary_chunk.size()));
        append_uint32(output, 0x004e4942U);
        output.insert(output.end(), binary_chunk.begin(), binary_chunk.end());
    }
    return output;
}

[[nodiscard]] Result<std::string>
build_json(const ExportData& document, const std::vector<PrimitiveOutput>& primitives,
           const std::vector<ByteRange>& views, const std::vector<Accessor>& accessors,
           const std::vector<std::optional<std::uint32_t>>& image_views,
           std::span<const ModelImageMimeType> image_mime_types,
           std::span<const std::string> image_uris, std::string_view buffer_uri, bool glb) {
    std::string output{"{\"asset\":{\"version\":\"2.0\",\"generator\":\"Elf3D\""};
    append_preserved_metadata(output, document.asset_metadata);
    output.push_back('}');
    bool texture_transform = false;
    bool unlit = false;
    bool ior = false;
    bool specular = false;
    bool emissive_strength = false;
    for (const MaterialView& material : document.materials) {
        const ModelMaterialDescription& value = material.description;
        texture_transform = texture_transform ||
                            has_nondefault_transform(value.base_color_texture_mapping) ||
                            has_nondefault_transform(value.metallic_roughness_texture_mapping) ||
                            has_nondefault_transform(value.normal_texture_mapping) ||
                            has_nondefault_transform(value.occlusion_texture_mapping) ||
                            has_nondefault_transform(value.emissive_texture_mapping);
        unlit = unlit || value.unlit;
        ior = ior || value.ior != 1.5F;
        specular = specular || value.specular_factor != 1.0F ||
                   value.specular_color_factor != Float3{1.0F, 1.0F, 1.0F};
        emissive_strength = emissive_strength || value.emissive_strength != 1.0F;
    }
    std::vector<std::string_view> extensions_used;
    const auto add_generated_extension = [&extensions_used](std::string_view name, bool used) {
        if (used) {
            add_extension_used(extensions_used, name);
        }
    };
    add_generated_extension("KHR_texture_transform", texture_transform);
    add_generated_extension("KHR_materials_unlit", unlit);
    add_generated_extension("KHR_materials_ior", ior);
    add_generated_extension("KHR_materials_specular", specular);
    add_generated_extension("KHR_materials_emissive_strength", emissive_strength);
    add_preserved_extensions_used(extensions_used, document.root_metadata);
    add_preserved_extensions_used(extensions_used, document.asset_metadata);
    for (const DocumentSceneView& scene : document.scenes) {
        add_preserved_extensions_used(extensions_used, scene.metadata);
    }
    for (const NodeView& node : document.nodes) {
        add_preserved_extensions_used(extensions_used, node.metadata);
    }
    for (const MeshView& mesh : document.meshes) {
        add_preserved_extensions_used(extensions_used, mesh.metadata);
    }
    for (const PrimitiveView& primitive : document.primitives) {
        add_preserved_extensions_used(extensions_used, primitive.metadata);
    }
    for (const MaterialView& material : document.materials) {
        add_preserved_extensions_used(extensions_used, material.metadata);
    }
    for (const ImageView& image : document.images) {
        add_preserved_extensions_used(extensions_used, image.metadata);
    }
    for (const TextureView& texture : document.textures) {
        add_preserved_extensions_used(extensions_used, texture.metadata);
    }
    for (const SamplerView& sampler : document.samplers) {
        add_preserved_extensions_used(extensions_used, sampler.metadata);
    }
    if (!extensions_used.empty()) {
        output.append(",\"extensionsUsed\":[");
        for (std::size_t index = 0; index < extensions_used.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            append_string(output, extensions_used[index]);
        }
        output.push_back(']');
    }
    if (!document.scenes.empty()) {
        if (document.default_scene.has_value()) {
            const auto default_scene =
                std::find_if(document.scenes.begin(), document.scenes.end(),
                             [&document](const DocumentSceneView& scene) noexcept {
                                 return scene.id == *document.default_scene;
                             });
            if (default_scene == document.scenes.end()) {
                return Error{ErrorCode::invalid_argument,
                             "Document default scene is not in the scene table"};
            }
            const Result<std::uint32_t> default_scene_index =
                checked_index(static_cast<std::size_t>(default_scene - document.scenes.begin()));
            if (!default_scene_index) {
                return default_scene_index.error();
            }
            output.append(",\"scene\":");
            append_unsigned(output, default_scene_index.value());
        }
        output.append(",\"scenes\":[");
        for (std::size_t index = 0; index < document.scenes.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const DocumentSceneView& scene = document.scenes[index];
            output.push_back('{');
            bool has_member = false;
            if (!scene.name.empty()) {
                append_string(output, "name");
                output.push_back(':');
                append_string(output, scene.name);
                has_member = true;
            }
            if (!scene.roots.empty()) {
                if (has_member) {
                    output.push_back(',');
                }
                output.append("\"nodes\":[");
                for (std::size_t root = 0; root < scene.roots.size(); ++root) {
                    const std::optional<std::uint32_t> node =
                        find_index(document.node_indices, scene.roots[root]);
                    if (!node) {
                        return Error{ErrorCode::invalid_argument,
                                     "Scene root is not in this document"};
                    }
                    if (root != 0U) {
                        output.push_back(',');
                    }
                    append_unsigned(output, *node);
                }
                output.push_back(']');
                has_member = true;
            }
            append_preserved_metadata(output, scene.metadata, has_member);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!document.nodes.empty()) {
        output.append(",\"nodes\":[");
        std::uint32_t camera = 0;
        for (std::size_t index = 0; index < document.nodes.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const NodeView& node = document.nodes[index];
            output.push_back('{');
            bool first = true;
            const auto separator = [&output, &first]() {
                if (!first) {
                    output.push_back(',');
                }
                first = false;
            };
            if (!node.name.empty()) {
                separator();
                append_string(output, "name");
                output.push_back(':');
                append_string(output, node.name);
            }
            if (node.mesh) {
                const std::optional<std::uint32_t> mesh =
                    find_index(document.mesh_indices, *node.mesh);
                if (!mesh) {
                    return Error{ErrorCode::invalid_argument, "Node mesh is not in this document"};
                }
                separator();
                output.append("\"mesh\":");
                append_unsigned(output, *mesh);
            }
            if (node.perspective_camera) {
                separator();
                output.append("\"camera\":");
                append_unsigned(output, camera);
                ++camera;
            }
            separator();
            output.append("\"matrix\":[");
            for (std::size_t element = 0; element < node.local_matrix.elements.size(); ++element) {
                if (element != 0U) {
                    output.push_back(',');
                }
                append_float(output, node.local_matrix.elements[element]);
            }
            output.push_back(']');
            if (!node.children.empty()) {
                separator();
                output.append("\"children\":[");
                for (std::size_t child = 0; child < node.children.size(); ++child) {
                    const std::optional<std::uint32_t> child_index =
                        find_index(document.node_indices, node.children[child]);
                    if (!child_index) {
                        return Error{ErrorCode::invalid_argument,
                                     "Node child is not in this document"};
                    }
                    if (child != 0U) {
                        output.push_back(',');
                    }
                    append_unsigned(output, *child_index);
                }
                output.push_back(']');
            }
            append_preserved_metadata(output, node.metadata);
            output.push_back('}');
        }
        output.push_back(']');
    }
    bool has_cameras = false;
    for (const NodeView& node : document.nodes) {
        has_cameras = has_cameras || node.perspective_camera.has_value();
    }
    if (has_cameras) {
        output.append(",\"cameras\":[");
        bool first = true;
        for (const NodeView& node : document.nodes) {
            if (!node.perspective_camera) {
                continue;
            }
            if (!first) {
                output.push_back(',');
            }
            first = false;
            const ModelPerspectiveCameraDescription& camera = *node.perspective_camera;
            output.append("{\"type\":\"perspective\",\"perspective\":{\"yfov\":");
            append_float(output, camera.vertical_field_of_view_radians);
            output.append(",\"znear\":");
            append_float(output, camera.near_plane);
            output.append(",\"zfar\":");
            append_float(output, camera.far_plane);
            output.append("}}");
        }
        output.push_back(']');
    }
    if (!document.meshes.empty()) {
        output.append(",\"meshes\":[");
        for (std::size_t index = 0; index < document.meshes.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const MeshView& mesh = document.meshes[index];
            output.push_back('{');
            if (!mesh.name.empty()) {
                append_string(output, "name");
                output.push_back(':');
                append_string(output, mesh.name);
                output.push_back(',');
            }
            output.append("\"primitives\":[");
            for (std::size_t primitive = 0; primitive < mesh.primitives.size(); ++primitive) {
                const std::optional<std::uint32_t> primitive_index =
                    find_index(document.primitive_indices, mesh.primitives[primitive]);
                if (!primitive_index || *primitive_index >= primitives.size()) {
                    return Error{ErrorCode::invalid_argument,
                                 "Mesh primitive is not in this document"};
                }
                if (primitive != 0U) {
                    output.push_back(',');
                }
                const PrimitiveOutput& value = primitives[*primitive_index];
                output.append("{\"attributes\":{\"POSITION\":");
                append_unsigned(output, value.positions);
                if (value.normals) {
                    output.append(",\"NORMAL\":");
                    append_unsigned(output, *value.normals);
                }
                if (value.texcoord0) {
                    output.append(",\"TEXCOORD_0\":");
                    append_unsigned(output, *value.texcoord0);
                }
                if (value.texcoord1) {
                    output.append(",\"TEXCOORD_1\":");
                    append_unsigned(output, *value.texcoord1);
                }
                if (value.colors) {
                    output.append(",\"COLOR_0\":");
                    append_unsigned(output, *value.colors);
                }
                output.append("},\"indices\":");
                append_unsigned(output, value.indices);
                output.append(",\"material\":");
                append_unsigned(output, value.material);
                output.append(",\"mode\":4");
                append_preserved_metadata(output, document.primitives[*primitive_index].metadata);
                output.push_back('}');
            }
            output.push_back(']');
            append_preserved_metadata(output, mesh.metadata);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!document.materials.empty()) {
        output.append(",\"materials\":[");
        for (std::size_t index = 0; index < document.materials.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const ModelMaterialDescription& material = document.materials[index].description;
            const auto texture = [&document](TextureId id) -> Result<std::optional<std::uint32_t>> {
                if (!id.is_valid()) {
                    return std::optional<std::uint32_t>{};
                }
                const std::optional<std::uint32_t> found = find_index(document.texture_indices, id);
                return found ? Result<std::optional<std::uint32_t>>{found}
                             : Result<std::optional<std::uint32_t>>{
                                   Error{ErrorCode::invalid_argument,
                                         "Material texture is not in this document"}};
            };
            const Result<std::optional<std::uint32_t>> base = texture(material.base_color_texture);
            const Result<std::optional<std::uint32_t>> metal =
                texture(material.metallic_roughness_texture);
            const Result<std::optional<std::uint32_t>> normal = texture(material.normal_texture);
            const Result<std::optional<std::uint32_t>> occlusion =
                texture(material.occlusion_texture);
            const Result<std::optional<std::uint32_t>> emissive =
                texture(material.emissive_texture);
            if (!base || !metal || !normal || !occlusion || !emissive) {
                return Error{ErrorCode::invalid_argument,
                             "Material texture is not in this document"};
            }
            output.append("{\"pbrMetallicRoughness\":{\"baseColorFactor\":");
            append_float4(output, material.base_color);
            output.append(",\"metallicFactor\":");
            append_float(output, material.metallic_factor);
            output.append(",\"roughnessFactor\":");
            append_float(output, material.roughness_factor);
            if (base.value()) {
                output.append(",\"baseColorTexture\":");
                append_texture_info(output, *base.value(), material.base_color_texture_mapping);
            }
            if (metal.value()) {
                output.append(",\"metallicRoughnessTexture\":");
                append_texture_info(output, *metal.value(),
                                    material.metallic_roughness_texture_mapping);
            }
            output.push_back('}');
            if (normal.value()) {
                output.append(",\"normalTexture\":");
                append_texture_info(
                    output, *normal.value(), material.normal_texture_mapping,
                    std::pair<std::string_view, float>{"scale", material.normal_scale});
            }
            if (occlusion.value()) {
                output.append(",\"occlusionTexture\":");
                append_texture_info(
                    output, *occlusion.value(), material.occlusion_texture_mapping,
                    std::pair<std::string_view, float>{"strength", material.occlusion_strength});
            }
            if (emissive.value()) {
                output.append(",\"emissiveTexture\":");
                append_texture_info(output, *emissive.value(), material.emissive_texture_mapping);
            }
            output.append(",\"emissiveFactor\":");
            append_float3(output, material.emissive_factor);
            if (material.alpha_mode != ModelAlphaMode::opaque) {
                output.append(",\"alphaMode\":");
                append_string(output,
                              material.alpha_mode == ModelAlphaMode::mask ? "MASK" : "BLEND");
            }
            if (material.alpha_mode == ModelAlphaMode::mask) {
                output.append(",\"alphaCutoff\":");
                append_float(output, material.alpha_cutoff);
            }
            if (material.double_sided) {
                output.append(",\"doubleSided\":true");
            }
            const ModelJsonMetadataView metadata = document.materials[index].metadata;
            if (material.unlit || material.ior != 1.5F || material.emissive_strength != 1.0F ||
                material.specular_factor != 1.0F ||
                material.specular_color_factor != Float3{1.0F, 1.0F, 1.0F} ||
                !metadata.extensions.empty()) {
                output.append(",\"extensions\":{");
                bool first = true;
                std::vector<std::string_view> emitted;
                emitted.reserve(metadata.extensions.size() + 4U);
                if (material.unlit &&
                    begin_extension_member(output, first, emitted, "KHR_materials_unlit")) {
                    output.append("{}");
                }
                if (material.ior != 1.5F &&
                    begin_extension_member(output, first, emitted, "KHR_materials_ior")) {
                    output.append("{\"ior\":");
                    append_float(output, material.ior);
                    output.push_back('}');
                }
                if (material.emissive_strength != 1.0F &&
                    begin_extension_member(output, first, emitted,
                                           "KHR_materials_emissive_strength")) {
                    output.append("{\"emissiveStrength\":");
                    append_float(output, material.emissive_strength);
                    output.push_back('}');
                }
                if (material.specular_factor != 1.0F ||
                    material.specular_color_factor != Float3{1.0F, 1.0F, 1.0F}) {
                    if (begin_extension_member(output, first, emitted, "KHR_materials_specular")) {
                        output.append("{\"specularFactor\":");
                        append_float(output, material.specular_factor);
                        output.append(",\"specularColorFactor\":");
                        append_float3(output, material.specular_color_factor);
                        output.push_back('}');
                    }
                }
                append_preserved_extension_members(output, first, emitted, metadata.extensions);
                output.push_back('}');
            }
            append_preserved_extras(output, metadata);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!document.textures.empty()) {
        output.append(",\"textures\":[");
        for (std::size_t index = 0; index < document.textures.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const ModelTextureDescription& texture = document.textures[index].description;
            const std::optional<std::uint32_t> image =
                find_index(document.image_indices, texture.image);
            const std::optional<std::uint32_t> sampler =
                find_index(document.sampler_indices, texture.sampler);
            if (!image || !sampler) {
                return Error{ErrorCode::invalid_argument, "Texture source is not in this document"};
            }
            output.append("{\"sampler\":");
            append_unsigned(output, *sampler);
            output.append(",\"source\":");
            append_unsigned(output, *image);
            append_preserved_metadata(output, document.textures[index].metadata);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!document.images.empty()) {
        output.append(",\"images\":[");
        for (std::size_t index = 0; index < document.images.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            if (image_views[index]) {
                if (index >= image_mime_types.size()) {
                    return Error{ErrorCode::invalid_argument,
                                 "Embedded image output MIME type is missing"};
                }
                output.append("{\"bufferView\":");
                append_unsigned(output, *image_views[index]);
                output.append(",\"mimeType\":");
                append_string(output, image_mime_text(image_mime_types[index]));
                append_preserved_metadata(output, document.images[index].metadata);
                output.push_back('}');
            } else {
                output.append("{\"uri\":");
                if (index >= image_uris.size()) {
                    return Error{ErrorCode::invalid_argument,
                                 "External image output URI is missing"};
                }
                append_string(output, image_uris[index]);
                append_preserved_metadata(output, document.images[index].metadata);
                output.push_back('}');
            }
        }
        output.push_back(']');
    }
    if (!document.samplers.empty()) {
        output.append(",\"samplers\":[");
        for (std::size_t index = 0; index < document.samplers.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const ModelSamplerDescription& sampler = document.samplers[index].description;
            output.append("{\"magFilter\":");
            append_unsigned(output, filter_value(sampler.mag_filter));
            output.append(",\"minFilter\":");
            append_unsigned(output, filter_value(sampler.min_filter));
            output.append(",\"wrapS\":");
            append_unsigned(output, wrap_value(sampler.wrap_u));
            output.append(",\"wrapT\":");
            append_unsigned(output, wrap_value(sampler.wrap_v));
            append_preserved_metadata(output, document.samplers[index].metadata);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!views.empty()) {
        std::uint32_t byte_length = 0;
        for (const ByteRange& view : views) {
            byte_length = std::max(byte_length, view.offset + view.length);
        }
        output.append(",\"buffers\":[{");
        if (!glb) {
            output.append("\"uri\":");
            append_string(output, buffer_uri);
            output.push_back(',');
        }
        output.append("\"byteLength\":");
        append_unsigned(output, byte_length);
        output.append("}],\"bufferViews\":[");
        for (std::size_t index = 0; index < views.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const ByteRange& view = views[index];
            output.append("{\"buffer\":0");
            if (view.offset != 0U) {
                output.append(",\"byteOffset\":");
                append_unsigned(output, view.offset);
            }
            output.append(",\"byteLength\":");
            append_unsigned(output, view.length);
            output.push_back('}');
        }
        output.push_back(']');
    }
    if (!accessors.empty()) {
        output.append(",\"accessors\":[");
        for (std::size_t index = 0; index < accessors.size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            const Accessor& accessor = accessors[index];
            output.append("{\"bufferView\":");
            append_unsigned(output, accessor.view);
            output.append(",\"componentType\":");
            append_unsigned(output, accessor.component_type);
            output.append(",\"count\":");
            append_unsigned(output, accessor.count);
            output.append(",\"type\":");
            append_string(output, accessor.type);
            if (accessor.bounds) {
                output.append(",\"min\":");
                append_float3(output, accessor.bounds->minimum);
                output.append(",\"max\":");
                append_float3(output, accessor.bounds->maximum);
            }
            output.push_back('}');
        }
        output.push_back(']');
    }
    append_preserved_metadata(output, document.root_metadata);
    output.push_back('}');
    return output;
}

struct OutputArtifact {
    std::filesystem::path final_path;
    std::vector<std::byte> bytes;
    std::filesystem::path staged_path;
    std::filesystem::path backup_path;
    bool has_existing_file = false;
    bool backed_up = false;
    bool published = false;
};

[[nodiscard]] std::vector<std::byte> bytes_from_text(std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return result;
}

[[nodiscard]] Result<std::filesystem::path>
available_sibling_path(const std::filesystem::path& final_path, std::string_view purpose) {
    for (std::uint32_t index = 0; index != 1024U; ++index) {
        const std::filesystem::path candidate =
            final_path.parent_path() / (final_path.filename().string() + ".elf3d-" +
                                        std::string{purpose} + "-" + std::to_string(index));
        std::error_code error;
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Could not inspect a staged glTF output path"};
        }
        if (!exists) {
            return candidate;
        }
    }
    return Error{ErrorCode::source_file_write_failed,
                 "Could not reserve a staged glTF output path"};
}

void remove_file_if_present(const std::filesystem::path& path) {
    if (!path.empty()) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
}

[[nodiscard]] Result<void> rollback_output(std::vector<OutputArtifact>& artifacts) {
    bool complete = true;
    for (OutputArtifact& artifact : artifacts) {
        if (artifact.published) {
            std::error_code error;
            std::filesystem::remove(artifact.final_path, error);
            complete = complete && !error;
        }
    }
    for (OutputArtifact& artifact : artifacts) {
        if (artifact.backed_up) {
            std::error_code error;
            std::filesystem::rename(artifact.backup_path, artifact.final_path, error);
            if (!error) {
                artifact.backed_up = false;
                artifact.backup_path.clear();
            }
            complete = complete && !error;
        }
        if (!artifact.staged_path.empty()) {
            std::error_code error;
            std::filesystem::remove(artifact.staged_path, error);
            complete = complete && !error;
        }
    }
    if (!complete) {
        return Error{ErrorCode::source_file_write_failed,
                     "Could not restore every pre-existing glTF output; recovery backups were "
                     "retained"};
    }
    return {};
}

[[nodiscard]] Error rollback_failure(std::vector<OutputArtifact>& artifacts, Error failure) {
    const Result<void> rolled_back = rollback_output(artifacts);
    return rolled_back ? std::move(failure) : rolled_back.error();
}

[[nodiscard]] Result<void> publish_output(std::vector<OutputArtifact>& artifacts) {
    for (OutputArtifact& artifact : artifacts) {
        std::error_code error;
        const bool exists = std::filesystem::exists(artifact.final_path, error);
        if (error || (exists && !std::filesystem::is_regular_file(artifact.final_path, error))) {
            return Error{ErrorCode::source_file_write_failed,
                         "A glTF output path cannot be replaced transactionally"};
        }
        artifact.has_existing_file = exists;
    }
    for (OutputArtifact& artifact : artifacts) {
        const Result<std::filesystem::path> staged =
            available_sibling_path(artifact.final_path, "stage");
        if (!staged) {
            return rollback_failure(artifacts, staged.error());
        }
        artifact.staged_path = staged.value();
        if (const Result<void> written = write_file(artifact.staged_path, artifact.bytes);
            !written) {
            return rollback_failure(artifacts, written.error());
        }
    }
    for (OutputArtifact& artifact : artifacts) {
        if (!artifact.has_existing_file) {
            continue;
        }
        const Result<std::filesystem::path> backup =
            available_sibling_path(artifact.final_path, "backup");
        if (!backup) {
            return rollback_failure(artifacts, backup.error());
        }
        artifact.backup_path = backup.value();
        std::error_code error;
        std::filesystem::rename(artifact.final_path, artifact.backup_path, error);
        if (error) {
            return rollback_failure(
                artifacts, Error{ErrorCode::source_file_write_failed,
                                 "Could not stage an existing glTF output for replacement"});
        }
        artifact.backed_up = true;
    }
    for (OutputArtifact& artifact : artifacts) {
        std::error_code error;
        std::filesystem::rename(artifact.staged_path, artifact.final_path, error);
        if (error) {
            return rollback_failure(artifacts,
                                    Error{ErrorCode::source_file_write_failed,
                                          "Could not publish a complete glTF output set"});
        }
        artifact.published = true;
    }
    for (OutputArtifact& artifact : artifacts) {
        if (artifact.backed_up) {
            remove_file_if_present(artifact.backup_path);
            artifact.backed_up = false;
            artifact.backup_path.clear();
        }
    }
    return {};
}

[[nodiscard]] ModelWriteReport
build_write_report(std::span<const EncodedImageOutput> encoded_images,
                   bool preserved_metadata_dropped) {
    ModelWriteReport report;
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

[[nodiscard]] Result<ModelWriteReport>
save(const std::filesystem::path& path, DocumentView document, const ModelWriteOptions& options) {
    const std::string extension = path.extension().string();
    if (extension != ".gltf" && extension != ".glb") {
        return Error{ErrorCode::unsupported_model_format,
                     "Document output paths must use a .gltf or .glb extension"};
    }
    const bool glb = extension == ".glb";
    const bool embedded = options.image_policy == ModelImageWritePolicy::embedded ||
                          (options.image_policy == ModelImageWritePolicy::automatic && glb);
    const Result<ExportData> collected = collect_document(document);
    if (!collected) {
        return collected.error();
    }
    const ExportData& value = collected.value();
    BinaryBuilder binary;
    std::vector<ByteRange> views;
    std::vector<Accessor> accessors;
    std::vector<PrimitiveOutput> primitives;
    primitives.reserve(value.primitives.size());
    for (const PrimitiveView& primitive : value.primitives) {
        const Result<PrimitiveOutput> output =
            append_primitive(primitive, value, binary, views, accessors);
        if (!output) {
            return output.error();
        }
        primitives.push_back(output.value());
    }
    std::vector<EncodedImageOutput> encoded_images;
    encoded_images.reserve(value.images.size());
    std::vector<ModelImageMimeType> image_mime_types;
    image_mime_types.reserve(value.images.size());
    std::vector<std::optional<std::uint32_t>> image_views;
    image_views.reserve(value.images.size());
    for (const ImageView& image : value.images) {
        Result<EncodedImageOutput> encoded = encoded_image(image);
        if (!encoded) {
            return encoded.error();
        }
        encoded_images.push_back(std::move(encoded).value());
        image_mime_types.push_back(encoded_images.back().mime_type);
        if (embedded) {
            const Result<ByteRange> range = binary.append_bytes(encoded_images.back().bytes);
            if (!range) {
                return range.error();
            }
            const Result<std::uint32_t> view = append_buffer_view(views, range.value());
            if (!view) {
                return view.error();
            }
            image_views.push_back(view.value());
        } else {
            image_views.push_back(std::nullopt);
        }
    }
    const std::string stem = path.stem().string();
    const std::string buffer_uri = stem + ".bin";
    std::vector<std::string> image_uris;
    image_uris.reserve(encoded_images.size());
    for (std::size_t index = 0; index < encoded_images.size(); ++index) {
        image_uris.push_back(stem + ".image_" + std::to_string(index) +
                             std::string{image_extension(encoded_images[index].mime_type)});
    }
    const Result<std::string> json = build_json(value, primitives, views, accessors, image_views,
                                                image_mime_types, image_uris, buffer_uri, glb);
    if (!json) {
        return json.error();
    }
    std::vector<OutputArtifact> artifacts;
    if (!embedded) {
        for (std::size_t index = 0; index < encoded_images.size(); ++index) {
            artifacts.push_back(OutputArtifact{path.parent_path() / image_uris[index],
                                               std::move(encoded_images[index].bytes)});
        }
    }
    if (glb) {
        const Result<std::vector<std::byte>> container = build_glb(json.value(), binary.bytes());
        if (!container) {
            return container.error();
        }
        artifacts.push_back(OutputArtifact{path, container.value()});
    } else {
        if (!binary.bytes().empty()) {
            artifacts.push_back(OutputArtifact{path.parent_path() / buffer_uri, binary.bytes()});
        }
        artifacts.push_back(OutputArtifact{path, bytes_from_text(json.value())});
    }
    if (const Result<void> published = publish_output(artifacts); !published) {
        return published.error();
    }
    return build_write_report(encoded_images, value.preserved_metadata_dropped);
}

} // namespace

Result<ModelWriteReport> save_document(const std::filesystem::path& path, DocumentView document,
                                       const ModelWriteOptions& options) noexcept {
    try {
        return save(path, document, options);
    } catch (const std::bad_alloc&) {
        fatal_export_allocation_failure();
    } catch (...) {
        fatal_unexpected_export_boundary_exception();
    }
}

} // namespace elf3d::gltf

namespace elf3d {

Result<ModelWriteReport> save_document(std::string_view path_utf8, DocumentView document,
                                       const ModelWriteOptions& options) noexcept {
    try {
        std::u8string utf8;
        utf8.reserve(path_utf8.size());
        for (const char character : path_utf8) {
            utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
        }
        return gltf::save_document(std::filesystem::path{utf8}, document, options);
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D glTF exporter memory allocation failed");
    } catch (...) {
        fatal_error("Elf3D glTF exporter encountered an unexpected exception");
    }
}

} // namespace elf3d
