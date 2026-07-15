module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.image;
import elf.model;

namespace elf3d::gltf::importer_detail {

constexpr std::uintmax_t maximum_source_file_size = 512ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t maximum_buffer_file_size = 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maximum_total_buffer_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t maximum_cgltf_allocation_bytes =
    static_cast<std::size_t>(2ULL * 1024ULL * 1024ULL * 1024ULL);
constexpr cgltf_size maximum_node_count = 131072;
constexpr std::size_t maximum_node_hierarchy_depth = 1024;
constexpr cgltf_size maximum_mesh_count = 65536;
constexpr cgltf_size maximum_primitive_count = 262144;
constexpr cgltf_size maximum_accessor_count = 262144;
constexpr std::uint64_t maximum_total_vertices = 50000000;
constexpr std::uint64_t maximum_total_indices = 150000000;
[[noreturn]] void fatal_gltf_allocation_failure() noexcept {
    fatal_error("Elf3D glTF importer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_gltf_boundary_exception() noexcept {
    fatal_error("Elf3D glTF importer encountered an unexpected exception");
}

void add_diagnostic(std::vector<ModelLoadDiagnostic>& diagnostics,
                    ModelLoadDiagnosticCategory category, ModelLoadDiagnosticCode code,
                    std::string message, std::optional<std::string> source_context) {
    diagnostics.push_back(ModelLoadDiagnostic{ModelLoadDiagnosticSeverity::warning, category, code,
                                              std::move(message), std::move(source_context)});
}

[[nodiscard]] bool supported_required_extension(std::string_view extension) noexcept {
    return extension == "KHR_texture_transform" || extension == "KHR_materials_unlit" ||
           extension == "KHR_materials_emissive_strength" || extension == "KHR_materials_ior" ||
           extension == "KHR_mesh_quantization";
}

[[nodiscard]] bool extension_has_full_support(std::string_view extension) noexcept {
    return supported_required_extension(extension) || extension == "KHR_materials_specular";
}

void add_optional_extension_diagnostic(std::vector<ModelLoadDiagnostic>& diagnostics,
                                       std::string_view extension) {
    ModelLoadDiagnosticCategory category = ModelLoadDiagnosticCategory::extension;
    ModelLoadDiagnosticCode code = ModelLoadDiagnosticCode::unsupported_optional_extension;
    std::string behavior = "is unsupported and was ignored";
    if (extension == "KHR_lights_punctual") {
        category = ModelLoadDiagnosticCategory::light;
        code = ModelLoadDiagnosticCode::ignored_lights;
        behavior = "was parsed by cgltf, but Elf3D has no scene-light model; lights were ignored";
    } else if (extension == "EXT_mesh_gpu_instancing") {
        category = ModelLoadDiagnosticCategory::geometry;
        code = ModelLoadDiagnosticCode::ignored_instancing;
        behavior = "is unsupported; ordinary node/mesh data was loaded without GPU instances";
    } else if (extension == "KHR_materials_specular") {
        category = ModelLoadDiagnosticCategory::material;
        code = ModelLoadDiagnosticCode::material_fallback;
        behavior = "factors are rendered, but extension textures use the documented fallback";
    } else if (extension == "KHR_materials_variants") {
        category = ModelLoadDiagnosticCategory::material;
        behavior = "is unsupported; each primitive's default material was used";
    } else if (extension.starts_with("KHR_materials_")) {
        category = ModelLoadDiagnosticCategory::material;
        code = ModelLoadDiagnosticCode::material_fallback;
        behavior = "is unsupported; the core metallic-roughness material fallback was used";
    } else if (extension == "KHR_draco_mesh_compression" ||
               extension == "EXT_meshopt_compression") {
        category = ModelLoadDiagnosticCategory::geometry;
        behavior = "is unsupported; import succeeds only where ordinary fallback geometry exists";
    } else if (extension == "KHR_texture_basisu" || extension == "EXT_texture_webp") {
        category = ModelLoadDiagnosticCategory::texture;
        behavior = "is unsupported; ordinary PNG/JPEG fallback images are used when present";
    }
    add_diagnostic(diagnostics, category, code,
                   "Optional extension " + std::string{extension} + " " + behavior,
                   std::string{extension});
}

[[nodiscard]] bool supported_primitive_type(cgltf_primitive_type type) noexcept {
    return type == cgltf_primitive_type_triangles || type == cgltf_primitive_type_triangle_strip ||
           type == cgltf_primitive_type_triangle_fan;
}

[[nodiscard]] bool texture_failure_can_fallback(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::unsupported_image_mime_type:
    case ErrorCode::unsupported_image_extension:
    case ErrorCode::image_decode_failed:
    case ErrorCode::zero_image_dimensions:
    case ErrorCode::excessive_image_dimensions:
    case ErrorCode::decoded_image_size_overflow:
    case ErrorCode::image_resource_limit_exceeded:
        return true;
    default:
        return false;
    }
}

struct alignas(std::max_align_t) AllocationHeader {
    std::size_t size = 0;
};

[[nodiscard]] void* bounded_allocate(void* user, cgltf_size size) noexcept {
    auto* context = static_cast<AllocationContext*>(user);
    if (context == nullptr || size > maximum_cgltf_allocation_bytes ||
        context->live_bytes > maximum_cgltf_allocation_bytes - size ||
        size > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)) {
        return nullptr;
    }
    void* allocation = std::malloc(sizeof(AllocationHeader) + size);
    if (allocation == nullptr) {
        return nullptr;
    }
    auto* header = static_cast<AllocationHeader*>(allocation);
    header->size = size;
    context->live_bytes += size;
    return header + 1;
}

