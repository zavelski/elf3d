module;

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "exporter_internal.hpp"

#include <png.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.image;

namespace elf3d::gltf {

namespace exporter_output {

using OutputFile = std::pair<std::filesystem::path, std::vector<std::byte>>;

[[nodiscard]] Result<void> publish(std::vector<OutputFile>& files);

} // namespace exporter_output

namespace {

using namespace exporter_detail;

[[noreturn]] void fatal_export_allocation_failure() noexcept {
    fatal_error("Elf3D glTF exporter memory allocation failed");
}

[[noreturn]] void fatal_unexpected_export_boundary_exception() noexcept {
    fatal_error("Elf3D glTF exporter encountered an unexpected exception");
}

[[nodiscard]] Result<std::uint32_t> checked_index(std::size_t value) noexcept {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::size_overflow, "glTF output exceeds the 32-bit index limit"};
    }
    return static_cast<std::uint32_t>(value);
}

template <typename Id>
[[nodiscard]] std::optional<std::uint32_t> find_index(const std::vector<IdIndex<Id>>& indices,
                                                      Id id) noexcept {
    for (const IdIndex<Id>& entry : indices) {
        if (entry.id == id) {
            return entry.index;
        }
    }
    return std::nullopt;
}

template <typename T>
[[nodiscard]] Result<void> append_view(std::vector<T>& destination, Result<T> source) {
    if (!source) {
        return source.error();
    }
    destination.push_back(source.value());
    return {};
}

template <typename Id, typename T>
[[nodiscard]] Result<void> append_index(std::vector<IdIndex<Id>>& destination, const T& view,
                                        std::size_t index) {
    const Result<std::uint32_t> converted = checked_index(index);
    if (!converted) {
        return converted.error();
    }
    destination.push_back(IdIndex<Id>{view.id, converted.value()});
    return {};
}

