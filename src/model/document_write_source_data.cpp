#include "document_write_source_internal.h"

#include <array>
#include <limits>

namespace elf3d::model::detail {
namespace {
[[nodiscard]] WriteSourceCorner vertex_at(const PrimitiveView& source, std::uint32_t vertex)
{
    const auto data = source.data;
    WriteSourceCorner result;
    result.vertex = vertex;
    result.material = source.material;
    result.position = data.positions[result.vertex];
    result.normal = data.normals.empty() ? Float3{} : data.normals[result.vertex];
    result.tangent = data.tangents.empty() ? Float4{} : data.tangents[result.vertex];
    result.uv0 = data.texcoord0.empty() ? Float2{} : data.texcoord0[result.vertex];
    result.uv1 = data.texcoord1.empty() ? Float2{} : data.texcoord1[result.vertex];
    result.color = data.colors.empty() ? Color4{} : data.colors[result.vertex];
    return result;
}

[[nodiscard]] bool valid_vertex_shape(PrimitiveDataView data)
{
    const std::array counts{data.normals.size(), data.tangents.size(), data.texcoord0.size(),
                            data.texcoord1.size(), data.colors.size()};
    for (const auto count : counts) {
        if (count != 0 && count != data.positions.size())
            return false;
    }
    return true;
}
[[nodiscard]] Result<WriteSourceChunk>
copy_vertices(const PrimitiveView& source, std::size_t offset, std::span<std::byte> destination)
{
    const auto total = source.data.positions.size();
    if (!valid_vertex_shape(source.data) || offset > total ||
        total > std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::invalid_argument, "Write vertex source shape or offset is invalid"};
    const auto count = std::min({total - offset, destination.size() / sizeof(WriteSourceCorner),
                                 write_source_chunk_bytes / sizeof(WriteSourceCorner),
                                 write_source_chunk_elements});
    if (count == 0 && offset < total)
        return Error{ErrorCode::invalid_argument, "Write destination cannot hold a source vertex"};
    for (std::size_t index = 0; index < count; ++index) {
        const auto vertex = vertex_at(source, static_cast<std::uint32_t>(offset + index));
        std::memcpy(destination.data() + index * sizeof(vertex), &vertex, sizeof(vertex));
    }
    return WriteSourceChunk{count * sizeof(WriteSourceCorner), count, total};
}

[[nodiscard]] Result<WriteSourceChunk> copy_corners(const PrimitiveView& source, std::size_t offset,
                                                    std::span<std::byte> destination)
{
    const auto total = source.data.indices.size();
    if (offset > total || !valid_vertex_shape(source.data)) {
        return Error{ErrorCode::invalid_argument, "Write corner source shape or offset is invalid"};
    }
    const auto count = std::min({total - offset, destination.size() / sizeof(WriteSourceCorner),
                                 write_source_chunk_bytes / sizeof(WriteSourceCorner),
                                 write_source_chunk_elements});
    if (count == 0 && offset < total) {
        return Error{ErrorCode::invalid_argument, "Write destination cannot hold a source corner"};
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto id = source.data.indices[offset + index];
        if (id >= source.data.positions.size())
            return Error{ErrorCode::mesh_index_out_of_range,
                         "Write corner index is outside its positions"};
        auto corner = vertex_at(source, id);
        std::memcpy(destination.data() + index * sizeof(corner), &corner, sizeof(corner));
    }
    return WriteSourceChunk{count * sizeof(WriteSourceCorner), count, total};
}
} // namespace

Result<WriteSourceChunk> write_vertex_data(DocumentView source, WriteSourceDataRequest request,
                                           std::span<std::byte> destination)
{
    if (request.address.object != WriteSourceObject::primitive) {
        return Error{ErrorCode::invalid_argument, "Write attribute address is not a primitive"};
    }
    const auto data = source.primitive_at(request.address.index).value().data;
    switch (request.data) {
    case WriteSourceData::positions:
        return copy_write_source(data.positions, request.offset, destination);
    case WriteSourceData::normals:
        return copy_write_source(data.normals, request.offset, destination);
    case WriteSourceData::tangents:
        return copy_write_source(data.tangents, request.offset, destination);
    case WriteSourceData::uv0:
        return copy_write_source(data.texcoord0, request.offset, destination);
    case WriteSourceData::uv1:
        return copy_write_source(data.texcoord1, request.offset, destination);
    case WriteSourceData::colors:
        return copy_write_source(data.colors, request.offset, destination);
    default:
        return Error{ErrorCode::invalid_argument, "Write attribute stream is invalid"};
    }
}

Result<WriteSourceChunk> write_index_data(DocumentView source, WriteSourceDataRequest request,
                                          std::span<std::byte> destination)
{
    if (request.address.object != WriteSourceObject::primitive) {
        return Error{ErrorCode::invalid_argument, "Write index address is not a primitive"};
    }
    const auto primitive = source.primitive_at(request.address.index).value();
    switch (request.data) {
    case WriteSourceData::indices:
        return copy_write_source(primitive.data.indices, request.offset, destination);
    case WriteSourceData::vertices:
        return copy_vertices(primitive, request.offset, destination);
    case WriteSourceData::corners:
        return copy_corners(primitive, request.offset, destination);
    default:
        return Error{ErrorCode::invalid_argument, "Write index stream is invalid"};
    }
}

Result<WriteSourceChunk> write_resource_data(DocumentView source, WriteSourceDataRequest request,
                                             std::span<std::byte> destination)
{
    const auto address = request.address;
    if (address.object == WriteSourceObject::node && request.data == WriteSourceData::children) {
        return copy_write_source(source.node_at(address.index).value().children, request.offset,
                                 destination);
    }
    if (address.object == WriteSourceObject::scene && request.data == WriteSourceData::roots) {
        return copy_write_source(source.scene_at(address.index).value().roots, request.offset,
                                 destination);
    }
    if (address.object == WriteSourceObject::mesh && request.data == WriteSourceData::primitives) {
        return copy_write_source(source.mesh_at(address.index).value().primitives, request.offset,
                                 destination);
    }
    if (address.object == WriteSourceObject::image) {
        const auto image = source.image_at(address.index).value();
        switch (request.data) {
        case WriteSourceData::image_pixels:
            return copy_write_source(image.pixels, request.offset, destination);
        case WriteSourceData::image_source:
            return copy_write_source(image.source_bytes, request.offset, destination);
        default:
            break;
        }
    }
    return Error{ErrorCode::invalid_argument,
                 "Write resource stream does not belong to its address"};
}
} // namespace elf3d::model::detail