void bounded_deallocate(void* user, void* data) noexcept {
    if (data == nullptr) {
        return;
    }
    auto* context = static_cast<AllocationContext*>(user);
    auto* header = static_cast<AllocationHeader*>(data) - 1;
    if (context != nullptr && header->size <= context->live_bytes) {
        context->live_bytes -= header->size;
    }
    std::free(header);
}

struct FileReadMessages {
    ErrorCode error_code;
    std::string_view open_failure;
    std::string_view read_failure;
};

[[nodiscard]] Result<std::vector<std::byte>> read_file_bytes(const std::filesystem::path& path,
                                                             std::uintmax_t size,
                                                             const FileReadMessages& messages) {
    try {
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        std::ifstream stream{path, std::ios::binary};
        if (!stream) {
            return Error{messages.error_code, messages.open_failure};
        }
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            return Error{messages.error_code, messages.read_failure};
        }
        return bytes;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::uintmax_t> source_file_size(const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        std::error_code existence_error;
        const bool exists = std::filesystem::exists(path, existence_error);
        return Error{!existence_error && !exists ? ErrorCode::source_file_not_found
                                                 : ErrorCode::source_file_read_failed,
                     !existence_error && !exists
                         ? "The glTF source file does not exist"
                         : "The glTF source file metadata could not be read"};
    }
    if (file_size == 0) {
        return Error{ErrorCode::source_file_read_failed, "The glTF source file is empty"};
    }
    if (file_size > maximum_source_file_size ||
        file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        file_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The glTF source file exceeds the 512 MiB source limit"};
    }
    return file_size;
}

[[nodiscard]] Result<std::vector<std::byte>> read_source(const std::filesystem::path& path) {
    const Result<std::uintmax_t> size = source_file_size(path);
    if (!size) {
        return size.error();
    }
    constexpr FileReadMessages messages{ErrorCode::source_file_read_failed,
                                        "The glTF source file could not be opened for reading",
                                        "The glTF source file could not be read completely"};
    return read_file_bytes(path, size.value(), messages);
}

[[nodiscard]] std::string lower_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

