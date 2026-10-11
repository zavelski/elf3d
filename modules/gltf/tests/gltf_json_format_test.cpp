#include "../src/exporter_json_internal.hpp"
#include "../src/json_document.h"

#include <elf3d/model.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace {

using elf3d::ModelNumericWritePolicy;
using elf3d::gltf::exporter_detail::format_json;
namespace json = elf3d::gltf::json;

[[nodiscard]] bool bounded_lines(std::string_view text)
{
    std::size_t width = 0;
    for (const char character : text) {
        if (character == '\n') {
            if (width > 140U) {
                return false;
            }
            width = 0;
        } else {
            width = character == '\t' ? (width / 4U + 1U) * 4U : width + 1U;
        }
    }
    return true;
}

[[nodiscard]] bool layout_tokens(std::string_view text)
{
    return text.find("9007199254740993") != std::string_view::npos &&
           text.find("0.12345678901234567890123456789") != std::string_view::npos &&
           text.find("\"kisp\": {\n") != std::string_view::npos &&
           text.find("\"EXT_example\": {\n") != std::string_view::npos;
}

[[nodiscard]] bool layout_and_tokens()
{
    std::string source =
        R"json({"extras":{"kisp":{"units":"m","integer":9007199254740993,"precise":0.12345678901234567890123456789,"escape":"brace } and quote \"","label":"\uD834\uDD1E"}},"extensions":{"EXT_example":{"nested":[{"a":1},{"b":[true,null,2]}]}},"scenes":[{"nodes":[)json";
    for (std::size_t index = 0; index < 300U; ++index) {
        if (index != 0U) {
            source.push_back(',');
        }
        source.append(std::to_string(index));
    }
    source.append("]}]}");
    const auto formatted = format_json(source, ModelNumericWritePolicy::round_trip);
    if (!formatted || !layout_tokens(formatted.value().text)) {
        return false;
    }
    const auto repeated = format_json(formatted.value().text, ModelNumericWritePolicy::round_trip);
    if (!repeated || repeated.value().text != formatted.value().text) {
        return false;
    }
    if (!bounded_lines(formatted.value().text)) {
        return false;
    }
    const std::string long_token(200U, 'x');
    const auto long_string =
        format_json("{\"name\":\"" + long_token + "\"}", ModelNumericWritePolicy::round_trip);
    return long_string && long_string.value().text.find(long_token) != std::string::npos;
}

[[nodiscard]] bool texture_omissions(const json::Document& document, std::size_t materials)
{
    const std::size_t first = document.values[materials].children[0];
    const auto pbr = json::member(document, first, "pbrMetallicRoughness");
    const auto texture = pbr ? json::member(document, *pbr, "baseColorTexture") : std::nullopt;
    if (!texture || json::member(document, *texture, "extensions")) {
        return false;
    }
    for (std::size_t index = 1; index < 3U; ++index) {
        const std::size_t material = document.values[materials].children[index];
        const auto slot =
            json::member(document, material, index == 1U ? "normalTexture" : "emissiveTexture");
        if (!slot || !json::member(document, *slot, "extensions")) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool omission_tokens(std::string_view text)
{
    return text.find("EXT_absent") == std::string_view::npos &&
           text.find("extensionsRequired") != std::string_view::npos &&
           text.find("0, 0.004, 0, 1") != std::string_view::npos;
}

[[nodiscard]] bool default_omissions()
{
    constexpr std::string_view source = R"json({
      "asset":{"version":"2.0"},
      "extensionsUsed":["KHR_texture_transform","EXT_absent"],
      "extensionsRequired":["KHR_texture_transform"],
      "nodes":[{"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],"translation":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"extras":{}},
               {"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0.004,0,1]}],
      "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0,0],"scale":[1,1]}}}}},
                   {"normalTexture":{"index":0,"extensions":{"KHR_texture_transform":{"scale":[1,1],"texCoord":1}}}},
                   {"emissiveTexture":{"index":0,"extensions":{"KHR_texture_transform":{"scale":[1,1],"extras":{"meaningful":true}}}}}],
      "extras":{"matrix":[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]}
    })json";
    const auto formatted = format_json(source, ModelNumericWritePolicy::round_trip);
    if (!formatted) {
        return false;
    }
    const auto parsed = json::parse(formatted.value().text);
    if (!parsed) {
        return false;
    }
    const auto& document = parsed.value();
    const auto nodes = json::member(document, 0U, "nodes");
    const auto materials = json::member(document, 0U, "materials");
    const auto extras = json::member(document, 0U, "extras");
    if (!nodes || !materials || !extras) {
        return false;
    }
    if (!document.values[document.values[*nodes].children[0]].children.empty() ||
        !json::member(document, *extras, "matrix")) {
        return false;
    }
    if (!omission_tokens(formatted.value().text)) {
        return false;
    }
    return texture_omissions(document, *materials);
}

[[nodiscard]] bool float_round_trip()
{
    const std::array<float, 11> values{0.0F,
                                       -0.0F,
                                       0.004F,
                                       0.1F,
                                       -123.456F,
                                       std::numeric_limits<float>::lowest(),
                                       std::numeric_limits<float>::max(),
                                       std::numeric_limits<float>::min(),
                                       std::numeric_limits<float>::denorm_min(),
                                       std::nextafter(1.0F, 0.0F),
                                       std::nextafter(1.0F, 2.0F)};
    for (const float expected : values) {
        std::string token;
        elf3d::gltf::exporter_detail::append_float(token, expected);
        float actual = 0;
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), actual);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            actual != expected || (actual == 0.0F && std::signbit(actual))) {
            return false;
        }
        if (expected == 0.004F && token != "0.004") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool matrix_cleaning()
{
    constexpr std::string_view source = R"json({"nodes":[
      {"matrix":[0.00000001,1,0,0,-1,0.00000001,0,0,0,0,1,0,0.00000001,0,0,1]},
      {"matrix":[2,0.00000001,0,0,0,2,0,0,0,0,2,0,0,0,0,1]},
      {"matrix":[1,0.1,0,0,0,1,0,0,0,0,1,0,0,0,0,1]}],
      "extras":{"matrix":[0.00000001,1,0,0,-1,0.00000001,0,0,0,0,1,0,0,0,0,1]}})json";
    const auto round_trip = format_json(source, ModelNumericWritePolicy::round_trip);
    const auto cleaned = format_json(source, ModelNumericWritePolicy::clean_rotation);
    if (!round_trip || !cleaned || round_trip.value().cleaned_matrices != 0U ||
        cleaned.value().cleaned_matrices != 1U) {
        return false;
    }
    const auto parsed = json::parse(cleaned.value().text);
    if (!parsed) {
        return false;
    }
    const auto nodes = json::member(parsed.value(), 0U, "nodes");
    const std::size_t first = parsed.value().values[*nodes].children[0];
    const auto matrix = json::member(parsed.value(), first, "matrix");
    const auto& children = parsed.value().values[*matrix].children;
    return parsed.value().values[children[0]].token == "0" &&
           parsed.value().values[children[12]].token == "0.00000001" &&
           cleaned.value().text.find("2, 0.00000001, 0, 0") != std::string::npos &&
           cleaned.value().text.find("1, 0.1, 0, 0") != std::string::npos &&
           cleaned.value().text.find("0.00000001, 1, 0, 0") != std::string::npos;
}

[[nodiscard]] bool malformed_and_bounded()
{
    for (const std::string_view input :
         {"", "{", "[1,]", "{\"n\":01}", "{\"a\":1,\"a\":2}", "{\"key\":true false}",
          "{\"s\":\"\\q\"}", "[NaN]", "[] extra"}) {
        if (json::parse(input)) {
            return false;
        }
    }
    const std::string nested(json::maximum_json_depth + 1U, '[');
    const std::string closes(json::maximum_json_depth + 1U, ']');
    return !json::parse(nested + "0" + closes);
}

} // namespace

int elf3d_gltf_json_format_test()
{
    if (!layout_and_tokens()) {
        return 1;
    }
    if (!default_omissions()) {
        return 2;
    }
    if (!float_round_trip() || !matrix_cleaning()) {
        return 3;
    }
    if (!malformed_and_bounded()) {
        return 4;
    }
    return 0;
}
