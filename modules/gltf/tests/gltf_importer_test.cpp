#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

import elf.gltf;
import elf.model;

namespace {

constexpr std::array<std::uint8_t, 77> asymmetric_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
     0x73, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xc0,
     0x00, 0xc2, 0x0c, 0xff, 0xff, 0xff, 0x67, 0x00, 0x00, 0x1e, 0xef, 0x04, 0xfc, 0xa3, 0xc8, 0xb4,
     0xf7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

constexpr std::string_view jpeg_base64 =
    "/9j/4AAQSkZJRgABAQAAAQABAAD/"
    "2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKS"
    "j/"
    "2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKC"
    "j/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAP/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/"
    "8QAFAEBAAAAAAAAAAAAAAAAAAAABv/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AJAB58//2Q==";

[[nodiscard]] std::uint64_t next_suffix() noexcept {
    static std::uint64_t value = 0;
    return ++value;
}

class TemporaryDirectory final {
  public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path() /
                ("elf3d_model_gltf_import_" + std::to_string(next_suffix()))) {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

void append_byte(std::vector<std::byte>& output, std::uint8_t value) {
    output.push_back(static_cast<std::byte>(value));
}

void append_u16(std::vector<std::byte>& output, std::uint16_t value) {
    append_byte(output, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append_u32(std::vector<std::byte>& output, std::uint32_t value) {
    append_byte(output, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    append_byte(output, static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

void append_float(std::vector<std::byte>& output, float value) {
    append_u32(output, std::bit_cast<std::uint32_t>(value));
}

[[nodiscard]] std::vector<std::byte> triangle_positions() {
    std::vector<std::byte> output;
    for (const float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}) {
        append_float(output, value);
    }
    return output;
}

[[nodiscard]] std::vector<std::byte> textured_geometry() {
    std::vector<std::byte> output = triangle_positions();
    for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}) {
        append_float(output, value);
    }
    for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}) {
        append_float(output, value);
    }
    return output;
}

[[nodiscard]] bool write_bytes(const std::filesystem::path& path,
                               std::span<const std::byte> bytes) {
    std::ofstream stream{path, std::ios::binary};
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] bool write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream stream{path, std::ios::binary};
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::string base64(std::span<const std::byte> bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve((bytes.size() + 2U) / 3U * 4U);
    for (std::size_t index = 0; index < bytes.size(); index += 3U) {
        const std::uint32_t first = std::to_integer<std::uint8_t>(bytes[index]);
        const std::uint32_t second =
            index + 1U < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 1U]) : 0U;
        const std::uint32_t third =
            index + 2U < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 2U]) : 0U;
        const std::uint32_t value = (first << 16U) | (second << 8U) | third;
        output.push_back(alphabet[(value >> 18U) & 0x3fU]);
        output.push_back(alphabet[(value >> 12U) & 0x3fU]);
        output.push_back(index + 1U < bytes.size() ? alphabet[(value >> 6U) & 0x3fU] : '=');
        output.push_back(index + 2U < bytes.size() ? alphabet[value & 0x3fU] : '=');
    }
    return output;
}

[[nodiscard]] std::uint32_t decode_base64_value(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<std::uint32_t>(character - 'A');
    }
    if (character >= 'a' && character <= 'z') {
        return static_cast<std::uint32_t>(character - 'a' + 26);
    }
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint32_t>(character - '0' + 52);
    }
    return character == '+' ? 62U : 63U;
}

[[nodiscard]] std::vector<std::byte> decode_base64(std::string_view source) {
    std::vector<std::byte> output;
    for (std::size_t index = 0; index < source.size(); index += 4U) {
        const std::uint32_t combined =
            (decode_base64_value(source[index]) << 18U) |
            (decode_base64_value(source[index + 1U]) << 12U) |
            ((source[index + 2U] == '=' ? 0U : decode_base64_value(source[index + 2U])) << 6U) |
            (source[index + 3U] == '=' ? 0U : decode_base64_value(source[index + 3U]));
        append_byte(output, static_cast<std::uint8_t>((combined >> 16U) & 0xffU));
        if (source[index + 2U] != '=') {
            append_byte(output, static_cast<std::uint8_t>((combined >> 8U) & 0xffU));
        }
        if (source[index + 3U] != '=') {
            append_byte(output, static_cast<std::uint8_t>(combined & 0xffU));
        }
    }
    return output;
}

[[nodiscard]] std::vector<std::byte> make_glb(std::string json, std::vector<std::byte> binary) {
    while (json.size() % 4U != 0U) {
        json.push_back(' ');
    }
    while (binary.size() % 4U != 0U) {
        binary.push_back(std::byte{0});
    }
    std::vector<std::byte> output;
    append_u32(output, 0x46546c67U);
    append_u32(output, 2U);
    append_u32(output, static_cast<std::uint32_t>(20U + json.size() + 8U + binary.size()));
    append_u32(output, static_cast<std::uint32_t>(json.size()));
    append_u32(output, 0x4e4f534aU);
    for (const char character : json) {
        append_byte(output, static_cast<std::uint8_t>(character));
    }
    append_u32(output, static_cast<std::uint32_t>(binary.size()));
    append_u32(output, 0x004e4942U);
    output.insert(output.end(), binary.begin(), binary.end());
    return output;
}

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 0.00001F;
}