[[nodiscard]] ErrorCode parse_error_code(cgltf_result result, ErrorCode format_error) noexcept {
    switch (result) {
    case cgltf_result_data_too_short:
    case cgltf_result_invalid_json:
    case cgltf_result_unknown_format:
    case cgltf_result_legacy_gltf:
    case cgltf_result_invalid_gltf:
        return format_error;
    case cgltf_result_out_of_memory:
        return ErrorCode::resource_limit_exceeded;
    default:
        return ErrorCode::scene_import_failed;
    }
}

[[nodiscard]] std::string_view parse_error_message(cgltf_result result, bool is_glb) noexcept {
    switch (result) {
    case cgltf_result_data_too_short:
        return is_glb ? "The GLB file is truncated" : "The glTF source is truncated";
    case cgltf_result_invalid_json:
        return is_glb ? "The GLB JSON chunk is malformed" : "The glTF JSON is malformed";
    case cgltf_result_unknown_format:
    case cgltf_result_legacy_gltf:
        return "The source is not supported glTF 2.0 content";
    case cgltf_result_invalid_gltf:
        return "The source contains invalid glTF data";
    case cgltf_result_out_of_memory:
        return "cgltf could not allocate memory while parsing the source";
    default:
        return "cgltf could not parse the source";
    }
}

[[nodiscard]] Error parse_error(cgltf_result result, bool is_glb) {
    const ErrorCode format_error = is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf;
    return Error{parse_error_code(result, format_error), parse_error_message(result, is_glb)};
}

[[nodiscard]] bool checked_add(std::uint64_t& total, std::uint64_t value,
                               std::uint64_t maximum) noexcept {
    if (value > maximum || total > maximum - value) {
        return false;
    }
    total += value;
    return true;
}

[[nodiscard]] std::optional<std::uint64_t>
expanded_triangle_index_count(const cgltf_primitive& primitive,
                              std::uint64_t source_index_count) noexcept {
    if (primitive.type == cgltf_primitive_type_triangle_strip ||
        primitive.type == cgltf_primitive_type_triangle_fan) {
        if (source_index_count < 3) {
            return source_index_count;
        }
        if (source_index_count - 2 > std::numeric_limits<std::uint64_t>::max() / 3ULL) {
            return std::nullopt;
        }
        return (source_index_count - 2) * 3ULL;
    }
    return source_index_count;
}

[[nodiscard]] Result<std::size_t> trace_node_path(const cgltf_data& data, std::size_t start,
                                                  std::vector<std::uint8_t>& states,
                                                  const std::vector<std::size_t>& depths,
                                                  std::vector<std::size_t>& path) {
    std::size_t current = start;
    while (states[current] == 0U) {
        states[current] = 1U;
        path.push_back(current);
        const cgltf_node* parent = data.nodes[current].parent;
        if (parent == nullptr) {
            return 0U;
        }
        if (parent < data.nodes || parent >= data.nodes + data.nodes_count) {
            return Error{ErrorCode::invalid_node_hierarchy,
                         "A glTF node parent is outside the node table"};
        }
        current = static_cast<std::size_t>(parent - data.nodes);
        if (states[current] == 1U) {
            return Error{ErrorCode::invalid_node_hierarchy,
                         "The glTF node hierarchy contains a cycle"};
        }
        if (states[current] == 2U) {
            return depths[current];
        }
    }
    return 0U;
}

[[nodiscard]] Result<void> finish_node_path(std::vector<std::size_t>& path,
                                            std::vector<std::uint8_t>& states,
                                            std::vector<std::size_t>& depths,
                                            std::size_t parent_depth) {
    while (!path.empty()) {
        const std::size_t index = path.back();
        path.pop_back();
        if (++parent_depth > maximum_node_hierarchy_depth) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "The glTF node hierarchy exceeds the depth limit of 1024"};
        }
        depths[index] = parent_depth;
        states[index] = 2U;
    }
    return {};
}

