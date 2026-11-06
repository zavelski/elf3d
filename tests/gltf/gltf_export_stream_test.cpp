#include <elf3d/model.h>
#include <gltf/exporter_container.hpp>
#include <gltf/exporter_index_output.h>
#include <gltf/exporter_internal.hpp>
#include <gltf/exporter_output.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace elf3d;
constexpr std::uint32_t vertex_count = 180003;

[[nodiscard]] PrimitiveData large_geometry()
{
    PrimitiveData data;
    data.positions.resize(vertex_count);
    data.indices.resize(vertex_count);
    for (std::uint32_t index = 0; index < vertex_count; ++index) {
        const auto base = static_cast<float>(index / 3);
        data.positions[index] = {base + (index % 3 == 2 ? 1.0F : 0.0F),
                                 index % 3 == 1 ? 1.0F : 0.0F, 0};
        data.indices[index] = index;
    }
    return data;
}

[[nodiscard]] Result<Document> create_source(PrimitiveData geometry)
{
    Document document;
    const auto scene = document.create_scene("Streamed binary scene");
    const auto material = document.create_material({});
    const auto mesh = document.create_mesh("Large streamed mesh");
    const auto node = document.create_node("Large streamed node");
    if (!scene || !material || !mesh || !node) {
        return Error{ErrorCode::invalid_argument, "Cannot construct streamed exporter fixture"};
    }
    if (!document.create_primitive(mesh.value(), material.value(), std::move(geometry)) ||
        !document.set_node_mesh(node.value(), mesh.value()) ||
        !document.add_scene_root(scene.value(), node.value())) {
        return Error{ErrorCode::invalid_argument, "Cannot populate streamed exporter fixture"};
    }
    return document;
}

[[nodiscard]] bool geometry_matches(DocumentView first, DocumentView second)
{
    const auto expected = first.primitive_at(0);
    const auto actual = second.primitive_at(0);
    if (!expected || !actual) {
        return false;
    }
    const auto a = expected.value().data;
    const auto b = actual.value().data;
    return a.positions.size() == b.positions.size() && a.indices.size() == b.indices.size() &&
           std::equal(a.positions.begin(), a.positions.end(), b.positions.begin()) &&
           std::equal(a.indices.begin(), a.indices.end(), b.indices.begin());
}

[[nodiscard]] bool save_large(const std::filesystem::path& directory, std::string_view extension,
                              Document& document)
{
    const auto path = directory / ("large_stream" + std::string{extension});
    const auto revision = document.revision();
    const auto written = save_document(path.string(), document.view());
    if (!written || document.revision() != revision) {
        return false;
    }
    const auto loaded = load_document(path.string());
    if (!loaded || !geometry_matches(document.view(), loaded.value().document.view())) {
        return false;
    }
    const auto binary = extension == ".glb" ? path : directory / "large_stream.bin";
    return std::filesystem::file_size(binary) > 2 * 1024 * 1024;
}

using namespace elf3d::gltf::exporter_detail;
using elf3d::gltf::exporter_output::OutputFiles;
constexpr std::size_t container_chunk = 64 * 1024;

[[nodiscard]] PrimitiveData index_boundary_geometry(std::uint32_t maximum)
{
    PrimitiveData result;
    for (std::uint32_t i = 0; i <= maximum; ++i) {
        result.positions.push_back(
            {static_cast<float>(i / 3 * 3 + (i % 3 == 1)), i % 3 == 2 ? 1.0F : 0.0F, 0});
        result.indices.push_back(i);
    }
    if (result.indices.size() % 3 == 1)
        result.indices.insert(result.indices.end(), {0, 2});
    return result;
}

