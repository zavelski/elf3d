#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import elf.image;

namespace {

constexpr std::string_view jpeg_base64 =
    "/9j/4AAQSkZJRgABAQAAAQABAAD/"
    "2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKS"
    "j/"
    "2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKC"
    "j/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAP/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/"
    "8QAFAEBAAAAAAAAAAAAAAAAAAAABv/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/AJAB58//2Q==";

class TemporaryDirectory final {
  public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path() / "elf3d_model_gltf_export_test") {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_, error);
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

[[nodiscard]] std::uint32_t base64_value(char character) noexcept {
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
    output.reserve(source.size() / 4U * 3U);
    for (std::size_t index = 0; index < source.size(); index += 4U) {
        const std::uint32_t combined =
            (base64_value(source[index]) << 18U) | (base64_value(source[index + 1U]) << 12U) |
            ((source[index + 2U] == '=' ? 0U : base64_value(source[index + 2U])) << 6U) |
            (source[index + 3U] == '=' ? 0U : base64_value(source[index + 3U]));
        output.push_back(static_cast<std::byte>((combined >> 16U) & 0xffU));
        if (source[index + 2U] != '=') {
            output.push_back(static_cast<std::byte>((combined >> 8U) & 0xffU));
        }
        if (source[index + 3U] != '=') {
            output.push_back(static_cast<std::byte>(combined & 0xffU));
        }
    }
    return output;
}

[[nodiscard]] bool same_bytes(std::span<const std::byte> first,
                              std::span<const std::byte> second) noexcept {
    return first.size() == second.size() &&
           std::equal(first.begin(), first.end(), second.begin(), second.end());
}

[[nodiscard]] bool write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream stream{path, std::ios::binary};
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] std::optional<std::vector<std::byte>> read_bytes(const std::filesystem::path& path) {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    const std::vector<char> characters{std::istreambuf_iterator<char>{stream},
                                       std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes;
    bytes.reserve(characters.size());
    for (const char character : characters) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return bytes;
}

struct TextureResources {
    elf3d::TextureId decoded;
    elf3d::TextureId source;
};

struct SurfaceResources {
    elf3d::MaterialId material;
    elf3d::MeshId mesh;
};

[[nodiscard]] elf3d::Result<TextureResources>
create_textures(elf3d::Document& document, const elf3d::image::DecodedImage& decoded,
                std::span<const std::byte> jpeg) {
    constexpr std::array<std::byte, 4> rgba{std::byte{0xff}, std::byte{0x20}, std::byte{0x10},
                                            std::byte{0xff}};
    const auto decoded_image = document.create_image(
        elf3d::ModelImageDescription{1U, 1U, elf3d::PixelFormat::rgba8_unorm, rgba});
    const auto source_image = document.create_image(
        elf3d::ModelImageDescription{decoded.width, decoded.height, elf3d::PixelFormat::rgba8_unorm,
                                     decoded.pixels, elf3d::ModelImageMimeType::jpeg, jpeg});
    const auto sampler = document.create_sampler();
    if (!decoded_image || !source_image || !sampler) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create exporter test images"};
    }
    const auto decoded_texture = document.create_texture(
        elf3d::ModelTextureDescription{decoded_image.value(), sampler.value()});
    const auto source_texture = document.create_texture(
        elf3d::ModelTextureDescription{source_image.value(), sampler.value()});
    if (!decoded_texture || !source_texture) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create exporter test textures"};
    }
    return TextureResources{decoded_texture.value(), source_texture.value()};
}

[[nodiscard]] elf3d::Result<SurfaceResources> create_surface(elf3d::Document& document,
                                                             const TextureResources& textures) {
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color = elf3d::Color4{0.25F, 0.5F, 0.75F, 1.0F};
    material_description.double_sided = true;
    material_description.metallic_factor = 0.25F;
    material_description.roughness_factor = 0.75F;
    material_description.emissive_factor = elf3d::Float3{0.5F, 0.25F, 0.125F};
    material_description.emissive_strength = 2.0F;
    material_description.ior = 1.25F;
    material_description.base_color_texture = textures.decoded;
    material_description.emissive_texture = textures.source;
    const auto material = document.create_material(material_description);
    const auto mesh = document.create_mesh("shared mesh");
    if (!material || !mesh) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create exporter test material or mesh"};
    }

    elf3d::PrimitiveData primitive;
    primitive.positions = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    primitive.normals = {{0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F}};
    primitive.texcoord0 = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
    primitive.indices = {0U, 1U, 2U};
    if (!document.create_primitive(mesh.value(), material.value(), std::move(primitive))) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create exporter test primitive"};
    }
    elf3d::PrimitiveData wide_indices;
    wide_indices.positions.resize(257U);
    wide_indices.positions[1U] = {1.0F, 0.0F, 0.0F};
    wide_indices.positions[256U] = {0.0F, 1.0F, 0.0F};
    wide_indices.indices = {0U, 1U, 256U};
    if (!document.create_primitive(mesh.value(), material.value(), std::move(wide_indices))) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create wide-index exporter test primitive"};
    }
    return SurfaceResources{material.value(), mesh.value()};
}