[[nodiscard]] Result<void> validate_node_hierarchy(const cgltf_data& data) {
    std::vector<std::uint8_t> states(data.nodes_count, 0U);
    std::vector<std::size_t> depths(data.nodes_count, 0U);
    std::vector<std::size_t> path;
    path.reserve(data.nodes_count);
    for (std::size_t start = 0; start < data.nodes_count; ++start) {
        if (states[start] == 2U) {
            continue;
        }
        path.clear();
        const Result<std::size_t> parent_depth = trace_node_path(data, start, states, depths, path);
        if (!parent_depth) {
            return parent_depth.error();
        }
        if (const Result<void> finished =
                finish_node_path(path, states, depths, parent_depth.value());
            !finished) {
            return finished.error();
        }
    }
    return {};
}

struct ResourceTotals {
    cgltf_size primitive_count = 0;
    std::uint64_t buffer_bytes = 0;
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
};

[[nodiscard]] Result<void> validate_buffer_limits(const cgltf_data& data, ResourceTotals& totals) {
    for (cgltf_size index = 0; index < data.buffers_count; ++index) {
        if (data.buffers[index].size > maximum_buffer_file_size ||
            !checked_add(totals.buffer_bytes, static_cast<std::uint64_t>(data.buffers[index].size),
                         maximum_total_buffer_bytes)) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "Declared glTF buffers exceed the importer byte limits"};
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_primitive_limits(const cgltf_primitive& primitive,
                                                     ResourceTotals& totals) {
    const cgltf_accessor* positions =
        cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    if (positions != nullptr &&
        !checked_add(totals.vertices, static_cast<std::uint64_t>(positions->count),
                     maximum_total_vertices)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The glTF vertex count exceeds the importer limit"};
    }
    const std::uint64_t source_index_count =
        primitive.indices != nullptr ? static_cast<std::uint64_t>(primitive.indices->count)
        : positions != nullptr       ? static_cast<std::uint64_t>(positions->count)
                                     : 0;
    const std::optional<std::uint64_t> index_count =
        expanded_triangle_index_count(primitive, source_index_count);
    if (!index_count.has_value() ||
        !checked_add(totals.indices, *index_count, maximum_total_indices)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The glTF expanded triangle index count exceeds the importer limit"};
    }
    return {};
}

[[nodiscard]] Result<void> validate_mesh_limits(const cgltf_data& data, ResourceTotals& totals) {
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        if (mesh.primitives_count > maximum_primitive_count - totals.primitive_count) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "The glTF primitive count exceeds the importer limit"};
        }
        totals.primitive_count += mesh.primitives_count;
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const Result<void> valid =
                validate_primitive_limits(mesh.primitives[primitive_index], totals);
            if (!valid) {
                return valid.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_resource_limits(const cgltf_data& data) {
    if (data.nodes_count > maximum_node_count || data.meshes_count > maximum_mesh_count ||
        data.accessors_count > maximum_accessor_count) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "glTF object counts exceed the importer resource limits"};
    }
    ResourceTotals totals;
    if (const Result<void> buffers = validate_buffer_limits(data, totals); !buffers) {
        return buffers.error();
    }
    return validate_mesh_limits(data, totals);
}

[[nodiscard]] Result<void> validate_required_extensions(const cgltf_data& data) {
    for (cgltf_size index = 0; index < data.extensions_required_count; ++index) {
        const char* extension = data.extensions_required[index];
        const std::string_view name = extension != nullptr ? extension : "<unnamed>";
        if (supported_required_extension(name)) {
            continue;
        }
        if (name == "KHR_materials_specular") {
            bool uses_specular_textures = false;
            for (cgltf_size material_index = 0; material_index < data.materials_count;
                 ++material_index) {
                const cgltf_material& material = data.materials[material_index];
                uses_specular_textures =
                    uses_specular_textures ||
                    (material.has_specular &&
                     (material.specular.specular_texture.texture != nullptr ||
                      material.specular.specular_color_texture.texture != nullptr));
            }
            if (!uses_specular_textures) {
                continue;
            }
        }
        return Error{ErrorCode::unsupported_required_extension,
                     "Unsupported required glTF extension: " + std::string{name}};
    }
    return {};
}

