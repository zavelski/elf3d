#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

import elf.assets;
import elf.gltf;
import elf.scene;

namespace {

constexpr std::array<std::uint8_t, 77> asymmetric_png{
    {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
     0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a,
     0x73, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xc0,
     0x00, 0xc2, 0x0c, 0xff, 0xff, 0xff, 0x67, 0x00, 0x00, 0x1e, 0xef, 0x04, 0xfc, 0xa3, 0xc8, 0xb4,
     0xf7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82}};

std::span<const std::byte> byte_span(std::span<const std::uint8_t> values) {
    return std::as_bytes(values);
}

class TemporaryDirectory final {
  public:
    explicit TemporaryDirectory(const std::filesystem::path& retained_path = {}) {
        if (!retained_path.empty()) {
            path_ = retained_path;
            retain_ = true;
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        } else {
            const auto suffix =
                std::chrono::high_resolution_clock::now().time_since_epoch().count();
            path_ = std::filesystem::temp_directory_path() /
                    ("elf3d_gltf_test_" + std::to_string(suffix));
        }
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        if (retain_) {
            return;
        }
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
    bool retain_ = false;
};

void append_byte(std::vector<std::byte>& bytes, std::uint8_t value) {
    bytes.push_back(static_cast<std::byte>(value));
}

void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    append_byte(bytes, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(bytes, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    append_byte(bytes, static_cast<std::uint8_t>(value & 0xffU));
    append_byte(bytes, static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    append_byte(bytes, static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    append_byte(bytes, static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

void append_float(std::vector<std::byte>& bytes, float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

[[nodiscard]] bool write_bytes(const std::filesystem::path& path,
                               std::span<const std::byte> bytes) {
    std::ofstream stream{path, std::ios::binary};
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] bool write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream{path, std::ios::binary};
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::string base64(std::span<const std::byte> bytes) {
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::uint32_t first = std::to_integer<std::uint8_t>(bytes[index]);
        const std::uint32_t second =
            index + 1 < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 1]) : 0;
        const std::uint32_t third =
            index + 2 < bytes.size() ? std::to_integer<std::uint8_t>(bytes[index + 2]) : 0;
        const std::uint32_t value = (first << 16U) | (second << 8U) | third;
        result.push_back(alphabet[(value >> 18U) & 0x3fU]);
        result.push_back(alphabet[(value >> 12U) & 0x3fU]);
        result.push_back(index + 1 < bytes.size() ? alphabet[(value >> 6U) & 0x3fU] : '=');
        result.push_back(index + 2 < bytes.size() ? alphabet[value & 0x3fU] : '=');
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> triangle_positions() {
    std::vector<std::byte> bytes;
    for (const float value : {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}) {
        append_float(bytes, value);
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte> textured_geometry() {
    std::vector<std::byte> bytes = triangle_positions();
    for (const float value : {0.0F, 0.0F, 2.0F, 0.0F, 0.0F, -1.0F}) {
        append_float(bytes, value);
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte> dual_uv_geometry() {
    std::vector<std::byte> bytes = triangle_positions();
    for (const float value : {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F}) {
        append_float(bytes, value);
    }
    for (const float value : {0.25F, 0.25F, 0.75F, 0.25F, 0.25F, 0.75F}) {
        append_float(bytes, value);
    }
    for (const float value :
         {1.0F, 0.0F, 0.0F, 0.5F, 0.0F, 1.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F}) {
        append_float(bytes, value);
    }
    return bytes;
}

[[nodiscard]] std::string dual_uv_json() {
    return R"json({
  "asset":{"version":"2.0"},
  "extensionsUsed":["KHR_texture_transform","KHR_materials_unlit","KHR_materials_emissive_strength","KHR_materials_ior","KHR_materials_specular"],
  "extensionsRequired":["KHR_texture_transform","KHR_materials_unlit","KHR_materials_emissive_strength","KHR_materials_ior","KHR_materials_specular"],
  "buffers":[{"uri":"dual_uv.bin","byteLength":132}],
  "bufferViews":[
    {"buffer":0,"byteLength":36},
    {"buffer":0,"byteOffset":36,"byteLength":24},
    {"buffer":0,"byteOffset":60,"byteLength":24},
    {"buffer":0,"byteOffset":84,"byteLength":48}
  ],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
    {"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"}
  ],
  "images":[{"uri":"asymmetric.png"}],
  "textures":[{"source":0}],
  "materials":[{
    "pbrMetallicRoughness":{
      "baseColorFactor":[0.5,0.6,0.7,0.4],
      "baseColorTexture":{"index":0,"texCoord":0,"extensions":{"KHR_texture_transform":{"offset":[0.25,0.5],"scale":[2.0,3.0],"rotation":0.5,"texCoord":1}}},
      "metallicRoughnessTexture":{"index":0,"texCoord":0}
    },
    "normalTexture":{"index":0,"texCoord":1,"scale":0.75},
    "occlusionTexture":{"index":0,"texCoord":1,"strength":0.6},
    "emissiveTexture":{"index":0,"texCoord":1},
    "emissiveFactor":[0.1,0.2,0.3],
    "alphaMode":"MASK",
    "alphaCutoff":0.35,
    "extensions":{
      "KHR_materials_unlit":{},
      "KHR_materials_emissive_strength":{"emissiveStrength":2.0},
      "KHR_materials_ior":{"ior":1.33},
      "KHR_materials_specular":{"specularFactor":0.75,"specularColorFactor":[0.8,0.9,1.0]}
    }
  }],
  "cameras":[{"type":"perspective","perspective":{"yfov":0.8,"znear":0.05,"zfar":500.0}}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1,"TEXCOORD_1":2,"COLOR_0":3},"material":0}]}],
  "nodes":[{"name":"DualUvCameraModel","mesh":0,"camera":0}],
  "scenes":[{"nodes":[0]}],"scene":0
})json";
}

[[nodiscard]] std::vector<std::byte> quad_geometry(bool include_indices) {
    std::vector<std::byte> bytes;
    for (const float value :
         {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F}) {
        append_float(bytes, value);
    }
    if (include_indices) {
        constexpr std::array<std::uint8_t, 4> indices{{0U, 1U, 2U, 3U}};
        for (const std::uint8_t index : indices) {
            append_byte(bytes, index);
        }
    }
    return bytes;
}

[[nodiscard]] std::vector<std::byte> quantized_positions() {
    constexpr std::array<std::uint16_t, 9> values{{0, 0, 0, 65535, 0, 0, 0, 65535, 0}};
    std::vector<std::byte> bytes;
    for (const std::uint16_t value : values) {
        append_u16(bytes, value);
    }
    return bytes;
}

[[nodiscard]] std::string textured_json(std::string_view image_member,
                                        std::string_view sampler_member) {
    return std::string{
               R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"textured.bin","byteLength":60}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{)json"} +
           std::string{image_member} + R"json(}],"samplers":[{)json" + std::string{sampler_member} +
           R"json(}],"textures":[{"sampler":0,"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.5,0.6,0.7,0.8],"metallicFactor":0.25,"roughnessFactor":0.75,"baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0}},"doubleSided":true}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})json";
}

[[nodiscard]] std::vector<std::byte> decode_test_base64(std::string_view source) {
    const auto value = [](char character) -> std::uint32_t {
        if (character >= 'A' && character <= 'Z') {
            return character - 'A';
        }
        if (character >= 'a' && character <= 'z') {
            return character - 'a' + 26;
        }
        if (character >= '0' && character <= '9') {
            return character - '0' + 52;
        }
        return character == '+' ? 62U : character == '/' ? 63U : 0U;
    };
    std::vector<std::byte> result;
    for (std::size_t index = 0; index < source.size(); index += 4) {
        const std::uint32_t combined =
            (value(source[index]) << 18U) | (value(source[index + 1]) << 12U) |
            ((source[index + 2] == '=' ? 0U : value(source[index + 2])) << 6U) |
            (source[index + 3] == '=' ? 0U : value(source[index + 3]));
        result.push_back(static_cast<std::byte>((combined >> 16U) & 0xffU));
        if (source[index + 2] != '=') {
            result.push_back(static_cast<std::byte>((combined >> 8U) & 0xffU));
        }
        if (source[index + 3] != '=') {
            result.push_back(static_cast<std::byte>(combined & 0xffU));
        }
    }
    return result;
}

[[nodiscard]] std::vector<std::byte> rich_buffer() {
    std::vector<std::byte> bytes;
    const std::vector<std::byte> positions = triangle_positions();
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        bytes.insert(bytes.end(), positions.begin() + static_cast<std::ptrdiff_t>(vertex * 12),
                     positions.begin() + static_cast<std::ptrdiff_t>((vertex + 1) * 12));
        append_float(bytes, 99.0F);
    }
    for (int vertex = 0; vertex < 3; ++vertex) {
        append_byte(bytes, 0);
        append_byte(bytes, 0);
        append_byte(bytes, 127);
    }
    append_byte(bytes, 0);
    append_byte(bytes, 1);
    append_byte(bytes, 2);
    append_u16(bytes, 0);
    append_u16(bytes, 1);
    append_u16(bytes, 2);
    append_byte(bytes, 0);
    append_byte(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 1);
    append_u32(bytes, 2);
    return bytes;
}

[[nodiscard]] std::string rich_gltf_json() {
    return R"json({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "rich.bin", "byteLength": 80}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48, "byteStride": 16},
    {"buffer": 0, "byteOffset": 48, "byteLength": 9},
    {"buffer": 0, "byteOffset": 57, "byteLength": 3},
    {"buffer": 0, "byteOffset": 60, "byteLength": 6},
    {"buffer": 0, "byteOffset": 68, "byteLength": 12}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5120, "normalized": true, "count": 3, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5121, "count": 3, "type": "SCALAR"},
    {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
    {"bufferView": 4, "componentType": 5125, "count": 3, "type": "SCALAR"}
  ],
  "materials": [
    {"pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.1, 0.2, 0.5]}, "doubleSided": true},
    {"pbrMetallicRoughness": {"baseColorFactor": [0.1, 0.8, 0.2, 1.0]}}
  ],
  "meshes": [{"name": "SharedMesh", "primitives": [
    {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 2, "material": 0},
    {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 3, "material": 1},
    {"attributes": {"POSITION": 0, "NORMAL": 1}, "indices": 4}
  ]}],
  "nodes": [
    {"name": "Root", "translation": [2, 0, 0], "children": [1, 2]},
    {"name": "MatrixNode", "matrix": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,3,0,1], "mesh": 0},
    {"name": "Instance", "translation": [-4, 0, 0], "mesh": 0}
  ],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})json";
}

