#pragma once

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>
#include <elf3d/model/detail/document_builder.h>

#include <cgltf.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::gltf::importer_input {

struct BufferLoadContext {
    std::optional<ErrorCode> error_code;
    std::string diagnostic;
};

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value);
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
cgltf_result read_external_file(const cgltf_memory_options* memory,
                                const cgltf_file_options* file_options, const char* path,
                                cgltf_size* size, void** data);
void release_external_file(const cgltf_memory_options* memory,
                           const cgltf_file_options* file_options, void* data);

} // namespace elf3d::gltf::importer_input

namespace elf3d::gltf::importer_geometry {

[[nodiscard]] Result<std::vector<std::uint32_t>> import_indices(const cgltf_primitive& primitive,
                                                                std::size_t vertex_count,
                                                                std::string_view context);

} // namespace elf3d::gltf::importer_geometry

namespace elf3d::gltf::importer_metadata {

struct ImportedMetadataIds {
    std::span<const DocumentSceneId> scenes;
    std::span<const std::optional<NodeId>> nodes;
    std::span<const std::optional<MeshId>> meshes;
    std::span<const std::vector<std::optional<PrimitiveId>>> primitives;
    std::span<const std::vector<MaterialId>> materials;
    std::span<const std::optional<ImageId>> images;
    std::span<const std::optional<TextureId>> textures;
    std::span<const std::optional<SamplerId>> samplers;
};

[[nodiscard]] Result<void> attach_imported_metadata(const cgltf_data& data,
                                                    const ImportedMetadataIds& ids,
                                                    Document& document,
                                                    std::vector<ModelLoadDiagnostic>& diagnostics);

} // namespace elf3d::gltf::importer_metadata

namespace elf3d::gltf::importer_encoding {

[[nodiscard]] Result<std::vector<std::byte>> decode_base64(std::string_view payload);
[[nodiscard]] Result<std::string> percent_decode(std::string_view uri);

} // namespace elf3d::gltf::importer_encoding