[[nodiscard]] Result<void> validate_buffer_uris(const cgltf_data& data) {
    for (cgltf_size index = 0; index < data.buffers_count; ++index) {
        const char* uri = data.buffers[index].uri;
        if (uri == nullptr || std::string_view{uri}.starts_with("data:")) {
            continue;
        }
        if (std::string_view{uri}.find("://") != std::string_view::npos) {
            const std::string message =
                "Remote glTF buffer URIs are unsupported: " + std::string{uri};
            return Error{ErrorCode::unsupported_remote_uri, message};
        }
    }
    return {};
}

[[nodiscard]] Result<ModelImageMimeType> mime_from_text(std::string_view mime) {
    if (mime == "image/png") {
        return ModelImageMimeType::png;
    }
    if (mime == "image/jpeg" || mime == "image/jpg") {
        return ModelImageMimeType::jpeg;
    }
    return Error{ErrorCode::unsupported_image_mime_type,
                 "Supported glTF image MIME types are image/png and image/jpeg"};
}

void decode_image_json_strings(cgltf_data& data) noexcept {
    for (cgltf_size index = 0; index < data.images_count; ++index) {
        cgltf_image& source = data.images[index];
        if (source.mime_type != nullptr) {
            cgltf_decode_string(source.mime_type);
        }
        if (source.uri != nullptr) {
            cgltf_decode_string(source.uri);
        }
    }
}

[[nodiscard]] Result<ModelImageMimeType> mime_from_extension(const std::filesystem::path& path) {
    const std::string extension = lower_extension(path);
    if (extension == ".png") {
        return ModelImageMimeType::png;
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return ModelImageMimeType::jpeg;
    }
    return Error{ErrorCode::unsupported_image_extension,
                 "Supported external image extensions are .png, .jpg, and .jpeg"};
}

[[nodiscard]] bool encoded_matches(ModelImageMimeType mime,
                                   std::span<const std::byte> bytes) noexcept {
    if (mime == ModelImageMimeType::png) {
        constexpr std::uint8_t signature[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
        if (bytes.size() < std::size(signature)) {
            return false;
        }
        for (std::size_t index = 0; index < std::size(signature); ++index) {
            if (std::to_integer<std::uint8_t>(bytes[index]) != signature[index]) {
                return false;
            }
        }
        return true;
    }
    return bytes.size() >= 3 && std::to_integer<std::uint8_t>(bytes[0]) == 0xffU &&
           std::to_integer<std::uint8_t>(bytes[1]) == 0xd8U &&
           std::to_integer<std::uint8_t>(bytes[2]) == 0xffU;
}

[[nodiscard]] Result<std::uintmax_t> image_file_size(const std::filesystem::path& path) {
    std::error_code filesystem_error;
    const std::uintmax_t size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        std::error_code existence_error;
        const bool exists = std::filesystem::exists(path, existence_error);
        return Error{!existence_error && !exists ? ErrorCode::missing_external_image
                                                 : ErrorCode::external_image_read_failed,
                     !existence_error && !exists ? "A referenced external image does not exist"
                                                 : "External image metadata could not be read"};
    }
    if (size == 0 || size > image::maximum_encoded_bytes ||
        size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "External image is empty or exceeds the 64 MiB encoded-byte limit"};
    }
    return size;
}

[[nodiscard]] Result<std::vector<std::byte>> read_image_file(const std::filesystem::path& path) {
    const Result<std::uintmax_t> size = image_file_size(path);
    if (!size) {
        return size.error();
    }
    constexpr FileReadMessages messages{
        ErrorCode::external_image_read_failed,
        "A referenced external image could not be opened for reading",
        "A referenced external image could not be read completely"};
    return read_file_bytes(path, size.value(), messages);
}

} // namespace elf3d::gltf::importer_detail
