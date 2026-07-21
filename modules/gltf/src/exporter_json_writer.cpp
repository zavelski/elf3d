module;

#include <elf3d/core/assert.h>
#include <elf3d/model.h>

#include "exporter_json_internal.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

module elf.gltf;

namespace elf3d::gltf::exporter_detail {
namespace {

void append_preserved_json(std::string& output, std::string_view value) {
    output.push_back(preserved_json_begin);
    output.append(value);
    output.push_back(preserved_json_end);
}

} // namespace

void append_string(std::string& output, std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char character : value) {
        switch (character) {
        case '"':
            output.append("\\\"");
            break;
        case '\\':
            output.append("\\\\");
            break;
        case '\b':
            output.append("\\b");
            break;
        case '\f':
            output.append("\\f");
            break;
        case '\n':
            output.append("\\n");
            break;
        case '\r':
            output.append("\\r");
            break;
        case '\t':
            output.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output.append("\\u00");
                output.push_back(hex[(static_cast<unsigned char>(character) >> 4U) & 0x0fU]);
                output.push_back(hex[static_cast<unsigned char>(character) & 0x0fU]);
            } else {
                output.push_back(character);
            }
            break;
        }
    }
    output.push_back('"');
}

bool begin_extension_member(std::string& output, bool& first,
                            std::vector<std::string_view>& emitted, std::string_view name) {
    if (std::find(emitted.begin(), emitted.end(), name) != emitted.end()) {
        return false;
    }
    if (!first) {
        output.push_back(',');
    }
    first = false;
    append_string(output, name);
    output.push_back(':');
    emitted.push_back(name);
    return true;
}

void append_preserved_extension_members(std::string& output, bool& first,
                                        std::vector<std::string_view>& emitted,
                                        std::span<const ModelJsonExtension> extensions) {
    for (const ModelJsonExtension& extension : extensions) {
        if (begin_extension_member(output, first, emitted, extension.name)) {
            append_preserved_json(output, extension.data);
        }
    }
}

void append_preserved_extras(std::string& output, ModelJsonMetadataView metadata, bool has_member) {
    if (!metadata.extras_json.has_value()) {
        return;
    }
    if (has_member) {
        output.push_back(',');
    }
    output.append("\"extras\":");
    append_preserved_json(output, *metadata.extras_json);
}

void append_preserved_metadata(std::string& output, ModelJsonMetadataView metadata,
                               bool has_member) {
    if (!metadata.extensions.empty()) {
        if (has_member) {
            output.push_back(',');
        }
        output.append("\"extensions\":{");
        bool first = true;
        std::vector<std::string_view> emitted;
        emitted.reserve(metadata.extensions.size());
        append_preserved_extension_members(output, first, emitted, metadata.extensions);
        output.push_back('}');
        has_member = true;
    }
    append_preserved_extras(output, metadata, has_member);
}

void add_extension_used(std::vector<std::string_view>& extensions, std::string_view name) {
    if (std::find(extensions.begin(), extensions.end(), name) == extensions.end()) {
        extensions.push_back(name);
    }
}

void add_preserved_extensions_used(std::vector<std::string_view>& destination,
                                   ModelJsonMetadataView metadata) {
    for (const ModelJsonExtension& extension : metadata.extensions) {
        add_extension_used(destination, extension.name);
    }
}

void append_unsigned(std::string& output, std::uint32_t value) {
    std::array<char, 16> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    ELF3D_ASSERT(result.ec == std::errc{});
    output.append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
}

void append_float(std::string& output, float value) {
    std::array<char, 64> buffer{};
    const std::to_chars_result result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                      std::chars_format::general, std::numeric_limits<float>::max_digits10);
    ELF3D_ASSERT(result.ec == std::errc{});
    output.append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
}

void append_float2(std::string& output, Float2 value) {
    output.push_back('[');
    append_float(output, value.x);
    output.push_back(',');
    append_float(output, value.y);
    output.push_back(']');
}

void append_float3(std::string& output, Float3 value) {
    output.push_back('[');
    append_float(output, value.x);
    output.push_back(',');
    append_float(output, value.y);
    output.push_back(',');
    append_float(output, value.z);
    output.push_back(']');
}

void append_float4(std::string& output, Color4 value) {
    output.push_back('[');
    append_float(output, value.red);
    output.push_back(',');
    append_float(output, value.green);
    output.push_back(',');
    append_float(output, value.blue);
    output.push_back(',');
    append_float(output, value.alpha);
    output.push_back(']');
}

std::uint32_t wrap_value(TextureWrap value) noexcept {
    switch (value) {
    case TextureWrap::repeat:
        return 10497U;
    case TextureWrap::mirrored_repeat:
        return 33648U;
    case TextureWrap::clamp_to_edge:
        return 33071U;
    }
    ELF3D_ASSERT(false);
    return 10497U;
}

std::uint32_t filter_value(TextureFilter value) noexcept {
    switch (value) {
    case TextureFilter::nearest:
        return 9728U;
    case TextureFilter::linear:
        return 9729U;
    case TextureFilter::nearest_mipmap_nearest:
        return 9984U;
    case TextureFilter::linear_mipmap_nearest:
        return 9985U;
    case TextureFilter::nearest_mipmap_linear:
        return 9986U;
    case TextureFilter::linear_mipmap_linear:
        return 9987U;
    }
    ELF3D_ASSERT(false);
    return 9729U;
}

bool has_nondefault_transform(TextureMapping mapping) noexcept {
    return mapping.transform.offset != Float2{} || mapping.transform.scale != Float2{1.0F, 1.0F} ||
           mapping.transform.rotation_radians != 0.0F;
}

std::string_view image_mime_text(ModelImageMimeType mime) noexcept {
    return mime == ModelImageMimeType::jpeg ? "image/jpeg" : "image/png";
}

void append_texture_info(std::string& output, std::uint32_t texture, TextureMapping mapping,
                         std::optional<std::pair<std::string_view, float>> scalar) {
    output.append("{\"index\":");
    append_unsigned(output, texture);
    if (scalar.has_value() && scalar->second != 1.0F) {
        output.push_back(',');
        append_string(output, scalar->first);
        output.push_back(':');
        append_float(output, scalar->second);
    }
    if (mapping.texcoord_set != 0U) {
        output.append(",\"texCoord\":");
        append_unsigned(output, mapping.texcoord_set);
    }
    if (has_nondefault_transform(mapping)) {
        output.append(",\"extensions\":{\"KHR_texture_transform\":{");
        bool first = true;
        const auto property = [&output, &first](std::string_view name) {
            if (!first) {
                output.push_back(',');
            }
            first = false;
            append_string(output, name);
            output.push_back(':');
        };
        if (mapping.transform.offset != Float2{}) {
            property("offset");
            append_float2(output, mapping.transform.offset);
        }
        if (mapping.transform.rotation_radians != 0.0F) {
            property("rotation");
            append_float(output, mapping.transform.rotation_radians);
        }
        if (mapping.transform.scale != Float2{1.0F, 1.0F}) {
            property("scale");
            append_float2(output, mapping.transform.scale);
        }
        if (mapping.texcoord_set != 0U) {
            property("texCoord");
            append_unsigned(output, mapping.texcoord_set);
        }
        output.append("}}");
    }
    output.push_back('}');
}

} // namespace elf3d::gltf::exporter_detail
