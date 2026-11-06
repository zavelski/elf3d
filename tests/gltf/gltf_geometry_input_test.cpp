#include <elf3d/model.h>
#include <gltf/exporter_container.hpp>
#include <gltf/exporter_geometry_input.h>
#include <gltf/exporter_output.hpp>
#include <model/document_write_source.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace elf3d;
using namespace elf3d::gltf::exporter_detail;
using namespace elf3d::model::detail;
constexpr std::uint32_t index_count = 18009;
[[nodiscard]] Float3 position_at(std::uint32_t index, std::uint32_t count)
{
    if (index + 1 == count)
        return {1000000, 2000000, 3000000};
    const auto value = static_cast<float>(index);
    return {value, static_cast<float>(index % 7), -value};
}
[[nodiscard]] std::uint32_t index_at(std::uint32_t index, std::uint32_t vertices)
{
    return index + 1 == index_count ? vertices - 2 : index % 17;
}
[[nodiscard]] Float2 uv_at(std::uint32_t index)
{
    return {static_cast<float>(index % 31) / 31.0F, static_cast<float>(index % 29) / 29.0F};
}
[[nodiscard]] Color4 color_at(std::uint32_t index)
{
    return {static_cast<float>(index % 251) / 251.0F, 0.25F, 0.5F, 1};
}
[[nodiscard]] PrimitiveData geometry(std::uint32_t vertices)
{
    PrimitiveData data;
    for (std::uint32_t i = 0; i < vertices; ++i) {
        data.positions.push_back(position_at(i, vertices));
        data.normals.push_back({0, 0, 1});
        data.tangents.push_back({1, 0, 0, i % 2 == 0 ? 1.0F : -1.0F});
        data.texcoord0.push_back(uv_at(i));
        data.texcoord1.push_back(uv_at(i + 7));
        data.colors.push_back(color_at(i));
    }
    for (std::uint32_t i = 0; i < index_count; ++i)
        data.indices.push_back(index_at(i, vertices));
    return data;
}
[[nodiscard]] Result<Document> fixture(std::uint32_t vertices)
{
    Document result;
    const auto material = result.create_material();
    const auto mesh = result.create_mesh("Every authored vertex");
    const auto node = result.create_node("Unused vertex remains in bounds");
    const auto scene = result.create_scene("Owned ordinary input");
    if (!material || !mesh || !node || !scene)
        return Error{ErrorCode::invalid_argument, "Cannot create ordinary input fixture"};
    if (!result.create_primitive(mesh.value(), material.value(), geometry(vertices)) ||
        !result.set_node_mesh(node.value(), mesh.value()) ||
        !result.add_scene_root(scene.value(), node.value()))
        return Error{ErrorCode::invalid_argument, "Cannot populate ordinary input fixture"};
    return result;
}
[[nodiscard]] Result<ExportPackage> package_for(DocumentView document,
                                                const std::filesystem::path& path)
{
    auto metadata = ExportSourceMetadata::capture(document);
    if (!metadata)
        return metadata.error();
    ExportPackage result;
    result.document.metadata_owner = std::move(metadata).value();
    if (const auto bound = result.document.metadata_owner->bind(result.document); !bound)
        return bound.error();
    if (const auto opened = result.binary.open(path); !opened)
        return opened.error();
    result.glb = true;
    result.embedded = true;
    return result;
}
[[nodiscard]] Result<std::size_t> feed(ExportGeometryInput& output,
                                       const DocumentWriteSource& reader, DocumentView document,
                                       ExportPackage& package)
{
    std::array<std::byte, write_source_chunk_bytes> bytes;
    std::size_t passes = 0;
    while (const auto request = output.request()) {
        if (request->data == WriteSourceData::indices && request->offset == 0)
            ++passes;
        // Odd early fragments exercise index alignment across packet boundaries.
        const std::size_t capacity = request->offset < 11 ? 17 : bytes.size();
        const auto chunk = reader.read_data(document, *request, std::span{bytes}.first(capacity));
        if (!chunk)
            return chunk.error();
        if (const auto accepted = output.accept(
                *request, chunk.value(), std::span{bytes}.first(chunk.value().bytes), package);
            !accepted)
            return accepted.error();
    }
    return passes;
}
[[nodiscard]] bool optional_attributes_present(const PrimitiveOutput& output)
{
    return !output.tangents && output.normals && output.texcoord0 && output.texcoord1 &&
           output.colors;
}
[[nodiscard]] bool inspect_metadata(const PrimitiveOutput& output, const ExportPackage& package,
                                    std::uint32_t vertices)
{
    const auto& position = package.accessors[output.positions];
    const auto& indices = package.accessors[output.indices];
    const auto maximum = vertices - 2;
    const auto type = maximum <= 255 ? 5121U : (maximum <= 65535 ? 5123U : 5125U);
    return position.count == vertices && position.bounds &&
           position.bounds->maximum == Float3{1000000, 2000000, 3000000} &&
           indices.count == index_count && indices.component_type == type &&
           optional_attributes_present(output);
}
[[nodiscard]] bool publish(const std::filesystem::path& path, ExportPackage& package)
{
    const auto json = build_json(package);
    if (!json)
        return false;
    elf3d::gltf::exporter_output::OutputFiles files;
    const auto staged = stage_glb(path, json.value(), package.binary, files);
    const bool committed = staged && files.publish();
    const auto discarded = files.discard();
    return committed && discarded.has_value();
}
[[nodiscard]] bool attribute_counts_match(PrimitiveDataView data, std::uint32_t vertices)
{
    return data.positions.size() == vertices && data.normals.size() == vertices &&
           data.texcoord0.size() == vertices && data.texcoord1.size() == vertices &&
           data.colors.size() == vertices && data.tangents.empty();
}
[[nodiscard]] bool attributes_match(PrimitiveDataView data, std::uint32_t vertices)
{
    if (!attribute_counts_match(data, vertices))
        return false;
    for (std::uint32_t i = 0; i < vertices; ++i) {
        if (data.positions[i] != position_at(i, vertices) || data.normals[i] != Float3{0, 0, 1} ||
            data.texcoord0[i] != uv_at(i) || data.texcoord1[i] != uv_at(i + 7) ||
            data.colors[i] != color_at(i))
            return false;
    }
    return true;
}
[[nodiscard]] bool reload_matches(const std::filesystem::path& path, std::uint32_t vertices)
{
    const auto loaded = load_document(path.string());
    if (!loaded)
        return false;
    const auto primitive = loaded.value().document.primitive_at(0).value();
    if (!attributes_match(primitive.data, vertices) || primitive.data.indices.size() != index_count)
        return false;
    for (std::uint32_t i = 0; i < index_count; ++i) {
        if (primitive.data.indices[i] != index_at(i, vertices))
            return false;
    }
    return true;
}
[[nodiscard]] bool finish_file(const std::filesystem::path& path, ExportPackage& package,
                               std::uint32_t vertices)
{
    const bool written = publish(path, package);
    const auto discarded = package.binary.discard();
    return written && discarded && reload_matches(path, vertices);
}
[[nodiscard]] bool round_trip(const std::filesystem::path& directory, std::uint32_t vertices)
{
    auto created = fixture(vertices);
    if (!created)
        return false;
    auto document = std::move(created).value();
    const auto path = directory / ("owned-ordinary-" + std::to_string(vertices) + ".glb");
    auto package = package_for(document.view(), path);
    const auto reader = DocumentWriteSource::create(document.view());
    if (!package || !reader)
        return false;
    const auto source = package.value().document.metadata_owner->primitive_source(0).value();
    auto output = ExportGeometryInput::create(source, package.value().document);
    if (!output)
        return false;
    const auto fed = feed(output.value(), reader.value(), document.view(), package.value());
    document = Document{};
    if (!fed || fed.value() != 2)
        return false;
    const auto ready = output.value().output();
    if (!ready || !inspect_metadata(ready.value(), package.value(), vertices))
        return false;
    package.value().primitives.push_back(ready.value());
    return finish_file(path, package.value(), vertices);
}
[[nodiscard]] bool advance_to(WriteSourceData stream, ExportGeometryInput& output,
                              const DocumentWriteSource& reader, DocumentView document,
                              ExportPackage& package)
{
    std::array<std::byte, write_source_chunk_bytes> bytes;
    while (const auto request = output.request()) {
        if (request->data == stream)
            return true;
        const auto chunk = reader.read_data(document, *request, bytes);
        if (!chunk || !output.accept(*request, chunk.value(),
                                     std::span{bytes}.first(chunk.value().bytes), package))
            return false;
    }
    return false;
}
struct InvalidInput {
    WriteSourceData data;
    ErrorCode error;
};
void invalidate(std::span<std::byte> bytes, WriteSourceData data)
{
    if (data == WriteSourceData::positions) {
        const Float3 invalid{std::numeric_limits<float>::quiet_NaN(), 0, 0};
        std::memcpy(bytes.data(), &invalid, sizeof(invalid));
    } else if (data == WriteSourceData::tangents) {
        const Float4 invalid{0, 0, 0, 1};
        std::memcpy(bytes.data(), &invalid, sizeof(invalid));
    } else {
        constexpr std::uint32_t invalid = 19;
        std::memcpy(bytes.data(), &invalid, sizeof(invalid));
    }
}
[[nodiscard]] bool matches_error(const Result<void>& result, ErrorCode error)
{
    return !result && result.error().code() == error;
}
[[nodiscard]] bool rejected_input(const std::filesystem::path& directory, InvalidInput invalid)
{
    auto document = fixture(19);
    if (!document)
        return false;
    auto package = package_for(document.value().view(), directory / "rejected-ordinary.glb");
    const auto reader = DocumentWriteSource::create(document.value().view());
    if (!package || !reader)
        return false;
    const auto source = package.value().document.metadata_owner->primitive_source(0).value();
    auto output = ExportGeometryInput::create(source, package.value().document);
    if (!output || !advance_to(invalid.data, output.value(), reader.value(),
                               document.value().view(), package.value()))
        return false;
    const auto request = *output.value().request();
    std::array<std::byte, write_source_chunk_bytes> bytes;
    const auto chunk = reader.value().read_data(document.value().view(), request, bytes);
    if (!chunk)
        return false;
    invalidate(bytes, invalid.data);
    const auto accepted = output.value().accept(
        request, chunk.value(), std::span{bytes}.first(chunk.value().bytes), package.value());
    const auto discarded = package.value().binary.discard();
    return matches_error(accepted, invalid.error) && !output.value().ready() && discarded;
}
[[nodiscard]] bool rejected_protocol(const std::filesystem::path& directory)
{
    auto document = fixture(19);
    if (!document)
        return false;
    auto package = package_for(document.value().view(), directory / "rejected-protocol.glb");
    const auto reader = DocumentWriteSource::create(document.value().view());
    if (!package || !reader)
        return false;
    auto source = package.value().document.metadata_owner->primitive_source(0).value();
    const auto original = source;
    source.elements[1]++;
    if (ExportGeometryInput::create(source, package.value().document))
        return false;
    source = original;
    source.primitive = {};
    if (ExportGeometryInput::create(source, package.value().document))
        return false;
    auto output = ExportGeometryInput::create(original, package.value().document);
    if (!output || output.value().output())
        return false;
    auto request = *output.value().request();
    std::array<std::byte, write_source_chunk_bytes> bytes;
    const auto chunk = reader.value().read_data(document.value().view(), request, bytes);
    if (!chunk)
        return false;
    request.offset++;
    const bool rejected = !output.value().accept(
        request, chunk.value(), std::span{bytes}.first(chunk.value().bytes), package.value());
    const auto discarded = package.value().binary.discard();
    return rejected && discarded;
}
} // namespace
bool elf3d_geometry_input_tests(const std::filesystem::path& directory)
{
    for (const auto vertices : {19U, 9007U, 70003U}) {
        if (!round_trip(directory, vertices)) {
            std::fprintf(stderr, "FAIL ordinary owned geometry round trip: %u vertices\n",
                         vertices);
            return false;
        }
    }
    constexpr std::array invalid{
        InvalidInput{WriteSourceData::positions, ErrorCode::non_finite_position},
        InvalidInput{WriteSourceData::tangents, ErrorCode::invalid_accessor},
        InvalidInput{WriteSourceData::indices, ErrorCode::mesh_index_out_of_range}};
    for (const auto test : invalid) {
        if (!rejected_input(directory, test))
            return false;
    }
    return rejected_protocol(directory);
}
