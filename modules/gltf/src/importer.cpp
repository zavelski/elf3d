module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/model/detail/imported_metadata.h>

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.image;
import elf.math;
import elf.model;

namespace elf3d::gltf {
namespace {

constexpr std::uintmax_t maximum_source_file_size = 512ULL * 1024ULL * 1024ULL;
constexpr std::uintmax_t maximum_buffer_file_size = 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maximum_total_buffer_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t maximum_cgltf_allocation_bytes =
    static_cast<std::size_t>(2ULL * 1024ULL * 1024ULL * 1024ULL);
constexpr cgltf_size maximum_node_count = 65536;
constexpr std::size_t maximum_node_hierarchy_depth = 1024;
constexpr cgltf_size maximum_mesh_count = 65536;
constexpr cgltf_size maximum_primitive_count = 262144;
constexpr cgltf_size maximum_accessor_count = 262144;
constexpr std::uint64_t maximum_total_vertices = 50000000;
constexpr std::uint64_t maximum_total_indices = 150000000;
constexpr std::uint64_t maximum_total_encoded_image_bytes = 512ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t maximum_total_decoded_image_bytes = 512ULL * 1024ULL * 1024ULL;

[[noreturn]] void fatal_gltf_allocation_failure() noexcept {
    fatal_error("Elf3D glTF importer memory allocation failed");
}

[[noreturn]] void fatal_unexpected_gltf_boundary_exception() noexcept {
    fatal_error("Elf3D glTF importer encountered an unexpected exception");
}

void add_diagnostic(std::vector<ModelLoadDiagnostic>& diagnostics,
                    ModelLoadDiagnosticCategory category, ModelLoadDiagnosticCode code,
                    std::string message, std::optional<std::string> source_context = std::nullopt) {
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

constexpr std::size_t maximum_preserved_json_block_bytes = 1024ULL * 1024ULL;
constexpr std::size_t maximum_preserved_json_bytes = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t maximum_preserved_extension_name_bytes = 256ULL;

struct MetadataBudget {
    std::size_t bytes = 0;
};

[[nodiscard]] bool source_has_metadata(const cgltf_extras& extras,
                                       cgltf_size extensions_count) noexcept {
    return extras.data != nullptr || extras.end_offset > extras.start_offset ||
           extensions_count != 0;
}

void add_metadata_not_preserved(std::vector<ModelLoadDiagnostic>& diagnostics,
                                std::string_view context, std::string_view reason) {
    add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::metadata,
                   ModelLoadDiagnosticCode::metadata_not_preserved,
                   std::string{context} + " metadata was not preserved: " + std::string{reason},
                   std::string{context});
}

[[nodiscard]] bool reserve_metadata_bytes(MetadataBudget& budget, std::size_t bytes) noexcept {
    if (bytes > maximum_preserved_json_bytes - budget.bytes) {
        return false;
    }
    budget.bytes += bytes;
    return true;
}

[[nodiscard]] ModelJsonMetadata copy_metadata(const cgltf_extras& extras,
                                              const cgltf_extension* extensions,
                                              cgltf_size extensions_count, MetadataBudget& budget,
                                              std::vector<ModelLoadDiagnostic>& diagnostics,
                                              std::string_view context) {
    ModelJsonMetadata result;
    if (extras.data != nullptr) {
        const std::string_view data{extras.data};
        if (data.empty() || data.size() > maximum_preserved_json_block_bytes ||
            !reserve_metadata_bytes(budget, data.size())) {
            add_metadata_not_preserved(diagnostics, context,
                                       "extras exceed the bounded raw-JSON budget");
        } else {
            result.extras_json = std::string{data};
        }
    }
    result.extensions.reserve(extensions_count);
    for (cgltf_size index = 0; index < extensions_count; ++index) {
        const cgltf_extension& extension = extensions[index];
        if (extension.name == nullptr || extension.data == nullptr) {
            add_metadata_not_preserved(diagnostics, context,
                                       "an unknown extension has missing raw data");
            continue;
        }
        const std::string_view name{extension.name};
        const std::string_view data{extension.data};
        const bool duplicate = std::any_of(result.extensions.begin(), result.extensions.end(),
                                           [name](const ModelJsonExtension& preserved) noexcept {
                                               return preserved.name == name;
                                           });
        if (duplicate || name.empty() || name.size() > maximum_preserved_extension_name_bytes ||
            data.empty() || data.size() > maximum_preserved_json_block_bytes ||
            !reserve_metadata_bytes(budget, name.size() + data.size())) {
            add_metadata_not_preserved(
                diagnostics, context,
                duplicate ? "an unknown extension name is duplicated"
                          : "an unknown extension exceeds the bounded raw-JSON budget");
            continue;
        }
        result.extensions.push_back(ModelJsonExtension{std::string{name}, std::string{data}});
    }
    return result;
}

[[nodiscard]] ModelJsonMetadata copy_metadata(const cgltf_data& source, const cgltf_extras& extras,
                                              const cgltf_extension* extensions,
                                              cgltf_size extensions_count, MetadataBudget& budget,
                                              std::vector<ModelLoadDiagnostic>& diagnostics,
                                              std::string_view context) {
    ModelJsonMetadata result =
        copy_metadata(extras, extensions, extensions_count, budget, diagnostics, context);
    if (extras.data != nullptr || extras.end_offset <= extras.start_offset) {
        return result;
    }
    if (source.json == nullptr || extras.end_offset > source.json_size) {
        add_metadata_not_preserved(diagnostics, context,
                                   "legacy raw extras offsets are outside the source JSON");
        return result;
    }
    const std::string_view data{source.json + extras.start_offset,
                                extras.end_offset - extras.start_offset};
    if (data.empty() || data.size() > maximum_preserved_json_block_bytes ||
        !reserve_metadata_bytes(budget, data.size())) {
        add_metadata_not_preserved(diagnostics, context,
                                   "extras exceed the bounded raw-JSON budget");
        return result;
    }
    result.extras_json = std::string{data};
    return result;
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

struct CgltfDeleter {
    void operator()(cgltf_data* data) const noexcept {
        cgltf_free(data);
    }
};

using CgltfData = std::unique_ptr<cgltf_data, CgltfDeleter>;

struct BufferLoadContext {
    std::optional<ErrorCode> error_code;
    std::string diagnostic;
};

struct AllocationContext {
    std::size_t live_bytes = 0;
};

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

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const char8_t character : utf8) {
        result.push_back(static_cast<char>(character));
    }
    return result;
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

cgltf_result read_external_file(const cgltf_memory_options* memory,
                                const cgltf_file_options* file_options, const char* path,
                                cgltf_size* size, void** data) {
    auto* context = static_cast<BufferLoadContext*>(file_options->user_data);
    if (context == nullptr || path == nullptr || size == nullptr || data == nullptr) {
        return cgltf_result_invalid_options;
    }

    try {
        const std::filesystem::path file_path = path_from_utf8(path);
        std::error_code filesystem_error;
        const std::uintmax_t file_size = std::filesystem::file_size(file_path, filesystem_error);
        if (filesystem_error) {
            std::error_code existence_error;
            const bool exists = std::filesystem::exists(file_path, existence_error);
            context->error_code = !existence_error && !exists ? ErrorCode::missing_external_buffer
                                                              : ErrorCode::source_file_read_failed;
            context->diagnostic = "Could not open external glTF buffer: " + path_to_utf8(file_path);
            return !existence_error && !exists ? cgltf_result_file_not_found
                                               : cgltf_result_io_error;
        }
        if (file_size > maximum_buffer_file_size) {
            context->error_code = ErrorCode::resource_limit_exceeded;
            context->diagnostic =
                "External glTF buffer exceeds the 1 GiB file limit: " + path_to_utf8(file_path);
            return cgltf_result_io_error;
        }

        const std::uintmax_t requested_size = *size == 0 ? file_size : *size;
        if (requested_size > file_size ||
            requested_size > static_cast<std::uintmax_t>(std::numeric_limits<cgltf_size>::max())) {
            context->error_code = ErrorCode::invalid_buffer_range;
            context->diagnostic =
                "External glTF buffer is shorter than its declared byte length: " +
                path_to_utf8(file_path);
            return cgltf_result_data_too_short;
        }

        void* file_data = cgltf_allocate(memory, static_cast<cgltf_size>(requested_size));
        if (file_data == nullptr && requested_size != 0) {
            return cgltf_result_out_of_memory;
        }

        std::ifstream stream{file_path, std::ios::binary};
        if (!stream || requested_size > static_cast<std::uintmax_t>(
                                            std::numeric_limits<std::streamsize>::max())) {
            cgltf_deallocate(memory, file_data);
            context->error_code = ErrorCode::source_file_read_failed;
            context->diagnostic = "Could not read external glTF buffer: " + path_to_utf8(file_path);
            return cgltf_result_io_error;
        }
        if (requested_size != 0) {
            stream.read(static_cast<char*>(file_data),
                        static_cast<std::streamsize>(requested_size));
            if (!stream) {
                cgltf_deallocate(memory, file_data);
                context->error_code = ErrorCode::source_file_read_failed;
                context->diagnostic =
                    "Failed while reading external glTF buffer: " + path_to_utf8(file_path);
                return cgltf_result_io_error;
            }
        }

        *size = static_cast<cgltf_size>(requested_size);
        *data = file_data;
        return cgltf_result_success;
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

[[nodiscard]] Result<std::vector<std::byte>> read_source(const std::filesystem::path& path) {
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

    try {
        std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
        std::ifstream stream{path, std::ios::binary};
        if (!stream) {
            return Error{ErrorCode::source_file_read_failed,
                         "The glTF source file could not be opened for reading"};
        }
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            return Error{ErrorCode::source_file_read_failed,
                         "The glTF source file could not be read completely"};
        }
        return bytes;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] std::string lower_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

[[nodiscard]] Error parse_error(cgltf_result result, bool is_glb) {
    switch (result) {
    case cgltf_result_data_too_short:
        return Error{is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                     is_glb ? "The GLB file is truncated" : "The glTF source is truncated"};
    case cgltf_result_invalid_json:
        return Error{is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                     is_glb ? "The GLB JSON chunk is malformed" : "The glTF JSON is malformed"};
    case cgltf_result_unknown_format:
    case cgltf_result_legacy_gltf:
        return Error{is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                     "The source is not supported glTF 2.0 content"};
    case cgltf_result_invalid_gltf:
        return Error{is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                     "The source contains invalid glTF data"};
    case cgltf_result_out_of_memory:
        return Error{ErrorCode::resource_limit_exceeded,
                     "cgltf could not allocate memory while parsing the source"};
    default:
        return Error{ErrorCode::scene_import_failed, "cgltf could not parse the source"};
    }
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
        std::size_t current = start;
        std::size_t parent_depth = 0U;
        while (states[current] == 0U) {
            states[current] = 1U;
            path.push_back(current);
            const cgltf_node* parent = data.nodes[current].parent;
            if (parent == nullptr) {
                break;
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
                parent_depth = depths[current];
                break;
            }
        }
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
    }
    return {};
}

[[nodiscard]] Result<void> validate_resource_limits(const cgltf_data& data) {
    if (data.nodes_count > maximum_node_count || data.meshes_count > maximum_mesh_count ||
        data.accessors_count > maximum_accessor_count) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "glTF object counts exceed the importer resource limits"};
    }

