#include "exporter_container.hpp"
#include "exporter_internal.hpp"
#include "exporter_output.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>

namespace elf3d::gltf::exporter_detail {
namespace {
[[nodiscard]] Result<void> write_word(std::ofstream& stream, std::uint32_t value)
{
    const std::array bytes{static_cast<std::byte>(value & 0xffU),
                           static_cast<std::byte>((value >> 8U) & 0xffU),
                           static_cast<std::byte>((value >> 16U) & 0xffU),
                           static_cast<std::byte>((value >> 24U) & 0xffU)};
    return exporter_output::write_bytes(stream, bytes);
}

[[nodiscard]] Result<void> write_header(std::ofstream& stream, std::uint32_t total,
                                        std::uint32_t json_size)
{
    const std::array words{0x46546c67U, 2U, total, json_size, 0x4e4f534aU};
    for (const auto value : words) {
        if (const auto written = write_word(stream, value); !written) {
            return written.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> write_padding(std::ofstream& stream, std::size_t count, std::byte fill)
{
    const std::array padding{fill, fill, fill};
    return exporter_output::write_bytes(stream, std::span{padding}.first(count));
}

[[nodiscard]] Result<std::uint32_t> container_size(std::size_t json, std::size_t binary)
{
    constexpr auto limit = std::numeric_limits<std::uint32_t>::max();
    if (json > limit || binary > limit)
        return Error{ErrorCode::size_overflow, "GLB output exceeds the 32-bit container limit"};
    const auto padded_json = static_cast<std::uint64_t>(json) + (4U - json % 4U) % 4U;
    const auto padded_binary = static_cast<std::uint64_t>(binary) + (4U - binary % 4U) % 4U;
    const auto total = 20U + padded_json + (binary == 0 ? 0 : 8U + padded_binary);
    if (total > limit)
        return Error{ErrorCode::size_overflow, "GLB output exceeds the 32-bit container limit"};
    return static_cast<std::uint32_t>(total);
}
} // namespace

Result<ExportGlbOutput> ExportGlbOutput::create(const std::filesystem::path& path,
                                                std::size_t json_size, std::size_t binary_size,
                                                exporter_output::OutputFiles& files)
{
    const auto total = container_size(json_size, binary_size);
    if (!total)
        return total.error();
    const auto staged = files.stage(path);
    if (!staged)
        return staged.error();
    ExportGlbOutput output;
    output.stream_.open(staged.value(), std::ios::binary | std::ios::trunc);
    if (!output.stream_)
        return Error{ErrorCode::source_file_write_failed, "Cannot open staged GLB output"};
    const auto padded_json = json_size + (4U - json_size % 4U) % 4U;
    if (const auto header =
            write_header(output.stream_, total.value(), static_cast<std::uint32_t>(padded_json));
        !header)
        return header.error();
    output.json_size_ = json_size;
    output.binary_size_ = binary_size;
    output.written_ = 20;
    return output;
}

bool ExportGlbOutput::ready() const noexcept
{
    return phase_ == Phase::complete;
}
std::size_t ExportGlbOutput::written_bytes() const noexcept
{
    return written_;
}
Result<void> ExportGlbOutput::append_json(std::string_view json)
{
    constexpr std::size_t chunk_bytes = 64 * 1024;
    const auto count = std::min(chunk_bytes, json_size_ - offset_);
    const auto text = std::as_bytes(std::span{json.data(), json.size()}).subspan(offset_, count);
    if (const auto written = exporter_output::write_bytes(stream_, text); !written)
        return written.error();
    offset_ += count;
    written_ += count;
    if (offset_ != json_size_)
        return {};
    const auto padding = (4U - json_size_ % 4U) % 4U;
    if (const auto padded = write_padding(stream_, padding, std::byte{' '}); !padded)
        return padded.error();
    written_ += padding;
    offset_ = 0;
    phase_ = Phase::binary_header;
    return {};
}
Result<void> ExportGlbOutput::append_binary_header()
{
    if (binary_size_ == 0) {
        phase_ = Phase::finish;
        return {};
    }
    const auto padding = (4U - binary_size_ % 4U) % 4U;
    if (const auto length = write_word(stream_, static_cast<std::uint32_t>(binary_size_ + padding));
        !length)
        return length.error();
    if (const auto type = write_word(stream_, 0x004e4942U); !type)
        return type.error();
    written_ += 8;
    phase_ = Phase::binary;
    return {};
}
Result<void> ExportGlbOutput::append_binary(BinaryBuilder& binary)
{
    std::array<std::byte, 64 * 1024> bytes;
    const auto count = std::min(bytes.size(), binary_size_ - offset_);
    const auto chunk = std::span{bytes}.first(count);
    if (const auto read = binary.read(offset_, chunk); !read)
        return read.error();
    if (const auto written = exporter_output::write_bytes(stream_, chunk); !written)
        return written.error();
    offset_ += count;
    written_ += count;
    if (offset_ != binary_size_)
        return {};
    const auto padding = (4U - binary_size_ % 4U) % 4U;
    if (const auto padded = write_padding(stream_, padding, std::byte{0}); !padded)
        return padded.error();
    written_ += padding;
    phase_ = Phase::finish;
    return {};
}
Result<void> ExportGlbOutput::advance(std::string_view json, BinaryBuilder& binary)
{
    if (!stream_.is_open())
        return Error{ErrorCode::invalid_argument, "GLB staging is not active"};
    if (json.size() != json_size_ || binary.size() != binary_size_)
        return Error{ErrorCode::invalid_argument, "GLB staging input size changed"};
    switch (phase_) {
    case Phase::json:
        return append_json(json);
    case Phase::binary_header:
        return append_binary_header();
    case Phase::binary:
        return append_binary(binary);
    case Phase::finish: {
        auto closed = exporter_output::close_file(stream_);
        phase_ = closed ? Phase::complete : Phase::disposed;
        return closed;
    }
    default:
        return Error{ErrorCode::invalid_argument, "GLB staging is already finished"};
    }
}
Result<void> ExportGlbOutput::discard()
{
    phase_ = Phase::disposed;
    if (!stream_.is_open())
        return {};
    return exporter_output::close_file(stream_);
}

Result<void> stage_glb(const std::filesystem::path& path, std::string_view json,
                       BinaryBuilder& binary, exporter_output::OutputFiles& files)
{
    auto output = ExportGlbOutput::create(path, json.size(), binary.size(), files);
    if (!output)
        return output.error();
    while (!output.value().ready()) {
        if (const auto written = output.value().advance(json, binary); !written) {
            const auto closed = output.value().discard();
            return closed ? Result<void>{written.error()} : Result<void>{closed.error()};
        }
    }
    return {};
}
} // namespace elf3d::gltf::exporter_detail
