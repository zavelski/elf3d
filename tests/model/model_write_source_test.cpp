#include <elf3d/model.h>
#include <model/document_write_source.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::model::detail;
static_assert(sizeof(DocumentWriteSource) <= 256);
static_assert(std::is_nothrow_move_constructible_v<DocumentWriteSource>);

[[nodiscard]] bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "FAIL write source: %s\n", message);
    return value;
}

[[nodiscard]] PrimitiveData make_geometry()
{
    PrimitiveData geometry;
    constexpr std::uint32_t vertices = 9003;
    for (std::uint32_t vertex = 0; vertex < vertices; ++vertex) {
        geometry.positions.push_back(
            {static_cast<float>(vertex / 3), static_cast<float>(vertex % 3), 0});
        geometry.indices.push_back(vertex);
    }
    geometry.normals.assign(vertices, {0, 0, 1});
    geometry.tangents.assign(vertices, {1, 0, 0, -1});
    geometry.texcoord0.assign(vertices, {0.25F, 0.5F});
    geometry.texcoord1.assign(vertices, {0.75F, 1});
    geometry.colors.assign(vertices, {1, 0.5F, 0.25F, 1});
    return geometry;
}

[[nodiscard]] bool add_children(Document& document, NodeId node)
{
    for (unsigned child = 0; child < 4201; ++child) {
        const auto id = document.create_node("child").value();
        if (!document.set_parent(id, node))
            return false;
    }
    return true;
}

[[nodiscard]] bool add_images(Document& document)
{
    const std::vector<std::byte> pixels(1024 * 1024, std::byte{37});
    if (!document.create_image({512, 512, PixelFormat::rgba8_unorm, pixels}))
        return false;
    std::vector<std::byte> encoded(130019, std::byte{23});
    constexpr std::array signature{std::byte{0x76}, std::byte{0x2f}, std::byte{0x31},
                                   std::byte{0x01}};
    std::copy(signature.begin(), signature.end(), encoded.begin());
    return document
        .create_image({0, 0, PixelFormat::rgba8_unorm, {}, ModelImageMimeType::exr, encoded})
        .has_value();
}

[[nodiscard]] Document make_source()
{
    Document document;
    const auto scene = document.create_scene("Source scene").value();
    const auto node = document.create_node(std::string(130001, 'n') + "\xE2\x82\xAC").value();
    const auto mesh = document.create_mesh("Source mesh").value();
    ModelMaterialDescription description;
    description.name = "Material name";
    const auto first = document.create_material(description).value();
    const auto primitive = document.create_primitive(mesh, first, make_geometry());
    const auto attached = document.set_node_mesh(node, mesh);
    const auto rooted = document.add_scene_root(scene, node);
    const auto camera = document.set_node_perspective_camera(node, {0.8F, 0.1F, 100});
    ModelCameraPropertiesDescription camera_properties;
    camera_properties.name = "Authored camera";
    const auto properties = document.set_node_camera_properties(node, camera_properties);
    if (!primitive || !attached || !rooted || !camera || !properties)
        return Document{};
    if (!add_images(document))
        return Document{};
    if (!add_children(document, node))
        return Document{};
    ModelDocumentMetadata metadata;
    metadata.root.extras_json = "{\"padding\":\"" + std::string(130007, 'x') + "\"}";
    metadata.root.extensions.push_back(
        {"EXT_write_source", "{\"payload\":\"" + std::string(130009, 'z') + "\"}"});
    metadata.required_extensions.push_back("EXT_write_source");
    if (!document.attach_verified_metadata(std::move(metadata)))
        return Document{};
    return document;
}

[[nodiscard]] bool bounded_chunk(const Result<WriteSourceChunk>& chunk, std::size_t total)
{
    return chunk && chunk.value().bytes <= write_source_chunk_bytes &&
           chunk.value().total == total && chunk.value().elements != 0;
}