    cgltf_size primitive_count = 0;
    std::uint64_t total_buffer_bytes = 0;
    std::uint64_t total_vertices = 0;
    std::uint64_t total_indices = 0;
    for (cgltf_size buffer_index = 0; buffer_index < data.buffers_count; ++buffer_index) {
        if (data.buffers[buffer_index].size > maximum_buffer_file_size ||
            !checked_add(total_buffer_bytes,
                         static_cast<std::uint64_t>(data.buffers[buffer_index].size),
                         maximum_total_buffer_bytes)) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "Declared glTF buffers exceed the importer byte limits"};
        }
    }
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        if (mesh.primitives_count > maximum_primitive_count - primitive_count) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "The glTF primitive count exceeds the importer limit"};
        }
        primitive_count += mesh.primitives_count;
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            const cgltf_accessor* positions =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            if (positions != nullptr &&
                !checked_add(total_vertices, static_cast<std::uint64_t>(positions->count),
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
            if (!index_count.has_value()) {
                return Error{ErrorCode::resource_limit_exceeded,
                             "The glTF expanded triangle index count exceeds the importer limit"};
            }
            if (!checked_add(total_indices, *index_count, maximum_total_indices)) {
                return Error{ErrorCode::resource_limit_exceeded,
                             "The glTF expanded triangle index count exceeds the importer limit"};
            }
        }
    }
    return {};
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

[[nodiscard]] int base64_value(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    return character == '/' ? 63 : -1;
}

[[nodiscard]] Result<std::vector<std::byte>> decode_base64(std::string_view payload) {
    if (payload.empty() || payload.size() % 4 != 0 ||
        payload.size() / 4 > image::maximum_encoded_bytes / 3 + 1) {
        return Error{ErrorCode::invalid_base64_payload,
                     "Image data URI contains an invalid base64 length"};
    }
    try {
        std::vector<std::byte> result;
        result.reserve(payload.size() / 4 * 3);
        for (std::size_t index = 0; index < payload.size(); index += 4) {
            const bool final_group = index + 4 == payload.size();
            const bool pad2 = payload[index + 2] == '=';
            const bool pad3 = payload[index + 3] == '=';
            const int first = base64_value(payload[index]);
            const int second = base64_value(payload[index + 1]);
            const int third = pad2 ? 0 : base64_value(payload[index + 2]);
            const int fourth = pad3 ? 0 : base64_value(payload[index + 3]);
            if (first < 0 || second < 0 || third < 0 || fourth < 0 || (pad2 && !pad3) ||
                ((pad2 || pad3) && !final_group)) {
                return Error{ErrorCode::invalid_base64_payload,
                             "Image data URI contains malformed base64 padding or characters"};
            }
            const std::uint32_t value = (static_cast<std::uint32_t>(first) << 18U) |
                                        (static_cast<std::uint32_t>(second) << 12U) |
                                        (static_cast<std::uint32_t>(third) << 6U) |
                                        static_cast<std::uint32_t>(fourth);
            result.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
            if (!pad2) {
                result.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
            }
            if (!pad3) {
                result.push_back(static_cast<std::byte>(value & 0xffU));
            }
        }
        if (result.size() > image::maximum_encoded_bytes) {
            return Error{ErrorCode::image_resource_limit_exceeded,
                         "Decoded image data URI exceeds the 64 MiB encoded-byte limit"};
        }
        return result;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
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

[[nodiscard]] Result<std::string> percent_decode(std::string_view uri) {
    try {
        std::string result;
        result.reserve(uri.size());
        for (std::size_t index = 0; index < uri.size(); ++index) {
            if (uri[index] != '%') {
                result.push_back(uri[index]);
                continue;
            }
            if (index + 2 >= uri.size()) {
                return Error{ErrorCode::external_image_read_failed,
                             "External image URI contains a truncated percent escape"};
            }
            const auto hex = [](char value) -> int {
                if (value >= '0' && value <= '9') {
                    return value - '0';
                }
                if (value >= 'a' && value <= 'f') {
                    return value - 'a' + 10;
                }
                return value >= 'A' && value <= 'F' ? value - 'A' + 10 : -1;
            };
            const int high = hex(uri[index + 1]);
            const int low = hex(uri[index + 2]);
            if (high < 0 || low < 0) {
                return Error{ErrorCode::external_image_read_failed,
                             "External image URI contains an invalid percent escape"};
            }
            result.push_back(static_cast<char>((high << 4) | low));
            index += 2;
        }
        return result;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<std::byte>> read_image_file(const std::filesystem::path& path) {
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
    try {
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        std::ifstream stream{path, std::ios::binary};
        stream.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            return Error{ErrorCode::external_image_read_failed,
                         "A referenced external image could not be read completely"};
        }
        return bytes;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

struct EncodedImage {
    ModelImageMimeType mime = ModelImageMimeType::png;
    std::vector<std::byte> bytes;
};

struct ImageImportBudget {
    std::uint64_t encoded_bytes = 0;
    std::uint64_t decoded_bytes = 0;
};

[[nodiscard]] Result<EncodedImage> encoded_image(const cgltf_image& source,
                                                 const std::filesystem::path& gltf_path) {
    if (source.uri != nullptr) {
        const std::string_view uri{source.uri};
        if (uri.starts_with("data:")) {
            const std::size_t comma = uri.find(',');
            if (comma == std::string_view::npos || comma <= 5) {
                return Error{ErrorCode::malformed_data_uri,
                             "glTF image data URI is missing metadata or payload"};
            }
            const std::string_view metadata = uri.substr(5, comma - 5);
            constexpr std::string_view base64_suffix = ";base64";
            if (!metadata.ends_with(base64_suffix)) {
                return Error{ErrorCode::malformed_data_uri,
                             "glTF image data URIs must use base64 encoding"};
            }
            Result<ModelImageMimeType> mime =
                mime_from_text(metadata.substr(0, metadata.size() - base64_suffix.size()));
            if (!mime) {
                return mime.error();
            }
            Result<std::vector<std::byte>> bytes = decode_base64(uri.substr(comma + 1));
            if (!bytes) {
                return bytes.error();
            }
            if (!encoded_matches(mime.value(), bytes.value())) {
                return Error{ErrorCode::image_decode_failed,
                             "Image data URI payload does not match its declared MIME type"};
            }
            return EncodedImage{mime.value(), std::move(bytes).value()};
        }
        if (uri.find("://") != std::string_view::npos) {
            return Error{ErrorCode::unsupported_remote_uri,
                         "Remote HTTP/HTTPS glTF image URIs are unsupported"};
        }
        Result<std::string> decoded_uri = percent_decode(uri);
        if (!decoded_uri) {
            return decoded_uri.error();
        }
        const std::filesystem::path image_path =
            gltf_path.parent_path() / path_from_utf8(decoded_uri.value());
        Result<ModelImageMimeType> mime = source.mime_type != nullptr
                                              ? mime_from_text(source.mime_type)
                                              : mime_from_extension(image_path);
        if (!mime) {
            return mime.error();
        }
        Result<std::vector<std::byte>> bytes = read_image_file(image_path);
        if (!bytes) {
            return Error{bytes.error().code(),
                         std::string{bytes.error().message()} + ": " + path_to_utf8(image_path)};
        }
        if (!encoded_matches(mime.value(), bytes.value())) {
            return Error{ErrorCode::image_decode_failed,
                         "External image bytes do not match the declared or inferred format"};
        }
        return EncodedImage{mime.value(), std::move(bytes).value()};
    }

    if (source.buffer_view == nullptr) {
        return Error{ErrorCode::invalid_image_buffer_view,
                     "A glTF image has neither a URI nor a buffer view"};
    }
    if (source.mime_type == nullptr) {
        return Error{ErrorCode::unsupported_image_mime_type,
                     "A buffer-view glTF image requires image/png or image/jpeg MIME type"};
    }
    Result<ModelImageMimeType> mime = mime_from_text(source.mime_type);
    if (!mime) {
        return mime.error();
    }
    const cgltf_buffer_view& view = *source.buffer_view;
    if (view.buffer == nullptr || view.buffer->data == nullptr || view.size == 0 ||
        view.offset > view.buffer->size || view.size > view.buffer->size - view.offset ||
        view.size > image::maximum_encoded_bytes) {
        return Error{view.size > image::maximum_encoded_bytes
                         ? ErrorCode::image_resource_limit_exceeded
                         : ErrorCode::image_range_out_of_bounds,
                     "A GLB image buffer view is empty or outside its source buffer"};
    }
    try {
        const auto* begin = static_cast<const std::byte*>(view.buffer->data) + view.offset;
        std::vector<std::byte> bytes(begin, begin + view.size);
        if (!encoded_matches(mime.value(), bytes)) {
            return Error{ErrorCode::image_decode_failed,
                         "GLB image bytes do not match the declared MIME type"};
        }
        return EncodedImage{mime.value(), std::move(bytes)};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<ImageId> image_for(const cgltf_data& data, const cgltf_image* source,
                                        const std::filesystem::path& gltf_path,
                                        DocumentBuilder& builder,
                                        std::vector<std::optional<ImageId>>& images,
                                        ImageImportBudget& budget) {
    if (source == nullptr || source < data.images || source >= data.images + data.images_count) {
        return Error{ErrorCode::invalid_image_handle,
                     "A glTF texture references an image outside the image table"};
    }
    const std::size_t index = static_cast<std::size_t>(source - data.images);
    if (images[index].has_value()) {
        return images[index].value();
    }
    Result<EncodedImage> encoded = encoded_image(*source, gltf_path);
    if (!encoded) {
        return encoded.error();
    }
    std::uint64_t encoded_total = budget.encoded_bytes;
    if (!checked_add(encoded_total, static_cast<std::uint64_t>(encoded.value().bytes.size()),
                     maximum_total_encoded_image_bytes)) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Imported scene images exceed the 512 MiB encoded-image limit"};
    }
    Result<image::DecodedImage> decoded = image::decode_png_or_jpeg(encoded.value().bytes);
    if (!decoded) {
        return decoded.error();
    }
    std::uint64_t decoded_total = budget.decoded_bytes;
    if (!checked_add(decoded_total, static_cast<std::uint64_t>(decoded.value().pixels.size()),
                     maximum_total_decoded_image_bytes)) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Imported scene images exceed the 512 MiB decoded-image limit"};
    }
    const Result<ImageId> created = builder.create_image(ModelImageDescription{
        decoded.value().width, decoded.value().height, ModelPixelFormat::rgba8_unorm,
        decoded.value().pixels, encoded.value().mime, encoded.value().bytes});
    if (!created) {
        return created.error();
    }
    budget.encoded_bytes = encoded_total;
    budget.decoded_bytes = decoded_total;
    images[index] = created.value();
    return created.value();
}

[[nodiscard]] Result<ModelTextureWrap> texture_wrap(cgltf_wrap_mode wrap) {
    switch (wrap) {
    case cgltf_wrap_mode_repeat:
        return ModelTextureWrap::repeat;
    case cgltf_wrap_mode_mirrored_repeat:
        return ModelTextureWrap::mirrored_repeat;
    case cgltf_wrap_mode_clamp_to_edge:
        return ModelTextureWrap::clamp_to_edge;
    default:
        return Error{ErrorCode::invalid_sampler_wrap,
                     "A glTF sampler contains an invalid wrap mode"};
    }
}

[[nodiscard]] Result<ModelTextureFilter> min_filter(cgltf_filter_type filter) {
    switch (filter) {
    case cgltf_filter_type_undefined:
    case cgltf_filter_type_linear:
        return ModelTextureFilter::linear;
    case cgltf_filter_type_nearest:
        return ModelTextureFilter::nearest;
    case cgltf_filter_type_nearest_mipmap_nearest:
        return ModelTextureFilter::nearest_mipmap_nearest;
    case cgltf_filter_type_linear_mipmap_nearest:
        return ModelTextureFilter::linear_mipmap_nearest;
    case cgltf_filter_type_nearest_mipmap_linear:
        return ModelTextureFilter::nearest_mipmap_linear;
    case cgltf_filter_type_linear_mipmap_linear:
        return ModelTextureFilter::linear_mipmap_linear;
    default:
        return Error{ErrorCode::invalid_sampler_filter,
                     "A glTF sampler contains an invalid minification filter"};
    }
}

[[nodiscard]] Result<ModelTextureFilter> mag_filter(cgltf_filter_type filter) {
    switch (filter) {
    case cgltf_filter_type_undefined:
    case cgltf_filter_type_linear:
        return ModelTextureFilter::linear;
    case cgltf_filter_type_nearest:
        return ModelTextureFilter::nearest;
    default:
        return Error{ErrorCode::invalid_sampler_filter,
                     "A glTF sampler magnification filter must be NEAREST or LINEAR"};
    }
}

[[nodiscard]] Result<ModelSamplerDescription> sampler_description(const cgltf_sampler* sampler) {
    if (sampler == nullptr) {
        return ModelSamplerDescription{};
    }
    Result<ModelTextureWrap> wrap_u = texture_wrap(sampler->wrap_s);
    Result<ModelTextureWrap> wrap_v = texture_wrap(sampler->wrap_t);
    Result<ModelTextureFilter> minimum = min_filter(sampler->min_filter);
    Result<ModelTextureFilter> magnification = mag_filter(sampler->mag_filter);
    if (!wrap_u) {
        return wrap_u.error();
    }
    if (!wrap_v) {
        return wrap_v.error();
    }
    if (!minimum) {
        return minimum.error();
    }
    if (!magnification) {
        return magnification.error();
    }
    return ModelSamplerDescription{wrap_u.value(), wrap_v.value(), minimum.value(),
                                   magnification.value()};
}

[[nodiscard]] Result<SamplerId> sampler_for(const cgltf_data& data, const cgltf_sampler* source,
                                            DocumentBuilder& builder,
                                            std::vector<std::optional<SamplerId>>& samplers,
                                            std::optional<SamplerId>& default_sampler) {
    if (source == nullptr) {
        if (!default_sampler.has_value()) {
            const Result<SamplerId> created = builder.create_sampler(ModelSamplerDescription{});
            if (!created) {
                return created.error();
            }
            default_sampler = created.value();
        }
        return *default_sampler;
    }
    if (source < data.samplers || source >= data.samplers + data.samplers_count) {
        return Error{ErrorCode::invalid_sampler_description,
                     "A glTF texture references a sampler outside the sampler table"};
    }
    const std::size_t index = static_cast<std::size_t>(source - data.samplers);
    if (samplers[index].has_value()) {
        return samplers[index].value();
    }
    Result<ModelSamplerDescription> description = sampler_description(source);
    if (!description) {
        return description.error();
    }
    const Result<SamplerId> created = builder.create_sampler(description.value());
    if (!created) {
        return created.error();
    }
    samplers[index] = created.value();
    return created.value();
}

[[nodiscard]] std::string mesh_context(const cgltf_mesh& mesh, cgltf_size mesh_index,
                                       cgltf_size primitive_index);

[[nodiscard]] Result<void> validate_texture_inputs(const cgltf_data& data) {
    for (cgltf_size sampler_index = 0; sampler_index < data.samplers_count; ++sampler_index) {
        const Result<ModelSamplerDescription> sampler =
            sampler_description(&data.samplers[sampler_index]);
        if (!sampler) {
            return sampler.error();
        }
    }
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            if (!supported_primitive_type(primitive.type)) {
                continue;
            }
            const cgltf_accessor* positions =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            for (cgltf_int set = 0;
                 set < static_cast<cgltf_int>(model_maximum_texture_coordinate_sets); ++set) {
                const cgltf_accessor* texcoords =
                    cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, set);
                if (texcoords == nullptr) {
                    continue;
                }
                const std::string semantic = " TEXCOORD_" + std::to_string(set);
                if (texcoords->type != cgltf_type_vec2 || texcoords->count == 0) {
                    return Error{ErrorCode::invalid_texcoord,
                                 mesh_context(mesh, mesh_index, primitive_index) + semantic +
                                     " must be a non-empty VEC2 accessor"};
                }
                if (positions != nullptr && texcoords->count != positions->count) {
                    return Error{ErrorCode::mismatched_texcoord_count,
                                 mesh_context(mesh, mesh_index, primitive_index) + semantic +
                                     " count does not match POSITION"};
                }
            }
        }
    }
    return {};
}

[[nodiscard]] Result<TextureId> texture_for(const cgltf_data& data, const cgltf_texture* source,
                                            const std::filesystem::path& gltf_path,
                                            DocumentBuilder& builder,
                                            std::vector<std::optional<ImageId>>& images,
                                            std::vector<std::optional<TextureId>>& textures,
                                            std::vector<std::optional<SamplerId>>& samplers,
                                            std::optional<SamplerId>& default_sampler,
                                            ImageImportBudget& image_budget) {
    if (source == nullptr || source < data.textures ||
        source >= data.textures + data.textures_count) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "A glTF material references a texture outside the texture table"};
    }
    const std::size_t index = static_cast<std::size_t>(source - data.textures);
    if (textures[index].has_value()) {
        return textures[index].value();
    }
    if (source->image == nullptr) {
        return Error{ErrorCode::unsupported_image_mime_type,
                     "The glTF texture has no ordinary PNG/JPEG fallback image"};
    }
    Result<ImageId> image =
        image_for(data, source->image, gltf_path, builder, images, image_budget);
    if (!image) {
        return image.error();
    }
    Result<SamplerId> sampler =
        sampler_for(data, source->sampler, builder, samplers, default_sampler);
    if (!sampler) {
        return sampler.error();
    }
    const Result<TextureId> created =
        builder.create_texture(ModelTextureDescription{image.value(), sampler.value()});
    if (!created) {
        return created.error();
    }
    textures[index] = created.value();
    return created.value();
}

