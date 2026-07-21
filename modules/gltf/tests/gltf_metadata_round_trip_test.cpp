#include <elf3d/model.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

import elf.gltf;
import elf.model;

namespace {

constexpr std::string_view metadata_fixture = R"json({
  "asset":{"version":"2.0","extras":{"tag":"asset"},"extensions":{"EXT_elf_raw":{"tag":"asset-ext"}}},
  "extensionsUsed":["KHR_materials_ior","EXT_elf_raw"],
  "extras":{"tag":"root","nested":[1,true,null,{"k":"brace }"}]},
  "extensions":{"EXT_elf_raw":{"tag":"root-ext","nested":{"array":[1,2]}}},
  "buffers":[{"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/","byteLength":96}],
  "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
  "accessors":[
    {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","extras":{"tag":"accessor"},"extensions":{"EXT_elf_raw":{"tag":"accessor-ext"}}},
    {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
    {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}
  ],
  "images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAFElEQVR4nGP4z8DAAMIM////ZwAAHu8E/KPItPcAAAAASUVORK5CYII=","extras":{"tag":"image"},"extensions":{"EXT_elf_raw":{"tag":"image-ext"}}}],
  "samplers":[{"extras":{"tag":"sampler"},"extensions":{"EXT_elf_raw":{"tag":"sampler-ext"}}}],
  "textures":[{"source":0,"sampler":0,"extras":{"tag":"texture"},"extensions":{"EXT_elf_raw":{"tag":"texture-ext"}}}],
  "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}},"extras":{"tag":"material"},"extensions":{"KHR_materials_ior":{"ior":1.33},"EXT_elf_raw":{"tag":"material-ext"}}}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0,"extras":{"tag":"primitive"},"extensions":{"EXT_elf_raw":{"tag":"primitive-ext"}}}],"extras":{"tag":"mesh"},"extensions":{"EXT_elf_raw":{"tag":"mesh-ext"}}}],
  "nodes":[{"mesh":0,"extras":{"tag":"node"},"extensions":{"EXT_elf_raw":{"tag":"node-ext"}}}],
  "scenes":[{"nodes":[0],"extras":{"tag":"scene"},"extensions":{"EXT_elf_raw":{"tag":"scene-ext"}}}],
  "scene":0
})json";

constexpr std::string_view expected_root_extras =
    R"json({"tag":"root","nested":[1,true,null,{"k":"brace }"}]})json";
constexpr std::string_view expected_root_extension =
    R"json({"tag":"root-ext","nested":{"array":[1,2]}})json";

