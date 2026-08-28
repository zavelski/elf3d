#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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

constexpr std::string_view authored_json = R"json({
  "asset":{"version":"2.0"},
  "buffers":[{"uri":"authored_tangent.bin","byteLength":144}],
  "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":48}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TANGENT":3}}]}],
  "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
})json";

constexpr std::string_view normal_mapped_authored_json = R"json({
  "asset":{"version":"2.0"},
  "buffers":[{"uri":"authored_tangent.bin","byteLength":144}],
  "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":48}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"}],
  "images":[{"uri":"normal.png"}],"textures":[{"source":0}],
  "materials":[{"normalTexture":{"index":0}}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TANGENT":3},"material":0}]}],
  "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
})json";

class TemporaryDirectory final {
  public:
    TemporaryDirectory() : path_(std::filesystem::path{ELF3D_TEST_BINARY_DIR} / "gltf_tangent") {
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

void append_u16(std::vector<std::byte>& output, std::uint16_t value) {
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void append_float(std::vector<std::byte>& output, float value) {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    for (unsigned shift : {0U, 8U, 16U, 24U}) {
        output.push_back(static_cast<std::byte>((bits >> shift) & 0xffU));
    }
}

void append_values(std::vector<std::byte>& output, std::initializer_list<float> values) {
    for (const float value : values) {
        append_float(output, value);
    }
}

[[nodiscard]] std::vector<std::byte> textured_geometry() {
    std::vector<std::byte> output;
    append_values(output, {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
    append_values(output, {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F});
    append_values(output, {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F});
    return output;
}

[[nodiscard]] std::vector<std::byte> tangent_geometry(elf3d::Float4 tangent) {
    std::vector<std::byte> output = textured_geometry();
    for (std::size_t index = 0; index < 3U; ++index) {
        append_values(output, {tangent.x, tangent.y, tangent.z, tangent.w});
    }
    return output;
}

[[nodiscard]] std::vector<std::byte> dual_texcoord_geometry() {
    std::vector<std::byte> output = textured_geometry();
    append_values(output, {0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F});
    return output;
}

[[nodiscard]] std::vector<std::byte> mirrored_seam_geometry() {
    std::vector<std::byte> output;
    append_values(output, {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 0.0F});
    for (std::size_t index = 0; index < 4U; ++index) {
        append_values(output, {0.0F, 0.0F, 1.0F});
    }
    append_values(output, {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F});
    constexpr std::array<std::uint16_t, 6> indices{0U, 1U, 2U, 0U, 2U, 3U};
    for (const std::uint16_t index : indices) {
        append_u16(output, index);
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
    std::ofstream stream{path};
    stream << text;
    return static_cast<bool>(stream);
}

[[nodiscard]] bool nearly_equal(float left, float right) noexcept {
    return std::abs(left - right) <= 0.0001F;
}

[[nodiscard]] bool usable_tangents(const elf3d::PrimitiveDataView& primitive) noexcept {
    if (primitive.tangents.size() != primitive.positions.size() || primitive.tangents.empty()) {
        return false;
    }
    return std::all_of(primitive.tangents.begin(), primitive.tangents.end(),
                       [](const elf3d::Float4& tangent) noexcept {
                           const float length =
                               std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y +
                                         tangent.z * tangent.z);
                           return std::isfinite(length) && nearly_equal(length, 1.0F) &&
                                  (tangent.w == -1.0F || tangent.w == 1.0F);
                       });
}

[[nodiscard]] bool has_fallback(const elf3d::ModelLoadReport& report) {
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(), [](const auto& item) {
        return item.code == elf3d::ModelLoadDiagnosticCode::normal_map_fallback;
    });
}

[[nodiscard]] int test_authored_tangent(const TemporaryDirectory& temporary) {
    const auto geometry = tangent_geometry({2.0F, 0.0F, 0.0F, -1.0F});
    const auto path = temporary.path() / "authored_tangent.gltf";
    if (!write_bytes(temporary.path() / "authored_tangent.bin", geometry) ||
        !write_text(path, authored_json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && usable_tangents(primitive.value().data) &&
                   primitive.value().data.tangents[0] == elf3d::Float4{1.0F, 0.0F, 0.0F, -1.0F}
               ? 0
               : 2;
}

[[nodiscard]] int test_generated_tangent(const TemporaryDirectory& temporary) {
    constexpr std::string_view json = R"json({
      "asset":{"version":"2.0"},"buffers":[{"uri":"generated.bin","byteLength":96}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"normal.png"}],"textures":[{"source":0}],"materials":[{"normalTexture":{"index":0,"scale":0.5}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
    })json";
    const auto path = temporary.path() / "generated.gltf";
    if (!write_bytes(temporary.path() / "generated.bin", textured_geometry()) ||
        !write_text(path, json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && usable_tangents(primitive.value().data) &&
                   !has_fallback(loaded.value().report)
               ? 0
               : 2;
}

[[nodiscard]] int test_uv1_tangent(const TemporaryDirectory& temporary) {
    constexpr std::string_view json = R"json({
      "asset":{"version":"2.0"},"buffers":[{"uri":"uv1.bin","byteLength":120}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":24}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":3,"componentType":5126,"count":3,"type":"VEC2"}],
      "images":[{"uri":"normal.png"}],"textures":[{"source":0}],"materials":[{"normalTexture":{"index":0,"texCoord":1}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TEXCOORD_1":3},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
    })json";
    const auto path = temporary.path() / "uv1.gltf";
    if (!write_bytes(temporary.path() / "uv1.bin", dual_texcoord_geometry()) ||
        !write_text(path, json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && usable_tangents(primitive.value().data) &&
                   std::abs(primitive.value().data.tangents[0].y) >= 0.9F
               ? 0
               : 2;
}

[[nodiscard]] bool has_both_handedness_signs(std::span<const elf3d::Float4> tangents) {
    const bool negative = std::any_of(tangents.begin(), tangents.end(),
                                      [](elf3d::Float4 tangent) { return tangent.w < 0.0F; });
    const bool positive = std::any_of(tangents.begin(), tangents.end(),
                                      [](elf3d::Float4 tangent) { return tangent.w > 0.0F; });
    return negative && positive;
}

[[nodiscard]] int test_mirrored_seam(const TemporaryDirectory& temporary) {
    constexpr std::string_view json = R"json({
      "asset":{"version":"2.0"},"buffers":[{"uri":"seam.bin","byteLength":140}],
      "bufferViews":[{"buffer":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":12}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"}],
      "images":[{"uri":"normal.png"}],"textures":[{"source":0}],"materials":[{"normalTexture":{"index":0}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
    })json";
    const auto path = temporary.path() / "seam.gltf";
    if (!write_bytes(temporary.path() / "seam.bin", mirrored_seam_geometry()) ||
        !write_text(path, json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && primitive.value().data.positions.size() > 4U &&
                   usable_tangents(primitive.value().data) &&
                   has_both_handedness_signs(primitive.value().data.tangents)
               ? 0
               : 2;
}

[[nodiscard]] int test_missing_uv_fallback(const TemporaryDirectory& temporary) {
    constexpr std::string_view json = R"json({
      "asset":{"version":"2.0"},"buffers":[{"uri":"missing_uv.bin","byteLength":72}],
      "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"}],
      "images":[{"uri":"normal.png"}],"textures":[{"source":0}],"materials":[{"normalTexture":{"index":0}}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"material":0}]}],"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}]
    })json";
    const std::vector<std::byte> full_geometry = textured_geometry();
    std::vector<std::byte> geometry(full_geometry.begin(), full_geometry.begin() + 72);
    const auto path = temporary.path() / "missing_uv.gltf";
    if (!write_bytes(temporary.path() / "missing_uv.bin", geometry) || !write_text(path, json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && primitive.value().data.tangents.empty() &&
                   has_fallback(loaded.value().report)
               ? 0
               : 2;
}

[[nodiscard]] bool rejects_authored_tangent(const TemporaryDirectory& temporary, std::string json,
                                            const std::vector<std::byte>& geometry) {
    const auto path = temporary.path() / "authored_tangent.gltf";
    if (!write_bytes(temporary.path() / "authored_tangent.bin", geometry) ||
        !write_text(path, json)) {
        return false;
    }
    return !elf3d::load_document(path.string());
}

[[nodiscard]] bool discards_unusable_authored_tangent(const TemporaryDirectory& temporary) {
    const auto path = temporary.path() / "authored_tangent.gltf";
    if (!write_bytes(temporary.path() / "authored_tangent.bin",
                     tangent_geometry({0.0F, 0.0F, 0.0F, 1.0F})) ||
        !write_text(path, authored_json)) {
        return false;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && primitive.value().data.tangents.empty() &&
           has_fallback(loaded.value().report);
}

[[nodiscard]] int test_unusable_authored_tangent_regeneration(const TemporaryDirectory& temporary) {
    const auto path = temporary.path() / "authored_tangent.gltf";
    if (!write_bytes(temporary.path() / "authored_tangent.bin",
                     tangent_geometry({0.0F, 0.0F, 0.0F, 1.0F})) ||
        !write_text(path, normal_mapped_authored_json)) {
        return 1;
    }
    const auto loaded = elf3d::load_document(path.string());
    const auto primitive = loaded ? loaded.value().document.primitive_at(0U)
                                  : elf3d::Result<elf3d::PrimitiveView>{elf3d::Error{
                                        elf3d::ErrorCode::invalid_argument, "load failed"}};
    return primitive && usable_tangents(primitive.value().data) &&
                   has_fallback(loaded.value().report)
               ? 0
               : 2;
}

[[nodiscard]] int test_invalid_authored_tangents(const TemporaryDirectory& temporary) {
    const auto valid = tangent_geometry({2.0F, 0.0F, 0.0F, -1.0F});
    std::string wrong_type{authored_json};
    wrong_type.replace(wrong_type.rfind("\"VEC4\""), 6U, "\"VEC3\"");
    std::string wrong_component{authored_json};
    wrong_component.replace(wrong_component.rfind("5126"), 4U, "5123");
    std::string wrong_count{authored_json};
    wrong_count[wrong_count.rfind("\"count\":3") + 8U] = '2';
    if (!rejects_authored_tangent(temporary, wrong_type, valid)) {
        return 1;
    }
    if (!rejects_authored_tangent(temporary, wrong_component, valid)) {
        return 2;
    }
    if (!rejects_authored_tangent(temporary, wrong_count, valid)) {
        return 3;
    }
    if (!discards_unusable_authored_tangent(temporary)) {
        return 4;
    }
    if (!rejects_authored_tangent(temporary, std::string{authored_json},
                                  tangent_geometry({1.0F, 0.0F, 0.0F, 0.0F}))) {
        return 5;
    }
    const elf3d::Float4 non_finite{std::numeric_limits<float>::infinity(), 0.0F, 0.0F, 1.0F};
    return rejects_authored_tangent(temporary, std::string{authored_json},
                                    tangent_geometry(non_finite))
               ? 0
               : 6;
}

using TangentTest = int (*)(const TemporaryDirectory&);

} // namespace

int elf3d_gltf_tangent_test() {
    TemporaryDirectory temporary;
    if (!write_bytes(temporary.path() / "normal.png", std::as_bytes(std::span{asymmetric_png}))) {
        return 1;
    }
    constexpr std::array<TangentTest, 7> tests{
        {test_authored_tangent, test_generated_tangent, test_uv1_tangent, test_mirrored_seam,
         test_missing_uv_fallback, test_unusable_authored_tangent_regeneration,
         test_invalid_authored_tangents}};
    for (std::size_t index = 0; index < tests.size(); ++index) {
        if (const int result = tests[index](temporary); result != 0) {
            return 10 + static_cast<int>(index) * 10 + result;
        }
    }
    return 0;
}
