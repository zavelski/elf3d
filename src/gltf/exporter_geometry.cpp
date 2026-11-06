#include "exporter_geometry_input.h"

#include <elf3d/core/assert.h>
#include <model/document_handle_access.h>
#include <model/document_storage.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace elf3d::gltf::exporter_detail {
using namespace model::detail;
namespace {
[[nodiscard]] std::size_t source_count(const WriteSourceRecord& source,
                                       WriteSourceData stream) noexcept
{
    return source.elements[static_cast<std::size_t>(stream)];
}
[[nodiscard]] bool valid_shape(const WriteSourceRecord& source) noexcept
{
    const auto vertices = source_count(source, WriteSourceData::positions);
    const auto indices = source_count(source, WriteSourceData::indices);
    if (vertices == 0 || vertices > std::numeric_limits<std::uint32_t>::max() || indices == 0 ||
        indices > std::numeric_limits<std::uint32_t>::max() || indices % 3 != 0)
        return false;
    for (std::size_t index = 1; index < 6; ++index) {
        if (source.elements[index] != 0 && source.elements[index] != vertices)
            return false;
    }
    return true;
}
[[nodiscard]] Result<std::uint32_t> primary_material(const ExportData& metadata,
                                                     MaterialId material)
{
    const auto value = DocumentHandleAccess::value(material);
    if (value == 0 || value > metadata.material_indices.size())
        return Error{ErrorCode::invalid_material_id,
                     "Geometry material is outside the captured table"};
    const auto& entry = metadata.material_indices[static_cast<std::size_t>(value - 1)];
    if (entry.id != material)
        return Error{ErrorCode::invalid_material_id,
                     "Geometry material belongs to another Document"};
    return entry.index;
}
[[nodiscard]] Result<void> validate_source(const WriteSourceRecord& source,
                                           const ExportData& metadata)
{
    if (source.address.object != WriteSourceObject::primitive ||
        source.address.index >= metadata.primitives.size() || !valid_shape(source))
        return Error{ErrorCode::invalid_mesh_data, "Ordinary geometry source shape is invalid"};
    if (source.material_description.name.data() != nullptr ||
        !source.material_description.name.empty())
        return Error{ErrorCode::invalid_argument, "Geometry source contains a borrowed name"};
    const auto& primitive = metadata.primitives[source.address.index];
    if (primitive.id != source.primitive || primitive.mesh != source.mesh ||
        primitive.material != source.material)
        return Error{ErrorCode::invalid_primitive_id,
                     "Geometry source does not match the captured primitive"};
    return {};
}
[[nodiscard]] Result<std::uint32_t>
register_attribute(ByteRange range, AccessorDescription description, ExportPackage& package)
{
    if (description.count > std::numeric_limits<std::uint32_t>::max() ||
        package.accessors.size() > std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::size_overflow, "Geometry exceeds output accessor limits"};
    const auto view = append_buffer_view(package.views, range);
    if (!view)
        return view.error();
    const auto index = static_cast<std::uint32_t>(package.accessors.size());
    package.accessors.push_back({view.value(), description.component_type,
                                 static_cast<std::uint32_t>(description.count), description.type,
                                 description.bounds});
    return index;
}
template <typename T, typename Consume>
[[nodiscard]] Result<void> consume_values(std::span<const std::byte> bytes, std::size_t count,
                                          Consume consume)
{
    // The caller validated both element and byte bounds before this aligned copy.
    std::array<T, write_source_chunk_elements> values;
    std::memcpy(values.data(), bytes.data(), bytes.size());
    return consume(std::span<const T>{values}.first(count));
}
} // namespace

Result<std::uint32_t> append_buffer_view(std::vector<ByteRange>& views, ByteRange range)
{
    if (views.size() > std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::size_overflow, "Geometry exceeds output buffer-view limits"};
    const auto index = static_cast<std::uint32_t>(views.size());
    views.push_back(range);
    return index;
}

Result<ExportGeometryInput> ExportGeometryInput::create(WriteSourceRecord source,
                                                        const ExportData& metadata)
{
    if (const auto valid = validate_source(source, metadata); !valid)
        return valid.error();
    const auto material = primary_material(metadata, source.material);
    if (!material)
        return material.error();
    ExportGeometryInput result;
    result.source_ = source;
    result.output_.material = material.value();
    return result;
}