class TemporaryDirectory final {
  public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path() / "elf3d_gltf_metadata_test") {
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

[[nodiscard]] bool metadata_matches(elf3d::ModelJsonMetadataView metadata, std::string_view scope) {
    const std::string expected_extras = "{\"tag\":\"" + std::string{scope} + "\"}";
    const std::string expected_extension = "{\"tag\":\"" + std::string{scope} + "-ext\"}";
    return metadata.extras_json == std::optional<std::string_view>{expected_extras} &&
           metadata.extensions.size() == 1U && metadata.extensions[0].name == "EXT_elf_raw" &&
           metadata.extensions[0].data == expected_extension;
}

[[nodiscard]] bool metadata_is_empty(elf3d::ModelJsonMetadataView metadata) noexcept {
    return !metadata.extras_json.has_value() && metadata.extensions.empty();
}

[[nodiscard]] bool root_metadata_matches(const elf3d::DocumentView& view) {
    return view.root_metadata().extras_json ==
               std::optional<std::string_view>{expected_root_extras} &&
           view.root_metadata().extensions.size() == 1U &&
           view.root_metadata().extensions[0].data == expected_root_extension &&
           metadata_matches(view.asset_metadata(), "asset");
}

[[nodiscard]] bool scene_graph_metadata_matches(const elf3d::DocumentView& view) {
    const auto scene = view.scene_at(0U);
    const auto node = view.node_at(0U);
    const auto mesh = view.mesh_at(0U);
    const auto primitive = view.primitive_at(0U);
    return scene && node && mesh && primitive &&
           metadata_matches(scene.value().metadata, "scene") &&
           metadata_matches(node.value().metadata, "node") &&
           metadata_matches(mesh.value().metadata, "mesh") &&
           metadata_matches(primitive.value().metadata, "primitive");
}

[[nodiscard]] bool resource_metadata_matches(const elf3d::DocumentView& view) {
    const auto material = view.material_at(0U);
    const auto image = view.image_at(0U);
    const auto texture = view.texture_at(0U);
    const auto sampler = view.sampler_at(0U);
    return material && image && texture && sampler &&
           metadata_matches(material.value().metadata, "material") &&
           metadata_matches(image.value().metadata, "image") &&
           metadata_matches(texture.value().metadata, "texture") &&
           metadata_matches(sampler.value().metadata, "sampler") &&
           material.value().description.ior == 1.33F;
}

[[nodiscard]] bool all_supported_metadata_matches(const elf3d::Document& document) {
    const elf3d::DocumentView view = document.view();
    return root_metadata_matches(view) && scene_graph_metadata_matches(view) &&
           resource_metadata_matches(view);
}

[[nodiscard]] bool scene_graph_metadata_is_empty(const elf3d::DocumentView& view) {
    const auto scene = view.scene_at(0U);
    const auto node = view.node_at(0U);
    const auto mesh = view.mesh_at(0U);
    const auto primitive = view.primitive_at(0U);
    return scene && node && mesh && primitive && metadata_is_empty(scene.value().metadata) &&
           metadata_is_empty(node.value().metadata) && metadata_is_empty(mesh.value().metadata) &&
           metadata_is_empty(primitive.value().metadata);
}

[[nodiscard]] bool resource_metadata_is_empty(const elf3d::DocumentView& view) {
    const auto material = view.material_at(0U);
    const auto image = view.image_at(0U);
    const auto texture = view.texture_at(0U);
    const auto sampler = view.sampler_at(0U);
    return material && image && texture && sampler &&
           metadata_is_empty(material.value().metadata) &&
           metadata_is_empty(image.value().metadata) &&
           metadata_is_empty(texture.value().metadata) &&
           metadata_is_empty(sampler.value().metadata);
}

[[nodiscard]] bool all_supported_metadata_is_empty(const elf3d::Document& document) {
    const elf3d::DocumentView view = document.view();
    return metadata_is_empty(view.root_metadata()) && metadata_is_empty(view.asset_metadata()) &&
           scene_graph_metadata_is_empty(view) && resource_metadata_is_empty(view);
}

[[nodiscard]] bool has_unpreserved_metadata_diagnostic(const elf3d::ModelLoadReport& report) {
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [](const elf3d::ModelLoadDiagnostic& diagnostic) noexcept {
                           return diagnostic.code ==
                                  elf3d::ModelLoadDiagnosticCode::metadata_not_preserved;
                       });
}

[[nodiscard]] bool round_trip_preserves(const std::filesystem::path& path,
                                        const elf3d::Document& document) {
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    return written && written.value().diagnostics.empty() && loaded &&
           all_supported_metadata_matches(loaded.value().document);
}

[[nodiscard]] bool stale_write_drops(const std::filesystem::path& path,
                                     const elf3d::Document& document) {
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    return written && written.value().diagnostics.size() == 1U &&
           written.value().diagnostics[0].code ==
               elf3d::ModelWriteDiagnosticCode::preserved_metadata_dropped_after_mutation &&
           loaded && all_supported_metadata_is_empty(loaded.value().document) &&
           loaded.value().document.material_at(0U).value().description.ior == 1.33F;
}

