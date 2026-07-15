module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>

#include <cgltf.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>

module elf.gltf;

import elf.core;

namespace elf3d::gltf::importer_input {

struct BufferLoadContext {
    std::optional<ErrorCode> error_code;
    std::string diagnostic;
};

std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

namespace {

constexpr std::uintmax_t maximum_buffer_file_size = 1024ULL * 1024ULL * 1024ULL;

[[noreturn]] void fatal_gltf_allocation_failure() noexcept {
    fatal_error("Elf3D glTF importer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_gltf_boundary_exception() noexcept {
    fatal_error("Elf3D glTF importer encountered an unexpected exception");
}

[[nodiscard]] void* cgltf_allocate(const cgltf_memory_options* memory, cgltf_size size) noexcept {
    return memory->alloc_func != nullptr ? memory->alloc_func(memory->user_data, size)
                                         : std::malloc(size);
}

void cgltf_deallocate(const cgltf_memory_options* memory, void* data) noexcept {
    if (memory->free_func != nullptr) {
        memory->free_func(memory->user_data, data);
    } else {
        std::free(data);
    }
}

[[nodiscard]] cgltf_result external_file_size(BufferLoadContext& context,
                                              const std::filesystem::path& path,
                                              std::uintmax_t& size) {
    std::error_code filesystem_error;
    size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        std::error_code existence_error;
        const bool exists = std::filesystem::exists(path, existence_error);
        context.error_code = !existence_error && !exists ? ErrorCode::missing_external_buffer
                                                         : ErrorCode::source_file_read_failed;
        context.diagnostic = "Could not open external glTF buffer: " + path_to_utf8(path);
        return !existence_error && !exists ? cgltf_result_file_not_found : cgltf_result_io_error;
    }
    if (size > maximum_buffer_file_size) {
        context.error_code = ErrorCode::resource_limit_exceeded;
        context.diagnostic =
            "External glTF buffer exceeds the 1 GiB file limit: " + path_to_utf8(path);
        return cgltf_result_io_error;
    }
    return cgltf_result_success;
}

[[nodiscard]] cgltf_result requested_external_size(BufferLoadContext& context,
                                                   const std::filesystem::path& path,
                                                   std::uintmax_t file_size,
                                                   cgltf_size declared_size,
                                                   std::uintmax_t& requested_size) {
    requested_size = declared_size == 0 ? file_size : declared_size;
    if (requested_size <= file_size &&
        requested_size <= static_cast<std::uintmax_t>(std::numeric_limits<cgltf_size>::max())) {
        return cgltf_result_success;
    }
    context.error_code = ErrorCode::invalid_buffer_range;
    context.diagnostic =
        "External glTF buffer is shorter than its declared byte length: " + path_to_utf8(path);
    return cgltf_result_data_too_short;
}

[[nodiscard]] cgltf_result read_external_bytes(const cgltf_memory_options* memory,
                                               BufferLoadContext& context,
                                               const std::filesystem::path& path,
                                               std::uintmax_t requested_size, void** data) {
    void* file_data = cgltf_allocate(memory, static_cast<cgltf_size>(requested_size));
    if (file_data == nullptr && requested_size != 0) {
        return cgltf_result_out_of_memory;
    }
    std::ifstream stream{path, std::ios::binary};
    if (!stream ||
        requested_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        cgltf_deallocate(memory, file_data);
        context.error_code = ErrorCode::source_file_read_failed;
        context.diagnostic = "Could not read external glTF buffer: " + path_to_utf8(path);
        return cgltf_result_io_error;
    }
    if (requested_size != 0) {
        stream.read(static_cast<char*>(file_data), static_cast<std::streamsize>(requested_size));
        if (!stream) {
            cgltf_deallocate(memory, file_data);
            context.error_code = ErrorCode::source_file_read_failed;
            context.diagnostic = "Failed while reading external glTF buffer: " + path_to_utf8(path);
            return cgltf_result_io_error;
        }
    }
    *data = file_data;
    return cgltf_result_success;
}

[[nodiscard]] cgltf_result read_external_file_impl(const cgltf_memory_options* memory,
                                                   BufferLoadContext& context,
                                                   const std::filesystem::path& path,
                                                   cgltf_size& size, void** data) {
    std::uintmax_t file_size = 0;
    if (const cgltf_result result = external_file_size(context, path, file_size);
        result != cgltf_result_success) {
        return result;
    }
    std::uintmax_t requested_size = 0;
    if (const cgltf_result result =
            requested_external_size(context, path, file_size, size, requested_size);
        result != cgltf_result_success) {
        return result;
    }
    const cgltf_result result = read_external_bytes(memory, context, path, requested_size, data);
    if (result == cgltf_result_success) {
        size = static_cast<cgltf_size>(requested_size);
    }
    return result;
}

} // namespace

cgltf_result read_external_file(const cgltf_memory_options* memory,
                                const cgltf_file_options* file_options, const char* path,
                                cgltf_size* size, void** data) {
    auto* context = static_cast<BufferLoadContext*>(file_options->user_data);
    if (context == nullptr || path == nullptr || size == nullptr || data == nullptr) {
        return cgltf_result_invalid_options;
    }
    try {
        return read_external_file_impl(memory, *context, path_from_utf8(path), *size, data);
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

void release_external_file(const cgltf_memory_options* memory, const cgltf_file_options*,
                           void* data) {
    cgltf_deallocate(memory, data);
}

} // namespace elf3d::gltf::importer_input