[[nodiscard]] std::string mesh_context(const cgltf_mesh& mesh, cgltf_size mesh_index,
                                       cgltf_size primitive_index) {
    std::string result = "mesh ";
    result +=
        mesh.name != nullptr ? std::string{"'"} + mesh.name + "'" : std::to_string(mesh_index);
    result += ", primitive " + std::to_string(primitive_index);
    return result;
}

[[nodiscard]] std::string node_context(const cgltf_node& node, cgltf_size node_index) {
    std::string result = "node ";
    result += node.name != nullptr ? node.name : std::to_string(node_index);
    return result;
}

[[nodiscard]] Result<std::vector<bool>> reachable_nodes(const cgltf_data& data) {
    try {
        std::vector<bool> reachable(data.nodes_count, false);
        std::vector<const cgltf_node*> queue;
        const auto append_root = [&](const cgltf_node* node) -> Result<void> {
            if (node == nullptr || node < data.nodes || node >= data.nodes + data.nodes_count) {
                return Error{ErrorCode::invalid_node_hierarchy,
                             "A glTF scene contains an invalid root node"};
            }
            const std::size_t index = static_cast<std::size_t>(node - data.nodes);
            if (reachable[index]) {
                return {};
            }
            reachable[index] = true;
            queue.push_back(node);
            return {};
        };

        if (data.scenes_count != 0) {
            for (cgltf_size scene_index = 0; scene_index < data.scenes_count; ++scene_index) {
                const cgltf_scene& scene = data.scenes[scene_index];
                for (cgltf_size root_index = 0; root_index < scene.nodes_count; ++root_index) {
                    const Result<void> result = append_root(scene.nodes[root_index]);
                    if (!result) {
                        return result.error();
                    }
                }
            }
        } else {
            for (cgltf_size index = 0; index < data.nodes_count; ++index) {
                if (data.nodes[index].parent == nullptr) {
                    const Result<void> result = append_root(&data.nodes[index]);
                    if (!result) {
                        return result.error();
                    }
                }
            }
        }

        for (std::size_t queue_index = 0; queue_index < queue.size(); ++queue_index) {
            const cgltf_node* node = queue[queue_index];
            for (cgltf_size child_index = 0; child_index < node->children_count; ++child_index) {
                const cgltf_node* child = node->children[child_index];
                if (child == nullptr || child < data.nodes ||
                    child >= data.nodes + data.nodes_count || child->parent != node) {
                    return Error{ErrorCode::invalid_node_hierarchy,
                                 "A glTF scene contains an invalid child link"};
                }
                const std::size_t index = static_cast<std::size_t>(child - data.nodes);
                if (reachable[index]) {
                    continue;
                }
                reachable[index] = true;
                queue.push_back(child);
            }
        }
        return reachable;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<float>>
unpack_float3(const cgltf_accessor& accessor, ErrorCode error_code, std::string_view context) {
    if (accessor.count > std::numeric_limits<std::size_t>::max() / 3) {
        return Error{ErrorCode::size_overflow,
                     std::string{context} + " accessor size overflows addressable memory"};
    }
    if (accessor.type != cgltf_type_vec3 || accessor.count == 0) {
        return Error{error_code, std::string{context} + " requires a non-empty VEC3 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * 3;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<float>> unpack_float2(const cgltf_accessor& accessor,
                                                       std::string_view context) {
    if (accessor.count > std::numeric_limits<std::size_t>::max() / 2) {
        return Error{ErrorCode::size_overflow,
                     std::string{context} + " accessor size overflows addressable memory"};
    }
    if (accessor.type != cgltf_type_vec2 || accessor.count == 0) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " requires a non-empty VEC2 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * 2;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_texcoord,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<std::vector<float>> unpack_color(const cgltf_accessor& accessor,
                                                      std::string_view context) {
    const std::size_t components = accessor.type == cgltf_type_vec3   ? 3U
                                   : accessor.type == cgltf_type_vec4 ? 4U
                                                                      : 0U;
    if (components == 0 || accessor.count == 0 ||
        accessor.count > std::numeric_limits<std::size_t>::max() / components) {
        return Error{ErrorCode::invalid_accessor,
                     std::string{context} + " requires a non-empty VEC3 or VEC4 accessor"};
    }
    try {
        const std::size_t float_count = static_cast<std::size_t>(accessor.count) * components;
        std::vector<float> values(float_count);
        if (cgltf_accessor_unpack_floats(&accessor, values.data(), float_count) != float_count) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} + " could not be decoded from its accessor"};
        }
        return values;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<ModelTextureMapping> texture_mapping(const cgltf_texture_view& view,
                                                          std::string_view context) {
    const cgltf_int selected_set =
        view.has_transform && view.transform.has_texcoord ? view.transform.texcoord : view.texcoord;
    if (selected_set < 0 ||
        selected_set >= static_cast<cgltf_int>(model_maximum_texture_coordinate_sets)) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " references unsupported TEXCOORD_" +
                         std::to_string(selected_set) + "; Elf3D supports UV sets 0 and 1"};
    }

    ModelTextureMapping mapping;
    mapping.texcoord_set = static_cast<std::uint32_t>(selected_set);
    if (view.has_transform) {
        mapping.transform.offset = {view.transform.offset[0], view.transform.offset[1]};
        mapping.transform.scale = {view.transform.scale[0], view.transform.scale[1]};
        mapping.transform.rotation_radians = view.transform.rotation;
    }
    if (!std::isfinite(mapping.transform.offset.x) || !std::isfinite(mapping.transform.offset.y) ||
        !std::isfinite(mapping.transform.scale.x) || !std::isfinite(mapping.transform.scale.y) ||
        !std::isfinite(mapping.transform.rotation_radians)) {
        return Error{ErrorCode::invalid_texcoord,
                     std::string{context} + " contains a non-finite texture transform"};
    }
    return mapping;
}

struct ImportedTextureView {
    TextureId texture;
    ModelTextureMapping mapping;
};

struct ImportedMesh {
    std::optional<MeshId> mesh;
    std::vector<std::optional<PrimitiveId>> primitives;
    std::uint64_t primitive_count = 0;
};

struct ConstructedDocument {
    Document document;
    DocumentSceneId default_scene;
};

struct ImportedDocumentIds {
    std::vector<DocumentSceneId> scenes;
    std::vector<std::optional<NodeId>> nodes;
    std::vector<std::optional<MeshId>> meshes;
    std::vector<std::vector<std::optional<PrimitiveId>>> primitives;
    std::vector<std::optional<MaterialId>> material_cache;
    std::vector<std::vector<MaterialId>> materials;
    std::vector<std::optional<ImageId>> images;
    std::vector<std::optional<TextureId>> textures;
    std::vector<std::optional<SamplerId>> samplers;
};

using TexcoordAvailability = std::array<bool, model_maximum_texture_coordinate_sets>;

[[nodiscard]] TexcoordAvailability
primitive_texcoord_availability(const cgltf_primitive& primitive) {
    TexcoordAvailability availability{};
    for (cgltf_int set = 0; set < static_cast<cgltf_int>(model_maximum_texture_coordinate_sets);
         ++set) {
        availability[static_cast<std::size_t>(set)] =
            cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, set) != nullptr;
    }
    return availability;
}