[[nodiscard]] bool index_boundary_case(const std::filesystem::path& directory,
                                       std::uint32_t maximum, std::uint32_t expected_type)
{
    auto source = create_source(index_boundary_geometry(maximum));
    if (!source)
        return false;
    ModelWriteOptions options;
    const auto stem = "index-boundary-" + std::to_string(maximum);
    const auto text_path = directory / (stem + ".gltf");
    if (const auto saved = save_document(text_path.string(), source.value().view(), options);
        !saved) {
        std::fprintf(stderr, "Boundary Save: %s\n", saved.error().message());
        return false;
    }
    std::ifstream file{text_path};
    const std::string text{std::istreambuf_iterator<char>{file}, {}};
    file.close();
    if (text.find("\"componentType\": " + std::to_string(expected_type)) == text.npos) {
        std::fprintf(stderr, "Boundary type %u not found\n", expected_type);
        return false;
    }
    for (auto extension : {".glb", ".gltf"}) {
        const auto path = directory / (stem + extension);
        if (const auto saved = save_document(path.string(), source.value().view(), options);
            !saved) {
            std::fprintf(stderr, "Boundary second Save: %s\n", saved.error().message());
            return false;
        }
        auto loaded = load_document(path.string());
        if (!loaded || !geometry_matches(source.value().view(), loaded.value().document.view())) {
            std::fprintf(stderr, "Boundary reload/match loaded=%d\n", static_cast<bool>(loaded));
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool index_boundaries(const std::filesystem::path& directory)
{
    for (const auto boundary :
         {std::pair{254U, 5121U}, {255U, 5123U}, {65534U, 5123U}, {65535U, 5125U}}) {
        BinaryBuilder binary;
        if (!binary.open(directory / "index-boundary-binary.glb"))
            return false;
        const std::array indices{0U, 1U, boundary.first};
        const auto encoded = binary.append_indices(indices);
        if (!encoded || encoded.value().component_type != boundary.second ||
            !index_boundary_case(directory, boundary.first, boundary.second)) {
            std::fprintf(stderr, "FAIL index boundary: %u\n", boundary.first);
            return false;
        }
    }
    const std::array reserved{0U, 1U, std::numeric_limits<std::uint32_t>::max()};
    BinaryBuilder binary;
    if (!binary.open(directory / "index-reserved-binary.glb"))
        return false;
    ExportIndexOutput stream{reserved.back()};
    return !binary.append_indices(reserved) && !stream.append(reserved, binary);
}

[[nodiscard]] std::uint32_t read_word(std::ifstream& input)
{
    std::uint32_t value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8) {
        const auto byte = input.get();
        if (byte < 0)
            return 0;
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return value;
}
[[nodiscard]] bool check_json(std::ifstream& input, std::string_view json)
{
    std::array<char, container_chunk> bytes;
    for (std::size_t offset = 0; offset < json.size();) {
        const auto count = std::min(bytes.size(), json.size() - offset);
        input.read(bytes.data(), static_cast<std::streamsize>(count));
        if (!input || !std::equal(bytes.begin(), bytes.begin() + count, json.begin() + offset))
            return false;
        offset += count;
    }
    for (std::size_t padding = json.size() % 4; padding != 0 && padding < 4; ++padding) {
        if (input.get() != ' ')
            return false;
    }
    return true;
}
[[nodiscard]] bool check_binary(std::ifstream& input, std::size_t size)
{
    if (size == 0)
        return input.peek() == std::ifstream::traits_type::eof();
    const auto padding = (4 - size % 4) % 4;
    if (read_word(input) != size + padding || read_word(input) != 0x004e4942U)
        return false;
    for (std::size_t i = 0; i < size; ++i) {
        if (input.get() != static_cast<int>((i % container_chunk) % 251))
            return false;
    }
    for (std::size_t i = 0; i < padding; ++i) {
        if (input.get() != 0)
            return false;
    }
    return input.peek() == std::ifstream::traits_type::eof();
}
[[nodiscard]] bool check_container(const std::filesystem::path& path, std::string_view json,
                                   std::size_t binary_size)
{
    std::ifstream input{path, std::ios::binary};
    if (read_word(input) != 0x46546c67U || read_word(input) != 2 ||
        read_word(input) != std::filesystem::file_size(path))
        return false;
    const auto padded_json = json.size() + (4 - json.size() % 4) % 4;
    return read_word(input) == padded_json && read_word(input) == 0x4e4f534aU &&
           check_json(input, json) && check_binary(input, binary_size);
}
[[nodiscard]] bool fill_binary(BinaryBuilder& binary, std::size_t count)
{
    std::array<std::byte, container_chunk> bytes;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::byte>(i % 251);
    for (std::size_t offset = 0; offset < count;) {
        const auto size = std::min(bytes.size(), count - offset);
        if (!binary.append_bytes(std::span{bytes}.first(size)))
            return false;
        offset += size;
    }
    return true;
}
[[nodiscard]] bool drive_container(ExportGlbOutput& output, std::string_view json,
                                   BinaryBuilder& binary)
{
    std::size_t previous = output.written_bytes();
    unsigned steps = 0;
    while (!output.ready()) {
        if (!output.advance(json, binary))
            return false;
        const auto now = output.written_bytes();
        if (now < previous || now - previous > container_chunk + 3)
            return false;
        previous = now;
        ++steps;
    }
    return steps > 1 && !output.advance(json, binary);
}
[[nodiscard]] bool container_case(const std::filesystem::path& directory, std::size_t bytes)
{
    const auto path = directory / ("container-" + std::to_string(bytes) + ".glb");
    BinaryBuilder binary;
    OutputFiles files;
    if (!binary.open(path) || !fill_binary(binary, bytes))
        return false;
    const std::string json = "{\"asset\":{\"version\":\"2.0\"},\"extras\":\"" +
                             std::string(2 * container_chunk + 1, 'x') + "\"}";
    auto output = ExportGlbOutput::create(path, json.size(), binary.size(), files);
    if (!output || !drive_container(output.value(), json, binary) || !files.publish())
        return false;
    const bool correct = check_container(path, json, bytes);
    const auto discarded = binary.discard();
    return correct && discarded.has_value() && files.discard().has_value();
}
[[nodiscard]] bool cancel_after_binary_chunk(ExportGlbOutput& output, BinaryBuilder& binary)
{
    if (output.advance("wrong size", binary))
        return false;
    for (unsigned step = 0; step < 3; ++step) {
        if (!output.advance("{}", binary))
            return false;
    }
    return output.discard().has_value() && !output.ready() && !output.advance("{}", binary);
}
[[nodiscard]] bool cancelled_container(const std::filesystem::path& directory, Document& source)
{
    const auto path = directory / "large_stream.glb";
    BinaryBuilder binary;
    OutputFiles files;
    if (!binary.open(path) || !fill_binary(binary, 3 * container_chunk + 3))
        return false;
    auto output = ExportGlbOutput::create(path, 2, binary.size(), files);
    if (!output || !cancel_after_binary_chunk(output.value(), binary))
        return false;
    if (!files.discard() || !binary.discard())
        return false;
    const auto loaded = load_document(path.string());
    return loaded && geometry_matches(source.view(), loaded.value().document.view());
}
[[nodiscard]] bool oversized_container(const std::filesystem::path& directory)
{
    OutputFiles files;
    constexpr auto limit = std::numeric_limits<std::uint32_t>::max();
    const auto path = directory / "oversized-container.glb";
    return !ExportGlbOutput::create(path, limit, 0, files) &&
           !ExportGlbOutput::create(path, 2, limit, files) && !std::filesystem::exists(path);
}

[[nodiscard]] bool no_staging_files(const std::filesystem::path& directory)
{
    for (const auto& file : std::filesystem::directory_iterator{directory}) {
        const auto name = file.path().filename().string();
        if (name.find(".elf3d-stage-") != std::string::npos ||
            name.find(".elf3d-binary-") != std::string::npos) {
            return false;
        }
    }
    return true;
}
} // namespace

bool elf3d_export_stream_tests(const std::filesystem::path& directory)
{
    auto source = create_source(large_geometry());
    const bool passed =
        source && save_large(directory, ".glb", source.value()) &&
        save_large(directory, ".gltf", source.value()) &&
        cancelled_container(directory, source.value()) && container_case(directory, 0) &&
        container_case(directory, 3 * container_chunk + 3) && oversized_container(directory) &&
        index_boundaries(directory) && no_staging_files(directory);
    if (!passed)
        std::fprintf(stderr, "FAIL streamed export / container / index boundaries\n");
    return passed;
}
