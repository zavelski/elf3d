#include <elf3d/model_write.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace elf3d;
constexpr std::uint32_t corners = 9003;
[[nodiscard]] Float3 point(std::uint32_t index)
{
    return {static_cast<float>(index / 3) + (index % 3 == 2 ? 1.0F : 0.0F),
            index % 3 == 1 ? 1.0F : 0.0F, 0};
}
[[nodiscard]] Document
fixture(std::optional<ModelImageDescription> image_description = std::nullopt)
{
    Document document;
    const auto scene = document.create_scene("Writer scene").value();
    const auto mesh = document.create_mesh("Writer mesh").value();
    const auto node = document.create_node("Writer node").value();
    std::array<std::byte, 64 * 32 * 4> pixels;
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<std::byte>(i % 251);
    const auto image = document
                           .create_image(image_description.value_or(
                               ModelImageDescription{64, 32, PixelFormat::rgba8_unorm, pixels}))
                           .value();
    const auto sampler = document.create_sampler().value();
    const auto texture = document.create_texture({image, sampler}).value();
    ModelMaterialDescription description;
    description.base_color_texture = texture;
    const auto material = document.create_material(description).value();
    PrimitiveData data;
    data.positions.resize(corners + 1); // One retained unused vertex.
    data.normals.assign(corners + 1, {0, 0, 1});
    data.texcoord0.assign(corners + 1, {0.25F, 0.75F});
    data.indices.resize(corners);
    for (std::uint32_t i = 0; i <= corners; ++i)
        data.positions[i] = point(i);
    for (std::uint32_t i = 0; i < corners; ++i)
        data.indices[i] = i;
    if (!document.create_primitive(mesh, material, std::move(data)) ||
        !document.set_node_mesh(node, mesh) || !document.add_scene_root(scene, node))
        return Document{};
    return document;
}
[[nodiscard]] bool matches_pixels(DocumentView document)
{
    const auto image = document.image_at(0).value();
    if (image.width != 64 || image.height != 32 || image.pixels.size() != 64 * 32 * 4)
        return false;
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        if (image.pixels[i] != static_cast<std::byte>(i % 251))
            return false;
    }
    return true;
}
[[nodiscard]] bool matches_fixture(DocumentView document)
{
    const auto primitive = document.primitive_at(0).value();
    if (primitive.data.positions.size() != corners + 1 || primitive.data.indices.size() != corners)
        return false;
    for (std::uint32_t i = 0; i < corners; ++i) {
        if (primitive.data.positions[i] != point(i) || primitive.data.indices[i] != i)
            return false;
    }
    return primitive.data.positions.back() == point(corners) && matches_pixels(document);
}
[[nodiscard]] bool step(ModelFileWriter& writer, const ModelWriteSource& source,
                        DocumentView document,
                        std::size_t packet_capacity = model_write_packet_capacity)
{
    if (const auto request = writer.input_request()) {
        std::vector<std::byte> packet(packet_capacity);
        const auto read = source.read(document, *request, packet);
        if (!read) {
            std::fprintf(stderr, "writer source read: %s\n", read.error().message());
            return false;
        }
        const auto provided = writer.provide(std::span{packet}.first(read.value()));
        if (!provided)
            std::fprintf(stderr, "writer provide: %s\n", provided.error().message());
        return provided.has_value();
    }
    return writer.advance().has_value();
}
[[nodiscard]] bool successful(ModelFileWriter& writer)
{
    auto outcome = writer.take_outcome();
    if (!outcome || !outcome.value().committed || outcome.value().cancelled ||
        outcome.value().error || outcome.value().cleanup_error)
        return false;
    return !writer.take_outcome() && !writer.advance() && writer.discard().has_value();
}
[[nodiscard]] bool reload_fixture(const std::filesystem::path& path)
{
    const auto loaded = load_document(path.string());
    return loaded && matches_fixture(loaded.value().document.view());
}
[[nodiscard]] bool write_without_source_during_staging(const std::filesystem::path& path)
{
    auto document = fixture();
    const auto source = ModelWriteSource::create(document.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source).value();
    bool disposed = false;
    unsigned requests = 0;
    for (unsigned steps = 0; steps < 10000 && !writer.ready(); ++steps) {
        if (writer.progress().phase == ModelWritePhase::json) {
            document = Document{};
            disposed = true;
        }
        requests += writer.input_request().has_value() ? 1U : 0U;
        if (!step(writer, source, document.view()))
            return false;
    }
    if (!disposed || requests < 5 || !writer.ready())
        return false;
    return successful(writer) && reload_fixture(path);
}
[[nodiscard]] bool cancelled(ModelFileWriter& writer)
{
    if (!writer.advance({.cancel_requested = true}) || !writer.ready())
        return false;
    const auto outcome = writer.take_outcome();
    return outcome && !outcome.value().committed && outcome.value().cancelled &&
           !outcome.value().error && !outcome.value().cleanup_error;
}
[[nodiscard]] bool cancel_before_commit(const std::filesystem::path& path, bool during_staging)
{
    auto document = fixture();
    const auto source = ModelWriteSource::create(document.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source).value();
    if (during_staging) {
        for (unsigned steps = 0;
             steps < 10000 && writer.progress().phase != ModelWritePhase::staging; ++steps) {
            if (!step(writer, source, document.view()))
                return false;
        }
        if (!writer.advance())
            return false;
    }
    if (!cancelled(writer))
        return false;
    const auto loaded = load_document(path.string());
    return loaded && matches_fixture(loaded.value().document.view());
}
[[nodiscard]] bool late_cancel(const std::filesystem::path& path)
{
    auto document = fixture();
    const auto source = ModelWriteSource::create(document.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source).value();
    for (unsigned steps = 0; steps < 10000 && writer.progress().phase != ModelWritePhase::commit;
         ++steps) {
        if (!step(writer, source, document.view()))
            return false;
    }
    return writer.advance().has_value() && writer.advance({.cancel_requested = true}).has_value() &&
           writer.ready() && successful(writer);
}
[[nodiscard]] bool invalid_packets(const std::filesystem::path& path)
{
    auto document = fixture();
    const auto source = ModelWriteSource::create(document.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source).value();
    const auto request = *writer.input_request();
    std::array<std::byte, model_write_packet_capacity> bytes;
    if (source.read(document.view(), request, std::span{bytes}.first(16)))
        return false;
    const auto read = source.read(document.view(), request, bytes);
    if (!read || writer.provide(std::span{bytes}.first(16)))
        return false;
    if (!writer.provide(std::span{bytes}.first(read.value())) ||
        writer.provide(std::span{bytes}.first(read.value())))
        return false;
    return cancelled(writer);
}
[[nodiscard]] bool failed_input(ModelFileWriter& writer, Error error)
{
    if (!writer.reject_input(error) || !writer.advance() || !writer.ready())
        return false;
    const auto outcome = writer.take_outcome();
    return outcome && outcome.value().error && !outcome.value().committed &&
           !outcome.value().cancelled;
}
[[nodiscard]] bool stale_source(const std::filesystem::path& path)
{
    auto document = fixture();
    const auto source = ModelWriteSource::create(document.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source).value();
    const auto request = *writer.input_request();
    const auto node = document.node_at(0).value().id;
    Float4x4 matrix;
    matrix.elements[12] = 1;
    if (!document.set_node_matrix(node, matrix) || source.matches_document(document.view()))
        return false;
    std::array<std::byte, model_write_packet_capacity> bytes;
    bytes.fill(std::byte{0x55});
    const auto read = source.read(document.view(), request, bytes);
    if (read ||
        !std::all_of(bytes.begin(), bytes.end(), [](auto b) { return b == std::byte{0x55}; }))
        return false;
    return failed_input(writer, read.error());
}
[[nodiscard]] bool no_staging(const std::filesystem::path& directory)
{
    for (const auto& file : std::filesystem::directory_iterator{directory}) {
        if (file.path().filename().string().find(".elf3d-") != std::string::npos) {
            std::fprintf(stderr, "FAIL remaining file staging: %s\n",
                         file.path().filename().string().c_str());
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool image_output_matches(const std::filesystem::path& path,
                                        std::span<const std::byte> pixels,
                                        std::span<const std::byte> original_stream,
                                        bool preserve_stream)
{
    const auto reloaded = load_document(path.string());
    if (!reloaded)
        return false;
    const auto actual = reloaded.value().document.view().image_at(0).value();
    if (actual.width != 256 || actual.height != 192 ||
        !std::equal(pixels.begin(), pixels.end(), actual.pixels.begin(), actual.pixels.end()))
        return false;
    return !preserve_stream || std::equal(original_stream.begin(), original_stream.end(),
                                          actual.source_bytes.begin(), actual.source_bytes.end());
}

[[nodiscard]] bool changed_image_pixels(const std::filesystem::path& path,
                                        ModelWriteOptions options, std::size_t changed_byte)
{
    constexpr std::uint32_t width = 256, height = 192;
    std::vector<std::byte> pixels(width * height * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<std::byte>(i % 251);
    auto original = fixture(ModelImageDescription{width, height, PixelFormat::rgba8_unorm, pixels});
    auto seed = path;
    seed += ".source.glb";
    if (!save_document(seed.string(), original.view()))
        return false;
    original = Document{};
    auto loaded = load_document(seed.string());
    if (!loaded)
        return false;
    std::vector<std::byte> original_stream;
    Document changed;
    {
        const auto image = loaded.value().document.view().image_at(0).value();
        original_stream.assign(image.source_bytes.begin(), image.source_bytes.end());
        if (changed_byte < pixels.size())
            pixels[changed_byte] ^= std::byte{1};
        changed = fixture(ModelImageDescription{width, height, image.format, pixels,
                                                image.source_mime_type, image.source_bytes});
    }
    loaded.value().document = Document{};
    const auto source = ModelWriteSource::create(changed.view()).value();
    auto writer = ModelFileWriter::create(path.string(), source, options).value();
    for (unsigned steps = 0; steps < 10000 && !writer.ready(); ++steps) {
        if (!step(writer, source, changed.view(), model_write_packet_limit))
            return false;
    }
    if (!writer.ready() || !successful(writer))
        return false;
    return image_output_matches(path, pixels, original_stream, changed_byte == pixels.size());
}

[[nodiscard]] bool image_fallback_packets(const std::filesystem::path& directory,
                                          ModelWriteOptions options)
{
    for (const auto* extension : {".glb", ".gltf"}) {
        // Early/middle/last mismatch and an unchanged retained PNG. Three
        // pixel frames fit in one source packet, exercising read-ahead.
        for (const std::size_t offset : {0U, 65540U, 196607U, 196608U}) {
            const auto path = directory / (std::string{"image-packets"} + extension);
            if (!changed_image_pixels(path, options, offset)) {
                std::fprintf(stderr, "FAIL image fallback packet %s offset %zu\n", extension,
                             offset);
                return false;
            }
        }
    }
    return true;
}
} // namespace
bool elf3d_image_fallback_packet_tests(const std::filesystem::path& directory,
                                       ModelWriteOptions options)
{
    return image_fallback_packets(directory, options);
}

bool elf3d_writer_operation_tests(const std::filesystem::path& directory)
{
    const auto owned = directory / "writer-operation-cases";
    std::error_code error;
    std::filesystem::create_directories(owned, error);
    if (error)
        return false;
    if (!image_fallback_packets(owned, {}))
        return false;
    for (const auto* extension : {".glb", ".gltf"}) {
        const auto path = owned / (std::string{"writer-operation"} + extension);
        const std::array cases{write_without_source_during_staging(path),
                               cancel_before_commit(path, false),
                               cancel_before_commit(path, true),
                               late_cancel(path),
                               invalid_packets(path),
                               stale_source(path)};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            if (!cases[i]) {
                std::fprintf(stderr, "FAIL explicit file writer operation %s case %zu\n", extension,
                             i);
                return false;
            }
        }
    }
    return no_staging(owned);
}