[[nodiscard]] Result<bool>
texture_view_uses_unavailable_texcoord(const cgltf_texture_view& view,
                                       const TexcoordAvailability& available_texcoords,
                                       std::string_view context) {
    if (view.texture == nullptr) {
        return false;
    }
    Result<ModelTextureMapping> mapping = texture_mapping(view, context);
    if (!mapping) {
        return mapping.error();
    }
    return !available_texcoords[mapping.value().texcoord_set];
}

[[nodiscard]] Result<bool>
material_uses_unavailable_texcoord(const cgltf_material& material,
                                   const TexcoordAvailability& available_texcoords,
                                   std::string_view context) {
    auto check = [&](const cgltf_texture_view& view, std::string_view slot) -> Result<bool> {
        Result<bool> unavailable = texture_view_uses_unavailable_texcoord(
            view, available_texcoords, std::string{context} + " " + std::string{slot});
        if (!unavailable) {
            return unavailable.error();
        }
        return unavailable.value();
    };

    if (material.has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr = material.pbr_metallic_roughness;
        Result<bool> unavailable = check(pbr.base_color_texture, "base-color texture");
        if (!unavailable) {
            return unavailable.error();
        }
        if (unavailable.value()) {
            return true;
        }
        unavailable = check(pbr.metallic_roughness_texture, "metallic-roughness texture");
        if (!unavailable) {
            return unavailable.error();
        }
        if (unavailable.value()) {
            return true;
        }
    } else if (material.has_pbr_specular_glossiness) {
        Result<bool> unavailable = check(material.pbr_specular_glossiness.diffuse_texture,
                                         "specular-glossiness diffuse texture");
        if (!unavailable) {
            return unavailable.error();
        }
        if (unavailable.value()) {
            return true;
        }
    }

    Result<bool> unavailable = check(material.normal_texture, "normal texture");
    if (!unavailable) {
        return unavailable.error();
    }
    if (unavailable.value()) {
        return true;
    }
    unavailable = check(material.occlusion_texture, "occlusion texture");
    if (!unavailable) {
        return unavailable.error();
    }
    if (unavailable.value()) {
        return true;
    }
    unavailable = check(material.emissive_texture, "emissive texture");
    if (!unavailable) {
        return unavailable.error();
    }
    if (unavailable.value()) {
        return true;
    }
    return false;
}

[[nodiscard]] Result<ImportedTextureView> import_texture_view(
    const cgltf_data& data, const cgltf_texture_view& view, std::string_view slot_name,
    const std::filesystem::path& gltf_path, DocumentBuilder& builder,
    std::vector<std::optional<ImageId>>& images, std::vector<std::optional<TextureId>>& textures,
    std::vector<std::optional<SamplerId>>& samplers, std::optional<SamplerId>& default_sampler,
    const TexcoordAvailability& available_texcoords, bool& primitive_specific_fallback,
    std::vector<ModelLoadDiagnostic>& diagnostics, ImageImportBudget& image_budget,
    std::string_view context) {
    Result<ModelTextureMapping> mapping = texture_mapping(view, context);
    if (!mapping) {
        return mapping.error();
    }
    if (view.texture == nullptr) {
        return ImportedTextureView{{}, mapping.value()};
    }
    if (!available_texcoords[mapping.value().texcoord_set]) {
        primitive_specific_fallback = true;
        add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::texture,
                       ModelLoadDiagnosticCode::texture_fallback,
                       std::string{slot_name} + " texture references TEXCOORD_" +
                           std::to_string(mapping.value().texcoord_set) +
                           " that this primitive does not provide; the slot was disabled",
                       std::string{context});
        return ImportedTextureView{{}, mapping.value()};
    }
    if (view.texture->image == nullptr && (view.texture->has_basisu || view.texture->has_webp)) {
        add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::texture,
                       ModelLoadDiagnosticCode::texture_fallback,
                       std::string{slot_name} +
                           " texture uses an unsupported compressed/WebP source and has no "
                           "ordinary PNG/JPEG fallback; the slot was disabled",
                       std::string{context});
        return ImportedTextureView{{}, mapping.value()};
    }
    Result<TextureId> texture = texture_for(data, view.texture, gltf_path, builder, images,
                                            textures, samplers, default_sampler, image_budget);
    if (!texture) {
        if (texture_failure_can_fallback(texture.error().code())) {
            add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::texture,
                           ModelLoadDiagnosticCode::texture_fallback,
                           std::string{slot_name} +
                               " texture could not be imported and was "
                               "disabled: " +
                               texture.error().message(),
                           std::string{context});
            return ImportedTextureView{{}, mapping.value()};
        }
        return texture.error();
    }
    return ImportedTextureView{texture.value(), mapping.value()};
}