[[nodiscard]] bool has_diagnostic(const elf3d::ModelLoadReport& report,
                                  elf3d::ModelLoadDiagnosticCode code) {
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [code](const elf3d::ModelLoadDiagnostic& diagnostic) noexcept {
                           return diagnostic.code == code;
                       });
}

[[nodiscard]] bool same_bytes(std::span<const std::byte> left,
                              std::span<const std::byte> right) noexcept {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

[[nodiscard]] bool
supported_structure_statistics_match(const elf3d::DocumentStatistics& statistics) {
    return statistics.scenes == 2U && statistics.nodes == 2U && statistics.meshes == 1U &&
           statistics.primitives == 1U && statistics.vertices == 3U && statistics.indices == 3U;
}

[[nodiscard]] bool
supported_resource_statistics_match(const elf3d::DocumentStatistics& statistics) {
    return statistics.materials == 1U && statistics.images == 1U && statistics.textures == 1U &&
           statistics.samplers == 1U && statistics.perspective_cameras == 1U;
}

[[nodiscard]] bool supported_scene_and_geometry_match(const elf3d::LoadedDocument& loaded) {
    const elf3d::Document& document = loaded.document;
    const auto scene = document.scene_at(1U);
    const auto primitive = document.primitive_at(0U);
    return scene && primitive && loaded.default_scene == scene.value().id &&
           document.default_scene() == scene.value().id &&
           primitive.value().data.indices.size() == 3U && primitive.value().data.indices[0] == 0U &&
           primitive.value().data.indices[1] == 1U && primitive.value().data.indices[2] == 2U;
}

[[nodiscard]] bool supported_material_matches(const elf3d::Document& document) {
    const auto material = document.material_at(0U);
    return material && material.value().description.alpha_mode == elf3d::ModelAlphaMode::mask &&
           material.value().description.unlit &&
           nearly_equal(material.value().description.ior, 1.33F) &&
           nearly_equal(material.value().description.emissive_strength, 2.0F) &&
           material.value().description.base_color_texture_mapping.transform.offset ==
               elf3d::Float2{0.25F, 0.5F};
}

[[nodiscard]] bool supported_image_sampler_camera_match(const elf3d::Document& document) {
    const auto image = document.image_at(0U);
    const auto sampler = document.sampler_at(0U);
    const auto camera_node = document.node_at(0U);
    return image && sampler && camera_node &&
           image.value().source_mime_type == elf3d::ModelImageMimeType::png &&
           same_bytes(image.value().source_bytes, std::as_bytes(std::span{asymmetric_png})) &&
           sampler.value().description.wrap_u == elf3d::ModelTextureWrap::clamp_to_edge &&
           sampler.value().description.wrap_v == elf3d::ModelTextureWrap::mirrored_repeat &&
           camera_node.value().perspective_camera.has_value();
}

[[nodiscard]] bool write_supported_files(const TemporaryDirectory& temporary,
                                         const std::filesystem::path& gltf,
                                         std::span<const std::byte> geometry,
                                         std::string_view json) {
    return write_bytes(temporary.path() / "supported.bin", geometry) &&
           write_bytes(temporary.path() / "asymmetric.png",
                       std::as_bytes(std::span{asymmetric_png})) &&
           write_text(gltf, json);
}

[[nodiscard]] bool supported_statistics_match(const elf3d::LoadedDocument& loaded) {
    const elf3d::DocumentStatistics statistics = loaded.document.statistics();
    return supported_structure_statistics_match(statistics) &&
           supported_resource_statistics_match(statistics);
}

[[nodiscard]] bool supported_content_matches(const elf3d::LoadedDocument& loaded) {
    return supported_scene_and_geometry_match(loaded) &&
           supported_material_matches(loaded.document) &&
           supported_image_sampler_camera_match(loaded.document);
}

[[nodiscard]] int test_supported_document(const TemporaryDirectory& temporary) {
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
               ? 0
               : 5;
}

[[nodiscard]] std::string simple_triangle_json(std::string_view buffer_member) {
    return std::string{R"json({"asset":{"version":"2.0"},"buffers":[{)json"} +
           std::string{buffer_member} +
           R"json(}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
}

[[nodiscard]] int test_data_uri_buffer(const TemporaryDirectory& temporary) {
    const std::vector<std::byte> positions = triangle_positions();
    const std::filesystem::path path = temporary.path() / "data_buffer.gltf";
    const std::string member = "\"uri\":\"data:application/octet-stream;base64," +
                               base64(positions) + "\",\"byteLength\":36";
    if (!write_text(path, simple_triangle_json(member))) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    if (!loaded ||
        !has_diagnostic(loaded.value().report, elf3d::ModelLoadDiagnosticCode::generated_normals) ||
        loaded.value().document.default_scene().has_value()) {
        return 2;
    }
    return 0;
}

[[nodiscard]] int test_glb_container(const TemporaryDirectory& temporary) {
    const std::filesystem::path path = temporary.path() / "triangle.glb";
    const std::vector<std::byte> positions = triangle_positions();
    const std::string json = simple_triangle_json("\"byteLength\":36");
    if (!write_bytes(path, make_glb(json, positions)) || !elf3d::load_document(path.string())) {
        return 3;
    }
    return 0;
}

[[nodiscard]] int test_implicit_scene(const TemporaryDirectory& temporary) {
    const std::filesystem::path path = temporary.path() / "implicit_scene.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"plain.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}]})json";
    if (!write_bytes(temporary.path() / "plain.bin", triangle_positions()) ||
        !write_text(path, json)) {
        return 4;
    }
    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 5;
    }
    const auto scene = loaded.value().document.scene_at(0U);
    if (!scene || loaded.value().document.scene_count() != 1U ||
        loaded.value().document.default_scene().has_value() ||
        loaded.value().default_scene != scene.value().id || scene.value().roots.size() != 1U) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int test_embedded_image(const TemporaryDirectory& temporary) {
    std::vector<std::byte> embedded = textured_geometry();
    embedded.insert(embedded.end(), std::as_bytes(std::span{asymmetric_png}).begin(),
                    std::as_bytes(std::span{asymmetric_png}).end());
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":173}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":77}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"bufferView":3,"mimeType":"image/png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    const std::filesystem::path path = temporary.path() / "image.glb";
    if (!write_bytes(path, make_glb(json, embedded))) {
        return 6;
    }
    const auto loaded = elf3d::load_document(path.string());
    if (!loaded) {
        return 7;
    }
    const auto image = loaded.value().document.image_at(0U);
    if (!image ||
        !same_bytes(image.value().source_bytes, std::as_bytes(std::span{asymmetric_png}))) {
        return 7;
    }
    return 0;
}