[[nodiscard]] elf3d::Result<void> create_scenes(elf3d::Document& document, elf3d::MeshId mesh) {
    const auto first_scene = document.create_scene("first scene");
    const auto second_scene = document.create_scene("selected scene");
    const auto first_node = document.create_node("first node");
    const auto second_node = document.create_node("selected node");
    if (!first_scene || !second_scene || !first_node || !second_node ||
        !document.set_node_mesh(first_node.value(), mesh) ||
        !document.set_node_mesh(second_node.value(), mesh) ||
        !document.add_scene_root(first_scene.value(), first_node.value()) ||
        !document.add_scene_root(second_scene.value(), second_node.value()) ||
        !document.set_default_scene(second_scene.value())) {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                            "Could not create exporter test scenes"};
    }
    return {};
}

[[nodiscard]] elf3d::Result<elf3d::Document> create_document(std::span<const std::byte> jpeg) {
    const auto decoded = elf3d::image::decode_png_or_jpeg(jpeg);
    if (!decoded) {
        return decoded.error();
    }
    elf3d::Document document;
    const auto textures = create_textures(document, decoded.value(), jpeg);
    if (!textures) {
        return textures.error();
    }
    const auto surface = create_surface(document, textures.value());
    if (!surface) {
        return surface.error();
    }
    const auto scenes = create_scenes(document, surface.value().mesh);
    if (!scenes) {
        return scenes.error();
    }
    return elf3d::Result<elf3d::Document>{std::move(document)};
}

[[nodiscard]] bool has_expected_write_report(const elf3d::ModelWriteReport& report) noexcept {
    return report.diagnostics.size() == 1U &&
           report.diagnostics[0].code == elf3d::ModelWriteDiagnosticCode::image_reencoded_as_png;
}

[[nodiscard]] bool scene_selection_matches(const elf3d::Document& document,
                                           const elf3d::DocumentSceneView& first,
                                           const elf3d::DocumentSceneView& selected) {
    return document.scene_count() == 2U && document.node_count() == 2U &&
           first.name == "first scene" && selected.name == "selected scene" &&
           document.default_scene() == std::optional<elf3d::DocumentSceneId>{selected.id};
}

[[nodiscard]] bool material_matches(const elf3d::ModelMaterialDescription& description) {
    return description.base_color == elf3d::Color4{0.25F, 0.5F, 0.75F, 1.0F} &&
           description.double_sided && description.metallic_factor == 0.25F &&
           description.roughness_factor == 0.75F && description.emissive_strength == 2.0F &&
           description.ior == 1.25F;
}

[[nodiscard]] bool image_sources_match(const elf3d::ImageView& png,
                                       const elf3d::ImageView& jpeg_image,
                                       std::span<const std::byte> jpeg) {
    return png.source_mime_type == elf3d::ModelImageMimeType::png &&
           jpeg_image.source_mime_type == elf3d::ModelImageMimeType::jpeg &&
           same_bytes(jpeg_image.source_bytes, jpeg);
}

[[nodiscard]] bool document_matches(const elf3d::Document& document,
                                    std::span<const std::byte> jpeg) {
    const auto first_scene = document.scene_at(0U);
    const auto selected_scene = document.scene_at(1U);
    const auto material = document.material_at(0U);
    const auto png_image = document.image_at(0U);
    const auto jpeg_image = document.image_at(1U);
    if (!first_scene || !selected_scene || !material || !png_image || !jpeg_image) {
        return false;
    }
    return scene_selection_matches(document, first_scene.value(), selected_scene.value()) &&
           material_matches(material.value().description) &&
           image_sources_match(png_image.value(), jpeg_image.value(), jpeg);
}

[[nodiscard]] bool round_trip(const std::filesystem::path& path, const elf3d::Document& document,
                              std::span<const std::byte> jpeg,
                              const elf3d::ModelWriteOptions& options = {}) {
    const auto written = elf3d::save_document(path.string(), document.view(), options);
    if (!written || !has_expected_write_report(written.value())) {
        return false;
    }
    const auto loaded = elf3d::load_document(path.string());
    return loaded && document_matches(loaded.value().document, jpeg);
}

