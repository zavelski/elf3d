#include "exporter_binary_spool.hpp"
#include "exporter_output.hpp"

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace elf3d::gltf::exporter_detail {
struct ExportBinarySpool::Impl {
    std::filesystem::path path;
    std::ofstream stream;
    std::ifstream input;
    std::vector<std::byte> buffer;
    std::size_t used = 0;
    std::size_t size = 0;
    std::optional<Error> error;
    std::size_t read_offset = 0;
    bool sealed = false;

    [[nodiscard]] bool valid_read(std::size_t offset, std::size_t count) const noexcept
    {
        return count <= buffer.size() && offset <= size && count <= size - offset &&
               offset <= static_cast<std::size_t>(std::numeric_limits<std::streamoff>::max());
    }

    void flush()
    {
        if (error || used == 0) {
            return;
        }
        const auto written = exporter_output::write_bytes(stream, std::span{buffer}.first(used));
        used = 0;
        if (!written) {
            error = written.error();
        }
    }
    [[nodiscard]] Result<void> finish()
    {
        sealed = true;
        flush();
        if (stream.is_open()) {
            const auto closed = exporter_output::close_file(stream);
            if (!closed && !error) {
                error = closed.error();
            }
        }
        return error ? Result<void>{*error} : Result<void>{};
    }
};

ExportBinarySpool::ExportBinarySpool() noexcept = default;
ExportBinarySpool::~ExportBinarySpool() noexcept
{
    (void)discard();
}
ExportBinarySpool::ExportBinarySpool(ExportBinarySpool&&) noexcept = default;
ExportBinarySpool& ExportBinarySpool::operator=(ExportBinarySpool&& other) noexcept
{
    if (this != &other) {
        (void)discard();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

Result<void> ExportBinarySpool::open(const std::filesystem::path& target)
{
    ELF3D_ASSERT(!impl_);
    const auto path = exporter_output::available_sibling_path(target, "binary");
    if (!path) {
        return path.error();
    }
    impl_ = std::make_unique<Impl>();
    impl_->path = path.value();
    impl_->stream.open(impl_->path, std::ios::binary | std::ios::trunc);
    if (!impl_->stream) {
        return Error{ErrorCode::source_file_write_failed, "Cannot open staged binary output"};
    }
    impl_->buffer.resize(exporter_output::transfer_chunk_size);
    return {};
}

void ExportBinarySpool::append_byte(std::byte value)
{
    ELF3D_ASSERT(impl_);
    ELF3D_ASSERT(!impl_->sealed);
    if (impl_->error) {
        return;
    }
    if (impl_->used == impl_->buffer.size()) {
        impl_->flush();
    }
    if (!impl_->error) {
        impl_->buffer[impl_->used++] = value;
        ++impl_->size;
    }
}

void ExportBinarySpool::append(std::span<const std::byte> bytes)
{
    ELF3D_ASSERT(impl_);
    ELF3D_ASSERT(!impl_->sealed);
    while (!bytes.empty() && !impl_->error) {
        const auto count = std::min(bytes.size(), impl_->buffer.size() - impl_->used);
        std::copy_n(bytes.begin(), count, impl_->buffer.begin() + impl_->used);
        impl_->used += count;
        impl_->size += count;
        bytes = bytes.subspan(count);
        if (impl_->used == impl_->buffer.size()) {
            impl_->flush();
        }
    }
}

std::size_t ExportBinarySpool::size() const noexcept
{
    return impl_ ? impl_->size : 0;
}
Result<void> ExportBinarySpool::status() const noexcept
{
    return impl_ && impl_->error ? Result<void>{*impl_->error} : Result<void>{};
}

Result<void> ExportBinarySpool::read(std::size_t offset, std::span<std::byte> destination)
{
    ELF3D_ASSERT(impl_);
    if (!impl_->valid_read(offset, destination.size())) {
        return Error{ErrorCode::invalid_argument, "Staged binary read exceeds its bounded range"};
    }
    if (const auto ready = impl_->finish(); !ready) {
        return ready.error();
    }
    if (!impl_->input.is_open()) {
        impl_->input.open(impl_->path, std::ios::binary);
    }
    if (!impl_->input) {
        return Error{ErrorCode::source_file_write_failed, "Cannot read staged binary output"};
    }
    if (offset != impl_->read_offset) {
        impl_->input.seekg(static_cast<std::streamoff>(offset));
    }
    if (!destination.empty()) {
        impl_->input.read(reinterpret_cast<char*>(destination.data()),
                          static_cast<std::streamsize>(destination.size()));
    }
    if (!impl_->input) {
        return Error{ErrorCode::source_file_write_failed, "Cannot read staged binary block"};
    }
    impl_->read_offset = offset + destination.size();
    return {};
}

Result<void> ExportBinarySpool::copy_to(std::ostream& destination)
{
    ELF3D_ASSERT(impl_);
    if (const auto ready = impl_->finish(); !ready) {
        return ready.error();
    }
    for (std::size_t offset = 0; offset < size();) {
        const auto count = std::min(size() - offset, impl_->buffer.size());
        if (const auto loaded = read(offset, std::span{impl_->buffer}.first(count)); !loaded) {
            return loaded.error();
        }
        destination.write(reinterpret_cast<const char*>(impl_->buffer.data()),
                          static_cast<std::streamsize>(count));
        if (!destination) {
            return Error{ErrorCode::source_file_write_failed, "Cannot assemble staged GLB output"};
        }
        offset += count;
    }
    return {};
}

Result<std::filesystem::path> ExportBinarySpool::take_path()
{
    ELF3D_ASSERT(impl_);
    if (const auto ready = impl_->finish(); !ready) {
        return ready.error();
    }
    if (impl_->input.is_open()) {
        impl_->input.close();
        if (impl_->input.fail()) {
            return Error{ErrorCode::source_file_write_failed, "Cannot close staged binary input"};
        }
    }
    return std::exchange(impl_->path, {});
}

Result<void> ExportBinarySpool::discard() noexcept
{
    try {
        if (!impl_ || impl_->path.empty()) {
            return {};
        }
        if (impl_->stream.is_open()) {
            impl_->stream.close();
        }
        if (impl_->input.is_open()) {
            impl_->input.close();
        }
        std::error_code error;
        std::filesystem::remove(impl_->path, error);
        if (error) {
            return Error{ErrorCode::source_file_write_failed,
                         "Cannot discard staged binary output; temporary file retained"};
        }
        impl_->path.clear();
        return {};
    } catch (const std::bad_alloc&) {
        fatal_error("glTF binary staging cleanup allocation failed");
    } catch (...) {
        fatal_error("Unexpected glTF binary staging cleanup exception");
    }
}
} // namespace elf3d::gltf::exporter_detail
