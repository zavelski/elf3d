#include <elf3d/core/error.h>
#include <elf3d/core/result.h>

#include "importer_internal.hpp"
#include "json_document.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace elf3d::gltf::importer_detail {

[[nodiscard]] Result<void> decode_json_text(char* text)
{
    if (text == nullptr) {
        return {};
    }
    // Keep the existing no-allocation path for ordinary names and large data URIs.
    if (std::string_view{text}.find('\\') == std::string_view::npos) {
        return {};
    }
    const std::string token = "\"" + std::string{text} + "\"";
    const auto decoded = json::string_value(token);
    if (!decoded || decoded.value().find('\0') != std::string::npos) {
        return Error{ErrorCode::gltf_validation_failed,
                     "A glTF string contains invalid Unicode or an embedded null"};
    }
    // Decoding never grows the cgltf-owned encoded buffer, including surrogate pairs.
    std::copy(decoded.value().begin(), decoded.value().end(), text);
    text[decoded.value().size()] = '\0';
    return {};
}

template <typename Source>
[[nodiscard]] Result<void> decode_names(Source* objects, cgltf_size count)
{
    for (cgltf_size index = 0; index < count; ++index) {
        if (const auto decoded = decode_json_text(objects[index].name); !decoded) {
            return decoded.error();
        }
    }
    return {};
}

[[nodiscard]] Result<void> decode_document_json_strings(cgltf_data& data)
{
    if (const auto nodes = decode_names(data.nodes, data.nodes_count); !nodes) {
        return nodes.error();
    }
    if (const auto meshes = decode_names(data.meshes, data.meshes_count); !meshes) {
        return meshes.error();
    }
    if (const auto scenes = decode_names(data.scenes, data.scenes_count); !scenes) {
        return scenes.error();
    }
    if (const auto materials = decode_names(data.materials, data.materials_count); !materials) {
        return materials.error();
    }
    if (const auto cameras = decode_names(data.cameras, data.cameras_count); !cameras) {
        return cameras.error();
    }
    for (cgltf_size index = 0; index < data.images_count; ++index) {
        cgltf_image& source = data.images[index];
        if (const auto mime = decode_json_text(source.mime_type); !mime) {
            return mime.error();
        }
        if (const auto uri = decode_json_text(source.uri); !uri) {
            return uri.error();
        }
    }
    return {};
}

} // namespace elf3d::gltf::importer_detail
