#include <elf3d/model.h>

#include "../src/buffer_layout_repair.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <elf3d/internal/gltf.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gltf_import_test_support.h"

namespace {
using namespace elf3d::gltf::import_tests;

[[nodiscard]] bool supported_structure_statistics_match(const elf3d::DocumentStatistics& statistics)
{
    return statistics.scenes == 2U && statistics.nodes == 2U && statistics.meshes == 1U &&
           statistics.primitives == 1U && statistics.vertices == 3U && statistics.indices == 3U;
}

[[nodiscard]] bool supported_resource_statistics_match(const elf3d::DocumentStatistics& statistics)
{
    return statistics.materials == 1U && statistics.images == 1U && statistics.textures == 1U &&
           statistics.samplers == 1U && statistics.perspective_cameras == 1U;
}

[[nodiscard]] bool supported_scene_and_geometry_match(const elf3d::LoadedDocument& loaded)
{
    const elf3d::Document& document = loaded.document;
    const auto scene = document.scene_at(1U);
    const auto primitive = document.primitive_at(0U);
    return scene && primitive && loaded.default_scene == scene.value().id &&
           document.default_scene() == scene.value().id &&
           primitive.value().data.indices.size() == 3U && primitive.value().data.indices[0] == 0U &&
           primitive.value().data.indices[1] == 1U && primitive.value().data.indices[2] == 2U &&
           primitive.value().data.tangents.empty();
}

[[nodiscard]] bool supported_material_matches(const elf3d::Document& document)
{
    const auto material = document.material_at(0U);
    return material && material.value().description.alpha_mode == elf3d::AlphaMode::mask &&
           material.value().description.unlit &&
           nearly_equal(material.value().description.ior, 1.33F) &&
           nearly_equal(material.value().description.emissive_strength, 2.0F) &&
           material.value().description.base_color_texture_mapping.transform.offset ==
               elf3d::Float2{0.25F, 0.5F};
}

[[nodiscard]] bool supported_image_sampler_camera_match(const elf3d::Document& document)
{
    const auto image = document.image_at(0U);
    const auto sampler = document.sampler_at(0U);
    const auto camera_node = document.node_at(0U);
    return image && sampler && camera_node &&
           image.value().source_mime_type == elf3d::ModelImageMimeType::png &&
           same_bytes(image.value().source_bytes, std::as_bytes(std::span{asymmetric_png})) &&
           sampler.value().description.wrap_u == elf3d::TextureWrap::clamp_to_edge &&
           sampler.value().description.wrap_v == elf3d::TextureWrap::mirrored_repeat &&
           camera_node.value().perspective_camera.has_value();
}

[[nodiscard]] bool write_supported_files(const TemporaryDirectory& temporary,
                                         const std::filesystem::path& gltf,
                                         std::span<const std::byte> geometry, std::string_view json)
{
    return write_bytes(temporary.path() / "supported.bin", geometry) &&
           write_bytes(temporary.path() / "asymmetric.png",
                       std::as_bytes(std::span{asymmetric_png})) &&
           write_text(gltf, json);
}

[[nodiscard]] bool supported_statistics_match(const elf3d::LoadedDocument& loaded)
{
    const elf3d::DocumentStatistics statistics = loaded.document.statistics();
    return supported_structure_statistics_match(statistics) &&
           supported_resource_statistics_match(statistics);
}

[[nodiscard]] bool supported_content_matches(const elf3d::LoadedDocument& loaded)
{
    return supported_scene_and_geometry_match(loaded) &&
           supported_material_matches(loaded.document) &&
           supported_image_sampler_camera_match(loaded.document);
}

[[nodiscard]] int test_supported_document(const TemporaryDirectory& temporary)
{
    const std::vector<std::byte> geometry = textured_geometry();
    const std::filesystem::path path = temporary.path() / "supported.gltf";
    const std::string json = R"json({
      "asset":{"version":"2.0"},
      "extensionsUsed":["KHR_texture_transform","KHR_materials_unlit","KHR_materials_emissive_strength","KHR_materials_ior","KHR_materials_specular"],
      "extensionsRequired":["KHR_texture_transform","KHR_materials_unlit","KHR_materials_emissive_strength","KHR_materials_ior"],
      "buffers":[{"uri":"supported.bin","byteLength":96}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"asymmetric.png"}],
      "samplers":[{"wrapS":33071,"wrapT":33648,"minFilter":9987,"magFilter":9728}],
      "textures":[{"sampler":0,"source":0}],
      "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,0.4],"metallicFactor":0.25,"roughnessFactor":0.75,"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0.25,0.5],"scale":[2,3],"rotation":0.5}}}},"normalTexture":{"index":0,"scale":0.75},"occlusionTexture":{"index":0,"strength":0.6},"emissiveFactor":[0.1,0.2,0.3],"alphaMode":"MASK","alphaCutoff":0.35,"doubleSided":true,"extensions":{"KHR_materials_unlit":{},"KHR_materials_emissive_strength":{"emissiveStrength":2},"KHR_materials_ior":{"ior":1.33},"KHR_materials_specular":{"specularFactor":0.8,"specularColorFactor":[0.7,0.8,0.9]}}}],
      "cameras":[{"type":"perspective","perspective":{"yfov":0.8,"znear":0.05,"zfar":500}}],
      "meshes":[{"name":"Shared","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],
      "nodes":[{"name":"FirstRoot","mesh":0,"camera":0},{"name":"SecondRoot","mesh":0}],
      "scenes":[{"name":"First","nodes":[0]},{"name":"Second","nodes":[1]}],"scene":1
    })json";
    if (!write_supported_files(temporary, path, geometry, json)) {
        return 1;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 2;
    }
    if (!supported_statistics_match(loaded.value())) {
        return 3;
    }
    if (!supported_content_matches(loaded.value())) {
        return 4;
    }
    return has_diagnostic(loaded.value().report,
                          elf3d::ModelLoadDiagnosticCode::normal_map_fallback)
               ? 5
               : 0;
}