[[nodiscard]] Result<std::vector<std::uint32_t>> import_indices(const cgltf_primitive& primitive,
                                                                std::size_t vertex_count,
                                                                std::string_view context) {
    try {
        std::vector<std::uint32_t> source_indices;
        if (primitive.indices == nullptr) {
            if (vertex_count >
                static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
                return Error{ErrorCode::invalid_accessor,
                             std::string{context} + " has too many non-indexed vertices"};
            }
            source_indices.resize(vertex_count);
            for (std::size_t index = 0; index < vertex_count; ++index) {
                source_indices[index] = static_cast<std::uint32_t>(index);
            }
        } else {
            const cgltf_accessor& accessor = *primitive.indices;
            if (accessor.type != cgltf_type_scalar) {
                return Error{ErrorCode::invalid_accessor,
                             std::string{context} + " uses a non-scalar index accessor"};
            }
            if (accessor.component_type != cgltf_component_type_r_8u &&
                accessor.component_type != cgltf_component_type_r_16u &&
                accessor.component_type != cgltf_component_type_r_32u) {
                return Error{ErrorCode::unsupported_index_type,
                             std::string{context} +
                                 " uses an unsupported signed or non-integer index type"};
            }
            if (accessor.count == 0 || accessor.is_sparse) {
                return Error{ErrorCode::invalid_accessor,
                             std::string{context} +
                                 " requires a non-empty, non-sparse index accessor"};
            }
            source_indices.resize(accessor.count);
            if (cgltf_accessor_unpack_indices(&accessor, source_indices.data(),
                                              sizeof(std::uint32_t),
                                              accessor.count) != accessor.count) {
                return Error{ErrorCode::invalid_accessor,
                             std::string{context} + " index accessor could not be decoded"};
            }
        }
        for (const std::uint32_t index : source_indices) {
            if (static_cast<std::size_t>(index) >= vertex_count) {
                return Error{ErrorCode::imported_index_out_of_range,
                             std::string{context} + " contains an index outside POSITION"};
            }
        }

        if (primitive.type == cgltf_primitive_type_triangles) {
            if (source_indices.size() % 3 != 0) {
                return Error{ErrorCode::invalid_accessor,
                             std::string{context} +
                                 " triangle-list index count is not divisible by three"};
            }
            return source_indices;
        }
        if (primitive.type != cgltf_primitive_type_triangle_strip &&
            primitive.type != cgltf_primitive_type_triangle_fan) {
            return Error{ErrorCode::unsupported_primitive_mode,
                         std::string{context} +
                             " uses an unsupported points or lines primitive mode"};
        }
        if (source_indices.size() < 3 ||
            source_indices.size() - 2 > std::numeric_limits<std::size_t>::max() / 3) {
            return Error{ErrorCode::invalid_accessor,
                         std::string{context} +
                             " triangle strip/fan requires at least three indices"};
        }

        std::vector<std::uint32_t> triangles;
        triangles.reserve((source_indices.size() - 2) * 3);
        for (std::size_t index = 0; index + 2 < source_indices.size(); ++index) {
            if (primitive.type == cgltf_primitive_type_triangle_fan) {
                triangles.push_back(source_indices[0]);
                triangles.push_back(source_indices[index + 1]);
                triangles.push_back(source_indices[index + 2]);
            } else if (index % 2 == 0) {
                triangles.push_back(source_indices[index]);
                triangles.push_back(source_indices[index + 1]);
                triangles.push_back(source_indices[index + 2]);
            } else {
                triangles.push_back(source_indices[index + 1]);
                triangles.push_back(source_indices[index]);
                triangles.push_back(source_indices[index + 2]);
            }
        }
        return triangles;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

void generate_normals(std::span<const Float3> positions, std::vector<Float3>& normals,
                      std::span<const std::uint32_t> indices, std::uint64_t& degenerate_count,
                      std::uint64_t& fallback_count) {
    std::vector<Float3> accumulated(positions.size());
    for (std::size_t index = 0; index < indices.size(); index += 3) {
        const Float3 a = positions[indices[index]];
        const Float3 b = positions[indices[index + 1]];
        const Float3 c = positions[indices[index + 2]];
        const Float3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        const Float3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        const Float3 face{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z,
                          ab.x * ac.y - ab.y * ac.x};
        const float length_squared = face.x * face.x + face.y * face.y + face.z * face.z;
        if (!std::isfinite(length_squared) || length_squared <= 0.000000000001F) {
            ++degenerate_count;
            continue;
        }
        for (std::size_t offset = 0; offset < 3; ++offset) {
            Float3& normal = accumulated[indices[index + offset]];
            normal.x += face.x;
            normal.y += face.y;
            normal.z += face.z;
        }
    }

    normals.resize(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        const Float3 normal = accumulated[index];
        const float length_squared =
            normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (!std::isfinite(length_squared) || length_squared <= 0.000000000001F) {
            normals[index] = Float3{0.0F, 1.0F, 0.0F};
            ++fallback_count;
            continue;
        }
        const float inverse_length = 1.0F / std::sqrt(length_squared);
        normals[index] =
            Float3{normal.x * inverse_length, normal.y * inverse_length, normal.z * inverse_length};
    }
}

[[nodiscard]] Result<MaterialId> material_for(
    const cgltf_data& data, const cgltf_material* material, const std::filesystem::path& gltf_path,
    DocumentBuilder& builder, std::vector<std::optional<MaterialId>>& materials,
    std::vector<std::vector<MaterialId>>& material_ids, std::vector<std::optional<ImageId>>& images,
    std::vector<std::optional<TextureId>>& textures,
    std::vector<std::optional<SamplerId>>& samplers, std::optional<SamplerId>& default_sampler,
    const TexcoordAvailability& available_texcoords, std::optional<MaterialId>& default_material,
    std::vector<ModelLoadDiagnostic>& diagnostics, ImageImportBudget& image_budget) {
    if (material == nullptr) {
        if (!default_material.has_value()) {
            const Result<MaterialId> created = builder.create_material(ModelMaterialDescription{});
            if (!created) {
                return created.error();
            }
            default_material = created.value();
        }
        return *default_material;
    }

    const std::size_t index = static_cast<std::size_t>(material - data.materials);
    if (index >= materials.size()) {
        return Error{ErrorCode::scene_import_failed,
                     "A glTF primitive references a material outside the material table"};
    }
    const std::string material_name =
        material->name != nullptr ? material->name : std::to_string(index);
    const std::string context = "material " + material_name;
    Result<bool> primitive_specific =
        material_uses_unavailable_texcoord(*material, available_texcoords, context);
    if (!primitive_specific) {
        return primitive_specific.error();
    }
    bool primitive_specific_fallback = primitive_specific.value();
    if (materials[index].has_value() && !primitive_specific_fallback) {
        return materials[index].value();
    }

    ModelMaterialDescription description;
    if (material->has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr = material->pbr_metallic_roughness;
        description.base_color = Color4{pbr.base_color_factor[0], pbr.base_color_factor[1],
                                        pbr.base_color_factor[2], pbr.base_color_factor[3]};
        description.metallic_factor = pbr.metallic_factor;
        description.roughness_factor = pbr.roughness_factor;
        if (pbr.base_color_texture.texture != nullptr) {
            Result<ImportedTextureView> texture = import_texture_view(
                data, pbr.base_color_texture, "Base-color", gltf_path, builder, images, textures,
                samplers, default_sampler, available_texcoords, primitive_specific_fallback,
                diagnostics, image_budget, context + " base-color texture");
            if (!texture) {
                return texture.error();
            }
            description.base_color_texture = texture.value().texture;
            description.base_color_texture_mapping = texture.value().mapping;
        }
        if (pbr.metallic_roughness_texture.texture != nullptr) {
            Result<ImportedTextureView> texture =
                import_texture_view(data, pbr.metallic_roughness_texture, "Metallic-roughness",
                                    gltf_path, builder, images, textures, samplers, default_sampler,
                                    available_texcoords, primitive_specific_fallback, diagnostics,
                                    image_budget, context + " metallic-roughness texture");
            if (!texture) {
                return texture.error();
            }
            description.metallic_roughness_texture = texture.value().texture;
            description.metallic_roughness_texture_mapping = texture.value().mapping;
        }
    } else if (material->has_pbr_specular_glossiness) {
        const cgltf_pbr_specular_glossiness& pbr = material->pbr_specular_glossiness;
        description.base_color = Color4{pbr.diffuse_factor[0], pbr.diffuse_factor[1],
                                        pbr.diffuse_factor[2], pbr.diffuse_factor[3]};
        description.metallic_factor = 0.0F;
        description.roughness_factor = 1.0F - pbr.glossiness_factor;
        if (pbr.diffuse_texture.texture != nullptr) {
            Result<ImportedTextureView> texture =
                import_texture_view(data, pbr.diffuse_texture, "Specular-glossiness diffuse",
                                    gltf_path, builder, images, textures, samplers, default_sampler,
                                    available_texcoords, primitive_specific_fallback, diagnostics,
                                    image_budget, context + " specular-glossiness diffuse texture");
            if (!texture) {
                return texture.error();
            }
            description.base_color_texture = texture.value().texture;
            description.base_color_texture_mapping = texture.value().mapping;
        }
        if (pbr.specular_glossiness_texture.texture != nullptr) {
            add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::material,
                           ModelLoadDiagnosticCode::material_fallback,
                           "KHR_materials_pbrSpecularGlossiness diffuse data was approximated, "
                           "but its specular-glossiness texture is ignored",
                           context);
        }
    }
    description.emissive_factor = {material->emissive_factor[0], material->emissive_factor[1],
                                   material->emissive_factor[2]};
    if (material->has_emissive_strength) {
        description.emissive_strength = material->emissive_strength.emissive_strength;
    }
    description.unlit = material->unlit != 0;
    description.ior = material->has_ior ? material->ior.ior : 1.5F;
    if (material->has_specular) {
        description.specular_factor = material->specular.specular_factor;
        description.specular_color_factor = {material->specular.specular_color_factor[0],
                                             material->specular.specular_color_factor[1],
                                             material->specular.specular_color_factor[2]};
        if (material->specular.specular_texture.texture != nullptr ||
            material->specular.specular_color_texture.texture != nullptr) {
            add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::material,
                           ModelLoadDiagnosticCode::material_fallback,
                           "KHR_materials_specular factors are rendered, but its optional "
                           "textures are ignored",
                           context);
        }
    }
    if (material->normal_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_texture_view(
            data, material->normal_texture, "Normal", gltf_path, builder, images, textures,
            samplers, default_sampler, available_texcoords, primitive_specific_fallback,
            diagnostics, image_budget, context + " normal texture");
        if (!texture) {
            return texture.error();
        }
        description.normal_texture = texture.value().texture;
        description.normal_texture_mapping = texture.value().mapping;
        description.normal_scale = material->normal_texture.scale;
        if (description.normal_texture.is_valid()) {
            add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::material,
                           ModelLoadDiagnosticCode::normal_map_fallback,
                           "Normal texture was imported and preserved but is not rendered because "
                           "Elf3D does not yet have a complete tangent-space path",
                           context);
        }
    }
    if (material->occlusion_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_texture_view(
            data, material->occlusion_texture, "Occlusion", gltf_path, builder, images, textures,
            samplers, default_sampler, available_texcoords, primitive_specific_fallback,
            diagnostics, image_budget, context + " occlusion texture");
        if (!texture) {
            return texture.error();
        }
        description.occlusion_texture = texture.value().texture;
        description.occlusion_texture_mapping = texture.value().mapping;
        description.occlusion_strength = material->occlusion_texture.scale;
    }
    if (material->emissive_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_texture_view(
            data, material->emissive_texture, "Emissive", gltf_path, builder, images, textures,
            samplers, default_sampler, available_texcoords, primitive_specific_fallback,
            diagnostics, image_budget, context + " emissive texture");
        if (!texture) {
            return texture.error();
        }
        description.emissive_texture = texture.value().texture;
        description.emissive_texture_mapping = texture.value().mapping;
    }
    switch (material->alpha_mode) {
    case cgltf_alpha_mode_opaque:
        description.alpha_mode = ModelAlphaMode::opaque;
        break;
    case cgltf_alpha_mode_mask:
        description.alpha_mode = ModelAlphaMode::mask;
        break;
    case cgltf_alpha_mode_blend:
        description.alpha_mode = ModelAlphaMode::blend;
        break;
    default:
        return Error{ErrorCode::scene_import_failed, context + " contains an invalid alpha mode"};
    }
    description.alpha_cutoff = material->alpha_cutoff;
    if (!std::isfinite(description.base_color.red) ||
        !std::isfinite(description.base_color.green) ||
        !std::isfinite(description.base_color.blue) ||
        !std::isfinite(description.base_color.alpha) ||
        !std::isfinite(description.metallic_factor) ||
        !std::isfinite(description.roughness_factor) ||
        !math::is_finite(description.emissive_factor) || !std::isfinite(description.normal_scale) ||
        !std::isfinite(description.occlusion_strength) || !std::isfinite(description.ior) ||
        !std::isfinite(description.specular_factor) ||
        !math::is_finite(description.specular_color_factor) ||
        !std::isfinite(description.alpha_cutoff)) {
        return Error{ErrorCode::scene_import_failed,
                     context + " contains a non-finite material factor"};
    }

    description.double_sided = material->double_sided != 0;
    const Result<MaterialId> created = builder.create_material(description);
    if (!created) {
        return created.error();
    }
    if (!primitive_specific_fallback) {
        materials[index] = created.value();
    }
    material_ids[index].push_back(created.value());
    return created.value();
}