[[nodiscard]] bool test_automatic_round_trips(const std::filesystem::path& directory,
                                              const elf3d::Document& document,
                                              std::span<const std::byte> jpeg) {
    const std::filesystem::path gltf = directory / "automatic.gltf";
    const std::filesystem::path glb = directory / "automatic.glb";
    if (!round_trip(gltf, document, jpeg) || !round_trip(glb, document, jpeg)) {
        return false;
    }
    const auto jpeg_sidecar = read_bytes(directory / "automatic.image_1.jpg");
    const auto json = read_text(gltf);
    return json && json->starts_with("{\n\t\"asset\": {\n") &&
           json->find("\"componentType\": 5121") != std::string::npos &&
           json->find("\"componentType\": 5123") != std::string::npos &&
           std::filesystem::is_regular_file(directory / "automatic.image_0.png") &&
           jpeg_sidecar.has_value() && same_bytes(*jpeg_sidecar, jpeg) &&
           !std::filesystem::exists(directory / "automatic.image_0.jpg");
}

[[nodiscard]] bool absent_default_round_trip(const std::filesystem::path& path,
                                             const elf3d::Document& document) {
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    if (!written || !has_expected_write_report(written.value()) || !loaded) {
        return false;
    }
    const auto first_scene = loaded.value().document.scene_at(0U);
    return first_scene && !loaded.value().document.default_scene().has_value() &&
           loaded.value().default_scene == first_scene.value().id;
}

[[nodiscard]] bool test_absent_default_round_trips(const std::filesystem::path& directory,
                                                   std::span<const std::byte> jpeg) {
    auto created = create_document(jpeg);
    if (!created || !created.value().clear_default_scene()) {
        return false;
    }
    for (const std::string_view filename : {"no_default.gltf", "no_default.glb"}) {
        if (!absent_default_round_trip(directory / filename, created.value())) {
            return false;
        }
    }
    const auto json = read_text(directory / "no_default.gltf");
    return json && json->find("\"scene\":") == std::string::npos;
}

[[nodiscard]] bool empty_scene_round_trip(const std::filesystem::path& path,
                                          const elf3d::Document& document) {
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    if (!written || !has_expected_write_report(written.value()) || !loaded) {
        return false;
    }
    const auto selected_scene = loaded.value().document.scene_at(1U);
    const auto empty_scene = loaded.value().document.scene_at(2U);
    return selected_scene && empty_scene && loaded.value().document.scene_count() == 3U &&
           loaded.value().document.default_scene() == selected_scene.value().id &&
           empty_scene.value().name == "empty scene" && empty_scene.value().roots.empty();
}

[[nodiscard]] bool test_empty_scene_round_trips(const std::filesystem::path& directory,
                                                std::span<const std::byte> jpeg) {
    auto created = create_document(jpeg);
    if (!created || !created.value().create_scene("empty scene")) {
        return false;
    }
    for (const std::string_view filename : {"empty_scene.gltf", "empty_scene.glb"}) {
        if (!empty_scene_round_trip(directory / filename, created.value())) {
            return false;
        }
    }
    const auto json = read_text(directory / "empty_scene.gltf");
    return json && json->find(R"json("name": "empty scene")json") != std::string::npos &&
           json->find(R"json("nodes": [])json") == std::string::npos;
}

[[nodiscard]] bool test_empty_mesh_is_rejected(const std::filesystem::path& directory,
                                               std::span<const std::byte> jpeg) {
    auto created = create_document(jpeg);
    if (!created || !created.value().create_mesh("empty mesh")) {
        return false;
    }
    const std::filesystem::path output = directory / "empty_mesh.gltf";
    const auto written = elf3d::save_document(output.string(), created.value().view());
    return !written && written.error().code() == elf3d::ErrorCode::invalid_mesh_data &&
           !std::filesystem::exists(output) &&
           !std::filesystem::exists(directory / "empty_mesh.bin") &&
           !std::filesystem::exists(directory / "empty_mesh.image_0.png") &&
           !std::filesystem::exists(directory / "empty_mesh.image_1.jpg") &&
           !std::filesystem::exists(directory / "empty_mesh.gltf.elf3d-stage-0") &&
           !std::filesystem::exists(directory / "empty_mesh.gltf.elf3d-backup-0");
}