[[nodiscard]] bool sampler_matches(const elf3d::Document& document, std::size_t texture_index,
                                   elf3d::TextureFilter min_filter, elf3d::TextureFilter mag_filter)
{
    const auto texture = document.texture_at(texture_index);
    if (!texture) {
        return false;
    }

    const auto sampler = document.sampler(texture.value().description.sampler);
    return sampler && sampler.value().description.wrap_u == elf3d::TextureWrap::repeat &&
           sampler.value().description.wrap_v == elf3d::TextureWrap::repeat &&
           sampler.value().description.min_filter == min_filter &&
           sampler.value().description.mag_filter == mag_filter;
}

[[nodiscard]] int test_sampler_filter_defaults(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "sampler_defaults.gltf";
    const std::vector<std::byte> geometry = textured_geometry();
    const std::string json = R"json({
      "asset":{"version":"2.0"},
      "buffers":[{"uri":"supported.bin","byteLength":96}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"asymmetric.png"}],
      "samplers":[{}, {"minFilter":9729,"magFilter":9728}],
      "textures":[{"source":0},{"sampler":0,"source":0},{"sampler":1,"source":0}],
      "materials":[
        {"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
        {"pbrMetallicRoughness":{"baseColorTexture":{"index":1}}},
        {"pbrMetallicRoughness":{"baseColorTexture":{"index":2}}}
      ],
      "meshes":[{"primitives":[
        {"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0},
        {"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":1},
        {"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":2}
      ]}],
      "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
    })json";
    if (!write_supported_files(temporary, path, geometry, json)) {
        return 1;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded || loaded.value().document.statistics().textures != 3U ||
        loaded.value().document.statistics().samplers != 3U) {
        return 2;
    }

    const elf3d::Document& document = loaded.value().document;
    return sampler_matches(document, 0U, elf3d::TextureFilter::linear_mipmap_linear,
                           elf3d::TextureFilter::linear) &&
                   sampler_matches(document, 1U, elf3d::TextureFilter::linear_mipmap_linear,
                                   elf3d::TextureFilter::linear) &&
                   sampler_matches(document, 2U, elf3d::TextureFilter::linear,
                                   elf3d::TextureFilter::nearest)
               ? 0
               : 3;
}

[[nodiscard]] std::string hierarchy_json(std::size_t depth)
{
    std::string json = R"json({"asset":{"version":"2.0"},"nodes":[)json";
    for (std::size_t index = 0; index < depth; ++index) {
        if (index != 0U) {
            json.push_back(',');
        }
        if (index + 1U < depth) {
            json.append("{\"children\":[" + std::to_string(index + 1U) + "]}");
        } else {
            json.append("{}");
        }
    }
    json.append(R"json(],"scenes":[{"nodes":[0]}]})json");
    return json;
}