[[nodiscard]] Result<ImportedMesh>
import_mesh(const cgltf_data& data, const cgltf_mesh& mesh, cgltf_size mesh_index,
            const std::filesystem::path& gltf_path, const ModelLoadOptions& options,
            DocumentBuilder& builder, std::vector<std::optional<MaterialId>>& materials,
            std::vector<std::vector<MaterialId>>& material_ids,
            std::vector<std::optional<ImageId>>& images,
            std::vector<std::optional<TextureId>>& textures,
            std::vector<std::optional<SamplerId>>& samplers,
            std::optional<SamplerId>& default_sampler, std::optional<MaterialId>& default_material,
            std::vector<ModelLoadDiagnostic>& diagnostics, ImageImportBudget& image_budget) {
    try {
        ImportedMesh result;
        result.primitives.resize(mesh.primitives_count);
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            const std::string context = mesh_context(mesh, mesh_index, primitive_index);
            if (!supported_primitive_type(primitive.type)) {
                add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::geometry,
                               ModelLoadDiagnosticCode::skipped_unsupported_primitive,
                               context +
                                   " uses unsupported points or lines geometry and was skipped",
                               context);
                continue;
            }

            const cgltf_accessor* position_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            if (position_accessor == nullptr) {
                return Error{ErrorCode::missing_position_accessor,
                             context + " does not contain POSITION"};
            }
            Result<std::vector<float>> positions = unpack_float3(
                *position_accessor, ErrorCode::missing_position_accessor, context + " POSITION");
            if (!positions) {
                return positions.error();
            }
            const std::size_t vertex_count = positions.value().size() / 3;

            Result<std::vector<std::uint32_t>> indices =
                import_indices(primitive, vertex_count, context);
            if (!indices) {
                return indices.error();
            }
            const TexcoordAvailability available_texcoords =
                primitive_texcoord_availability(primitive);

            PrimitiveData primitive_data;
            primitive_data.positions.resize(vertex_count);
            for (std::size_t index = 0; index < vertex_count; ++index) {
                const Float3 position{positions.value()[index * 3],
                                      positions.value()[index * 3 + 1],
                                      positions.value()[index * 3 + 2]};
                if (!math::is_finite(position)) {
                    return Error{ErrorCode::non_finite_position,
                                 context + " contains a non-finite POSITION value"};
                }
                primitive_data.positions[index] = position;
            }

            for (cgltf_int set = 0;
                 set < static_cast<cgltf_int>(model_maximum_texture_coordinate_sets); ++set) {
                const cgltf_accessor* texcoord_accessor =
                    cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, set);
                if (texcoord_accessor == nullptr) {
                    continue;
                }
                const std::string semantic = "TEXCOORD_" + std::to_string(set);
                if (texcoord_accessor->count != position_accessor->count) {
                    return Error{ErrorCode::mismatched_texcoord_count,
                                 context + " " + semantic + " count does not match POSITION"};
                }
                Result<std::vector<float>> texcoords =
                    unpack_float2(*texcoord_accessor, context + " " + semantic);
                if (!texcoords) {
                    return texcoords.error();
                }
                std::vector<Float2>& target =
                    set == 0 ? primitive_data.texcoord0 : primitive_data.texcoord1;
                target.resize(vertex_count);
                for (std::size_t index = 0; index < vertex_count; ++index) {
                    const Float2 texcoord{texcoords.value()[index * 2],
                                          texcoords.value()[index * 2 + 1]};
                    if (!std::isfinite(texcoord.x) || !std::isfinite(texcoord.y)) {
                        return Error{ErrorCode::invalid_texcoord,
                                     context + " contains a non-finite " + semantic + " value"};
                    }
                    target[index] = texcoord;
                }
            }

            for (cgltf_size attribute_index = 0; attribute_index < primitive.attributes_count;
                 ++attribute_index) {
                const cgltf_attribute& attribute = primitive.attributes[attribute_index];
                if (attribute.type == cgltf_attribute_type_texcoord &&
                    attribute.index >=
                        static_cast<cgltf_int>(model_maximum_texture_coordinate_sets)) {
                    add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::geometry,
                                   ModelLoadDiagnosticCode::material_fallback,
                                   "Additional UV sets beyond TEXCOORD_1 are not preserved unless "
                                   "referenced, and referenced sets beyond 1 are rejected",
                                   context);
                    break;
                }
            }

            const cgltf_accessor* color_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0);
            if (color_accessor != nullptr) {
                if (color_accessor->count != position_accessor->count) {
                    return Error{ErrorCode::invalid_accessor,
                                 context + " COLOR_0 count does not match POSITION"};
                }
                Result<std::vector<float>> colors =
                    unpack_color(*color_accessor, context + " COLOR_0");
                if (!colors) {
                    return colors.error();
                }
                const std::size_t components = color_accessor->type == cgltf_type_vec3 ? 3U : 4U;
                primitive_data.colors.resize(vertex_count);
                for (std::size_t index = 0; index < vertex_count; ++index) {
                    Color4 color{colors.value()[index * components],
                                 colors.value()[index * components + 1],
                                 colors.value()[index * components + 2],
                                 components == 4U ? colors.value()[index * components + 3] : 1.0F};
                    if (!std::isfinite(color.red) || !std::isfinite(color.green) ||
                        !std::isfinite(color.blue) || !std::isfinite(color.alpha)) {
                        return Error{ErrorCode::invalid_accessor,
                                     context + " contains a non-finite COLOR_0 value"};
                    }
                    primitive_data.colors[index] = color;
                }
            }

            const cgltf_accessor* normal_accessor =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
            bool generate_normal_fallback = normal_accessor == nullptr;
            bool ignored_authored_normals = false;
            if (normal_accessor != nullptr) {
                if (normal_accessor->count != position_accessor->count) {
                    return Error{ErrorCode::mismatched_normal_count,
                                 context + " NORMAL count does not match POSITION"};
                }
                Result<std::vector<float>> normals = unpack_float3(
                    *normal_accessor, ErrorCode::invalid_accessor, context + " NORMAL");
                if (!normals) {
                    return normals.error();
                }
                primitive_data.normals.resize(vertex_count);
                for (std::size_t index = 0; index < vertex_count; ++index) {
                    Float3 normal{normals.value()[index * 3], normals.value()[index * 3 + 1],
                                  normals.value()[index * 3 + 2]};
                    const float length_squared =
                        normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
                    if (!math::is_finite(normal) || !std::isfinite(length_squared) ||
                        length_squared <= 0.000000000001F) {
                        if (!options.generate_missing_normals) {
                            return Error{ErrorCode::invalid_accessor,
                                         context + " contains an unusable NORMAL value"};
                        }
                        generate_normal_fallback = true;
                        ignored_authored_normals = true;
                        break;
                    }
                    const float inverse_length = 1.0F / std::sqrt(length_squared);
                    primitive_data.normals[index] =
                        Float3{normal.x * inverse_length, normal.y * inverse_length,
                               normal.z * inverse_length};
                }
            }
            if (generate_normal_fallback) {
                if (!options.generate_missing_normals) {
                    return Error{ErrorCode::missing_normals,
                                 context + " has no NORMAL accessor and generation is disabled"};
                }
                std::uint64_t degenerate_count = 0;
                std::uint64_t fallback_count = 0;
                generate_normals(primitive_data.positions, primitive_data.normals, indices.value(),
                                 degenerate_count, fallback_count);
                const std::string prefix = ignored_authored_normals
                                               ? "Ignored unusable authored normals and generated "
                                                 "replacements"
                                               : "Generated missing vertex normals";
                if (degenerate_count != 0 || fallback_count != 0) {
                    add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::geometry,
                                   ModelLoadDiagnosticCode::degenerate_geometry,
                                   prefix + "; ignored " + std::to_string(degenerate_count) +
                                       " degenerate triangles and used +Y fallback normals for " +
                                       std::to_string(fallback_count) + " vertices",
                                   context);
                } else {
                    add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::geometry,
                                   ModelLoadDiagnosticCode::generated_normals, prefix, context);
                }
            }
            primitive_data.indices = std::move(indices).value();

            if (!result.mesh.has_value()) {
                const Result<MeshId> created_mesh = builder.create_mesh(
                    mesh.name != nullptr ? std::string_view{mesh.name} : std::string_view{});
                if (!created_mesh) {
                    return created_mesh.error();
                }
                result.mesh = created_mesh.value();
            }
            const Result<MaterialId> material_result =
                material_for(data, primitive.material, gltf_path, builder, materials, material_ids,
                             images, textures, samplers, default_sampler, available_texcoords,
                             default_material, diagnostics, image_budget);
            if (!material_result) {
                return material_result.error();
            }
            const Result<PrimitiveId> primitive_result = builder.create_primitive(
                *result.mesh, material_result.value(), std::move(primitive_data));
            if (!primitive_result) {
                return primitive_result.error();
            }
            result.primitives[primitive_index] = primitive_result.value();
            ++result.primitive_count;
        }
        return result;
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] bool has_preserved_values(const ModelJsonMetadata& metadata) noexcept {
    return metadata.extras_json.has_value() || !metadata.extensions.empty();
}

void diagnose_unsupported_scope_metadata(const cgltf_data& data,
                                         std::vector<ModelLoadDiagnostic>& diagnostics) {
    bool found = false;
    for (cgltf_size index = 0; index < data.buffers_count; ++index) {
        found = found || source_has_metadata(data.buffers[index].extras,
                                             data.buffers[index].extensions_count);
    }
    for (cgltf_size index = 0; index < data.buffer_views_count; ++index) {
        found = found || source_has_metadata(data.buffer_views[index].extras,
                                             data.buffer_views[index].extensions_count);
    }
    for (cgltf_size index = 0; index < data.accessors_count; ++index) {
        found = found || source_has_metadata(data.accessors[index].extras,
                                             data.accessors[index].extensions_count);
    }
    for (cgltf_size index = 0; index < data.cameras_count; ++index) {
        const cgltf_camera& camera = data.cameras[index];
        found = found || source_has_metadata(camera.extras, camera.extensions_count);
        found = found || (camera.type == cgltf_camera_type_perspective
                              ? source_has_metadata(camera.data.perspective.extras, 0)
                              : source_has_metadata(camera.data.orthographic.extras, 0));
    }
    for (cgltf_size index = 0; index < data.skins_count; ++index) {
        found = found ||
                source_has_metadata(data.skins[index].extras, data.skins[index].extensions_count);
    }
    for (cgltf_size index = 0; index < data.animations_count; ++index) {
        const cgltf_animation& animation = data.animations[index];
        found = found || source_has_metadata(animation.extras, animation.extensions_count);
        for (cgltf_size sampler = 0; sampler < animation.samplers_count; ++sampler) {
            found = found || source_has_metadata(animation.samplers[sampler].extras,
                                                 animation.samplers[sampler].extensions_count);
        }
        for (cgltf_size channel = 0; channel < animation.channels_count; ++channel) {
            found = found || source_has_metadata(animation.channels[channel].extras,
                                                 animation.channels[channel].extensions_count);
        }
    }
    for (cgltf_size index = 0; index < data.lights_count; ++index) {
        found = found || source_has_metadata(data.lights[index].extras, 0);
    }
    for (cgltf_size index = 0; index < data.variants_count; ++index) {
        found = found || source_has_metadata(data.variants[index].extras, 0);
    }
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            for (cgltf_size mapping = 0; mapping < primitive.mappings_count; ++mapping) {
                found = found || source_has_metadata(primitive.mappings[mapping].extras, 0);
            }
        }
    }
    if (found) {
        add_metadata_not_preserved(
            diagnostics, "glTF auxiliary scopes",
            "buffers, buffer views, accessors, cameras, skins, and animations have no stable "
            "one-to-one Document metadata identity");
    }
}