[[nodiscard]] bool test_explicit_image_policies(const std::filesystem::path& directory,
                                                const elf3d::Document& document,
                                                std::span<const std::byte> jpeg) {
    elf3d::ModelWriteOptions external;
    external.image_policy = elf3d::ModelImageWritePolicy::external;
    const std::filesystem::path external_glb = directory / "external.glb";
    if (!round_trip(external_glb, document, jpeg, external) ||
        !std::filesystem::is_regular_file(directory / "external.image_0.png") ||
        !std::filesystem::is_regular_file(directory / "external.image_1.jpg")) {
        return false;
    }

    elf3d::ModelWriteOptions embedded;
    embedded.image_policy = elf3d::ModelImageWritePolicy::embedded;
    const std::filesystem::path embedded_gltf = directory / "embedded.gltf";
    return round_trip(embedded_gltf, document, jpeg, embedded) &&
           std::filesystem::is_regular_file(directory / "embedded.bin") &&
           !std::filesystem::exists(directory / "embedded.image_0.png") &&
           !std::filesystem::exists(directory / "embedded.image_1.jpg");
}

[[nodiscard]] bool write_original_outputs(const std::filesystem::path& primary,
                                          const std::filesystem::path& png,
                                          const std::filesystem::path& jpeg) {
    return write_text(primary, "original primary output") &&
           write_text(png, "original PNG sidecar") && write_text(jpeg, "original JPEG sidecar");
}

[[nodiscard]] bool occupy_jpeg_backups(const std::filesystem::path& directory,
                                       const std::filesystem::path& jpeg_sidecar) {
    for (std::uint32_t index = 0U; index < 1024U; ++index) {
        const std::filesystem::path blocker =
            directory /
            (jpeg_sidecar.filename().string() + ".elf3d-backup-" + std::to_string(index));
        if (!write_text(blocker, "occupied")) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool retained_outputs_match(const std::filesystem::path& primary,
                                          const std::filesystem::path& png,
                                          const std::filesystem::path& jpeg) {
    const auto retained_primary = read_text(primary);
    const auto retained_png = read_text(png);
    const auto retained_jpeg = read_text(jpeg);
    return retained_primary && *retained_primary == "original primary output" && retained_png &&
           *retained_png == "original PNG sidecar" && retained_jpeg &&
           *retained_jpeg == "original JPEG sidecar";
}

[[nodiscard]] bool rollback_files_are_absent(const std::filesystem::path& directory) {
    return !std::filesystem::exists(directory / "rollback.bin") &&
           !std::filesystem::exists(directory / "rollback.image_0.png.elf3d-backup-0") &&
           !std::filesystem::exists(directory / "rollback.image_0.png.elf3d-stage-0");
}

[[nodiscard]] bool test_transactional_failure(const std::filesystem::path& directory,
                                              const elf3d::Document& document) {
    const std::filesystem::path primary = directory / "rollback.gltf";
    const std::filesystem::path png_sidecar = directory / "rollback.image_0.png";
    const std::filesystem::path jpeg_sidecar = directory / "rollback.image_1.jpg";
    if (!write_original_outputs(primary, png_sidecar, jpeg_sidecar)) {
        return false;
    }
    if (!occupy_jpeg_backups(directory, jpeg_sidecar)) {
        return false;
    }
    const auto written = elf3d::save_document(primary.string(), document.view());
    return !written && written.error().code() == elf3d::ErrorCode::source_file_write_failed &&
           retained_outputs_match(primary, png_sidecar, jpeg_sidecar) &&
           rollback_files_are_absent(directory);
}

[[nodiscard]] bool test_unsupported_extension(const std::filesystem::path& directory,
                                              const elf3d::Document& document) {
    const std::filesystem::path unsupported = directory / "unsupported.obj";
    const auto written = elf3d::save_document(unsupported.string(), document.view());
    return !written && written.error().code() == elf3d::ErrorCode::unsupported_model_format &&
           !std::filesystem::exists(unsupported);
}

} // namespace

int elf3d_gltf_export_test() {
    TemporaryDirectory temporary;
    const std::vector<std::byte> jpeg = decode_base64(jpeg_base64);
    auto created = create_document(jpeg);
    if (!created) {
        return 1;
    }
    if (!test_automatic_round_trips(temporary.path(), created.value(), jpeg)) {
        return 2;
    }
    if (!test_absent_default_round_trips(temporary.path(), jpeg)) {
        return 3;
    }
    if (!test_empty_scene_round_trips(temporary.path(), jpeg)) {
        return 4;
    }
    if (!test_empty_mesh_is_rejected(temporary.path(), jpeg)) {
        return 5;
    }
    if (!test_explicit_image_policies(temporary.path(), created.value(), jpeg)) {
        return 6;
    }
    if (!test_transactional_failure(temporary.path(), created.value())) {
        return 7;
    }
    if (!test_unsupported_extension(temporary.path(), created.value())) {
        return 8;
    }
    return 0;
}