[[nodiscard]] std::vector<std::byte> make_glb(const std::string& json,
                                              std::span<const std::byte> binary) {
    std::string padded_json = json;
    while (padded_json.size() % 4 != 0) {
        padded_json.push_back(' ');
    }
    std::vector<std::byte> padded_binary(binary.begin(), binary.end());
    while (padded_binary.size() % 4 != 0) {
        padded_binary.push_back(std::byte{0});
    }

    std::vector<std::byte> result;
    const std::uint32_t total_length =
        static_cast<std::uint32_t>(12 + 8 + padded_json.size() + 8 + padded_binary.size());
    append_u32(result, 0x46546c67U);
    append_u32(result, 2);
    append_u32(result, total_length);
    append_u32(result, static_cast<std::uint32_t>(padded_json.size()));
    append_u32(result, 0x4e4f534aU);
    for (const char character : padded_json) {
        append_byte(result, static_cast<std::uint8_t>(character));
    }
    append_u32(result, static_cast<std::uint32_t>(padded_binary.size()));
    append_u32(result, 0x004e4942U);
    result.insert(result.end(), padded_binary.begin(), padded_binary.end());
    return result;
}

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) {
    return elf3d::detail::SceneHandleAccess::create_scene(1, value);
}

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) < 0.0001F;
}

} // namespace