[[nodiscard]] Result<void> attach_imported_metadata(const cgltf_data& data,
                                                    const ImportedDocumentIds& ids,
                                                    Document& document,
                                                    std::vector<ModelLoadDiagnostic>& diagnostics) {
    MetadataBudget budget;
    model::detail::ImportedDocumentMetadata imported;
    imported.root = copy_metadata(data.extras, data.data_extensions, data.data_extensions_count,
                                  budget, diagnostics, "document root");
    imported.asset = copy_metadata(data.asset.extras, data.asset.extensions,
                                   data.asset.extensions_count, budget, diagnostics, "asset");

    for (cgltf_size index = 0; index < data.scenes_count; ++index) {
        const cgltf_scene& source = data.scenes[index];
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, source.extensions, source.extensions_count, budget,
                          diagnostics, "scene " + std::to_string(index));
        if (has_preserved_values(metadata)) {
            imported.scenes.emplace_back(ids.scenes[index], std::move(metadata));
        }
    }
    for (cgltf_size index = 0; index < data.nodes_count; ++index) {
        const cgltf_node& source = data.nodes[index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        if (!ids.nodes[index].has_value()) {
            add_metadata_not_preserved(diagnostics, "node " + std::to_string(index),
                                       "the node was not imported");
            continue;
        }
        ModelJsonMetadata metadata =
            copy_metadata(source.extras, source.extensions, source.extensions_count, budget,
                          diagnostics, "node " + std::to_string(index));
        if (has_preserved_values(metadata)) {
            imported.nodes.emplace_back(*ids.nodes[index], std::move(metadata));
        }
    }
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& source_mesh = data.meshes[mesh_index];
        if (source_has_metadata(source_mesh.extras, source_mesh.extensions_count)) {
            if (!ids.meshes[mesh_index].has_value()) {
                add_metadata_not_preserved(diagnostics, "mesh " + std::to_string(mesh_index),
                                           "the mesh was not imported");
            } else {
                ModelJsonMetadata metadata = copy_metadata(
                    data, source_mesh.extras, source_mesh.extensions, source_mesh.extensions_count,
                    budget, diagnostics, "mesh " + std::to_string(mesh_index));
                if (has_preserved_values(metadata)) {
                    imported.meshes.emplace_back(*ids.meshes[mesh_index], std::move(metadata));
                }
            }
        }
        for (cgltf_size primitive_index = 0; primitive_index < source_mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& source = source_mesh.primitives[primitive_index];
            if (!source_has_metadata(source.extras, source.extensions_count)) {
                continue;
            }
            std::optional<PrimitiveId> id;
            if (mesh_index < ids.primitives.size() &&
                primitive_index < ids.primitives[mesh_index].size()) {
                id = ids.primitives[mesh_index][primitive_index];
            }
            const std::string context = "mesh " + std::to_string(mesh_index) + ", primitive " +
                                        std::to_string(primitive_index);
            if (!id.has_value()) {
                add_metadata_not_preserved(diagnostics, context, "the primitive was not imported");
                continue;
            }
            ModelJsonMetadata metadata =
                copy_metadata(source.extras, source.extensions, source.extensions_count, budget,
                              diagnostics, context);
            if (has_preserved_values(metadata)) {
                imported.primitives.emplace_back(*id, std::move(metadata));
            }
        }
    }
    for (cgltf_size index = 0; index < data.materials_count; ++index) {
        const cgltf_material& source = data.materials[index];
        if (!source_has_metadata(source.extras, source.extensions_count)) {
            continue;
        }
        if (ids.materials[index].empty()) {
            add_metadata_not_preserved(diagnostics, "material " + std::to_string(index),
                                       "the material was not imported");
            continue;
        }
        for (const MaterialId id : ids.materials[index]) {
            ModelJsonMetadata metadata =
                copy_metadata(source.extras, source.extensions, source.extensions_count, budget,
                              diagnostics, "material " + std::to_string(index));
            if (has_preserved_values(metadata)) {
                imported.materials.emplace_back(id, std::move(metadata));
            }
        }
    }
    const auto copy_indexed = [&](const auto& sources, cgltf_size source_count,
                                  const auto& ids_by_index, auto& destination,
                                  std::string_view label) {
        for (cgltf_size index = 0; index < source_count; ++index) {
            const auto& source = sources[index];
            if (!source_has_metadata(source.extras, source.extensions_count)) {
                continue;
            }
            const std::string context = std::string{label} + " " + std::to_string(index);
            if (!ids_by_index[index].has_value()) {
                add_metadata_not_preserved(diagnostics, context, "the object was not imported");
                continue;
            }
            ModelJsonMetadata metadata =
                copy_metadata(source.extras, source.extensions, source.extensions_count, budget,
                              diagnostics, context);
            if (has_preserved_values(metadata)) {
                destination.emplace_back(*ids_by_index[index], std::move(metadata));
            }
        }
    };
    copy_indexed(data.images, data.images_count, ids.images, imported.images, "image");
    copy_indexed(data.textures, data.textures_count, ids.textures, imported.textures, "texture");
    copy_indexed(data.samplers, data.samplers_count, ids.samplers, imported.samplers, "sampler");
    diagnose_unsupported_scope_metadata(data, diagnostics);
    return model::detail::DocumentMetadataAccess::attach_import_metadata(document,
                                                                         std::move(imported));
}

[[nodiscard]] Result<ConstructedDocument>
construct_document(const cgltf_data& data, const std::vector<bool>& reachable,
                   const std::filesystem::path& gltf_path, const ModelLoadOptions& options,
                   std::vector<ModelLoadDiagnostic>& diagnostics) {
    try {
        DocumentBuilder builder;
        ImportedDocumentIds imported_ids;
        imported_ids.scenes.reserve(data.scenes_count != 0 ? data.scenes_count : 1U);
        if (data.scenes_count != 0) {
            for (cgltf_size scene_index = 0; scene_index < data.scenes_count; ++scene_index) {
                const cgltf_scene& source_scene = data.scenes[scene_index];
                const Result<DocumentSceneId> scene_result = builder.create_scene(
                    source_scene.name != nullptr ? std::string_view{source_scene.name}
                                                 : std::string_view{});
                if (!scene_result) {
                    return scene_result.error();
                }
                imported_ids.scenes.push_back(scene_result.value());
            }
        } else {
            const Result<DocumentSceneId> scene_result = builder.create_scene();
            if (!scene_result) {
                return scene_result.error();
            }
            imported_ids.scenes.push_back(scene_result.value());
        }
        std::size_t default_scene_index = 0;
        if (data.scene != nullptr) {
            if (data.scenes_count == 0 || data.scene < data.scenes ||
                data.scene >= data.scenes + data.scenes_count) {
                return Error{ErrorCode::invalid_node_hierarchy,
                             "The glTF default scene is outside the scene table"};
            }
            default_scene_index = static_cast<std::size_t>(data.scene - data.scenes);
            const Result<void> default_result =
                builder.set_default_scene(imported_ids.scenes[default_scene_index]);
            if (!default_result) {
                return default_result.error();
            }
        } else {
            const Result<void> clear_result = builder.clear_default_scene();
            if (!clear_result) {
                return clear_result.error();
            }
        }
        const DocumentSceneId effective_default_scene = imported_ids.scenes[default_scene_index];

        std::vector<bool> skipped_nodes(data.nodes_count, false);
        std::vector<Float4x4> local_matrices(data.nodes_count);
        for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
            if (!reachable[node_index]) {
                continue;
            }
            const cgltf_node& node = data.nodes[node_index];
            float local_values[16]{};
            cgltf_node_transform_local(&node, local_values);
            std::copy_n(local_values, local_matrices[node_index].elements.size(),
                        local_matrices[node_index].elements.begin());
            if (!math::is_valid_affine_matrix(local_matrices[node_index])) {
                skipped_nodes[node_index] = true;
                const std::string context = node_context(node, node_index);
                add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::scene,
                               ModelLoadDiagnosticCode::skipped_invalid_transform,
                               context +
                                   " has a non-finite, non-affine, or non-invertible transform; "
                                   "the node and descendants were skipped",
                               context);
            }
        }

        std::vector<cgltf_size> skip_stack;
        skip_stack.reserve(data.nodes_count);
        for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
            if (reachable[node_index] && skipped_nodes[node_index]) {
                skip_stack.push_back(node_index);
            }
        }
        while (!skip_stack.empty()) {
            const cgltf_size parent_index = skip_stack.back();
            skip_stack.pop_back();
            const cgltf_node& parent = data.nodes[parent_index];
            for (cgltf_size child_offset = 0; child_offset < parent.children_count;
                 ++child_offset) {
                const cgltf_node* child = parent.children[child_offset];
                if (child == nullptr || child < data.nodes ||
                    child >= data.nodes + data.nodes_count) {
                    return Error{ErrorCode::scene_import_failed,
                                 "A glTF node references a child outside the node table"};
                }
                const cgltf_size child_index = static_cast<cgltf_size>(child - data.nodes);
                if (!reachable[child_index] || skipped_nodes[child_index]) {
                    continue;
                }
                skipped_nodes[child_index] = true;
                skip_stack.push_back(child_index);
                const std::string context = node_context(*child, child_index);
                add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::scene,
                               ModelLoadDiagnosticCode::skipped_invalid_transform,
                               context + " was skipped because an ancestor transform was skipped",
                               context);
            }
        }

        std::vector<bool> used_meshes(data.meshes_count, false);
        for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
            if (reachable[node_index] && !skipped_nodes[node_index] &&
                data.nodes[node_index].mesh != nullptr) {
                const std::size_t mesh_index =
                    static_cast<std::size_t>(data.nodes[node_index].mesh - data.meshes);
                if (mesh_index >= used_meshes.size()) {
                    return Error{ErrorCode::scene_import_failed,
                                 "A glTF node references a mesh outside the mesh table"};
                }
                used_meshes[mesh_index] = true;
            }
        }

        imported_ids.material_cache.resize(data.materials_count);
        imported_ids.materials.resize(data.materials_count);
        imported_ids.images.resize(data.images_count);
        imported_ids.textures.resize(data.textures_count);
        imported_ids.samplers.resize(data.samplers_count);
        std::optional<SamplerId> default_sampler;
        std::optional<MaterialId> default_material;
        ImageImportBudget image_budget;
        imported_ids.meshes.resize(data.meshes_count);
        imported_ids.primitives.resize(data.meshes_count);
        std::uint64_t imported_primitives = 0;
        for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
            if (!used_meshes[mesh_index]) {
                continue;
            }
            Result<ImportedMesh> imported =
                import_mesh(data, data.meshes[mesh_index], mesh_index, gltf_path, options, builder,
                            imported_ids.material_cache, imported_ids.materials,
                            imported_ids.images, imported_ids.textures, imported_ids.samplers,
                            default_sampler, default_material, diagnostics, image_budget);
            if (!imported) {
                return imported.error();
            }
            imported_primitives += imported.value().primitive_count;
            imported_ids.meshes[mesh_index] = imported.value().mesh;
            imported_ids.primitives[mesh_index] = std::move(imported).value().primitives;
        }
        if (imported_primitives == 0) {
            return Error{ErrorCode::empty_scene_geometry,
                         "The glTF document contains no supported triangle geometry"};
        }

        imported_ids.nodes.resize(data.nodes_count);
        for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
            if (!reachable[node_index] || skipped_nodes[node_index]) {
                continue;
            }
            const cgltf_node& node = data.nodes[node_index];
            const Result<NodeId> created_node = builder.create_node(
                options.import_node_names && node.name != nullptr ? std::string_view{node.name}
                                                                  : std::string_view{});
            if (!created_node) {
                return created_node.error();
            }
            imported_ids.nodes[node_index] = created_node.value();

            const Result<void> matrix_result =
                builder.set_node_matrix(created_node.value(), local_matrices[node_index]);
            if (!matrix_result) {
                const std::string node_name =
                    node.name != nullptr ? node.name : std::to_string(node_index);
                return Error{matrix_result.error().code(),
                             "Node " + node_name + ": " + matrix_result.error().message()};
            }

            if (node.camera != nullptr) {
                const std::string node_label =
                    node.name != nullptr ? std::string{node.name} : std::to_string(node_index);
                const std::string node_context = "node " + node_label;
                if (node.camera->type == cgltf_camera_type_perspective) {
                    ModelPerspectiveCameraDescription camera;
                    camera.vertical_field_of_view_radians = node.camera->data.perspective.yfov;
                    camera.near_plane = node.camera->data.perspective.znear;
                    camera.far_plane = node.camera->data.perspective.has_zfar
                                           ? node.camera->data.perspective.zfar
                                           : 1.0e9F;
                    const Result<void> camera_result =
                        builder.set_node_perspective_camera(created_node.value(), camera);
                    if (!camera_result) {
                        return Error{camera_result.error().code(),
                                     node_context + ": " + camera_result.error().message()};
                    }
                    if (!node.camera->data.perspective.has_zfar) {
                        add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::camera,
                                       ModelLoadDiagnosticCode::camera_fallback,
                                       "Infinite-far perspective camera was bounded to 1e9 world "
                                       "units",
                                       node_context);
                    }
                    if (node.camera->data.perspective.has_aspect_ratio) {
                        add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::camera,
                                       ModelLoadDiagnosticCode::camera_fallback,
                                       "Authored camera aspect ratio is not stored; the active "
                                       "viewport aspect ratio is used",
                                       node_context);
                    }
                } else if (node.camera->type == cgltf_camera_type_orthographic) {
                    add_diagnostic(diagnostics, ModelLoadDiagnosticCategory::camera,
                                   ModelLoadDiagnosticCode::camera_fallback,
                                   "Orthographic camera is not representable and was left as a "
                                   "transform-only entity",
                                   node_context);
                }
            }
        }

        for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
            if (!reachable[node_index] || skipped_nodes[node_index]) {
                continue;
            }
            const cgltf_node& node = data.nodes[node_index];
            if (node.parent != nullptr) {
                const std::size_t parent_index = static_cast<std::size_t>(node.parent - data.nodes);
                if (parent_index >= reachable.size() || !reachable[parent_index] ||
                    !imported_ids.nodes[parent_index].has_value()) {
                    return Error{ErrorCode::invalid_node_hierarchy,
                                 "A glTF node has a parent outside the imported scenes"};
                }
                const Result<void> parent_result =
                    builder.set_parent(imported_ids.nodes[node_index].value(),
                                       imported_ids.nodes[parent_index].value());
                if (!parent_result) {
                    return parent_result.error();
                }
            }

            if (node.mesh != nullptr) {
                const std::size_t mesh_index = static_cast<std::size_t>(node.mesh - data.meshes);
                if (mesh_index >= imported_ids.meshes.size()) {
                    return Error{ErrorCode::scene_import_failed,
                                 "A glTF node references a mesh outside the mesh table"};
                }
                if (imported_ids.meshes[mesh_index].has_value()) {
                    const Result<void> model_result = builder.set_node_mesh(
                        imported_ids.nodes[node_index].value(), *imported_ids.meshes[mesh_index]);
                    if (!model_result) {
                        return model_result.error();
                    }
                }
            }
        }

        const auto add_root = [&](DocumentSceneId scene_id,
                                  const cgltf_node* root) -> Result<void> {
            if (root == nullptr || root < data.nodes || root >= data.nodes + data.nodes_count) {
                return Error{ErrorCode::invalid_node_hierarchy,
                             "A glTF scene contains an invalid root node"};
            }
            const std::size_t root_index = static_cast<std::size_t>(root - data.nodes);
            if (!reachable[root_index] || skipped_nodes[root_index] ||
                !imported_ids.nodes[root_index].has_value()) {
                return {};
            }
            return builder.add_scene_root(scene_id, *imported_ids.nodes[root_index]);
        };

        if (data.scenes_count != 0) {
            for (cgltf_size scene_index = 0; scene_index < data.scenes_count; ++scene_index) {
                const cgltf_scene& source_scene = data.scenes[scene_index];
                for (cgltf_size root_index = 0; root_index < source_scene.nodes_count;
                     ++root_index) {
                    const Result<void> root_result =
                        add_root(imported_ids.scenes[scene_index], source_scene.nodes[root_index]);
                    if (!root_result) {
                        return root_result.error();
                    }
                }
            }
        } else {
            for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
                if (reachable[node_index] && !skipped_nodes[node_index] &&
                    data.nodes[node_index].parent == nullptr &&
                    imported_ids.nodes[node_index].has_value()) {
                    const Result<void> root_result = builder.add_scene_root(
                        imported_ids.scenes[0], *imported_ids.nodes[node_index]);
                    if (!root_result) {
                        return root_result.error();
                    }
                }
            }
        }

        Result<Document> document = builder.finish();
        if (!document) {
            return document.error();
        }
        Document value = std::move(document).value();
        const Result<void> metadata_result =
            attach_imported_metadata(data, imported_ids, value, diagnostics);
        if (!metadata_result) {
            return metadata_result.error();
        }
        return ConstructedDocument{std::move(value), effective_default_scene};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace

Result<LoadedDocument> load_document(const std::filesystem::path& path,
                                     const ModelLoadOptions& options) noexcept {
    try {
        const std::string extension = lower_extension(path);
        if (extension != ".gltf" && extension != ".glb") {
            return Error{ErrorCode::unsupported_scene_format,
                         "Scene loading currently supports only .gltf and .glb files"};
        }
        const bool is_glb = extension == ".glb";

        Result<std::vector<std::byte>> source_result = read_source(path);
        if (!source_result) {
            return source_result.error();
        }
        std::vector<std::byte> source = std::move(source_result).value();

        AllocationContext allocation_context;
        BufferLoadContext buffer_context;
        cgltf_options cgltf_options_value{};
        cgltf_options_value.memory.alloc_func = bounded_allocate;
        cgltf_options_value.memory.free_func = bounded_deallocate;
        cgltf_options_value.memory.user_data = &allocation_context;
        cgltf_options_value.file.read = read_external_file;
        cgltf_options_value.file.release = release_external_file;
        cgltf_options_value.file.user_data = &buffer_context;

        cgltf_data* parsed = nullptr;
        const cgltf_result parse_result =
            cgltf_parse(&cgltf_options_value, source.data(), source.size(), &parsed);
        if (parse_result != cgltf_result_success) {
            return parse_error(parse_result, is_glb);
        }
        CgltfData data{parsed};

        if ((is_glb && data->file_type != cgltf_file_type_glb) ||
            (!is_glb && data->file_type != cgltf_file_type_gltf)) {
            return Error{is_glb ? ErrorCode::malformed_glb : ErrorCode::malformed_gltf,
                         "The source contents do not match the file extension"};
        }

        decode_image_json_strings(*data);

        const Result<void> texture_input_result = validate_texture_inputs(*data);
        if (!texture_input_result) {
            return texture_input_result.error();
        }
        const cgltf_result initial_validation = cgltf_validate(data.get());
        if (initial_validation != cgltf_result_success) {
            return Error{initial_validation == cgltf_result_data_too_short
                             ? ErrorCode::invalid_buffer_range
                             : ErrorCode::gltf_validation_failed,
                         initial_validation == cgltf_result_data_too_short
                             ? "A glTF buffer view or accessor exceeds its declared buffer range"
                             : "cgltf structural validation rejected the source"};
        }
        const Result<void> extension_result = validate_required_extensions(*data);
        if (!extension_result) {
            return extension_result.error();
        }
        const Result<void> limit_result = validate_resource_limits(*data);
        if (!limit_result) {
            return limit_result.error();
        }
        const Result<void> hierarchy_result = validate_node_hierarchy(*data);
        if (!hierarchy_result) {
            return hierarchy_result.error();
        }
        const Result<void> uri_result = validate_buffer_uris(*data);
        if (!uri_result) {
            return uri_result.error();
        }

        const std::string source_path_utf8 = path_to_utf8(path);
        const cgltf_result buffers_result =
            cgltf_load_buffers(&cgltf_options_value, data.get(), source_path_utf8.c_str());
        if (buffers_result != cgltf_result_success) {
            if (buffer_context.error_code.has_value()) {
                return Error{*buffer_context.error_code, buffer_context.diagnostic};
            }
            return Error{buffers_result == cgltf_result_file_not_found
                             ? ErrorCode::missing_external_buffer
                             : ErrorCode::invalid_buffer_range,
                         "cgltf could not load or decode a declared buffer"};
        }
        Result<std::vector<bool>> reachable = reachable_nodes(*data);
        if (!reachable) {
            return reachable.error();
        }

        ModelLoadReport report;
        for (cgltf_size index = 0; index < data->extensions_used_count; ++index) {
            const char* extension_name = data->extensions_used[index];
            if (extension_name != nullptr && !extension_has_full_support(extension_name)) {
                add_optional_extension_diagnostic(report.diagnostics, extension_name);
            }
        }
        if (data->animations_count != 0) {
            add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::animation,
                           ModelLoadDiagnosticCode::ignored_animation,
                           "Animation clips and channels are not imported; static node transforms "
                           "were used");
        }
        bool has_reachable_skin = false;
        bool has_reachable_morph_targets = false;
        bool has_reachable_instancing = false;
        for (cgltf_size node_index = 0; node_index < data->nodes_count; ++node_index) {
            if (!reachable.value()[node_index]) {
                continue;
            }
            const cgltf_node& node = data->nodes[node_index];
            has_reachable_skin = has_reachable_skin || node.skin != nullptr;
            has_reachable_instancing =
                has_reachable_instancing || node.has_mesh_gpu_instancing != 0;
            if (node.mesh != nullptr) {
                for (cgltf_size primitive_index = 0; primitive_index < node.mesh->primitives_count;
                     ++primitive_index) {
                    has_reachable_morph_targets =
                        has_reachable_morph_targets ||
                        node.mesh->primitives[primitive_index].targets_count != 0;
                }
            }
        }
        if (has_reachable_skin) {
            add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::animation,
                           ModelLoadDiagnosticCode::ignored_skin,
                           "Skinning is not imported; meshes use their undeformed bind geometry");
        }
        if (has_reachable_morph_targets) {
            add_diagnostic(
                report.diagnostics, ModelLoadDiagnosticCategory::animation,
                ModelLoadDiagnosticCode::ignored_morph_targets,
                "Morph targets and weights are not imported; base mesh geometry is used");
        }
        if (has_reachable_instancing) {
            add_diagnostic(report.diagnostics, ModelLoadDiagnosticCategory::geometry,
                           ModelLoadDiagnosticCode::ignored_instancing,
                           "EXT_mesh_gpu_instancing attributes are not expanded; the base node is "
                           "loaded once");
        }
        Result<ConstructedDocument> constructed =
            construct_document(*data, reachable.value(), path, options, report.diagnostics);
        if (!constructed) {
            return constructed.error();
        }
        ConstructedDocument value = std::move(constructed).value();
        return LoadedDocument{std::move(value.document), value.default_scene, std::move(report)};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf

namespace elf3d {

Result<LoadedDocument> load_document(std::string_view path_utf8,
                                     const ModelLoadOptions& options) noexcept {
    try {
        std::u8string utf8;
        utf8.reserve(path_utf8.size());
        for (const char character : path_utf8) {
            utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
        }
        return gltf::load_document(std::filesystem::path{utf8}, options);
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D glTF importer memory allocation failed");
    } catch (...) {
        fatal_error("Elf3D glTF importer encountered an unexpected exception");
    }
}

} // namespace elf3d