ExportGeometryInput::Phase ExportGeometryInput::next(Phase phase) noexcept
{
    switch (phase) {
    case Phase::positions:
        return Phase::normals;
    case Phase::normals:
        return Phase::tangents;
    case Phase::tangents:
        return Phase::uv0;
    case Phase::uv0:
        return Phase::uv1;
    case Phase::uv1:
        return Phase::colors;
    case Phase::colors:
        return Phase::index_scan;
    case Phase::index_scan:
        return Phase::indices;
    default:
        return Phase::complete;
    }
}
WriteSourceData ExportGeometryInput::stream() const noexcept
{
    if (phase_ >= Phase::index_scan)
        return WriteSourceData::indices;
    constexpr std::array streams{WriteSourceData::positions, WriteSourceData::normals,
                                 WriteSourceData::tangents,  WriteSourceData::uv0,
                                 WriteSourceData::uv1,       WriteSourceData::colors};
    return streams[static_cast<std::size_t>(phase_)];
}
std::size_t ExportGeometryInput::count() const noexcept
{
    return source_count(source_, stream());
}
std::size_t ExportGeometryInput::stride() const noexcept
{
    if (phase_ >= Phase::index_scan)
        return sizeof(std::uint32_t);
    constexpr std::array strides{sizeof(Float3), sizeof(Float3), sizeof(Float4),
                                 sizeof(Float2), sizeof(Float2), sizeof(Color4)};
    return strides[static_cast<std::size_t>(phase_)];
}
void ExportGeometryInput::advance() noexcept
{
    phase_ = next(phase_);
    offset_ = 0;
    range_.reset();
    while (phase_ != Phase::complete && count() == 0)
        phase_ = next(phase_);
}
std::optional<WriteSourceDataRequest> ExportGeometryInput::request() const noexcept
{
    if (ready())
        return std::nullopt;
    return WriteSourceDataRequest{source_.address, stream(), offset_};
}
bool ExportGeometryInput::ready() const noexcept
{
    return phase_ == Phase::complete;
}
Result<PrimitiveOutput> ExportGeometryInput::output() const noexcept
{
    if (!ready())
        return Error{ErrorCode::invalid_argument, "Geometry output is not complete"};
    return output_;
}
bool ExportGeometryInput::matches(WriteSourceDataRequest value) const noexcept
{
    const auto expected = request();
    if (!expected)
        return false;
    return value.address.object == expected->address.object &&
           value.address.index == expected->address.index && value.data == expected->data &&
           value.offset == expected->offset;
}
bool ExportGeometryInput::valid_chunk(WriteSourceChunk chunk, std::size_t bytes) const noexcept
{
    if (chunk.bytes != bytes || chunk.bytes > write_source_chunk_bytes || chunk.elements == 0 ||
        chunk.elements > write_source_chunk_elements || chunk.elements * stride() != bytes)
        return false;
    return chunk.total == count() && offset_ <= chunk.total &&
           chunk.elements <= chunk.total - offset_;
}
Result<void> ExportGeometryInput::append_range(Result<ByteRange> part)
{
    if (!part)
        return part.error();
    if (!range_)
        range_ = part.value();
    else {
        ELF3D_ASSERT(part.value().offset == range_->offset + range_->length);
        range_->length += part.value().length;
    }
    return {};
}
Result<void> ExportGeometryInput::consume_positions(std::span<const Float3> values,
                                                    ExportPackage& package)
{
    PrimitiveDataView data;
    data.positions = values;
    const auto bounds = primitive_bounds(data);
    if (!bounds)
        return bounds.error();
    bounds_ = bounds_ ? merge(*bounds_, bounds.value()) : bounds.value();
    return append_range(package.binary.append_positions(values));
}
Result<void> ExportGeometryInput::consume_indices(std::span<const std::uint32_t> values,
                                                  ExportPackage& package)
{
    if (const auto valid =
            validate_indices(values, source_count(source_, WriteSourceData::positions));
        !valid)
        return valid.error();
    if (phase_ == Phase::index_scan) {
        maximum_index_ = std::max(maximum_index_, *std::max_element(values.begin(), values.end()));
        return {};
    }
    return indices_.append(values, package.binary);
}
Result<void> ExportGeometryInput::consume_normals(std::span<const Float3> values,
                                                  ExportPackage& package)
{
    if (const auto valid = validate_normals(values); !valid)
        return valid.error();
    return append_range(package.binary.append_positions(values));
}
Result<void> ExportGeometryInput::consume_uvs(std::span<const Float2> values,
                                              ExportPackage& package)
{
    const auto semantic = phase_ == Phase::uv0 ? "Primitive TEXCOORD_0 values must be finite"
                                               : "Primitive TEXCOORD_1 values must be finite";
    if (const auto valid = validate_texcoords(values, semantic); !valid)
        return valid.error();
    return append_range(package.binary.append_texcoords(values));
}
Result<void> ExportGeometryInput::consume_colors(std::span<const Color4> values,
                                                 ExportPackage& package)
{
    if (const auto valid = validate_colors(values); !valid)
        return valid.error();
    return append_range(package.binary.append_colors(values));
}
Result<void> ExportGeometryInput::consume(std::span<const std::byte> bytes, std::size_t elements,
                                          ExportPackage& package)
{
    switch (phase_) {
    case Phase::positions:
        return consume_values<Float3>(
            bytes, elements, [&](auto values) { return consume_positions(values, package); });
    case Phase::normals:
        return consume_values<Float3>(
            bytes, elements, [&](auto values) { return consume_normals(values, package); });
    case Phase::tangents:
        // File policy omits TANGENT, but invalid source tangents must still reject.
        return consume_values<Float4>(bytes, elements,
                                      [](auto values) { return validate_tangents(values); });
    case Phase::uv0:
    case Phase::uv1:
        return consume_values<Float2>(bytes, elements,
                                      [&](auto values) { return consume_uvs(values, package); });
    case Phase::colors:
        return consume_values<Color4>(bytes, elements,
                                      [&](auto values) { return consume_colors(values, package); });
    default:
        return consume_values<std::uint32_t>(
            bytes, elements, [&](auto values) { return consume_indices(values, package); });
    }
}
Result<void> ExportGeometryInput::finish_attribute(ExportPackage& package)
{
    ELF3D_ASSERT(range_);
    const auto type =
        phase_ == Phase::colors ? "VEC4" : (phase_ <= Phase::normals ? "VEC3" : "VEC2");
    const auto bounds = phase_ == Phase::positions ? bounds_ : std::optional<Bounds3>{};
    const auto accessor = register_attribute(*range_, {0, 5126U, count(), type, bounds}, package);
    if (!accessor)
        return accessor.error();
    switch (phase_) {
    case Phase::positions:
        output_.positions = accessor.value();
        break;
    case Phase::normals:
        output_.normals = accessor.value();
        break;
    case Phase::uv0:
        output_.texcoord0 = accessor.value();
        break;
    case Phase::uv1:
        output_.texcoord1 = accessor.value();
        break;
    default:
        output_.colors = accessor.value();
        break;
    }
    return {};
}
Result<void> ExportGeometryInput::finish_stream(ExportPackage& package)
{
    if (phase_ == Phase::index_scan)
        indices_ = ExportIndexOutput{maximum_index_};
    else if (phase_ == Phase::indices) {
        const auto range = indices_.finish(package.binary);
        if (!range)
            return range.error();
        const auto accessor = register_attribute(
            range.value(), {0, indices_.component_type(), count(), "SCALAR", {}}, package);
        if (!accessor)
            return accessor.error();
        output_.indices = accessor.value();
    } else if (phase_ != Phase::tangents) {
        if (const auto finished = finish_attribute(package); !finished)
            return finished.error();
    }
    advance();
    return {};
}
Result<void> ExportGeometryInput::accept(WriteSourceDataRequest value, WriteSourceChunk chunk,
                                         std::span<const std::byte> bytes, ExportPackage& package)
{
    if (!matches(value) || !valid_chunk(chunk, bytes.size()))
        return Error{ErrorCode::invalid_argument,
                     "Geometry input packet has an invalid size, stream or sequence"};
    if (const auto consumed = consume(bytes, chunk.elements, package); !consumed)
        return consumed.error();
    offset_ += chunk.elements;
    if (offset_ == chunk.total)
        return finish_stream(package);
    return {};
}
Result<PrimitiveOutput> drive_ordinary_geometry(std::size_t primitive, ExportPackage& package,
                                                DocumentView document)
{
    const auto description = package.document.metadata_owner->primitive_source(primitive);
    const auto reader = DocumentWriteSource::create(document);
    if (!description || !reader)
        return !description ? description.error() : reader.error();
    auto output = ExportGeometryInput::create(description.value(), package.document);
    if (!output)
        return output.error();
    std::array<std::byte, write_source_chunk_bytes> bytes;
    while (const auto request = output.value().request()) {
        const auto chunk = reader.value().read_data(document, *request, bytes);
        if (!chunk)
            return chunk.error();
        if (const auto accepted = output.value().accept(
                *request, chunk.value(), std::span{bytes}.first(chunk.value().bytes), package);
            !accepted)
            return accepted.error();
    }
    return output.value().output();
}
} // namespace elf3d::gltf::exporter_detail