int main(int argument_count, char** arguments) {
    TemporaryDirectory temporary{argument_count >= 2 && arguments[1] != nullptr
                                     ? std::filesystem::path{arguments[1]}
                                     : std::filesystem::path{}};
    const std::filesystem::path rich_path = temporary.path() / "rich.gltf";
    const std::vector<std::byte> rich_bytes = rich_buffer();
    if (rich_bytes.size() != 80 || !write_bytes(temporary.path() / "rich.bin", rich_bytes) ||
        !write_text(rich_path, rich_gltf_json())) {
        return 1;
    }

    elf3d::scene::Storage rich_storage{scene_id(1)};
    elf3d::scene::Storage& rich_builder{rich_storage};
    const elf3d::Result<elf3d::gltf::ImportReport> rich_result =
        elf3d::gltf::import_scene(rich_path, {}, rich_builder);
    if (!rich_result) {
        return 2;
    }
    const elf3d::SceneStatistics rich_statistics = rich_storage.statistics();
    const elf3d::SceneStatistics expected_rich{3, 2, 3, 3, 3, 9, 9, 3};
    if (rich_statistics != expected_rich || rich_storage.assets().meshes().size() != 3 ||
        rich_storage.assets().materials().size() != 3) {
        return 3;
    }
    const auto entities = rich_storage.entities();
    if (!entities[0].has_value() || !entities[1].has_value() || !entities[2].has_value() ||
        entities[1]->model->primitives.size() != 3 || entities[2]->model->primitives.size() != 3 ||
        entities[1]->model->primitives[0].mesh != entities[2]->model->primitives[0].mesh ||
        rich_storage.entity_name(entities[1]->id).value() != "MatrixNode") {
        return 4;
    }
    const auto matrix = rich_storage.local_matrix(entities[1]->id);
    if (!matrix || !nearly_equal(matrix.value().elements[13], 3.0F)) {
        return 5;
    }
    const std::optional<elf3d::Bounds3> rich_bounds = rich_storage.world_bounds();
    if (!rich_bounds.has_value() || !nearly_equal(rich_bounds->minimum.x, -2.0F) ||
        !nearly_equal(rich_bounds->minimum.y, 0.0F) ||
        !nearly_equal(rich_bounds->maximum.x, 3.0F) ||
        !nearly_equal(rich_bounds->maximum.y, 4.0F)) {
        return 6;
    }
    const auto& first_mesh = rich_storage.assets().meshes()[0];
    if (!nearly_equal(first_mesh.vertices[1].position.x, 1.0F) ||
        !nearly_equal(first_mesh.vertices[0].normal.z, 1.0F) ||
        rich_storage.assets().materials()[0].description.base_color.alpha != 0.5F ||
        !rich_storage.assets().materials()[0].description.double_sided ||
        rich_storage.assets().materials()[2].description.base_color !=
            elf3d::Color4{1.0F, 1.0F, 1.0F, 1.0F}) {
        return 7;
    }

    const std::vector<std::byte> positions = triangle_positions();
    const elf3d::SceneStatistics rich_before_failed_import = rich_storage.statistics();
    const std::uint64_t rich_revision_before_failed_import = rich_storage.revision();
    const std::filesystem::path partial_failure_path = temporary.path() / "partial_failure.gltf";
    const std::string partial_failure_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"partial_failure.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"cameras":[{"type":"perspective","perspective":{"yfov":0.8,"znear":1.0,"zfar":0.5}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0,"camera":0}],"scenes":[{"nodes":[0]}],"scene":0})json";
    const auto partial_failure_result =
        write_bytes(temporary.path() / "partial_failure.bin", positions) &&
                write_text(partial_failure_path, partial_failure_json)
            ? elf3d::gltf::import_scene(partial_failure_path, {}, rich_builder)
            : elf3d::Result<elf3d::gltf::ImportReport>{
                  elf3d::Error{elf3d::ErrorCode::source_file_read_failed,
                               "Could not write partial-failure fixture"}};
    if (partial_failure_result ||
        partial_failure_result.error().code() != elf3d::ErrorCode::invalid_camera_configuration ||
        rich_storage.statistics() != rich_before_failed_import ||
        rich_storage.revision() != rich_revision_before_failed_import) {
        return 68;
    }

    const std::filesystem::path invalid_transform_path =
        temporary.path() / "invalid_transform.gltf";
    const std::string invalid_transform_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"invalid_transform.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"name":"BadRoot","matrix":[0,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1],"children":[1]},{"name":"BadChild","mesh":0},{"name":"GoodRoot","mesh":0}],"scenes":[{"nodes":[0,2]}],"scene":0})json";
    if (!write_bytes(temporary.path() / "invalid_transform.bin", positions) ||
        !write_text(invalid_transform_path, invalid_transform_json)) {
        return 69;
    }
    elf3d::scene::Storage invalid_transform_storage{scene_id(45)};
    elf3d::scene::Storage& invalid_transform_builder{invalid_transform_storage};
    const auto invalid_transform_result =
        elf3d::gltf::import_scene(invalid_transform_path, {}, invalid_transform_builder);
    const bool has_invalid_transform_diagnostic =
        invalid_transform_result &&
        std::any_of(invalid_transform_result.value().diagnostics.begin(),
                    invalid_transform_result.value().diagnostics.end(),
                    [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                        return diagnostic.category == elf3d::SceneLoadDiagnosticCategory::scene &&
                               diagnostic.code ==
                                   elf3d::SceneLoadDiagnosticCode::skipped_invalid_transform;
                    });
    if (!invalid_transform_result || !has_invalid_transform_diagnostic ||
        invalid_transform_storage.statistics().model_entities != 1) {
        return 70;
    }

    const std::filesystem::path all_invalid_transform_path =
        temporary.path() / "all_invalid_transform.gltf";
    const std::string all_invalid_transform_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"all_invalid_transform.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"name":"OnlyBad","matrix":[1,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,1],"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})json";
    if (!write_bytes(temporary.path() / "all_invalid_transform.bin", positions) ||
        !write_text(all_invalid_transform_path, all_invalid_transform_json)) {
        return 71;
    }
    if (elf3d::gltf::import_scene(all_invalid_transform_path, {}, invalid_transform_builder)
            .error()
            .code() != elf3d::ErrorCode::empty_scene_geometry) {
        return 72;
    }

    const std::filesystem::path texture_fallback_path = temporary.path() / "texture_fallback.gltf";
    if (!write_bytes(temporary.path() / "textured.bin", textured_geometry()) ||
        !write_text(texture_fallback_path,
                    textured_json("\"uri\":\"data:image/webp;base64,AAAA\"", ""))) {
        return 73;
    }
    elf3d::scene::Storage texture_fallback_storage{scene_id(46)};
    elf3d::scene::Storage& texture_fallback_builder{texture_fallback_storage};
    const auto texture_fallback_result =
        elf3d::gltf::import_scene(texture_fallback_path, {}, texture_fallback_builder);
    const bool has_texture_fallback =
        texture_fallback_result &&
        std::any_of(texture_fallback_result.value().diagnostics.begin(),
                    texture_fallback_result.value().diagnostics.end(),
                    [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                        return diagnostic.category == elf3d::SceneLoadDiagnosticCategory::texture &&
                               diagnostic.code == elf3d::SceneLoadDiagnosticCode::texture_fallback;
                    });
    if (!texture_fallback_result || !has_texture_fallback ||
        texture_fallback_storage.statistics().texture_assets != 0) {
        return 74;
    }

    const std::string glb_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})json";
    const std::filesystem::path glb_path = temporary.path() / "missing_normals.glb";
    const std::vector<std::byte> glb = make_glb(glb_json, positions);
    if (!write_bytes(glb_path, glb)) {
        return 8;
    }
    elf3d::scene::Storage glb_storage{scene_id(2)};
    elf3d::scene::Storage& glb_builder{glb_storage};
    const auto glb_result = elf3d::gltf::import_scene(glb_path, {}, glb_builder);
    if (!glb_result || glb_storage.statistics() != elf3d::SceneStatistics{1, 1, 1, 1, 1, 3, 3, 1}) {
        return 9;
    }
    for (const elf3d::VertexPositionNormalTexCoord& vertex :
         glb_storage.assets().meshes()[0].vertices) {
        const float length =
            std::sqrt(vertex.normal.x * vertex.normal.x + vertex.normal.y * vertex.normal.y +
                      vertex.normal.z * vertex.normal.z);
        if (!std::isfinite(length) || !nearly_equal(length, 1.0F) ||
            !nearly_equal(vertex.normal.z, 1.0F)) {
            return 10;
        }
    }

    const std::string invalid_normal_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":72}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})json";
    std::vector<std::byte> invalid_normal_bytes = positions;
    invalid_normal_bytes.insert(invalid_normal_bytes.end(), 36, std::byte{0});
    const std::filesystem::path invalid_normal_path = temporary.path() / "invalid_normals.glb";
    const std::vector<std::byte> invalid_normal_glb =
        make_glb(invalid_normal_json, invalid_normal_bytes);
    if (!write_bytes(invalid_normal_path, invalid_normal_glb)) {
        return 75;
    }
    elf3d::scene::Storage invalid_normal_storage{scene_id(47)};
    elf3d::scene::Storage& invalid_normal_builder{invalid_normal_storage};
    const auto invalid_normal_result =
        elf3d::gltf::import_scene(invalid_normal_path, {}, invalid_normal_builder);
    const bool regenerated_invalid_normals =
        invalid_normal_result &&
        std::any_of(invalid_normal_result.value().diagnostics.begin(),
                    invalid_normal_result.value().diagnostics.end(),
                    [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                        return diagnostic.category ==
                                   elf3d::SceneLoadDiagnosticCategory::geometry &&
                               diagnostic.code == elf3d::SceneLoadDiagnosticCode::generated_normals;
                    });
    if (!invalid_normal_result || !regenerated_invalid_normals ||
        !nearly_equal(invalid_normal_storage.assets().meshes()[0].vertices[0].normal.z, 1.0F)) {
        return 76;
    }

    elf3d::scene::Storage disabled_storage{scene_id(3)};
    elf3d::scene::Storage& disabled_builder{disabled_storage};
    elf3d::SceneLoadOptions disabled_options;
    disabled_options.generate_missing_normals = false;
    if (elf3d::gltf::import_scene(glb_path, disabled_options, disabled_builder).error().code() !=
        elf3d::ErrorCode::missing_normals) {
        return 11;
    }
    elf3d::scene::Storage disabled_invalid_normal_storage{scene_id(48)};
    elf3d::scene::Storage& disabled_invalid_normal_builder{disabled_invalid_normal_storage};
    if (elf3d::gltf::import_scene(invalid_normal_path, disabled_options,
                                  disabled_invalid_normal_builder)
            .error()
            .code() != elf3d::ErrorCode::invalid_accessor) {
        return 77;
    }

    const std::filesystem::path degenerate_path = temporary.path() / "degenerate.glb";
    const std::vector<std::byte> degenerate_positions(36, std::byte{0});
    const std::vector<std::byte> degenerate_glb = make_glb(glb_json, degenerate_positions);
    if (!write_bytes(degenerate_path, degenerate_glb)) {
        return 12;
    }
    elf3d::scene::Storage degenerate_storage{scene_id(6)};
    elf3d::scene::Storage& degenerate_builder{degenerate_storage};
    const auto degenerate_result =
        elf3d::gltf::import_scene(degenerate_path, {}, degenerate_builder);
    if (!degenerate_result || degenerate_result.value().diagnostics.empty() ||
        degenerate_storage.assets().meshes()[0].vertices[0].normal !=
            elf3d::Float3{0.0F, 1.0F, 0.0F}) {
        return 13;
    }

    const std::filesystem::path data_path = temporary.path() / "data.gltf";
    const std::string data_json =
        "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"data:application/"
        "octet-stream;base64," +
        base64(positions) +
        "\",\"byteLength\":36}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],\"meshes\":[{\"primitives\":[{\"attributes\":{"
        "\"POSITION\":0}}]}],\"nodes\":[{\"mesh\":0}]}";
    if (!write_text(data_path, data_json)) {
        return 14;
    }
    elf3d::scene::Storage data_storage{scene_id(4)};
    elf3d::scene::Storage& data_builder{data_storage};
    if (!elf3d::gltf::import_scene(data_path, {}, data_builder)) {
        return 15;
    }

    const std::filesystem::path missing_path = temporary.path() / "missing.gltf";
    const std::string missing_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"absent.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_text(missing_path, missing_json)) {
        return 16;
    }
    elf3d::scene::Storage invalid_storage{scene_id(5)};
    elf3d::scene::Storage& invalid_builder{invalid_storage};
    if (elf3d::gltf::import_scene(missing_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::missing_external_buffer) {
        return 17;
    }

    const std::filesystem::path remote_path = temporary.path() / "remote.gltf";
    std::string remote_json = missing_json;
    remote_json.replace(remote_json.find("absent.bin"), std::string{"absent.bin"}.size(),
                        "https://example.com/model.bin");
    if (!write_text(remote_path, remote_json) ||
        elf3d::gltf::import_scene(remote_path, {}, invalid_builder).error().code() !=
            elf3d::ErrorCode::unsupported_remote_uri) {
        return 18;
    }

    const std::filesystem::path range_path = temporary.path() / "range.gltf";
    if (!write_bytes(temporary.path() / "range.bin", positions)) {
        return 19;
    }
    const std::string range_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"range.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":12}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}]})json";
    if (!write_text(range_path, range_json) ||
        elf3d::gltf::import_scene(range_path, {}, invalid_builder).error().code() !=
            elf3d::ErrorCode::invalid_buffer_range) {
        return 20;
    }

    const std::filesystem::path cycle_path = temporary.path() / "cycle.gltf";
    if (!write_text(
            cycle_path,
            R"json({"asset":{"version":"2.0"},"nodes":[{"children":[1]},{"children":[0]}]})json") ||
        elf3d::gltf::import_scene(cycle_path, {}, invalid_builder).error().code() !=
            elf3d::ErrorCode::gltf_validation_failed) {
        return 21;
    }

    const std::filesystem::path bad_index_path = temporary.path() / "bad_index.gltf";
    std::vector<std::byte> bad_index_buffer = positions;
    append_byte(bad_index_buffer, 0);
    append_byte(bad_index_buffer, 1);
    append_byte(bad_index_buffer, 3);
    if (!write_bytes(temporary.path() / "bad_index.bin", bad_index_buffer)) {
        return 22;
    }
    const std::string bad_index_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"bad_index.bin","byteLength":39}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":3}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":3,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_text(bad_index_path, bad_index_json)) {
        return 23;
    }
    if (elf3d::gltf::import_scene(bad_index_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::imported_index_out_of_range) {
        return 24;
    }

    const std::filesystem::path unsupported_path = temporary.path() / "unsupported.gltf";
    const std::string unsupported_json =
        "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"data:application/"
        "octet-stream;base64," +
        base64(positions) +
        "\",\"byteLength\":36}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],\"meshes\":[{\"primitives\":[{\"attributes\":{"
        "\"POSITION\":0},\"mode\":1}]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{"
        "\"nodes\":[0]}]}";
    if (!write_text(unsupported_path, unsupported_json)) {
        return 25;
    }
    if (elf3d::gltf::import_scene(unsupported_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::empty_scene_geometry) {
        return 26;
    }

    const std::filesystem::path required_path = temporary.path() / "required.gltf";
    if (!write_text(
            required_path,
            R"json({"asset":{"version":"2.0"},"extensionsRequired":["EXT_meshopt_compression"]})json")) {
        return 27;
    }
    if (elf3d::gltf::import_scene(required_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::unsupported_required_extension) {
        return 28;
    }

    const std::filesystem::path truncated_path = temporary.path() / "truncated.glb";
    std::vector<std::byte> truncated;
    append_u32(truncated, 0x46546c67U);
    append_u32(truncated, 2);
    if (!write_bytes(truncated_path, truncated)) {
        return 29;
    }
    if (elf3d::gltf::import_scene(truncated_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::malformed_glb) {
        return 30;
    }

    const std::filesystem::path limit_path = temporary.path() / "limit.gltf";
    std::string limit_json = "{\"asset\":{\"version\":\"2.0\"},\"nodes\":[";
    for (std::size_t index = 0; index < 65537; ++index) {
        if (index != 0) {
            limit_json.push_back(',');
        }
        limit_json += "{}";
    }
    limit_json += "]}";
    if (!write_text(limit_path, limit_json)) {
        return 31;
    }
    if (elf3d::gltf::import_scene(limit_path, {}, invalid_builder).error().code() !=
        elf3d::ErrorCode::resource_limit_exceeded) {
        return 32;
    }

    const std::vector<std::byte> textured_bytes = textured_geometry();
    if (!write_bytes(temporary.path() / "textured.bin", textured_bytes) ||
        !write_bytes(temporary.path() / "asymmetric.png", byte_span(asymmetric_png))) {
        return 33;
    }
    const std::filesystem::path textured_path = temporary.path() / "textured.gltf";
    if (!write_text(textured_path,
                    textured_json("\"uri\":\"asymmetric.png\"",
                                  "\"wrapS\":33071,\"wrapT\":33648,\"minFilter\":9987,"
                                  "\"magFilter\":9728"))) {
        return 34;
    }
    elf3d::scene::Storage textured_storage{scene_id(7)};
    elf3d::scene::Storage& textured_builder{textured_storage};
    const auto textured_result = elf3d::gltf::import_scene(textured_path, {}, textured_builder);
    const elf3d::SceneStatistics expected_textured{1, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 16, 1, 1};
    if (!textured_result || textured_storage.statistics() != expected_textured ||
        textured_storage.assets().images().size() != 1 ||
        textured_storage.assets().textures().size() != 1) {
        return 35;
    }
    const auto& textured_mesh = textured_storage.assets().meshes()[0];
    const auto& textured_texture = textured_storage.assets().textures()[0].description;
    const auto& textured_material = textured_storage.assets().materials()[0].description;
    if (textured_mesh.vertices[1].texcoord0 != elf3d::Float2{2.0F, 0.0F} ||
        textured_mesh.vertices[2].texcoord0 != elf3d::Float2{0.0F, -1.0F} ||
        textured_texture.sampler.wrap_u != elf3d::TextureWrap::clamp_to_edge ||
        textured_texture.sampler.wrap_v != elf3d::TextureWrap::mirrored_repeat ||
        textured_texture.sampler.min_filter != elf3d::TextureFilter::linear_mipmap_linear ||
        textured_texture.sampler.mag_filter != elf3d::TextureFilter::nearest ||
        textured_material.base_color_texture != textured_material.metallic_roughness_texture ||
        !nearly_equal(textured_material.metallic_factor, 0.25F) ||
        !nearly_equal(textured_material.roughness_factor, 0.75F) ||
        !textured_material.double_sided ||
        std::to_integer<std::uint8_t>(textured_storage.assets().images()[0].pixels[0]) != 255U ||
        std::to_integer<std::uint8_t>(textured_storage.assets().images()[0].pixels[8]) != 0U ||
        std::to_integer<std::uint8_t>(textured_storage.assets().images()[0].pixels[10]) != 255U) {
        return 36;
    }

    const std::filesystem::path data_image_path = temporary.path() / "data_image.gltf";
    const std::string image_data_uri =
        "\"uri\":\"data:image\\/png;base64," + base64(byte_span(asymmetric_png)) + "\"";
    if (!write_text(data_image_path, textured_json(image_data_uri, ""))) {
        return 37;
    }
    elf3d::scene::Storage data_image_storage{scene_id(8)};
    elf3d::scene::Storage& data_image_builder{data_image_storage};
    if (!elf3d::gltf::import_scene(data_image_path, {}, data_image_builder) ||
        data_image_storage.assets().textures()[0].description.sampler !=
            elf3d::SamplerDescription{}) {
        return 38;
    }

    std::vector<std::byte> embedded_binary = textured_bytes;
    embedded_binary.insert(embedded_binary.end(), byte_span(asymmetric_png).begin(),
                           byte_span(asymmetric_png).end());
    const std::string embedded_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"byteLength":137}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":77}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],"images":[{"bufferView":2,"mimeType":"image\/png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    const std::filesystem::path image_glb_path = temporary.path() / "embedded_image.glb";
    const std::vector<std::byte> image_glb = make_glb(embedded_json, embedded_binary);
    if (!write_bytes(image_glb_path, image_glb)) {
        return 39;
    }
    elf3d::scene::Storage image_glb_storage{scene_id(9)};
    elf3d::scene::Storage& image_glb_builder{image_glb_storage};
    if (!elf3d::gltf::import_scene(image_glb_path, {}, image_glb_builder) ||
        image_glb_storage.statistics().image_assets != 1 ||
        image_glb_storage.statistics().texture_assets != 1) {
        return 40;
    }

    const std::string malformed_data_json =
        textured_json("\"uri\":\"data:image/png;base64,@@@=\"", "");
    const std::filesystem::path malformed_data_path = temporary.path() / "bad_image_data.gltf";
    if (!write_text(malformed_data_path, malformed_data_json) ||
        elf3d::gltf::import_scene(malformed_data_path, {}, invalid_builder).error().code() !=
            elf3d::ErrorCode::invalid_base64_payload) {
        return 41;
    }

    std::string mismatch_json = textured_json("\"uri\":\"asymmetric.png\"", "");
    const std::string valid_uv_accessor =
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}";
    const std::size_t uv_position = mismatch_json.find(valid_uv_accessor);
    mismatch_json.replace(uv_position, valid_uv_accessor.size(),
                          "{\"bufferView\":1,\"componentType\":5126,\"count\":2,"
                          "\"type\":\"VEC2\"}");
    const std::filesystem::path mismatch_path = temporary.path() / "uv_mismatch.gltf";
    if (!write_text(mismatch_path, mismatch_json)) {
        return 42;
    }
    const auto mismatch_result = elf3d::gltf::import_scene(mismatch_path, {}, invalid_builder);
    if (mismatch_result ||
        mismatch_result.error().code() != elf3d::ErrorCode::mismatched_texcoord_count) {
        return 42;
    }

    constexpr std::array<int, 6> gltf_filters{{9728, 9729, 9984, 9985, 9986, 9987}};
    constexpr std::array<elf3d::TextureFilter, 6> expected_filters{
        {elf3d::TextureFilter::nearest, elf3d::TextureFilter::linear,
         elf3d::TextureFilter::nearest_mipmap_nearest, elf3d::TextureFilter::linear_mipmap_nearest,
         elf3d::TextureFilter::nearest_mipmap_linear, elf3d::TextureFilter::linear_mipmap_linear}};
    for (std::size_t index = 0; index < gltf_filters.size(); ++index) {
        const std::filesystem::path filter_path =
            temporary.path() / ("filter_" + std::to_string(index) + ".gltf");
        const std::string sampler = "\"minFilter\":" + std::to_string(gltf_filters[index]);
        if (!write_text(filter_path, textured_json("\"uri\":\"asymmetric.png\"", sampler))) {
            return 43;
        }
        elf3d::scene::Storage filter_storage{scene_id(20 + index)};
        elf3d::scene::Storage& filter_builder{filter_storage};
        if (!elf3d::gltf::import_scene(filter_path, {}, filter_builder) ||
            filter_storage.assets().textures()[0].description.sampler.min_filter !=
                expected_filters[index]) {
            return 44;
        }
    }

    constexpr std::string_view jpeg_base64 =
        "/9j/4AAQSkZJRgABAQAAAQABAAD/"
        "2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKS"
        "j/"
        "2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKC"
        "j/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAP/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/"
        "8QAFAEBAAAAAAAAAAAAAAAAAAAABv/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AJAB58//2Q==";
    const std::vector<std::byte> jpeg_bytes = decode_test_base64(jpeg_base64);
    const std::filesystem::path jpeg_path = temporary.path() / "external.jpg";
    const std::filesystem::path jpeg_gltf_path = temporary.path() / "jpeg.gltf";
    if (!write_bytes(jpeg_path, jpeg_bytes) ||
        !write_text(jpeg_gltf_path, textured_json("\"uri\":\"external.jpg\"", ""))) {
        return 45;
    }
    elf3d::scene::Storage jpeg_storage{scene_id(30)};
    elf3d::scene::Storage& jpeg_builder{jpeg_storage};
    if (!elf3d::gltf::import_scene(jpeg_gltf_path, {}, jpeg_builder) ||
        jpeg_storage.assets().images()[0].width != 1 ||
        jpeg_storage.assets().images()[0].height != 1) {
        return 46;
    }

    std::vector<std::byte> normalized_bytes = triangle_positions();
    constexpr std::array<std::uint8_t, 6> normalized_texcoords{{0, 0, 255, 0, 128, 255}};
    for (const std::uint8_t value : normalized_texcoords) {
        append_byte(normalized_bytes, value);
    }
    const std::filesystem::path normalized_buffer_path = temporary.path() / "normalized.bin";
    const std::filesystem::path normalized_path = temporary.path() / "normalized.gltf";
    const std::string normalized_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"normalized.bin","byteLength":42}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5121,"normalized":true,"count":3,"type":"VEC2"}],"images":[{"uri":"asymmetric.png"}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]})json";
    if (!write_bytes(normalized_buffer_path, normalized_bytes) ||
        !write_text(normalized_path, normalized_json)) {
        return 47;
    }
    elf3d::scene::Storage normalized_storage{scene_id(31)};
    elf3d::scene::Storage& normalized_builder{normalized_storage};
    if (!elf3d::gltf::import_scene(normalized_path, {}, normalized_builder) ||
        !nearly_equal(normalized_storage.assets().meshes()[0].vertices[1].texcoord0.x, 1.0F) ||
        !nearly_equal(normalized_storage.assets().meshes()[0].vertices[2].texcoord0.x,
                      128.0F / 255.0F) ||
        !nearly_equal(normalized_storage.assets().meshes()[0].vertices[2].texcoord0.y, 1.0F)) {
        return 48;
    }

    std::string missing_uv_json = textured_json("\"uri\":\"asymmetric.png\"", "");
    const std::string uv_attribute = ",\"TEXCOORD_0\":1";
    missing_uv_json.erase(missing_uv_json.find(uv_attribute), uv_attribute.size());
    const std::filesystem::path missing_uv_path = temporary.path() / "missing_uv.gltf";
    elf3d::scene::Storage missing_uv_storage{scene_id(32)};
    elf3d::scene::Storage& missing_uv_builder{missing_uv_storage};
    if (!write_text(missing_uv_path, missing_uv_json)) {
        return 49;
    }
    const auto missing_uv_result =
        elf3d::gltf::import_scene(missing_uv_path, {}, missing_uv_builder);
    const bool missing_uv_fallback =
        missing_uv_result &&
        std::any_of(missing_uv_result.value().diagnostics.begin(),
                    missing_uv_result.value().diagnostics.end(),
                    [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                        return diagnostic.category == elf3d::SceneLoadDiagnosticCategory::texture &&
                               diagnostic.code == elf3d::SceneLoadDiagnosticCode::texture_fallback;
                    });
    if (!missing_uv_result || !missing_uv_fallback ||
        missing_uv_storage.statistics().texture_assets != 0) {
        return 50;
    }

    std::string alpha_json = textured_json("\"uri\":\"asymmetric.png\"", "");
    const std::string double_sided = "\"doubleSided\":true";
    alpha_json.replace(alpha_json.find(double_sided), double_sided.size(),
                       "\"doubleSided\":true,\"alphaMode\":\"BLEND\"");
    const std::filesystem::path alpha_path = temporary.path() / "alpha.gltf";
    elf3d::scene::Storage alpha_storage{scene_id(33)};
    elf3d::scene::Storage& alpha_builder{alpha_storage};
    if (!write_text(alpha_path, alpha_json)) {
        return 51;
    }
    const auto alpha_result = elf3d::gltf::import_scene(alpha_path, {}, alpha_builder);
    if (!alpha_result ||
        alpha_storage.assets().materials()[0].description.alpha_mode != elf3d::AlphaMode::blend ||
        !nearly_equal(alpha_storage.assets().materials()[0].description.base_color.alpha, 0.8F)) {
        return 52;
    }

    const std::filesystem::path bad_sampler_path = temporary.path() / "bad_sampler.gltf";
    if (!write_text(bad_sampler_path,
                    textured_json("\"uri\":\"asymmetric.png\"", "\"magFilter\":9987")) ||
        elf3d::gltf::import_scene(bad_sampler_path, {}, invalid_builder).error().code() !=
            elf3d::ErrorCode::invalid_sampler_filter) {
        return 53;
    }

    const std::vector<std::byte> dual_uv_bytes = dual_uv_geometry();
    const std::filesystem::path dual_uv_path = temporary.path() / "dual_uv.gltf";
    if (dual_uv_bytes.size() != 132 ||
        !write_bytes(temporary.path() / "dual_uv.bin", dual_uv_bytes) ||
        !write_text(dual_uv_path, dual_uv_json())) {
        return 56;
    }
    elf3d::scene::Storage dual_uv_storage{scene_id(40)};
    elf3d::scene::Storage& dual_uv_builder{dual_uv_storage};
    const auto dual_uv_result = elf3d::gltf::import_scene(dual_uv_path, {}, dual_uv_builder);
    if (!dual_uv_result || dual_uv_storage.assets().meshes().size() != 1 ||
        dual_uv_storage.assets().materials().size() != 1 ||
        !dual_uv_storage.entities()[0]->camera.has_value()) {
        return 57;
    }
    const auto& dual_vertex = dual_uv_storage.assets().meshes()[0].vertices[1];
    const auto& dual_material = dual_uv_storage.assets().materials()[0].description;
    if (dual_vertex.texcoord0 != elf3d::Float2{1.0F, 0.0F} ||
        dual_vertex.texcoord1 != elf3d::Float2{0.75F, 0.25F} ||
        dual_vertex.color != elf3d::Color4{0.0F, 1.0F, 0.0F, 1.0F} ||
        dual_material.base_color_texture_mapping.texcoord_set != 1U ||
        dual_material.base_color_texture_mapping.transform.offset != elf3d::Float2{0.25F, 0.5F} ||
        dual_material.base_color_texture_mapping.transform.scale != elf3d::Float2{2.0F, 3.0F} ||
        !nearly_equal(dual_material.base_color_texture_mapping.transform.rotation_radians, 0.5F) ||
        dual_material.metallic_roughness_texture_mapping.texcoord_set != 0U ||
        dual_material.normal_texture_mapping.texcoord_set != 1U ||
        dual_material.occlusion_texture_mapping.texcoord_set != 1U ||
        dual_material.emissive_texture_mapping.texcoord_set != 1U ||
        dual_material.base_color_texture != dual_material.normal_texture ||
        dual_material.base_color_texture != dual_material.occlusion_texture ||
        dual_material.base_color_texture != dual_material.emissive_texture ||
        dual_material.alpha_mode != elf3d::AlphaMode::mask ||
        !nearly_equal(dual_material.alpha_cutoff, 0.35F) ||
        !nearly_equal(dual_material.base_color.alpha, 0.4F) || !dual_material.unlit ||
        !nearly_equal(dual_material.emissive_factor.x, 0.2F) ||
        !nearly_equal(dual_material.emissive_factor.y, 0.4F) ||
        !nearly_equal(dual_material.emissive_factor.z, 0.6F) ||
        !nearly_equal(dual_material.normal_scale, 0.75F) ||
        !nearly_equal(dual_material.occlusion_strength, 0.6F) ||
        !nearly_equal(dual_material.ior, 1.33F) ||
        !nearly_equal(dual_material.specular_factor, 0.75F) ||
        dual_material.specular_color_factor != elf3d::Float3{0.8F, 0.9F, 1.0F}) {
        return 58;
    }
    const elf3d::SceneStatistics dual_statistics = dual_uv_storage.statistics();
    if (dual_statistics.materials_with_normal_textures != 1 ||
        dual_statistics.materials_with_occlusion_textures != 1 ||
        dual_statistics.materials_with_emissive_textures != 1 ||
        std::none_of(
            dual_uv_result.value().diagnostics.begin(), dual_uv_result.value().diagnostics.end(),
            [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                return diagnostic.code == elf3d::SceneLoadDiagnosticCode::normal_map_fallback;
            })) {
        return 59;
    }

    std::string missing_uv1_json = dual_uv_json();
    const std::string uv1_attribute = "\"TEXCOORD_1\":2,";
    missing_uv1_json.erase(missing_uv1_json.find(uv1_attribute), uv1_attribute.size());
    const std::filesystem::path missing_uv1_path = temporary.path() / "missing_uv1.gltf";
    elf3d::scene::Storage missing_uv1_storage{scene_id(41)};
    elf3d::scene::Storage& missing_uv1_builder{missing_uv1_storage};
    if (!write_text(missing_uv1_path, missing_uv1_json)) {
        return 60;
    }
    const auto missing_uv1_result =
        elf3d::gltf::import_scene(missing_uv1_path, {}, missing_uv1_builder);
    const bool missing_uv1_fallback =
        missing_uv1_result &&
        std::any_of(missing_uv1_result.value().diagnostics.begin(),
                    missing_uv1_result.value().diagnostics.end(),
                    [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                        return diagnostic.category == elf3d::SceneLoadDiagnosticCategory::texture &&
                               diagnostic.code == elf3d::SceneLoadDiagnosticCode::texture_fallback;
                    });
    if (!missing_uv1_result || !missing_uv1_fallback ||
        missing_uv1_storage.statistics().texture_assets != 1) {
        return 60;
    }

    const std::vector<std::byte> strip_bytes = quad_geometry(true);
    const std::filesystem::path strip_path = temporary.path() / "strip.gltf";
    const std::string strip_json =
        R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"strip.bin","byteLength":52}],"bufferViews":[{"buffer":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":4,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":5}]}],"nodes":[{"mesh":0}]})json";
    if (!write_bytes(temporary.path() / "strip.bin", strip_bytes) ||
        !write_text(strip_path, strip_json)) {
        return 61;
    }
    elf3d::scene::Storage strip_storage{scene_id(42)};
    elf3d::scene::Storage& strip_builder{strip_storage};
    if (!elf3d::gltf::import_scene(strip_path, {}, strip_builder) ||
        strip_storage.assets().meshes()[0].indices !=
            std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3}) {
        return 62;
    }

    const std::vector<std::byte> fan_bytes = quad_geometry(false);
    const std::filesystem::path fan_path = temporary.path() / "fan.gltf";
    const std::string fan_json =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["KHR_materials_clearcoat"],"buffers":[{"uri":"fan.bin","byteLength":48}],"bufferViews":[{"buffer":0,"byteLength":48}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":6}]}],"nodes":[{"mesh":0}]})json";
    if (!write_bytes(temporary.path() / "fan.bin", fan_bytes) || !write_text(fan_path, fan_json)) {
        return 63;
    }
    elf3d::scene::Storage fan_storage{scene_id(43)};
    elf3d::scene::Storage& fan_builder{fan_storage};
    const auto fan_result = elf3d::gltf::import_scene(fan_path, {}, fan_builder);
    if (!fan_result ||
        fan_storage.assets().meshes()[0].indices != std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3} ||
        std::none_of(fan_result.value().diagnostics.begin(), fan_result.value().diagnostics.end(),
                     [](const elf3d::SceneLoadDiagnostic& diagnostic) {
                         return diagnostic.code ==
                                elf3d::SceneLoadDiagnosticCode::material_fallback;
                     })) {
        return 64;
    }

    constexpr std::uint64_t oversized_strip_index_count = 50000003ULL;
    constexpr std::uint64_t oversized_strip_buffer_bytes =
        36ULL + oversized_strip_index_count * 2ULL;
    const std::filesystem::path oversized_strip_path = temporary.path() / "oversized_strip.gltf";
    const std::string oversized_strip_json =
        std::string{
            R"json({"asset":{"version":"2.0"},"buffers":[{"uri":"oversized_strip.bin","byteLength":)json"} +
        std::to_string(oversized_strip_buffer_bytes) +
        R"json(}],"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":)json" +
        std::to_string(oversized_strip_index_count * 2ULL) +
        R"json(}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":)json" +
        std::to_string(oversized_strip_index_count) +
        R"json(,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":5}]}],"nodes":[{"mesh":0}]})json";
    elf3d::scene::Storage oversized_strip_storage{scene_id(45)};
    elf3d::scene::Storage& oversized_strip_builder{oversized_strip_storage};
    if (!write_text(oversized_strip_path, oversized_strip_json) ||
        elf3d::gltf::import_scene(oversized_strip_path, {}, oversized_strip_builder)
                .error()
                .code() != elf3d::ErrorCode::resource_limit_exceeded) {
        return 67;
    }

    const std::vector<std::byte> quantized_bytes = quantized_positions();
    const std::filesystem::path quantized_path = temporary.path() / "quantized.gltf";
    const std::string quantized_json =
        R"json({"asset":{"version":"2.0"},"extensionsUsed":["KHR_mesh_quantization"],"extensionsRequired":["KHR_mesh_quantization"],"buffers":[{"uri":"quantized.bin","byteLength":18}],"bufferViews":[{"buffer":0,"byteLength":18}],"accessors":[{"bufferView":0,"componentType":5123,"normalized":true,"count":3,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}]})json";
    if (!write_bytes(temporary.path() / "quantized.bin", quantized_bytes) ||
        !write_text(quantized_path, quantized_json)) {
        return 65;
    }
    elf3d::scene::Storage quantized_storage{scene_id(44)};
    elf3d::scene::Storage& quantized_builder{quantized_storage};
    const auto quantized_result = elf3d::gltf::import_scene(quantized_path, {}, quantized_builder);
    if (!quantized_result ||
        !nearly_equal(quantized_storage.assets().meshes()[0].vertices[1].position.x, 1.0F) ||
        !nearly_equal(quantized_storage.assets().meshes()[0].vertices[2].position.y, 1.0F)) {
        return 66;
    }

    const std::filesystem::path visual_fixture = std::filesystem::path{ELF3D_TEST_SOURCE_DIR} /
                                                 "tests" / "fixtures" / "elf3d_smoke" /
                                                 "elf3d_smoke.gltf";
    elf3d::scene::Storage fixture_storage{scene_id(34)};
    elf3d::scene::Storage& fixture_builder{fixture_storage};
    const auto fixture_result = elf3d::gltf::import_scene(visual_fixture, {}, fixture_builder);
    if (!fixture_result) {
        std::cerr << fixture_result.error().message() << '\n';
        return 54;
    }
    const elf3d::SceneStatistics expected_fixture_statistics{1, 1, 2, 2, 2,  8, 12,
                                                             4, 1, 2, 2, 16, 2, 1};
    const std::optional<elf3d::Bounds3> fixture_bounds = fixture_storage.world_bounds();
    if (!fixture_result.value().diagnostics.empty() ||
        fixture_storage.statistics() != expected_fixture_statistics ||
        !fixture_bounds.has_value() ||
        fixture_bounds->minimum != elf3d::Float3{-2.0F, -1.0F, 0.0F} ||
        fixture_bounds->maximum != elf3d::Float3{2.0F, 1.0F, 0.0F}) {
        return 55;
    }

    return 0;
}
