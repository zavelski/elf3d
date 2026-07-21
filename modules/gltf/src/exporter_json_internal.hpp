#pragma once

#include <elf3d/model.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace elf3d::gltf::exporter_detail {

inline constexpr char preserved_json_begin = '\x1e';
inline constexpr char preserved_json_end = '\x1f';

void append_string(std::string& output, std::string_view value);
[[nodiscard]] bool begin_extension_member(std::string& output, bool& first,
                                          std::vector<std::string_view>& emitted,
                                          std::string_view name);
void append_preserved_extension_members(std::string& output, bool& first,
                                        std::vector<std::string_view>& emitted,
                                        std::span<const ModelJsonExtension> extensions);
void append_preserved_extras(std::string& output, ModelJsonMetadataView metadata,
                             bool has_member = true);
void append_preserved_metadata(std::string& output, ModelJsonMetadataView metadata,
                               bool has_member = true);
void add_extension_used(std::vector<std::string_view>& extensions, std::string_view name);
void add_preserved_extensions_used(std::vector<std::string_view>& destination,
                                   ModelJsonMetadataView metadata);
void append_unsigned(std::string& output, std::uint32_t value);
void append_float(std::string& output, float value);
void append_float2(std::string& output, Float2 value);
void append_float3(std::string& output, Float3 value);
void append_float4(std::string& output, Color4 value);
[[nodiscard]] std::string format_json(std::string_view source);
[[nodiscard]] std::uint32_t wrap_value(TextureWrap value) noexcept;
[[nodiscard]] std::uint32_t filter_value(TextureFilter value) noexcept;
[[nodiscard]] bool has_nondefault_transform(TextureMapping mapping) noexcept;
[[nodiscard]] std::string_view image_mime_text(ModelImageMimeType mime) noexcept;
void append_texture_info(std::string& output, std::uint32_t texture, TextureMapping mapping,
                         std::optional<std::pair<std::string_view, float>> scalar = std::nullopt);

} // namespace elf3d::gltf::exporter_detail
