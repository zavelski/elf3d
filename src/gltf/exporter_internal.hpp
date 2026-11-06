#pragma once

#include "exporter_binary_spool.hpp"
#include "exporter_source_metadata.h"

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

// glTF reserves each component type's all-ones value for primitive restart.
[[nodiscard]] constexpr std::uint32_t index_component_type(std::uint32_t maximum) noexcept
{
    return maximum < 255U ? 5121U : (maximum < 65535U ? 5123U : 5125U);
}

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
    std::optional<std::uint32_t> tangents;
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
    std::unique_ptr<ExportSourceMetadata> metadata_owner;
    ModelJsonMetadataView root_metadata;
    ModelJsonMetadataView asset_metadata;
    std::span<const std::string> required_extensions;
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

[[nodiscard]] Result<std::uint32_t> append_buffer_view(std::vector<ByteRange>& views,
                                                       ByteRange range);

struct EncodedImageOutput {
    ExportBinarySpool data;
    ModelImageMimeType mime_type = ModelImageMimeType::png;
    bool reencoded = false;
};

struct EncodedImageInfo {
    ModelImageMimeType mime_type = ModelImageMimeType::png;
    bool reencoded = false;
};

class BinaryBuilder final {
  public:
    [[nodiscard]] Result<void> open(const OutputPath& target)
    {
        return data_.open(target);
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return data_.size();
    }
    [[nodiscard]] Result<void> copy_to(std::ostream& destination)
    {
        return data_.copy_to(destination);
    }
    [[nodiscard]] Result<void> read(std::size_t offset, std::span<std::byte> destination)
    {
        return data_.read(offset, destination);
    }
    [[nodiscard]] Result<OutputPath> take_path()
    {
        return data_.take_path();
    }
    [[nodiscard]] Result<void> discard() noexcept
    {
        return data_.discard();
    }
    [[nodiscard]] Result<ByteRange> append_positions(std::span<const Float3> values)
    {
        const Result<ByteRange> range = reserve(values.size(), 12U);
        if (!range) {
            return range.error();
        }
        for (const Float3 value : values) {
            append_float(value.x);
            append_float(value.y);
            append_float(value.z);
        }
        return completed(range.value());
    }

    [[nodiscard]] Result<ByteRange> append_texcoords(std::span<const Float2> values)
    {
        const Result<ByteRange> range = reserve(values.size(), 8U);
        if (!range) {
            return range.error();
        }
        for (const Float2 value : values) {
            append_float(value.x);
            append_float(value.y);
        }
        return completed(range.value());
    }

    [[nodiscard]] Result<ByteRange> append_colors(std::span<const Color4> values)
    {
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
        return completed(range.value());
    }

    [[nodiscard]] Result<ByteRange> append_tangents(std::span<const Float4> values)
    {
        const Result<ByteRange> range = reserve(values.size(), 16U);
        if (!range) {
            return range.error();
        }
        for (const Float4 value : values) {
            append_float(value.x);
            append_float(value.y);
            append_float(value.z);
            append_float(value.w);
        }
        return completed(range.value());
    }

    [[nodiscard]] Result<EncodedIndexRange> append_indices(std::span<const std::uint32_t> values)
    {
        std::uint32_t maximum = 0U;
        for (const std::uint32_t value : values) {
            if (value > maximum) {
                maximum = value;
            }
        }

        if (maximum == std::numeric_limits<std::uint32_t>::max())
            return Error{ErrorCode::mesh_index_out_of_range,
                         "Index uses reserved primitive restart"};
        const std::uint32_t component_type = index_component_type(maximum);
        const std::size_t stride =
            component_type == 5121U ? 1U : (component_type == 5123U ? 2U : 4U);
        const Result<ByteRange> range = reserve(values.size(), stride);

        if (!range) {
            return range.error();
        }
        for (const std::uint32_t value : values) {
            if (component_type == 5121U) {
                data_.append_byte(static_cast<std::byte>(value));
            } else if (component_type == 5123U) {
                append_uint16(static_cast<std::uint16_t>(value));
            } else {
                append_uint32(value);
            }
        }
        const auto written = data_.status();
        return written ? Result<EncodedIndexRange>{EncodedIndexRange{range.value(), component_type}}
                       : Result<EncodedIndexRange>{written.error()};
    }

    [[nodiscard]] Result<ByteRange> append_bytes(std::span<const std::byte> values)
    {
        const Result<ByteRange> range = reserve(values.size(), 1U);
        if (!range) {
            return range.error();
        }
        data_.append(values);
        return completed(range.value());
    }

  private:
    [[nodiscard]] Result<ByteRange> completed(ByteRange range) const noexcept
    {
        const auto written = data_.status();
        return written ? Result<ByteRange>{range} : Result<ByteRange>{written.error()};
    }

    [[nodiscard]] Result<ByteRange> reserve(std::size_t count, std::size_t stride)
    {
        if (const auto ready = data_.status(); !ready) {
            return ready.error();
        }
        if (count > std::numeric_limits<std::uint32_t>::max() / stride) {
            return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit buffer limit"};
        }
        const std::size_t length = count * stride;
        const auto padding = (4U - data_.size() % 4U) % 4U;
        if (data_.size() + padding > std::numeric_limits<std::uint32_t>::max() - length) {
            return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit buffer limit"};
        }
        for (std::size_t index = 0; index < padding; ++index) {
            data_.append_byte(std::byte{0});
        }
        return ByteRange{static_cast<std::uint32_t>(data_.size()),
                         static_cast<std::uint32_t>(length)};
    }

    void append_uint32(std::uint32_t value)
    {
        data_.append_byte(static_cast<std::byte>(value & 0xffU));
        data_.append_byte(static_cast<std::byte>((value >> 8U) & 0xffU));
        data_.append_byte(static_cast<std::byte>((value >> 16U) & 0xffU));
        data_.append_byte(static_cast<std::byte>((value >> 24U) & 0xffU));
    }

    void append_uint16(std::uint16_t value)
    {
        data_.append_byte(static_cast<std::byte>(value & 0xffU));
        data_.append_byte(static_cast<std::byte>((value >> 8U) & 0xffU));
    }

    void append_float(float value)
    {
        append_uint32(std::bit_cast<std::uint32_t>(value));
    }

    ExportBinarySpool data_;
};

struct ExportPackage {
    ExportData document;
    BinaryBuilder binary;
    std::vector<PrimitiveOutput> primitives;
    std::vector<ByteRange> views;
    std::vector<Accessor> accessors;
    std::vector<EncodedImageInfo> encoded_images;
    std::vector<ModelImageMimeType> image_mime_types;
    std::vector<std::optional<std::uint32_t>> image_views;
    std::vector<std::string> image_uris;
    std::string buffer_uri;
    bool glb = false;
    bool embedded = false;
    ModelNumericWritePolicy numeric_policy = ModelNumericWritePolicy::round_trip;
    std::size_t cleaned_matrices = 0;
};

[[nodiscard]] Result<EncodedImageOutput> encoded_image(std::size_t index,
                                                       const ExportData& metadata,
                                                       DocumentView document,
                                                       const OutputPath& target);
[[nodiscard]] Result<std::string> build_json(ExportPackage& package);
[[nodiscard]] Result<void> append_cameras(std::string& output, const ExportData& document);

} // namespace elf3d::gltf::exporter_detail