template <typename T, typename Id, typename Reader>
[[nodiscard]] Result<void> collect_indexed_views(std::vector<T>& views,
                                                 std::vector<IdIndex<Id>>& indices,
                                                 std::size_t count, Reader reader) {
    for (std::size_t index = 0; index < count; ++index) {
        if (const Result<void> appended = append_view(views, reader(index)); !appended) {
            return appended.error();
        }
        if (const Result<void> indexed = append_index(indices, views.back(), index); !indexed) {
            return indexed.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> collect_scenes(ExportData& result, DocumentView document) {
    for (std::size_t index = 0; index < document.scene_count(); ++index) {
        if (const Result<void> appended = append_view(result.scenes, document.scene_at(index));
            !appended) {
            return appended.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> collect_meshes(ExportData& result, DocumentView document) {
    for (std::size_t index = 0; index < document.mesh_count(); ++index) {
        if (const Result<void> appended = append_view(result.meshes, document.mesh_at(index));
            !appended) {
            return appended.error();
        }
        if (result.meshes.back().primitives.empty()) {
            return Error{ErrorCode::invalid_mesh_data,
                         "glTF export requires every document mesh to contain a primitive"};
        }
        if (const Result<void> indexed =
                append_index(result.mesh_indices, result.meshes.back(), index);
            !indexed) {
            return indexed.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> collect_document_views(ExportData& result, DocumentView document) {
    if (const Result<void> scenes = collect_scenes(result, document); !scenes) {
        return scenes.error();
    }
    if (const Result<void> nodes = collect_indexed_views(
            result.nodes, result.node_indices, document.node_count(),
            [document](std::size_t index) { return document.node_at(index); });
        !nodes) {
        return nodes.error();
    }
    if (const Result<void> meshes = collect_meshes(result, document); !meshes) {
        return meshes.error();
    }
    if (const Result<void> primitives = collect_indexed_views(
            result.primitives, result.primitive_indices, document.primitive_count(),
            [document](std::size_t index) { return document.primitive_at(index); });
        !primitives) {
        return primitives.error();
    }
    if (const Result<void> materials = collect_indexed_views(
            result.materials, result.material_indices, document.material_count(),
            [document](std::size_t index) { return document.material_at(index); });
        !materials) {
        return materials.error();
    }
    if (const Result<void> images = collect_indexed_views(
            result.images, result.image_indices, document.image_count(),
            [document](std::size_t index) { return document.image_at(index); });
        !images) {
        return images.error();
    }
    if (const Result<void> textures = collect_indexed_views(
            result.textures, result.texture_indices, document.texture_count(),
            [document](std::size_t index) { return document.texture_at(index); });
        !textures) {
        return textures.error();
    }
    return collect_indexed_views(
        result.samplers, result.sampler_indices, document.sampler_count(),
        [document](std::size_t index) { return document.sampler_at(index); });
}

void clear_preserved_metadata(ExportData& result) noexcept {
    result.root_metadata = {};
    result.asset_metadata = {};
    for (DocumentSceneView& scene : result.scenes) {
        scene.metadata = {};
    }
    for (NodeView& node : result.nodes) {
        node.metadata = {};
    }
    for (MeshView& mesh : result.meshes) {
        mesh.metadata = {};
    }
    for (PrimitiveView& primitive : result.primitives) {
        primitive.metadata = {};
    }
    for (MaterialView& material : result.materials) {
        material.metadata = {};
    }
    for (ImageView& image : result.images) {
        image.metadata = {};
    }
    for (TextureView& texture : result.textures) {
        texture.metadata = {};
    }
    for (SamplerView& sampler : result.samplers) {
        sampler.metadata = {};
    }
}

[[nodiscard]] Result<ExportData> collect_document(DocumentView document) {
    if (validate_document(document).has_errors()) {
        return Error{ErrorCode::invalid_argument, "Cannot export an invalid model document"};
    }
    ExportData result;
    result.root_metadata = document.root_metadata();
    result.asset_metadata = document.asset_metadata();
    result.default_scene = document.default_scene();
    if (const Result<void> views = collect_document_views(result, document); !views) {
        return views.error();
    }
    result.preserved_metadata_dropped = document.preserved_metadata_stale();
    if (result.preserved_metadata_dropped) {
        clear_preserved_metadata(result);
    }
    return result;
}

[[nodiscard]] std::string_view image_mime_text(ModelImageMimeType mime_type) noexcept {
    return mime_type == ModelImageMimeType::jpeg ? "image/jpeg" : "image/png";
}

[[nodiscard]] std::string_view image_extension(ModelImageMimeType mime_type) noexcept {
    return mime_type == ModelImageMimeType::jpeg ? ".jpg" : ".png";
}

[[nodiscard]] Result<std::uint32_t> append_buffer_view(std::vector<ByteRange>& views,
                                                       ByteRange range) {
    const Result<std::uint32_t> index = checked_index(views.size());
    if (!index) {
        return index.error();
    }
    views.push_back(range);
    return index.value();
}

[[nodiscard]] Result<std::uint32_t> append_accessor(std::vector<Accessor>& accessors,
                                                    const AccessorDescription& description) {
    const Result<std::uint32_t> converted_count = checked_index(description.count);
    if (!converted_count) {
        return converted_count.error();
    }
    const Result<std::uint32_t> index = checked_index(accessors.size());
    if (!index) {
        return index.error();
    }
    accessors.push_back(Accessor{description.view, description.component_type,
                                 converted_count.value(), description.type, description.bounds});
    return index.value();
}

[[nodiscard]] Result<void> append_position_attribute(const PrimitiveView& primitive,
                                                     BinaryBuilder& binary,
                                                     std::vector<ByteRange>& views,
                                                     std::vector<Accessor>& accessors,
                                                     PrimitiveOutput& output) {
    const Result<ByteRange> positions = binary.append_positions(primitive.data.positions);
    if (!positions) {
        return positions.error();
    }
    const Result<std::uint32_t> position_view = append_buffer_view(views, positions.value());
    if (!position_view) {
        return position_view.error();
    }
    const Result<std::uint32_t> position_accessor =
        append_accessor(accessors, {position_view.value(), 5126U, primitive.data.positions.size(),
                                    "VEC3", primitive.bounds});
    if (!position_accessor) {
        return position_accessor.error();
    }
    output.positions = position_accessor.value();
    return {};
}

[[nodiscard]] Result<std::uint32_t> append_vec3_attribute(std::span<const Float3> values,
                                                          BinaryBuilder& binary,
                                                          std::vector<ByteRange>& views,
                                                          std::vector<Accessor>& accessors) {
    const Result<ByteRange> bytes = binary.append_positions(values);
    if (!bytes) {
        return bytes.error();
    }
    const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
    if (!view) {
        return view.error();
    }
    return append_accessor(accessors, {view.value(), 5126U, values.size(), "VEC3", std::nullopt});
}

[[nodiscard]] Result<std::uint32_t> append_vec2_attribute(std::span<const Float2> values,
                                                          BinaryBuilder& binary,
                                                          std::vector<ByteRange>& views,
                                                          std::vector<Accessor>& accessors) {
    const Result<ByteRange> bytes = binary.append_texcoords(values);
    if (!bytes) {
        return bytes.error();
    }
    const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
    if (!view) {
        return view.error();
    }
    return append_accessor(accessors, {view.value(), 5126U, values.size(), "VEC2", std::nullopt});
}

[[nodiscard]] Result<void> append_optional_attributes(const PrimitiveView& primitive,
                                                      BinaryBuilder& binary,
                                                      std::vector<ByteRange>& views,
                                                      std::vector<Accessor>& accessors,
                                                      PrimitiveOutput& output) {
    if (!primitive.data.normals.empty()) {
        const Result<std::uint32_t> normals =
            append_vec3_attribute(primitive.data.normals, binary, views, accessors);
        if (!normals) {
            return normals.error();
        }
        output.normals = normals.value();
    }
    if (!primitive.data.texcoord0.empty()) {
        const Result<std::uint32_t> values =
            append_vec2_attribute(primitive.data.texcoord0, binary, views, accessors);
        if (!values) {
            return values.error();
        }
        output.texcoord0 = values.value();
    }
    if (!primitive.data.texcoord1.empty()) {
        const Result<std::uint32_t> values =
            append_vec2_attribute(primitive.data.texcoord1, binary, views, accessors);
        if (!values) {
            return values.error();
        }
        output.texcoord1 = values.value();
    }
    return {};
}

[[nodiscard]] Result<void> append_color_attribute(const PrimitiveView& primitive,
                                                  BinaryBuilder& binary,
                                                  std::vector<ByteRange>& views,
                                                  std::vector<Accessor>& accessors,
                                                  PrimitiveOutput& output) {
    if (primitive.data.colors.empty()) {
        return {};
    }
    const Result<ByteRange> bytes = binary.append_colors(primitive.data.colors);
    if (!bytes) {
        return bytes.error();
    }
    const Result<std::uint32_t> view = append_buffer_view(views, bytes.value());
    if (!view) {
        return view.error();
    }
    const Result<std::uint32_t> colors = append_accessor(
        accessors, {view.value(), 5126U, primitive.data.colors.size(), "VEC4", std::nullopt});
    if (!colors) {
        return colors.error();
    }
    output.colors = colors.value();
    return {};
}

[[nodiscard]] Result<void> append_index_attribute(const PrimitiveView& primitive,
                                                  BinaryBuilder& binary,
                                                  std::vector<ByteRange>& views,
                                                  std::vector<Accessor>& accessors,
                                                  PrimitiveOutput& output) {
    const Result<EncodedIndexRange> index_bytes = binary.append_indices(primitive.data.indices);
    if (!index_bytes) {
        return index_bytes.error();
    }
    const Result<std::uint32_t> index_view =
        append_buffer_view(views, index_bytes.value().bytes);
    if (!index_view) {
        return index_view.error();
    }
    const Result<std::uint32_t> index_accessor =
        append_accessor(accessors, {index_view.value(), index_bytes.value().component_type,
                                    primitive.data.indices.size(), "SCALAR", std::nullopt});
    if (!index_accessor) {
        return index_accessor.error();
    }
    output.indices = index_accessor.value();
    return {};
}

[[nodiscard]] Result<PrimitiveOutput>
append_primitive(const PrimitiveView& primitive, const ExportData& document, BinaryBuilder& binary,
                 std::vector<ByteRange>& views, std::vector<Accessor>& accessors) {
    const std::optional<std::uint32_t> material =
        find_index(document.material_indices, primitive.material);
    if (!material.has_value()) {
        return Error{ErrorCode::invalid_argument, "Primitive material is not in this document"};
    }
    PrimitiveOutput output;
    output.material = *material;
    if (const Result<void> positions =
            append_position_attribute(primitive, binary, views, accessors, output);
        !positions) {
        return positions.error();
    }
    if (const Result<void> attributes =
            append_optional_attributes(primitive, binary, views, accessors, output);
        !attributes) {
        return attributes.error();
    }
    if (const Result<void> colors =
            append_color_attribute(primitive, binary, views, accessors, output);
        !colors) {
        return colors.error();
    }
    if (const Result<void> indices =
            append_index_attribute(primitive, binary, views, accessors, output);
        !indices) {
        return indices.error();
    }
    return output;
}

void append_uint32(std::vector<std::byte>& output, std::uint32_t value) {
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
}

[[nodiscard]] Result<std::vector<std::byte>> build_glb(std::string json,
                                                       const std::vector<std::byte>& binary) {
    while (json.size() % 4U != 0U) {
        json.push_back(' ');
    }
    std::vector<std::byte> binary_chunk = binary;
    while (binary_chunk.size() % 4U != 0U) {
        binary_chunk.push_back(std::byte{0});
    }
    const std::size_t total =
        20U + json.size() + (binary_chunk.empty() ? 0U : 8U + binary_chunk.size());
    if (total > std::numeric_limits<std::uint32_t>::max()) {
        return Error{ErrorCode::size_overflow, "GLB output exceeds the 32-bit container limit"};
    }
    std::vector<std::byte> output;
    output.reserve(total);
    append_uint32(output, 0x46546c67U);
    append_uint32(output, 2U);
    append_uint32(output, static_cast<std::uint32_t>(total));
    append_uint32(output, static_cast<std::uint32_t>(json.size()));
    append_uint32(output, 0x4e4f534aU);
    for (const char value : json) {
        output.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }
    if (!binary_chunk.empty()) {
        append_uint32(output, static_cast<std::uint32_t>(binary_chunk.size()));
        append_uint32(output, 0x004e4942U);
        output.insert(output.end(), binary_chunk.begin(), binary_chunk.end());
    }
    return output;
}

[[nodiscard]] std::vector<std::byte> bytes_from_text(std::string_view text) {
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return result;
}

[[nodiscard]] ModelWriteReport
build_write_report(std::span<const EncodedImageOutput> encoded_images,
                   bool preserved_metadata_dropped) {
    ModelWriteReport report;
    for (std::size_t index = 0; index < encoded_images.size(); ++index) {
        if (!encoded_images[index].reencoded) {
            continue;
        }
        report.diagnostics.push_back(ModelWriteDiagnostic{
            ModelWriteDiagnosticSeverity::information, ModelWriteDiagnosticCategory::image,
            ModelWriteDiagnosticCode::image_reencoded_as_png,
            "Document image was encoded as PNG for glTF output", "image " + std::to_string(index)});
    }
    if (preserved_metadata_dropped) {
        report.diagnostics.push_back(ModelWriteDiagnostic{
            ModelWriteDiagnosticSeverity::warning, ModelWriteDiagnosticCategory::metadata,
            ModelWriteDiagnosticCode::preserved_metadata_dropped_after_mutation,
            "Preserved glTF extras and unknown extensions were omitted after document mutation",
            std::nullopt});
    }
    return report;
}

[[nodiscard]] Result<ExportPackage> initialize_export_package(const OutputPath& path,
                                                              DocumentView document,
                                                              const ModelWriteOptions& options) {
    const std::string extension = path.extension().string();
    if (extension != ".gltf" && extension != ".glb") {
        return Error{ErrorCode::unsupported_model_format,
                     "Document output paths must use a .gltf or .glb extension"};
    }
    Result<ExportData> collected = collect_document(document);
    if (!collected) {
        return collected.error();
    }
    ExportPackage package;
    package.document = std::move(collected).value();
    package.glb = extension == ".glb";
    package.embedded = options.image_policy == ModelImageWritePolicy::embedded ||
                       (options.image_policy == ModelImageWritePolicy::automatic && package.glb);
    package.primitives.reserve(package.document.primitives.size());
    package.encoded_images.reserve(package.document.images.size());
    package.image_mime_types.reserve(package.document.images.size());
    package.image_views.reserve(package.document.images.size());
    package.image_uris.reserve(package.document.images.size());
    return package;
}

[[nodiscard]] Result<void> append_package_primitives(ExportPackage& package) {
    for (const PrimitiveView& primitive : package.document.primitives) {
        const Result<PrimitiveOutput> output = append_primitive(
            primitive, package.document, package.binary, package.views, package.accessors);
        if (!output) {
            return output.error();
        }
        package.primitives.push_back(output.value());
    }
    return {};
}

[[nodiscard]] Result<void> append_package_images(ExportPackage& package) {
    for (const ImageView& image : package.document.images) {
        Result<EncodedImageOutput> encoded = encoded_image(image);
        if (!encoded) {
            return encoded.error();
        }
        package.encoded_images.push_back(std::move(encoded).value());
        package.image_mime_types.push_back(package.encoded_images.back().mime_type);
        if (!package.embedded) {
            package.image_views.push_back(std::nullopt);
            continue;
        }
        const Result<ByteRange> range =
            package.binary.append_bytes(package.encoded_images.back().bytes);
        if (!range) {
            return range.error();
        }
        const Result<std::uint32_t> view = append_buffer_view(package.views, range.value());
        if (!view) {
            return view.error();
        }
        package.image_views.push_back(view.value());
    }
    return {};
}

void append_package_uris(ExportPackage& package, const OutputPath& path) {
    const std::string stem = path.stem().string();
    package.buffer_uri = stem + ".bin";
    for (std::size_t index = 0; index < package.encoded_images.size(); ++index) {
        package.image_uris.push_back(
            stem + ".image_" + std::to_string(index) +
            std::string{image_extension(package.encoded_images[index].mime_type)});
    }
}

[[nodiscard]] Result<ExportPackage> prepare_export_package(const OutputPath& path,
                                                           DocumentView document,
                                                           const ModelWriteOptions& options) {
    Result<ExportPackage> initialized = initialize_export_package(path, document, options);
    if (!initialized) {
        return initialized.error();
    }
    ExportPackage package = std::move(initialized).value();
    if (const Result<void> primitives = append_package_primitives(package); !primitives) {
        return primitives.error();
    }
    if (const Result<void> images = append_package_images(package); !images) {
        return images.error();
    }
    append_package_uris(package, path);
    return package;
}

[[nodiscard]] Result<std::vector<exporter_output::OutputFile>>
build_output_artifacts(const OutputPath& path, ExportPackage& package, const std::string& json) {
    std::vector<exporter_output::OutputFile> artifacts;
    if (!package.embedded) {
        for (std::size_t index = 0; index < package.encoded_images.size(); ++index) {
            artifacts.emplace_back(path.parent_path() / package.image_uris[index],
                                   std::move(package.encoded_images[index].bytes));
        }
    }
    if (package.glb) {
        const Result<std::vector<std::byte>> container = build_glb(json, package.binary.bytes());
        if (!container) {
            return container.error();
        }
        artifacts.emplace_back(path, container.value());
        return artifacts;
    }
    if (!package.binary.bytes().empty()) {
        artifacts.emplace_back(path.parent_path() / package.buffer_uri, package.binary.bytes());
    }
    artifacts.emplace_back(path, bytes_from_text(json));
    return artifacts;
}

[[nodiscard]] Result<ModelWriteReport> save(const OutputPath& path, DocumentView document,
                                            const ModelWriteOptions& options) {
    Result<ExportPackage> prepared = prepare_export_package(path, document, options);
    if (!prepared) {
        return prepared.error();
    }
    ExportPackage package = std::move(prepared).value();
    const Result<std::string> json = build_json(package);
    if (!json) {
        return json.error();
    }
    Result<std::vector<exporter_output::OutputFile>> built =
        build_output_artifacts(path, package, json.value());
    if (!built) {
        return built.error();
    }
    std::vector<exporter_output::OutputFile> artifacts = std::move(built).value();
    if (const Result<void> published = exporter_output::publish(artifacts); !published) {
        return published.error();
    }
    return build_write_report(package.encoded_images, package.document.preserved_metadata_dropped);
}

} // namespace

Result<ModelWriteReport> save_document(const std::filesystem::path& path, DocumentView document,
                                       const ModelWriteOptions& options) noexcept {
    try {
        return save(path, document, options);
    } catch (const std::bad_alloc&) {
        fatal_export_allocation_failure();
    } catch (...) {
        fatal_unexpected_export_boundary_exception();
    }
}

} // namespace elf3d::gltf

namespace elf3d {

Result<ModelWriteReport> save_document(std::string_view path_utf8, DocumentView document,
                                       const ModelWriteOptions& options) noexcept {
    try {
        std::u8string utf8;
        utf8.reserve(path_utf8.size());
        for (const char character : path_utf8) {
            utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
        }
        return gltf::save_document(std::filesystem::path{utf8}, document, options);
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D glTF exporter memory allocation failed");
    } catch (...) {
        fatal_error("Elf3D glTF exporter encountered an unexpected exception");
    }
}

} // namespace elf3d
