module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <cgltf.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

[[nodiscard]] Result<EncodedImage> encoded_data_uri(std::string_view uri) {
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
    Result<std::vector<std::byte>> bytes = importer_encoding::decode_base64(uri.substr(comma + 1));
    if (!bytes) {
        return bytes.error();
    }
    if (!encoded_matches(mime.value(), bytes.value())) {
        return Error{ErrorCode::image_decode_failed,
                     "Image data URI payload does not match its declared MIME type"};
    }
    return EncodedImage{mime.value(), std::move(bytes).value()};
}

[[nodiscard]] Result<EncodedImage> encoded_external_image(const cgltf_image& source,
                                                          std::string_view uri,
                                                          const std::filesystem::path& gltf_path) {
    if (uri.find("://") != std::string_view::npos) {
        return Error{ErrorCode::unsupported_remote_uri,
                     "Remote HTTP/HTTPS glTF image URIs are unsupported"};
    }
    Result<std::string> decoded_uri = importer_encoding::percent_decode(uri);
    if (!decoded_uri) {
        return decoded_uri.error();
    }
    const std::filesystem::path image_path =
        gltf_path.parent_path() / path_from_utf8(decoded_uri.value());
    Result<ModelImageMimeType> mime = source.mime_type != nullptr ? mime_from_text(source.mime_type)
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

[[nodiscard]] bool valid_image_buffer_view(const cgltf_buffer_view& view) noexcept {
    return view.buffer != nullptr && view.buffer->data != nullptr && view.size != 0 &&
           view.offset <= view.buffer->size && view.size <= view.buffer->size - view.offset &&
           view.size <= image::maximum_encoded_bytes;
}

[[nodiscard]] Error invalid_image_buffer_view_error(const cgltf_buffer_view& view) {
    return Error{view.size > image::maximum_encoded_bytes ? ErrorCode::image_resource_limit_exceeded
                                                          : ErrorCode::image_range_out_of_bounds,
                 "A GLB image buffer view is empty or outside its source buffer"};
}

[[nodiscard]] Result<EncodedImage> copy_image_buffer_view(ModelImageMimeType mime,
                                                          const cgltf_buffer_view& view) {
    try {
        const auto* begin = static_cast<const std::byte*>(view.buffer->data) + view.offset;
        std::vector<std::byte> bytes(begin, begin + view.size);
        if (!encoded_matches(mime, bytes)) {
            return Error{ErrorCode::image_decode_failed,
                         "GLB image bytes do not match the declared MIME type"};
        }
        return EncodedImage{mime, std::move(bytes)};
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

[[nodiscard]] Result<EncodedImage> encoded_buffer_view(const cgltf_image& source) {
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
    if (!valid_image_buffer_view(view)) {
        return invalid_image_buffer_view_error(view);
    }
    return copy_image_buffer_view(mime.value(), view);
}

[[nodiscard]] Result<EncodedImage> encoded_image(const cgltf_image& source,
                                                 const std::filesystem::path& gltf_path) {
    if (source.uri != nullptr) {
        const std::string_view uri{source.uri};
        return uri.starts_with("data:") ? encoded_data_uri(uri)
                                        : encoded_external_image(source, uri, gltf_path);
    }
    return encoded_buffer_view(source);
}

[[nodiscard]] Result<ImageId> image_for(const cgltf_data& data, const cgltf_image* source,
                                        const std::filesystem::path& gltf_path,
                                        DocumentBuilder& builder, ImageImportState& images) {
    if (source == nullptr || source < data.images || source >= data.images + data.images_count) {
        return Error{ErrorCode::invalid_image_handle,
                     "A glTF texture references an image outside the image table"};
    }
    const std::size_t index = static_cast<std::size_t>(source - data.images);
    if (images.ids[index].has_value()) {
        return images.ids[index].value();
    }
    Result<EncodedImage> encoded = encoded_image(*source, gltf_path);
    if (!encoded) {
        return encoded.error();
    }
    std::uint64_t encoded_total = images.budget.encoded_bytes;
    if (!checked_add(encoded_total, static_cast<std::uint64_t>(encoded.value().bytes.size()),
                     maximum_total_encoded_image_bytes)) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     "Imported scene images exceed the 512 MiB encoded-image limit"};
    }
    Result<image::DecodedImage> decoded = image::decode_png_or_jpeg(encoded.value().bytes);
    if (!decoded) {
        return decoded.error();
    }
    std::uint64_t decoded_total = images.budget.decoded_bytes;
    if (!checked_add(decoded_total, static_cast<std::uint64_t>(decoded.value().pixels.size()),
                     maximum_total_decoded_image_bytes)) {
        return Error{ErrorCode::image_resource_limit_exceeded,
                     maximum_total_decoded_image_error_message};
    }
    const Result<ImageId> created = builder.create_image(ModelImageDescription{
        decoded.value().width, decoded.value().height, PixelFormat::rgba8_unorm,
        decoded.value().pixels, encoded.value().mime, encoded.value().bytes});
    if (!created) {
        return created.error();
    }
    images.budget.encoded_bytes = encoded_total;
    images.budget.decoded_bytes = decoded_total;
    images.ids[index] = created.value();
    return created.value();
}

[[nodiscard]] Result<TextureWrap> texture_wrap(cgltf_wrap_mode wrap) {
    switch (wrap) {
    case cgltf_wrap_mode_repeat:
        return TextureWrap::repeat;
    case cgltf_wrap_mode_mirrored_repeat:
        return TextureWrap::mirrored_repeat;
    case cgltf_wrap_mode_clamp_to_edge:
        return TextureWrap::clamp_to_edge;
    default:
        return Error{ErrorCode::invalid_sampler_wrap,
                     "A glTF sampler contains an invalid wrap mode"};
    }
}

[[nodiscard]] Result<TextureFilter> min_filter(cgltf_filter_type filter) {
    switch (filter) {
    case cgltf_filter_type_undefined:
        return TextureFilter::linear_mipmap_linear;
    case cgltf_filter_type_linear:
        return TextureFilter::linear;
    case cgltf_filter_type_nearest:
        return TextureFilter::nearest;
    case cgltf_filter_type_nearest_mipmap_nearest:
        return TextureFilter::nearest_mipmap_nearest;
    case cgltf_filter_type_linear_mipmap_nearest:
        return TextureFilter::linear_mipmap_nearest;
    case cgltf_filter_type_nearest_mipmap_linear:
        return TextureFilter::nearest_mipmap_linear;
    case cgltf_filter_type_linear_mipmap_linear:
        return TextureFilter::linear_mipmap_linear;
    default:
        return Error{ErrorCode::invalid_sampler_filter,
                     "A glTF sampler contains an invalid minification filter"};
    }
}

[[nodiscard]] constexpr SamplerDescription automatic_sampler_description() noexcept {
    return SamplerDescription{TextureWrap::repeat, TextureWrap::repeat,
                              TextureFilter::linear_mipmap_linear, TextureFilter::linear};
}

[[nodiscard]] Result<TextureFilter> mag_filter(cgltf_filter_type filter) {
    switch (filter) {
    case cgltf_filter_type_undefined:
    case cgltf_filter_type_linear:
        return TextureFilter::linear;
    case cgltf_filter_type_nearest:
        return TextureFilter::nearest;
    default:
        return Error{ErrorCode::invalid_sampler_filter,
                     "A glTF sampler magnification filter must be NEAREST or LINEAR"};
    }
}

[[nodiscard]] Result<SamplerDescription> sampler_description(const cgltf_sampler* sampler) {
    if (sampler == nullptr) {
        return automatic_sampler_description();
    }
    Result<TextureWrap> wrap_u = texture_wrap(sampler->wrap_s);
    Result<TextureWrap> wrap_v = texture_wrap(sampler->wrap_t);
    Result<TextureFilter> minimum = min_filter(sampler->min_filter);
    Result<TextureFilter> magnification = mag_filter(sampler->mag_filter);
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
    return SamplerDescription{wrap_u.value(), wrap_v.value(), minimum.value(),
                              magnification.value()};
}

[[nodiscard]] Result<SamplerId> sampler_for(const cgltf_data& data, const cgltf_sampler* source,
                                            DocumentBuilder& builder,
                                            std::vector<std::optional<SamplerId>>& samplers,
                                            std::optional<SamplerId>& default_sampler) {
    if (source == nullptr) {
        if (!default_sampler.has_value()) {
            const Result<SamplerId> created =
                builder.create_sampler(automatic_sampler_description());
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
    Result<SamplerDescription> description = sampler_description(source);
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

[[nodiscard]] Result<void> validate_sampler_inputs(const cgltf_data& data) {
    for (cgltf_size sampler_index = 0; sampler_index < data.samplers_count; ++sampler_index) {
        const Result<SamplerDescription> sampler =
            sampler_description(&data.samplers[sampler_index]);
        if (!sampler) {
            return sampler.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_primitive_texcoords(const cgltf_mesh& mesh,
                                                        cgltf_size mesh_index,
                                                        const cgltf_primitive& primitive,
                                                        cgltf_size primitive_index) {
    const cgltf_accessor* positions =
        cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
    for (cgltf_int set = 0; set < static_cast<cgltf_int>(maximum_texture_coordinate_sets); ++set) {
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
    return {};
}

[[nodiscard]] Result<void> validate_mesh_texcoords(const cgltf_data& data) {
    for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
        const cgltf_mesh& mesh = data.meshes[mesh_index];
        for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count;
             ++primitive_index) {
            const cgltf_primitive& primitive = mesh.primitives[primitive_index];
            if (!supported_primitive_type(primitive.type)) {
                continue;
            }
            const Result<void> valid =
                validate_primitive_texcoords(mesh, mesh_index, primitive, primitive_index);
            if (!valid) {
                return valid.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<void> validate_texture_inputs(const cgltf_data& data) {
    if (const Result<void> samplers = validate_sampler_inputs(data); !samplers) {
        return samplers.error();
    }
    return validate_mesh_texcoords(data);
}

[[nodiscard]] Result<TextureId> texture_for(ImportState& state, const cgltf_texture* source) {
    if (source == nullptr || source < state.data.textures ||
        source >= state.data.textures + state.data.textures_count) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "A glTF material references a texture outside the texture table"};
    }
    const std::size_t index = static_cast<std::size_t>(source - state.data.textures);
    if (state.ids.textures[index].has_value()) {
        return state.ids.textures[index].value();
    }
    if (source->image == nullptr) {
        return Error{ErrorCode::unsupported_image_mime_type,
                     "The glTF texture has no ordinary PNG/JPEG fallback image"};
    }
    Result<ImageId> image =
        image_for(state.data, source->image, state.gltf_path, state.builder, state.ids.images);
    if (!image) {
        return image.error();
    }
    Result<SamplerId> sampler = sampler_for(state.data, source->sampler, state.builder,
                                            state.ids.samplers, state.default_sampler);
    if (!sampler) {
        return sampler.error();
    }
    const Result<TextureId> created =
        state.builder.create_texture(ModelTextureDescription{image.value(), sampler.value()});
    if (!created) {
        return created.error();
    }
    state.ids.textures[index] = created.value();
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

struct ReachableNodeTraversal {
    std::vector<bool> reachable;
    std::vector<const cgltf_node*> queue;
};

[[nodiscard]] Result<void> append_reachable_root(const cgltf_data& data,
                                                 ReachableNodeTraversal& traversal,
                                                 const cgltf_node* node) {
    if (node == nullptr || node < data.nodes || node >= data.nodes + data.nodes_count) {
        return Error{ErrorCode::invalid_node_hierarchy,
                     "A glTF scene contains an invalid root node"};
    }
    const std::size_t index = static_cast<std::size_t>(node - data.nodes);
    if (!traversal.reachable[index]) {
        traversal.reachable[index] = true;
        traversal.queue.push_back(node);
    }
    return {};
}

[[nodiscard]] Result<void> append_scene_roots(const cgltf_data& data,
                                              ReachableNodeTraversal& traversal) {
    for (cgltf_size scene_index = 0; scene_index < data.scenes_count; ++scene_index) {
        const cgltf_scene& scene = data.scenes[scene_index];
        for (cgltf_size root_index = 0; root_index < scene.nodes_count; ++root_index) {
            const Result<void> result =
                append_reachable_root(data, traversal, scene.nodes[root_index]);
            if (!result) {
                return result.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<void> append_parentless_roots(const cgltf_data& data,
                                                   ReachableNodeTraversal& traversal) {
    for (cgltf_size index = 0; index < data.nodes_count; ++index) {
        if (data.nodes[index].parent == nullptr) {
            const Result<void> result = append_reachable_root(data, traversal, &data.nodes[index]);
            if (!result) {
                return result.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<void> append_reachable_child(const cgltf_data& data,
                                                  ReachableNodeTraversal& traversal,
                                                  const cgltf_node& parent,
                                                  const cgltf_node* child) {
    if (child == nullptr || child < data.nodes || child >= data.nodes + data.nodes_count ||
        child->parent != &parent) {
        return Error{ErrorCode::invalid_node_hierarchy,
                     "A glTF scene contains an invalid child link"};
    }
    const std::size_t index = static_cast<std::size_t>(child - data.nodes);
    if (!traversal.reachable[index]) {
        traversal.reachable[index] = true;
        traversal.queue.push_back(child);
    }
    return {};
}

[[nodiscard]] Result<void> append_reachable_descendants(const cgltf_data& data,
                                                        ReachableNodeTraversal& traversal) {
    for (std::size_t queue_index = 0; queue_index < traversal.queue.size(); ++queue_index) {
        const cgltf_node& parent = *traversal.queue[queue_index];
        for (cgltf_size child_index = 0; child_index < parent.children_count; ++child_index) {
            const Result<void> result =
                append_reachable_child(data, traversal, parent, parent.children[child_index]);
            if (!result) {
                return result.error();
            }
        }
    }
    return {};
}

[[nodiscard]] Result<std::vector<bool>> reachable_nodes(const cgltf_data& data) {
    try {
        ReachableNodeTraversal traversal{std::vector<bool>(data.nodes_count, false), {}};
        const Result<void> roots = data.scenes_count != 0
                                       ? append_scene_roots(data, traversal)
                                       : append_parentless_roots(data, traversal);
        if (!roots) {
            return roots.error();
        }
        if (const Result<void> descendants = append_reachable_descendants(data, traversal);
            !descendants) {
            return descendants.error();
        }
        return std::move(traversal.reachable);
    } catch (const std::bad_alloc&) {
        fatal_gltf_allocation_failure();
    } catch (...) {
        fatal_unexpected_gltf_boundary_exception();
    }
}

} // namespace elf3d::gltf::importer_detail