[[nodiscard]] int test_hierarchy_depth_limit(const TemporaryDirectory& temporary)
{
    constexpr std::size_t supported_depth = 5120U;
    constexpr std::size_t over_limit_depth = 8193U;
    const std::filesystem::path supported_path = temporary.path() / "hierarchy_supported.gltf";
    const std::filesystem::path over_limit_path = temporary.path() / "hierarchy_over_limit.gltf";
    if (!write_text(supported_path, hierarchy_json(supported_depth)) ||
        !write_text(over_limit_path, hierarchy_json(over_limit_depth))) {
        return 1;
    }

    const auto supported = elf3d::load_document(supported_path.string());
    if (supported || supported.error().code() != elf3d::ErrorCode::empty_scene_geometry) {
        return 2;
    }

    const auto over_limit = elf3d::load_document(over_limit_path.string());
    return !over_limit && over_limit.error().code() == elf3d::ErrorCode::resource_limit_exceeded
               ? 0
               : 3;
}

[[nodiscard]] std::vector<std::byte> quad_geometry()
{
    std::vector<std::byte> output;
    for (const float value :
         {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F}) {
        append_float(output, value);
    }
    constexpr std::array<std::uint8_t, 4> indices{0U, 1U, 2U, 3U};
    for (const std::uint8_t value : indices) {
        append_byte(output, value);
    }
    return output;
}

[[nodiscard]] int test_strip_and_fan(const TemporaryDirectory& temporary)
{
    const std::vector<std::byte> quad = quad_geometry();
    for (const std::uint32_t mode : {5U, 6U}) {
        const std::filesystem::path path =
            temporary.path() / (mode == 5U ? "strip.gltf" : "fan.gltf");
        const std::string json =
            R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"quad.bin","byteLength":52}],"bufferViews":[{"buffer":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":4,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":)json" +
            std::to_string(mode) + R"json(}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
        if (!write_bytes(temporary.path() / "quad.bin", quad) || !write_text(path, json)) {
            return 1;
        }

        const auto loaded = elf3d::load_document(path.string());
        if (!loaded) {
            return 2;
        }

        const auto primitive = loaded.value().document.primitive_at(0U);
        const std::array<std::uint32_t, 6> strip{0U, 1U, 2U, 2U, 1U, 3U};
        const std::array<std::uint32_t, 6> fan{0U, 1U, 2U, 0U, 2U, 3U};
        const std::span<const std::uint32_t> expected =
            mode == 5U ? std::span{strip} : std::span{fan};
        if (!primitive ||
            !std::equal(primitive.value().data.indices.begin(),
                        primitive.value().data.indices.end(), expected.begin(), expected.end())) {
            return 2;
        }
    }
    return 0;
}

[[nodiscard]] int test_sparse_geometry(const TemporaryDirectory& temporary)
{
    std::vector<std::byte> sparse(40U, std::byte{0});
    sparse[36] = std::byte{1};
    sparse[37] = std::byte{2};
    for (const float value : {1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}) {
        append_float(sparse, value);
    }

    const std::filesystem::path path = temporary.path() / "sparse.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"sparse.bin","byteLength":64}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":2},{"buffer":0,"byteOffset":40,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","sparse":{"count":2,"indices":{"bufferView":1,"componentType":5121},"values":{"bufferView":2}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(temporary.path() / "sparse.bin", sparse) || !write_text(path, json)) {
        return 3;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 4;
    }

    const auto primitive = loaded.value().document.primitive_at(0U);
    if (!primitive || primitive.value().data.positions[1].x != 1.0F ||
        primitive.value().data.positions[2].y != 1.0F) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int test_quantized_geometry(const TemporaryDirectory& temporary)
{
    std::vector<std::byte> quantized;
    constexpr std::array<std::uint16_t, 9> positions{0U, 0U, 0U, 65535U, 0U, 0U, 0U, 65535U, 0U};
    for (const std::uint16_t value : positions) {
        append_u16(quantized, value);
    }

    const std::filesystem::path path = temporary.path() / "quantized.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["KHR_mesh_quantization"],"extensionsRequired":["KHR_mesh_quantization"],"buffers":[{"uri":"quantized.bin","byteLength":18}],"bufferViews":[{"buffer":0,"byteLength":18}],"accessors":[{"bufferView":0,"componentType":5123,"normalized":true,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(temporary.path() / "quantized.bin", quantized) || !write_text(path, json)) {
        return 5;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 6;
    }

    const auto primitive = loaded.value().document.primitive_at(0U);
    return primitive && nearly_equal(primitive.value().data.positions[1].x, 1.0F) &&
                   nearly_equal(primitive.value().data.positions[2].y, 1.0F)
               ? 0
               : 6;
}