[[nodiscard]] int verify_preserved_metadata(const TemporaryDirectory& temporary,
                                            const elf3d::LoadedDocument& loaded) {
    if (!all_supported_metadata_matches(loaded.document)) {
        return 21;
    }
    if (!has_unpreserved_metadata_diagnostic(loaded.report)) {
        return 22;
    }
    if (loaded.document.preserved_metadata_stale()) {
        return 23;
    }
    if (!round_trip_preserves(temporary.path() / "preserved.gltf", loaded.document) ||
        !round_trip_preserves(temporary.path() / "preserved.glb", loaded.document)) {
        return 3;
    }
    const auto json = read_text(temporary.path() / "preserved.gltf");
    if (!json || json->find(R"json("extensionsUsed": ["KHR_materials_ior", "EXT_elf_raw"])json") ==
                     std::string::npos) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int mark_metadata_stale(elf3d::Document& document) {
    elf3d::Document foreign;
    const auto foreign_node = foreign.create_node();
    if (!foreign_node || document.set_node_matrix(foreign_node.value(), {}) ||
        document.preserved_metadata_stale()) {
        return 5;
    }
    const auto node = document.node_at(0U);
    if (!node) {
        return 6;
    }
    elf3d::Float4x4 changed = node.value().local_matrix;
    changed.elements[12] = 2.0F;
    if (!document.set_node_matrix(node.value().id, changed) ||
        !document.preserved_metadata_stale()) {
        return 7;
    }
    return 0;
}

[[nodiscard]] int verify_stale_outputs(const TemporaryDirectory& temporary,
                                       const elf3d::Document& document) {
    const elf3d::DocumentValidationReport validation = elf3d::validate_document(document.view());
    const std::size_t warnings = static_cast<std::size_t>(std::count_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const elf3d::DocumentDiagnostic& diagnostic) noexcept {
            return diagnostic.code == elf3d::DocumentDiagnosticCode::stale_preserved_metadata;
        }));
    if (validation.has_errors() || warnings != 1U ||
        !stale_write_drops(temporary.path() / "stale.gltf", document) ||
        !stale_write_drops(temporary.path() / "stale.glb", document)) {
        return 8;
    }
    const auto json = read_text(temporary.path() / "stale.gltf");
    if (!json || json->find("EXT_elf_raw") != std::string::npos ||
        json->find("KHR_materials_ior") == std::string::npos) {
        return 9;
    }
    return 0;
}

[[nodiscard]] int verify_replacement_stales(const std::filesystem::path& source) {
    auto loaded = elf3d::load_document(source.string());
    if (!loaded) {
        return 10;
    }
    const auto primitive = loaded.value().document.primitive_at(0U);
    if (!primitive) {
        return 10;
    }
    elf3d::PrimitiveData replacement;
    replacement.positions.assign(primitive.value().data.positions.begin(),
                                 primitive.value().data.positions.end());
    replacement.normals.assign(primitive.value().data.normals.begin(),
                               primitive.value().data.normals.end());
    replacement.texcoord0.assign(primitive.value().data.texcoord0.begin(),
                                 primitive.value().data.texcoord0.end());
    replacement.texcoord1.assign(primitive.value().data.texcoord1.begin(),
                                 primitive.value().data.texcoord1.end());
    replacement.colors.assign(primitive.value().data.colors.begin(),
                              primitive.value().data.colors.end());
    replacement.indices.assign(primitive.value().data.indices.begin(),
                               primitive.value().data.indices.end());
    if (!loaded.value().document.replace_primitive(primitive.value().id, std::move(replacement)) ||
        !loaded.value().document.preserved_metadata_stale()) {
        return 10;
    }
    return 0;
}

} // namespace

int elf3d_gltf_metadata_round_trip_test() {
    TemporaryDirectory temporary;
    const std::filesystem::path source = temporary.path() / "metadata.gltf";
    if (!write_text(source, metadata_fixture)) {
        return 1;
    }
    auto loaded = elf3d::load_document(source.string());
    if (!loaded) {
        return 20;
    }
    const int preserved = verify_preserved_metadata(temporary, loaded.value());
    if (preserved != 0) {
        return preserved;
    }
    const int stale = mark_metadata_stale(loaded.value().document);
    if (stale != 0) {
        return stale;
    }
    const int stale_outputs = verify_stale_outputs(temporary, loaded.value().document);
    if (stale_outputs != 0) {
        return stale_outputs;
    }
    return verify_replacement_stales(source);
}