namespace elf3d::gltf::importer_detail {

inline constexpr std::uint64_t maximum_total_encoded_image_bytes = 512ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t maximum_total_decoded_image_bytes = 512ULL * 1024ULL * 1024ULL;

using importer_geometry::import_indices;
using importer_input::BufferLoadContext;
using importer_input::path_from_utf8;
using importer_input::path_to_utf8;
using importer_input::read_external_file;
using importer_input::release_external_file;
using model::detail::DocumentBuilder;

struct CgltfDeleter {
    void operator()(cgltf_data* data) const noexcept {
        cgltf_free(data);
    }
};

using CgltfData = std::unique_ptr<cgltf_data, CgltfDeleter>;

struct AllocationContext {
    std::size_t live_bytes = 0;
};

struct EncodedImage {
    ModelImageMimeType mime = ModelImageMimeType::png;
    std::vector<std::byte> bytes;
};

struct ImageImportBudget {
    std::uint64_t encoded_bytes = 0;
    std::uint64_t decoded_bytes = 0;
};

struct ImageImportState {
    std::vector<std::optional<ImageId>> ids;
    ImageImportBudget budget;
};

struct ImportedDocumentIds {
    std::vector<DocumentSceneId> scenes;
    std::vector<std::optional<NodeId>> nodes;
    std::vector<std::optional<MeshId>> meshes;
    std::vector<std::vector<std::optional<PrimitiveId>>> primitives;
    std::vector<std::optional<MaterialId>> material_cache;
    std::vector<std::vector<MaterialId>> materials;
    ImageImportState images;
    std::vector<std::optional<TextureId>> textures;
    std::vector<std::optional<SamplerId>> samplers;
};

struct ImportState {
    const cgltf_data& data;
    const std::filesystem::path& gltf_path;
    const ModelLoadOptions& options;
    DocumentBuilder& builder;
    ImportedDocumentIds& ids;
    std::optional<SamplerId> default_sampler;
    std::optional<MaterialId> default_material;
    std::vector<ModelLoadDiagnostic>& diagnostics;
};

struct ImportedTextureView {
    TextureId texture;
    TextureMapping mapping;
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

using TexcoordAvailability = std::array<bool, maximum_texture_coordinate_sets>;

struct PrimitiveTextureState {
    const TexcoordAvailability& available_texcoords;
    bool& primitive_specific_fallback;
};

[[noreturn]] void fatal_gltf_allocation_failure() noexcept;
[[noreturn]] void fatal_unexpected_gltf_boundary_exception() noexcept;
void add_diagnostic(std::vector<ModelLoadDiagnostic>& diagnostics,
                    ModelLoadDiagnosticCategory category, ModelLoadDiagnosticCode code,
                    std::string message, std::optional<std::string> source_context = std::nullopt);
[[nodiscard]] bool extension_has_full_support(std::string_view extension) noexcept;
void add_optional_extension_diagnostic(std::vector<ModelLoadDiagnostic>& diagnostics,
                                       std::string_view extension);
[[nodiscard]] bool supported_primitive_type(cgltf_primitive_type type) noexcept;
[[nodiscard]] bool texture_failure_can_fallback(ErrorCode code) noexcept;
[[nodiscard]] bool checked_add(std::uint64_t& total, std::uint64_t value,
                               std::uint64_t maximum) noexcept;

[[nodiscard]] void* bounded_allocate(void* user, cgltf_size size) noexcept;
void bounded_deallocate(void* user, void* data) noexcept;
[[nodiscard]] Result<std::vector<std::byte>> read_source(const std::filesystem::path& path);
[[nodiscard]] std::string lower_extension(const std::filesystem::path& path);
[[nodiscard]] Error parse_error(cgltf_result result, bool is_glb);
[[nodiscard]] Result<void> validate_node_hierarchy(const cgltf_data& data);
[[nodiscard]] Result<void> validate_resource_limits(const cgltf_data& data);
[[nodiscard]] Result<void> validate_required_extensions(const cgltf_data& data);
[[nodiscard]] Result<void> validate_buffer_uris(const cgltf_data& data);
void decode_image_json_strings(cgltf_data& data) noexcept;
[[nodiscard]] Result<ModelImageMimeType> mime_from_text(std::string_view mime);
[[nodiscard]] Result<ModelImageMimeType> mime_from_extension(const std::filesystem::path& path);
[[nodiscard]] bool encoded_matches(ModelImageMimeType mime,
                                   std::span<const std::byte> bytes) noexcept;
[[nodiscard]] Result<std::vector<std::byte>> read_image_file(const std::filesystem::path& path);

[[nodiscard]] Result<TextureId> texture_for(ImportState& state, const cgltf_texture* source);
[[nodiscard]] Result<void> validate_texture_inputs(const cgltf_data& data);
[[nodiscard]] std::string mesh_context(const cgltf_mesh& mesh, cgltf_size mesh_index,
                                       cgltf_size primitive_index);
[[nodiscard]] std::string node_context(const cgltf_node& node, cgltf_size node_index);
[[nodiscard]] Result<std::vector<bool>> reachable_nodes(const cgltf_data& data);
[[nodiscard]] Result<std::vector<float>>
unpack_float3(const cgltf_accessor& accessor, ErrorCode error_code, std::string_view context);
[[nodiscard]] Result<std::vector<float>> unpack_float2(const cgltf_accessor& accessor,
                                                       std::string_view context);
[[nodiscard]] Result<std::vector<float>> unpack_color(const cgltf_accessor& accessor,
                                                      std::string_view context);
[[nodiscard]] Result<TextureMapping> texture_mapping(const cgltf_texture_view& view,
                                                     std::string_view context);
[[nodiscard]] TexcoordAvailability
primitive_texcoord_availability(const cgltf_primitive& primitive);
[[nodiscard]] Result<bool>
texture_view_uses_unavailable_texcoord(const cgltf_texture_view& view,
                                       const TexcoordAvailability& available_texcoords,
                                       std::string_view context);
[[nodiscard]] Result<bool>
material_uses_unavailable_texcoord(const cgltf_material& material,
                                   const TexcoordAvailability& available_texcoords,
                                   std::string_view context);
[[nodiscard]] Result<ImportedTextureView> import_texture_view(ImportState& state,
                                                              PrimitiveTextureState primitive_state,
                                                              const cgltf_texture_view& view,
                                                              std::string_view slot,
                                                              std::string_view context);
void generate_normals(std::span<const Float3> positions, std::vector<Float3>& normals,
                      std::span<const std::uint32_t> indices, std::uint64_t& degenerate_count,
                      std::uint64_t& fallback_count);
[[nodiscard]] Result<MaterialId> material_for(ImportState& state, const cgltf_material* material,
                                              const TexcoordAvailability& available_texcoords);
[[nodiscard]] Result<ImportedMesh> import_mesh(ImportState& state, const cgltf_mesh& mesh,
                                               cgltf_size mesh_index);

} // namespace elf3d::gltf::importer_detail