using GeometryTest = int (*)(const TemporaryDirectory&);

[[nodiscard]] int test_geometry_paths(const TemporaryDirectory& temporary)
{
    constexpr std::array<GeometryTest, 3> tests{{
        test_strip_and_fan,
        test_sparse_geometry,
        test_quantized_geometry,
    }};
    for (const GeometryTest test : tests) {
        const int result = test(temporary);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

[[nodiscard]] int test_optional_diagnostics(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "optional.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["EXT_optional"],"extensions":{"EXT_optional":{"value":1}},"buffers":[{"uri":"plain.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":0},{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_text(path, json)) {
        return 2;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded ||
        !has_diagnostic(loaded.value().report,
                        elf3d::ModelLoadDiagnosticCode::unsupported_optional_extension) ||
        !has_diagnostic(loaded.value().report,
                        elf3d::ModelLoadDiagnosticCode::skipped_unsupported_primitive) ||
        loaded.value().document.statistics().primitives != 1U) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_required_extension_error(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "required.gltf";
    std::string json = simple_triangle_json("\"uri\":\"plain.bin\",\"byteLength\":36");
    json.insert(json.find("\"buffers\""), "\"extensionsRequired\":[\"EXT_required\"],");
    if (!write_text(path, json)) {
        return 4;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (loaded || loaded.error().code() != elf3d::ErrorCode::unsupported_required_extension) {
        return 5;
    }
    return 0;
}

[[nodiscard]] bool
specular_glossiness_factors_match(const elf3d::ModelMaterialDescription& description) noexcept
{
    return description.base_color == elf3d::Color4{0.25F, 0.5F, 0.75F, 0.8F} &&
           nearly_equal(description.metallic_factor, 0.0F) &&
           nearly_equal(description.roughness_factor, 0.6F);
}

[[nodiscard]] bool
specular_glossiness_textures_match(const elf3d::ModelMaterialDescription& description) noexcept
{
    return description.base_color_texture.is_valid() &&
           !description.metallic_roughness_texture.is_valid();
}

[[nodiscard]] bool
required_specular_glossiness_material_matches(const elf3d::Document& document) noexcept
{
    const auto material = document.material_at(0U);
    return material && specular_glossiness_factors_match(material.value().description) &&
           specular_glossiness_textures_match(material.value().description);
}

[[nodiscard]] int test_required_specular_glossiness_fallback(const TemporaryDirectory& temporary)
{
    const std::filesystem::path path = temporary.path() / "required_specular_glossiness.gltf";
    constexpr std::string_view json =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_pbrSpecularGlossiness"],"extensionsRequired":["KHR_materials_pbrSpecularGlossiness"],"buffers":[{"uri":"specular_glossiness.bin","byteLength":96}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"uri":"specular_glossiness.png"}],"textures":[{"source":0},{"source":0}],"materials":[{"extensions":{"KHR_materials_pbrSpecularGlossiness":{"diffuseFactor":[0.25,0.5,0.75,0.8],"diffuseTexture":{"index":0},"specularFactor":[0.2,0.3,0.4],"glossinessFactor":0.4,"specularGlossinessTexture":{"index":1}}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(temporary.path() / "specular_glossiness.bin", textured_geometry()) ||
        !write_bytes(temporary.path() / "specular_glossiness.png",
                     std::as_bytes(std::span{asymmetric_png})) ||
        !write_text(path, json)) {
        return 6;
    }

    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 7;
    }
    if (!required_specular_glossiness_material_matches(loaded.value().document) ||
        !has_diagnostic(loaded.value().report, elf3d::ModelLoadDiagnosticCode::material_fallback)) {
        return 8;
    }
    return 0;
}

[[nodiscard]] int test_malformed_and_missing_normals(const TemporaryDirectory& temporary)
{
    const std::filesystem::path malformed_path = temporary.path() / "malformed.gltf";
    if (!write_text(malformed_path, "{not-json") || elf3d::load_document(malformed_path.string())) {
        return 6;
    }

    const std::filesystem::path optional_path = temporary.path() / "optional.gltf";
    const auto missing_normals =
        elf3d::load_document(optional_path.string(), elf3d::ModelLoadOptions{false, true});
    if (missing_normals || missing_normals.error().code() != elf3d::ErrorCode::missing_normals) {
        return 7;
    }
    return 0;
}

[[nodiscard]] int test_node_limit(const TemporaryDirectory& temporary)
{
    std::string json = R"json({"asset":{"version":"2.0"},"nodes":[)json";
    for (std::size_t index = 0; index <= 131072U; ++index) {
        if (index != 0U) {
            json.push_back(',');
        }
        json.append("{}");
    }
    json.append("]}");
    const std::filesystem::path path = temporary.path() / "limit.gltf";
    if (!write_text(path, json)) {
        return 8;
    }

    const auto loaded = elf3d::load_document(path.string());
    return !loaded && loaded.error().code() == elf3d::ErrorCode::resource_limit_exceeded ? 0 : 9;
}

[[nodiscard]] int test_signed_glb_buffer_layout_repair(const TemporaryDirectory&)
{
    using elf3d::gltf::importer_detail::GlbBufferViewLayout;
    constexpr std::size_t overflow_offset = 2147483648ULL;
    constexpr std::size_t bin_size = overflow_offset + 16U;
    std::array<GlbBufferViewLayout, 2> views{{{0U, overflow_offset}, {0U, 16U}}};
    const auto repaired =
        elf3d::gltf::importer_detail::recover_signed_glb_buffer_layout(bin_size, views);
    if (repaired.repaired_fields != 2U || repaired.buffer_size != bin_size ||
        views[1].offset != overflow_offset) {
        return 10;
    }

    std::array<GlbBufferViewLayout, 2> invalid_views{{{4U, overflow_offset}, {0U, 12U}}};
    const auto invalid = elf3d::gltf::importer_detail::recover_signed_glb_buffer_layout(
        overflow_offset + 16U, invalid_views);
    return invalid.repaired_fields == 0U && invalid.buffer_size == 0U &&
                   invalid_views[0].offset == 4U
               ? 0
               : 11;
}

using DiagnosticTest = int (*)(const TemporaryDirectory&);

[[nodiscard]] int test_diagnostics_and_errors(const TemporaryDirectory& temporary)
{
    if (!write_bytes(temporary.path() / "plain.bin", triangle_positions())) {
        return 1;
    }
    constexpr std::array<DiagnosticTest, 6> tests{{
        test_optional_diagnostics,
        test_required_specular_glossiness_fallback,
        test_required_extension_error,
        test_malformed_and_missing_normals,
        test_node_limit,
        test_signed_glb_buffer_layout_repair,
    }};
    for (const DiagnosticTest test : tests) {
        const int result = test(temporary);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

[[nodiscard]] int test_invalid_paths()
{
    const elf3d::Document empty_document;
    const std::string_view null_path{"bad\0name", 8};
    const auto null_load = elf3d::load_document(null_path);
    const auto null_save = elf3d::save_document(null_path, empty_document.view());
    if (null_load || null_load.error().code() != elf3d::ErrorCode::invalid_argument || null_save ||
        null_save.error().code() != elf3d::ErrorCode::invalid_argument) {
        return 501;
    }
#if defined(_WIN32)
    const std::string_view invalid_utf8{"\xc3\x28"};
    const auto invalid_load = elf3d::load_document(invalid_utf8);
    const auto invalid_save = elf3d::save_document(invalid_utf8, empty_document.view());
    if (invalid_load || invalid_load.error().code() != elf3d::ErrorCode::invalid_argument ||
        invalid_save || invalid_save.error().code() != elf3d::ErrorCode::invalid_argument) {
        return 502;
    }
#endif
    return 0;
}

} // namespace

int elf3d_gltf_import_test()
{
    if (const int invalid_path = test_invalid_paths(); invalid_path != 0) {
        return invalid_path;
    }
    TemporaryDirectory temporary;
    if (const int result = test_supported_document(temporary); result != 0) {
        return result;
    }
    if (const int result = test_sampler_filter_defaults(temporary); result != 0) {
        return 50 + result;
    }
    if (const int result = test_containers_and_images(temporary); result != 0) {
        return 100 + result;
    }
    if (const int result = test_geometry_paths(temporary); result != 0) {
        return 200 + result;
    }
    if (const int result = test_diagnostics_and_errors(temporary); result != 0) {
        return 300 + result;
    }
    if (const int result = test_hierarchy_depth_limit(temporary); result != 0) {
        return 400 + result;
    }
    return 0;
}
