#include <elf3d/model.h>
#include <gltf/exporter_image_input.h>
#include <gltf/exporter_png_encoder.h>
#include <image/image.h>
#include <model/document_write_source.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace elf3d;
using namespace elf3d::gltf::exporter_detail;
using namespace elf3d::model::detail;
enum class Mode { raw, exact, changed, corrupt, exr };
constexpr std::uint32_t width = 257, height = 73;
constexpr std::size_t pixel_bytes = static_cast<std::size_t>(width) * height * 4;
[[nodiscard]] std::byte sample(std::size_t offset, Mode mode)
{
    // Fixtures are below 2^32 bytes; unsigned hash wrapping is intentional.
    auto value = static_cast<std::uint32_t>(offset) * 2654435761U;
    value ^= value >> 15;
    value *= 2246822519U;
    value ^= value >> 13;
    auto result = static_cast<std::byte>(value & 0xffU);
    if (mode == Mode::changed && offset == 66001)
        result ^= std::byte{1};
    return result;
}
void fill(std::span<std::byte> bytes, std::size_t offset, Mode mode)
{
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = sample(offset + i, mode);
}
[[nodiscard]] Result<std::vector<std::byte>> original_png(const std::filesystem::path& directory)
{
    ExportBinarySpool output;
    if (const auto opened = output.open(directory / "image-candidate.png"); !opened)
        return opened.error();
    auto encoder = ExportPngEncoder::create(width, height, output);
    if (!encoder)
        return encoder.error();
    std::array<std::byte, write_source_chunk_bytes> chunk;
    for (std::size_t offset = 0; offset < pixel_bytes;) {
        const auto count = std::min(chunk.size(), pixel_bytes - offset);
        const auto pixels = std::span{chunk}.first(count);
        fill(pixels, offset, Mode::raw);
        if (const auto accepted = encoder.value().accept(pixels, output); !accepted)
            return accepted.error();
        offset += count;
    }
    if (const auto finished = encoder.value().finish(output); !finished)
        return finished.error();
    std::vector<std::byte> bytes(output.size());
    if (const auto read = output.read(0, bytes); !read)
        return read.error();
    if (const auto discarded = output.discard(); !discarded)
        return discarded.error();
    return bytes;
}
struct Fixture {
    Document document;
    ExportData metadata;
    WriteSourceRecord source;
    std::vector<std::byte> bytes;
    Mode mode = Mode::raw;
};
[[nodiscard]] Result<void> source_bytes(Fixture& fixture, const std::filesystem::path& directory)
{
    if (fixture.mode == Mode::raw)
        return {};
    if (fixture.mode == Mode::exr) {
        fixture.bytes.resize(81931, std::byte{7});
        constexpr std::array prefix{std::byte{0x76}, std::byte{0x2f}, std::byte{0x31},
                                    std::byte{0x01}, std::byte{2},    std::byte{0},
                                    std::byte{0},    std::byte{0}};
        std::copy(prefix.begin(), prefix.end(), fixture.bytes.begin());
        return {};
    }
    auto encoded = original_png(directory);
    if (!encoded)
        return encoded.error();
    fixture.bytes = std::move(encoded).value();
    if (fixture.mode == Mode::corrupt)
        fixture.bytes.resize(16);
    return {};
}
[[nodiscard]] Result<Fixture> make_fixture(const std::filesystem::path& directory, Mode mode)
{
    Fixture result;
    result.mode = mode;
    if (const auto made = source_bytes(result, directory); !made)
        return made.error();
    std::vector<std::byte> pixels;
    ModelImageDescription description;
    if (mode == Mode::exr)
        description.source_mime_type = ModelImageMimeType::exr;
    else {
        description.width = width;
        description.height = height;
        pixels.resize(pixel_bytes);
        fill(pixels, 0, mode);
        description.pixels = pixels;
        description.source_mime_type =
            mode == Mode::raw ? ModelImageMimeType::none : ModelImageMimeType::png;
    }
    description.source_bytes = result.bytes;
    if (const auto image = result.document.create_image(description); !image)
        return image.error();
    auto metadata = ExportSourceMetadata::capture(result.document.view());
    if (!metadata)
        return metadata.error();
    result.metadata.metadata_owner = std::move(metadata).value();
    if (const auto bound = result.metadata.metadata_owner->bind(result.metadata); !bound)
        return bound.error();
    result.source = result.metadata.metadata_owner->image_source(0).value();
    return result;
}
struct Passes {
    std::size_t source = 0, pixels = 0, decode = 0;
};
[[nodiscard]] bool same_passes(Passes a, Passes b)
{
    return a.source == b.source && a.pixels == b.pixels && a.decode == b.decode;
}
[[nodiscard]] Passes expected_passes(Mode mode)
{
    switch (mode) {
    case Mode::raw:
        return {0, 1, 0};
    case Mode::exact:
        return {2, 1, 1};
    case Mode::changed:
        return {1, 2, 1};
    case Mode::corrupt:
        return {1, 1, 1};
    default:
        return {1, 0, 0};
    }
}
[[nodiscard]] Result<Passes> feed(ExportImageInput& input, const Fixture& fixture)
{
    std::array<std::byte, write_source_chunk_bytes> bytes;
    Passes passes;
    while (!input.ready()) {
        const auto request = input.request();
        if (!request) {
            if (const auto advanced = input.advance(); !advanced)
                return advanced.error();
            ++passes.decode;
            continue;
        }
        const bool source = request->data == WriteSourceData::image_source;
        const auto total = source ? fixture.bytes.size() : pixel_bytes;
        const auto count =
            std::min(total - request->offset, request->offset < 12 ? 3U : bytes.size());
        const auto chunk = std::span{bytes}.first(count);
        if (request->offset == 0) {
            if (source)
                ++passes.source;
            else
                ++passes.pixels;
        }
        if (source)
            std::copy_n(fixture.bytes.begin() + request->offset, count, chunk.begin());
        else
            fill(chunk, request->offset, fixture.mode);
        if (const auto accepted = input.accept(*request, {count, count, total}, chunk); !accepted)
            return accepted.error();
    }
    return passes;
}
[[nodiscard]] bool inspect_memory(ExportImageMemory memory, Mode mode)
{
    if (memory.encoded_candidate != 0 || memory.decoded_candidate != 0)
        return false;
    if (mode == Mode::raw || mode == Mode::exr)
        return memory.peak_candidates == 0;
    return memory.peak_candidates != 0 &&
           memory.peak_candidates <= image::maximum_encoded_bytes + image::maximum_decoded_bytes;
}
[[nodiscard]] bool inspect_pixels(std::span<const std::byte> encoded, Mode mode)
{
    const auto decoded = image::decode_image(encoded);
    if (!decoded || decoded.value().width != width || decoded.value().height != height ||
        decoded.value().pixels.size() != pixel_bytes)
        return false;
    for (std::size_t i = 0; i < pixel_bytes; ++i) {
        if (decoded.value().pixels[i] != sample(i, mode))
            return false;
    }
    return true;
}
[[nodiscard]] bool inspect_output(EncodedImageOutput& output, const Fixture& fixture)
{
    std::vector<std::byte> bytes(output.data.size());
    if (!output.data.read(0, bytes))
        return false;
    const bool preserve = fixture.mode == Mode::exact || fixture.mode == Mode::exr;
    if (output.reencoded == preserve)
        return false;
    if (preserve)
        return bytes == fixture.bytes && output.mime_type == fixture.source.mime;
    return output.mime_type == ModelImageMimeType::png && inspect_pixels(bytes, fixture.mode);
}
[[nodiscard]] bool image_case(const std::filesystem::path& directory, Mode mode)
{
    auto fixture = make_fixture(directory, mode);
    if (!fixture)
        return false;
    auto input = ExportImageInput::create(fixture.value().source, fixture.value().metadata,
                                          directory / "owned-image.glb");
    if (!input)
        return false;
    fixture.value().document = Document{};
    const auto passes = feed(input.value(), fixture.value());
    if (!passes || !same_passes(passes.value(), expected_passes(mode)) ||
        !inspect_memory(input.value().memory(), mode))
        return false;
    auto output = input.value().take_output();
    if (!output)
        return false;
    const bool correct = inspect_output(output.value(), fixture.value());
    const auto discarded = output.value().data.discard();
    return correct && discarded;
}
[[nodiscard]] bool rejected_protocol(const std::filesystem::path& directory)
{
    auto fixture = make_fixture(directory, Mode::raw);
    if (!fixture)
        return false;
    auto source = fixture.value().source;
    source.elements[0]++;
    if (ExportImageInput::create(source, fixture.value().metadata, directory / "invalid-image.glb"))
        return false;
    auto input = ExportImageInput::create(fixture.value().source, fixture.value().metadata,
                                          directory / "invalid-image.glb");
    if (!input || input.value().take_output() || input.value().advance())
        return false;
    auto request = *input.value().request();
    request.offset++;
    constexpr std::array<std::byte, 4> bytes{};
    const bool rejected = !input.value().accept(request, {4, 4, pixel_bytes}, bytes);
    const auto discarded = input.value().discard();
    return rejected && discarded && !input.value().request();
}
[[nodiscard]] bool discard_candidate(const std::filesystem::path& directory)
{
    auto fixture = make_fixture(directory, Mode::exact);
    if (!fixture)
        return false;
    auto input = ExportImageInput::create(fixture.value().source, fixture.value().metadata,
                                          directory / "cancel-image.glb");
    if (!input)
        return false;
    const auto request = *input.value().request();
    if (!input.value().accept(request, {3, 3, fixture.value().bytes.size()},
                              std::span{fixture.value().bytes}.first(3)))
        return false;
    if (input.value().memory().encoded_candidate == 0)
        return false;
    const auto discarded = input.value().discard();
    return discarded && !input.value().ready() && !input.value().request() &&
           input.value().memory().encoded_candidate == 0;
}
} // namespace
bool elf3d_image_input_tests(const std::filesystem::path& directory)
{
    constexpr std::array cases{Mode::raw, Mode::exact, Mode::changed, Mode::corrupt, Mode::exr};
    for (const auto mode : cases) {
        if (!image_case(directory, mode)) {
            std::fprintf(stderr, "FAIL owned image input: case %u\n", static_cast<unsigned>(mode));
            return false;
        }
    }
    return rejected_protocol(directory) && discard_candidate(directory);
}