template <typename T>
[[nodiscard]] bool stream_matches(const DocumentWriteSource& reader, DocumentView document,
                                  WriteSourceDataRequest request, std::span<const T> expected)
{
    std::array<std::byte, write_source_chunk_bytes * 2 + 7> bytes;
    while (request.offset < expected.size()) {
        bytes.fill(std::byte{90});
        const auto chunk = reader.read_data(document, request, bytes);
        if (!bounded_chunk(chunk, expected.size()))
            return false;
        const auto part = std::as_bytes(expected.subspan(request.offset, chunk.value().elements));
        if (part.size() != chunk.value().bytes ||
            std::memcmp(part.data(), bytes.data(), part.size()) != 0)
            return false;
        const auto untouched = std::span{bytes}.subspan(part.size());
        if (!std::all_of(untouched.begin(), untouched.end(),
                         [](auto value) { return value == std::byte{90}; }))
            return false;
        request.offset += chunk.value().elements;
    }
    const auto end = reader.read_data(document, request, {});
    return end && end.value().bytes == 0 && end.value().total == expected.size();
}

[[nodiscard]] bool vertex_streams(const DocumentWriteSource& reader, DocumentView document)
{
    const auto data = document.primitive_at(0).value().data;
    const WriteSourceAddress address{WriteSourceObject::primitive, 0};
    return stream_matches(reader, document, {address, WriteSourceData::positions},
                          data.positions) &&
           stream_matches(reader, document, {address, WriteSourceData::normals}, data.normals) &&
           stream_matches(reader, document, {address, WriteSourceData::tangents}, data.tangents) &&
           stream_matches(reader, document, {address, WriteSourceData::uv0}, data.texcoord0) &&
           stream_matches(reader, document, {address, WriteSourceData::uv1}, data.texcoord1) &&
           stream_matches(reader, document, {address, WriteSourceData::colors}, data.colors) &&
           stream_matches(reader, document, {address, WriteSourceData::indices}, data.indices);
}

[[nodiscard]] bool resource_streams(const DocumentWriteSource& reader, DocumentView document)
{
    const auto image = document.image_at(0).value();
    const auto node = document.node_at(0).value();
    return stream_matches(reader, document,
                          {{WriteSourceObject::image, 1}, WriteSourceData::image_source},
                          document.image_at(1).value().source_bytes) &&
           stream_matches(reader, document,
                          {{WriteSourceObject::image, 0}, WriteSourceData::image_pixels},
                          image.pixels) &&
           stream_matches(reader, document,
                          {{WriteSourceObject::image, 0}, WriteSourceData::image_source},
                          image.source_bytes) &&
           stream_matches(reader, document,
                          {{WriteSourceObject::node, 0}, WriteSourceData::children},
                          node.children) &&
           stream_matches(reader, document, {{WriteSourceObject::scene, 0}, WriteSourceData::roots},
                          document.scene_at(0).value().roots) &&
           stream_matches(reader, document,
                          {{WriteSourceObject::mesh, 0}, WriteSourceData::primitives},
                          document.mesh_at(0).value().primitives);
}

[[nodiscard]] bool text_matches(const DocumentWriteSource& reader, DocumentView document,
                                WriteSourceTextRequest request, std::string_view expected)
{
    std::array<char, write_source_chunk_bytes * 2> bytes{};
    std::string result;
    while (request.offset < expected.size()) {
        const auto chunk = reader.read_text(document, request, bytes);
        if (!chunk || chunk.value().bytes == 0 || chunk.value().bytes > write_source_chunk_bytes ||
            chunk.value().total != expected.size())
            return false;
        result.append(bytes.data(), chunk.value().bytes);
        request.offset += chunk.value().bytes;
    }
    return result == expected;
}

[[nodiscard]] bool text_streams(const DocumentWriteSource& reader, DocumentView document)
{
    const auto metadata = document.root_metadata();
    const WriteSourceAddress root{WriteSourceObject::root, 0};
    return text_matches(reader, document, {{WriteSourceObject::node, 0}, WriteSourceText::name},
                        document.node_at(0).value().name) &&
           text_matches(reader, document, {root, WriteSourceText::extras}, *metadata.extras_json) &&
           text_matches(reader, document, {root, WriteSourceText::extension_name},
                        metadata.extensions[0].name) &&
           text_matches(reader, document, {root, WriteSourceText::extension_data},
                        metadata.extensions[0].data) &&
           text_matches(reader, document, {root, WriteSourceText::required_extension},
                        document.required_extensions()[0]) &&
           text_matches(reader, document, {{WriteSourceObject::camera, 0}, WriteSourceText::name},
                        "Authored camera");
}

