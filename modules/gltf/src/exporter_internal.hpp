#pragma once

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::gltf::exporter_detail {

using OutputPath = std::filesystem::path;

struct ByteRange {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct EncodedIndexRange {
    ByteRange bytes;
    std::uint32_t component_type = 5125U;
};

struct Accessor {
    std::uint32_t view = 0;
    std::uint32_t component_type = 0;
    std::uint32_t count = 0;
    std::string_view type;
    std::optional<Bounds3> bounds;
};

struct AccessorDescription {
    std::uint32_t view = 0;
    std::uint32_t component_type = 0;
    std::size_t count = 0;
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

    [[nodiscard]] Result<EncodedIndexRange>
    append_indices(std::span<const std::uint32_t> values) {
        std::uint32_t maximum = 0U;
        for (const std::uint32_t value : values) {
            if (value > maximum) {
                maximum = value;
            }
        }
        const std::uint32_t component_type =
            maximum <= std::numeric_limits<std::uint8_t>::max()
                ? 5121U
                : (maximum <= std::numeric_limits<std::uint16_t>::max() ? 5123U : 5125U);
        const std::size_t stride = component_type == 5121U ? 1U : (component_type == 5123U ? 2U : 4U);
        const Result<ByteRange> range = reserve(values.size(), stride);
        if (!range) {
            return range.error();
        }
        for (const std::uint32_t value : values) {
            if (component_type == 5121U) {
                bytes_.push_back(static_cast<std::byte>(value));
            } else if (component_type == 5123U) {
                append_uint16(static_cast<std::uint16_t>(value));
            } else {
                append_uint32(value);
            }
        }
        return EncodedIndexRange{range.value(), component_type};
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

    void append_uint16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::byte>(value & 0xffU));
        bytes_.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    }

    void append_float(float value) {
        append_uint32(std::bit_cast<std::uint32_t>(value));
    }

    std::vector<std::byte> bytes_;
};

struct ExportPackage {
    ExportData document;
    BinaryBuilder binary;
    std::vector<PrimitiveOutput> primitives;
    std::vector<ByteRange> views;
    std::vector<Accessor> accessors;
    std::vector<EncodedImageOutput> encoded_images;
    std::vector<ModelImageMimeType> image_mime_types;
    std::vector<std::optional<std::uint32_t>> image_views;
    std::vector<std::string> image_uris;
    std::string buffer_uri;
    bool glb = false;
    bool embedded = false;
};

[[nodiscard]] Result<EncodedImageOutput> encoded_image(const ImageView& image);
[[nodiscard]] Result<std::string> build_json(const ExportPackage& package);

} // namespace elf3d::gltf::exporter_detail