[[nodiscard]] int test_jpeg_image(const TemporaryDirectory& temporary) {
    const std::vector<std::byte> jpeg = decode_base64(jpeg_base64);
    const std::filesystem::path image_path = temporary.path() / "pixel.jpg";
    const std::filesystem::path gltf_path = temporary.path() / "jpeg.gltf";
    const std::string json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"supported.bin","byteLength":96}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"uri":"pixel.jpg"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(image_path, jpeg) || !write_text(gltf_path, json)) {
        return 8;
    }
    const auto loaded = elf3d::load_document(gltf_path.string());
    if (!loaded) {
        return 9;
    }
    const auto image = loaded.value().document.image_at(0U);
    return image && image.value().source_mime_type == elf3d::ModelImageMimeType::jpeg &&
                   same_bytes(image.value().source_bytes, jpeg)
               ? 0
               : 9;
}

using ContainerTest = int (*)(const TemporaryDirectory&);

[[nodiscard]] int test_containers_and_images(const TemporaryDirectory& temporary) {
    constexpr std::array<ContainerTest, 5> tests{{
        test_data_uri_buffer,
        test_glb_container,
        test_implicit_scene,
        test_embedded_image,
        test_jpeg_image,
    }};
    for (const ContainerTest test : tests) {
        const int result = test(temporary);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

[[nodiscard]] int test_hierarchy_depth_limit(const TemporaryDirectory& temporary) {
    std::string json = R"json({"asset":{"version":"2.0"},"nodes":[)json";
    constexpr std::size_t over_limit_depth = 1025U;
    for (std::size_t index = 0; index < over_limit_depth; ++index) {
        if (index != 0U) {
            json.push_back(',');
        }
        if (index + 1U < over_limit_depth) {
            json.append("{\"children\":[" + std::to_string(index + 1U) + "]}");
        } else {
            json.append("{}");
        }
    }
    json.append(R"json(],"scenes":[{"nodes":[0]}]})json");
    const std::filesystem::path path = temporary.path() / "hierarchy_depth.gltf";
    if (!write_text(path, json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    return !loaded && loaded.error().code() == elf3d::ErrorCode::resource_limit_exceeded ? 0 : 2;
}

[[nodiscard]] std::vector<std::byte> quad_geometry() {
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

[[nodiscard]] int test_strip_and_fan(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_sparse_geometry(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_quantized_geometry(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_geometry_paths(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_optional_diagnostics(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_required_extension_error(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_malformed_and_missing_normals(const TemporaryDirectory& temporary) {
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

[[nodiscard]] int test_node_limit(const TemporaryDirectory& temporary) {
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

using DiagnosticTest = int (*)(const TemporaryDirectory&);

[[nodiscard]] int test_diagnostics_and_errors(const TemporaryDirectory& temporary) {
    if (!write_bytes(temporary.path() / "plain.bin", triangle_positions())) {
        return 1;
    }
    constexpr std::array<DiagnosticTest, 4> tests{{
        test_optional_diagnostics,
        test_required_extension_error,
        test_malformed_and_missing_normals,
        test_node_limit,
    }};
    for (const DiagnosticTest test : tests) {
        const int result = test(temporary);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_gltf_import_test() {
    TemporaryDirectory temporary;
    if (const int result = test_supported_document(temporary); result != 0) {
        return result;
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