[[nodiscard]] bool corner_streams(const DocumentWriteSource& reader, DocumentView document)
{
    std::array<std::byte, write_source_chunk_bytes> bytes{};
    WriteSourceDataRequest request{{WriteSourceObject::primitive, 0}, WriteSourceData::corners};
    const auto data = document.primitive_at(0).value().data;
    while (request.offset < data.indices.size()) {
        const auto chunk = reader.read_data(document, request, bytes);
        if (!chunk || chunk.value().elements == 0)
            return false;
        for (std::size_t index = 0; index < chunk.value().elements; ++index) {
            WriteSourceCorner corner;
            std::memcpy(&corner, bytes.data() + index * sizeof(corner), sizeof(corner));
            const auto vertex = data.indices[request.offset + index];
            if (corner.vertex != vertex || corner.position != data.positions[vertex] ||
                corner.uv1 != data.texcoord1[vertex] ||
                corner.material != document.primitive_at(0).value().material)
                return false;
        }
        request.offset += chunk.value().elements;
    }
    return true;
}

[[nodiscard]] bool rejected_requests(const DocumentWriteSource& reader, DocumentView document)
{
    std::array<std::byte, 3> too_small{};
    const WriteSourceAddress primitive{WriteSourceObject::primitive, 0};
    if (reader.read_data(document, {primitive, WriteSourceData::positions}, too_small) ||
        reader.read_data(document, {primitive, WriteSourceData::positions, 9004}, too_small) ||
        reader.read_data(document, {{WriteSourceObject::image, 0}, WriteSourceData::positions},
                         too_small))
        return false;
    std::array<char, 32> text{};
    return !reader.record(document, {WriteSourceObject::node, document.node_count()}) &&
           !reader.record(document, {static_cast<WriteSourceObject>(255), 0}) &&
           !reader.read_text(document,
                             {{WriteSourceObject::root, 0}, WriteSourceText::extension_name, 1},
                             text) &&
           !reader.read_text(
               document, {{WriteSourceObject::root, 0}, WriteSourceText::extras, 0, 999999}, text);
}

[[nodiscard]] bool ownership_checks()
{
    Document first;
    Document foreign;
    const auto source = DocumentWriteSource::create(first.view()).value();
    if (source.matches(foreign.view()) || source.record(foreign.view(), {}))
        return false;
    Document moved = std::move(first);
    if (!source.matches(moved.view()) || source.matches(first.view()))
        return false;
    if (!moved.create_node("mutation") || source.matches(moved.view()) ||
        source.record(moved.view(), {}))
        return false;
    const auto current = DocumentWriteSource::create(moved.view()).value();
    moved = Document{};
    return !current.matches(moved.view()) && !current.record(moved.view(), {});
}

[[nodiscard]] bool scalar_records(const DocumentWriteSource& reader, DocumentView document)
{
    const auto material = reader.record(document, {WriteSourceObject::material, 0});
    const auto primitive = reader.record(document, {WriteSourceObject::primitive, 0});
    const auto camera = reader.record(document, {WriteSourceObject::camera, 0});
    const auto absent_camera = reader.record(document, {WriteSourceObject::camera, 1});
    if (!material || !primitive || !camera || !absent_camera)
        return false;
    return material.value().material_description.name.data() == nullptr &&
           material.value().name_bytes == std::string_view{"Material name"}.size() &&
           primitive.value().elements[0] == 9003 && camera.value().present &&
           !absent_camera.value().present;
}
} // namespace

int elf3d_model_write_source_test()
{
    auto document = make_source();
    const auto source = DocumentWriteSource::create(document.view());
    if (!require(source && document.primitive_count() == 1, "fixture and identity capture"))
        return 1;
    if (!require(vertex_streams(source.value(), document.view()) &&
                     resource_streams(source.value(), document.view()),
                 "bounded exact attribute, image and relationship streams"))
        return 2;
    if (!require(text_streams(source.value(), document.view()) &&
                     corner_streams(source.value(), document.view()),
                 "long UTF-8/text and material-aware current corners"))
        return 3;
    if (!require(scalar_records(source.value(), document.view()) &&
                     rejected_requests(source.value(), document.view()) && ownership_checks(),
                 "pointer-free scalar records and invalid/stale/foreign ownership rejection"))
        return 4;
    return 0;
}
