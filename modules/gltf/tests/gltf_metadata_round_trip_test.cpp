#include <elf3d/model.h>

#include "../src/json_document.h"

#include <algorithm>
#include <cstddef>
#include <elf3d/internal/gltf.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view metadata_fixture = R"json({
  "asset":{"version":"2.0","extras":{"tag":"asset"},"extensions":{"EXT_elf_raw":{"tag":"asset-ext"}}},
  "extensionsUsed":["KHR_materials_ior","KHR_texture_transform","KHR_materials_volume","EXT_elf_raw","EXT_fixture_table"],
  "extensionsRequired":["KHR_texture_transform"],
  "extras":{"tag":"root","nested":[1,true,null,{"k":"brace }"}]},
  "extensions":{"EXT_elf_raw":{"tag":"root-ext","nested":{"array":[1,2]}},"EXT_fixture_table":{"entries":[{"name":"Fixture entry","custom":{"exact":9007199254740993,"label":"Light ☀"}}]}},
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
  "materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}},"extras":{"tag":"material"},"extensions":{"KHR_materials_ior":{"ior":1.33},"KHR_materials_volume":{"thicknessFactor":1,"thicknessTexture":{"index":0,"extensions":{"KHR_texture_transform":{"offset":[0.25,0.5]}}},"attenuationDistance":2.5,"attenuationColor":[0.1,0.2,0.3]},"EXT_elf_raw":{"tag":"material-ext"},"EXT_fixture_table":{"entry":0}}}],
  "meshes":[{"name":"Mesh \"quoted\"","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0,"extras":{"tag":"primitive"},"extensions":{"EXT_elf_raw":{"tag":"primitive-ext"}}}],"extras":{"tag":"mesh"},"extensions":{"EXT_elf_raw":{"tag":"mesh-ext"}}}],
  "nodes":[{"name":"Node \"quoted\" \\ path \u2600","mesh":0,"extras":{"tag":"node"},"extensions":{"EXT_elf_raw":{"tag":"node-ext"}}}],
  "scenes":[{"name":"Scene \uD83C\uDF0D","nodes":[0],"extras":{"tag":"scene"},"extensions":{"EXT_elf_raw":{"tag":"scene-ext"}}}],
  "scene":0
})json";

constexpr std::string_view expected_root_extras =
    R"json({"tag":"root","nested":[1,true,null,{"k":"brace }"}]})json";
constexpr std::string_view expected_root_extension =
    R"json({"tag":"root-ext","nested":{"array":[1,2]}})json";

class TemporaryDirectory final {
  public:
    TemporaryDirectory() : path_(std::filesystem::path{ELF3D_TEST_BINARY_DIR} / "gltf_metadata")
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] bool write_text(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream{path, std::ios::binary};
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path)
{
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] bool json_equal(std::string_view left, std::string_view right)
{
    const auto parsed_left = elf3d::gltf::json::parse(left);
    const auto parsed_right = elf3d::gltf::json::parse(right);
    if (!parsed_left || !parsed_right) {
        return false;
    }
    const auto normalized_left = elf3d::gltf::json::write(parsed_left.value());
    const auto normalized_right = elf3d::gltf::json::write(parsed_right.value());
    return normalized_left && normalized_right &&
           normalized_left.value() == normalized_right.value();
}

[[nodiscard]] bool metadata_matches(elf3d::ModelJsonMetadataView metadata, std::string_view scope)
{
    const std::string expected_extras = "{\"tag\":\"" + std::string{scope} + "\"}";
    const std::string expected_extension = "{\"tag\":\"" + std::string{scope} + "-ext\"}";
    return metadata.extras_json && json_equal(*metadata.extras_json, expected_extras) &&
           !metadata.extensions.empty() && metadata.extensions[0].name == "EXT_elf_raw" &&
           json_equal(metadata.extensions[0].data, expected_extension);
}

[[nodiscard]] bool metadata_is_empty(elf3d::ModelJsonMetadataView metadata) noexcept
{
    return !metadata.extras_json.has_value() && metadata.extensions.empty();
}

[[nodiscard]] bool light_table_matches(elf3d::ModelJsonMetadataView metadata)
{
    constexpr std::string_view expected =
        R"json({"entries":[{"name":"Fixture entry","custom":{"exact":9007199254740993,"label":"Light ☀"}}]})json";
    const auto extension = std::find_if(metadata.extensions.begin(), metadata.extensions.end(),
                                        [](const elf3d::ModelJsonExtension& value) noexcept {
                                            return value.name == "EXT_fixture_table";
                                        });
    return extension != metadata.extensions.end() && json_equal(extension->data, expected);
}

[[nodiscard]] bool material_light_matches(elf3d::ModelJsonMetadataView metadata)
{
    const auto extension = std::find_if(metadata.extensions.begin(), metadata.extensions.end(),
                                        [](const elf3d::ModelJsonExtension& value) noexcept {
                                            return value.name == "EXT_fixture_table";
                                        });
    return extension != metadata.extensions.end() &&
           json_equal(extension->data, R"json({"entry":0})json");
}

[[nodiscard]] bool volume_matches(const elf3d::ModelMaterialDescription& material)
{
    if (!material.volume) {
        return false;
    }
    const auto& volume = *material.volume;
    return volume.thickness_factor == 1.0F && volume.attenuation_distance == 2.5F &&
           volume.attenuation_color == elf3d::Float3{0.1F, 0.2F, 0.3F} &&
           volume.thickness_texture == material.base_color_texture &&
           (!volume.thickness_texture.is_valid() ||
            volume.thickness_texture_mapping.transform.offset == elf3d::Float2{0.25F, 0.5F});
}

[[nodiscard]] bool root_metadata_matches(const elf3d::DocumentView& view)
{
    return view.root_metadata().extras_json &&
           json_equal(*view.root_metadata().extras_json, expected_root_extras) &&
           view.root_metadata().extensions.size() == 2U &&
           json_equal(view.root_metadata().extensions[0].data, expected_root_extension) &&
           light_table_matches(view.root_metadata()) && view.required_extensions().size() == 1U &&
           view.required_extensions()[0] == "KHR_texture_transform" &&
           metadata_matches(view.asset_metadata(), "asset");
}

[[nodiscard]] bool scene_graph_metadata_matches(const elf3d::DocumentView& view)
{
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

[[nodiscard]] bool resource_values_match(const elf3d::MaterialView& material,
                                         const elf3d::ImageView& image,
                                         const elf3d::TextureView& texture,
                                         const elf3d::SamplerView& sampler)
{
    return metadata_matches(material.metadata, "material") &&
           metadata_matches(image.metadata, "image") &&
           metadata_matches(texture.metadata, "texture") &&
           metadata_matches(sampler.metadata, "sampler") &&
           material_light_matches(material.metadata) && volume_matches(material.description) &&
           material.description.ior == 1.33F;
}

[[nodiscard]] bool resource_metadata_matches(const elf3d::DocumentView& view)
{
    const auto material = view.material_at(0U);
    const auto image = view.image_at(0U);
    const auto texture = view.texture_at(0U);
    const auto sampler = view.sampler_at(0U);
    return material && image && texture && sampler &&
           resource_values_match(material.value(), image.value(), texture.value(), sampler.value());
}

[[nodiscard]] bool all_supported_metadata_matches(const elf3d::Document& document)
{
    const elf3d::DocumentView view = document.view();
    return root_metadata_matches(view) && scene_graph_metadata_matches(view) &&
           resource_metadata_matches(view);
}

[[nodiscard]] bool names_match(const elf3d::Document& document)
{
    return document.node_at(0U).value().name == "Node \"quoted\" \\ path ☀" &&
           document.mesh_at(0U).value().name == "Mesh \"quoted\"" &&
           document.scene_at(0U).value().name == "Scene 🌍";
}

[[nodiscard]] bool scene_graph_metadata_is_empty(const elf3d::DocumentView& view)
{
    const auto scene = view.scene_at(0U);
    const auto node = view.node_at(0U);
    const auto mesh = view.mesh_at(0U);
    const auto primitive = view.primitive_at(0U);
    return scene && node && mesh && primitive && metadata_is_empty(scene.value().metadata) &&
           metadata_is_empty(node.value().metadata) && metadata_is_empty(mesh.value().metadata) &&
           metadata_is_empty(primitive.value().metadata);
}

[[nodiscard]] bool resource_metadata_is_empty(const elf3d::DocumentView& view)
{
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

[[nodiscard]] bool all_supported_metadata_is_empty(const elf3d::Document& document)
{
    const elf3d::DocumentView view = document.view();
    return metadata_is_empty(view.root_metadata()) && metadata_is_empty(view.asset_metadata()) &&
           scene_graph_metadata_is_empty(view) && resource_metadata_is_empty(view);
}

[[nodiscard]] bool has_unpreserved_metadata_diagnostic(const elf3d::ModelLoadReport& report)
{
    return std::any_of(report.diagnostics.begin(), report.diagnostics.end(),
                       [](const elf3d::ModelLoadDiagnostic& diagnostic) noexcept {
                           return diagnostic.code ==
                                  elf3d::ModelLoadDiagnosticCode::metadata_not_preserved;
                       });
}

[[nodiscard]] bool round_trip_preserves(const std::filesystem::path& path,
                                        const elf3d::Document& document)
{
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    return written && written.value().diagnostics.empty() && loaded &&
           !elf3d::validate_document(loaded.value().document.view()).has_errors() &&
           loaded.value().document.node_at(0U).value().local_matrix ==
               document.node_at(0U).value().local_matrix &&
           all_supported_metadata_matches(loaded.value().document) &&
           names_match(loaded.value().document);
}

[[nodiscard]] bool stale_write_drops(const std::filesystem::path& path,
                                     const elf3d::Document& document)
{
    const auto written = elf3d::save_document(path.string(), document.view());
    const auto loaded = elf3d::load_document(path.string());
    return written && written.value().diagnostics.size() == 1U &&
           written.value().diagnostics[0].code ==
               elf3d::ModelWriteDiagnosticCode::preserved_metadata_dropped_after_mutation &&
           loaded && all_supported_metadata_is_empty(loaded.value().document) &&
           loaded.value().document.material_at(0U).value().description.ior == 1.33F &&
           loaded.value().document.required_extensions().size() == 1U;
}

[[nodiscard]] int verify_preserved_metadata(const TemporaryDirectory& temporary,
                                            const elf3d::LoadedDocument& loaded)
{
    if (!all_supported_metadata_matches(loaded.document) || !names_match(loaded.document)) {
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
    if (!json || json->find(R"json("extensionsRequired": ["KHR_texture_transform"])json") ==
                     std::string::npos) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_matrix_edit(elf3d::Document& document)
{
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
    changed.elements = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1};
    if (!document.set_node_matrix(node.value().id, changed) ||
        document.preserved_metadata_stale()) {
        return 7;
    }
    if (!all_supported_metadata_matches(document) ||
        elf3d::validate_document(document.view()).has_errors()) {
        return 31;
    }
    return 0;
}

[[nodiscard]] int verify_stale_outputs(const TemporaryDirectory& temporary,
                                       const elf3d::Document& document)
{
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

[[nodiscard]] int verify_replacement_stales(const std::filesystem::path& source)
{
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

[[nodiscard]] int verify_volume_validation(const elf3d::Document& source)
{
    elf3d::Document document;
    elf3d::ModelMaterialDescription description;
    description.volume = elf3d::ModelVolumeDescription{};
    auto& volume = *description.volume;
    volume.thickness_texture = source.texture_at(0U).value().id;
    const auto foreign = document.create_material(description);
    if (foreign || foreign.error().code() != elf3d::ErrorCode::invalid_texture_id) {
        return 41;
    }
    volume.thickness_texture = {};
    volume.thickness_factor = -1.0F;
    if (document.create_material(description)) {
        return 42;
    }
    volume.thickness_factor = 1.0F;
    for (const float distance : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                                 std::numeric_limits<float>::quiet_NaN()}) {
        volume.attenuation_distance = distance;
        if (document.create_material(description)) {
            return 43;
        }
    }
    volume.attenuation_distance.reset();
    volume.attenuation_color.x = 1.1F;
    if (document.create_material(description)) {
        return 44;
    }
    volume.attenuation_color.x = 1.0F;
    volume.thickness_texture_mapping.texcoord_set = 2U;
    if (document.create_material(description)) {
        return 45;
    }
    volume.thickness_texture_mapping.texcoord_set = 0U;
    return document.create_material(description) ? 0 : 46;
}

[[nodiscard]] int verify_infinite_volume(const TemporaryDirectory& temporary)
{
    std::string fixture{metadata_fixture};
    constexpr std::string_view distance = "\"attenuationDistance\":2.5,";
    fixture.erase(fixture.find(distance), distance.size());
    const auto input = temporary.path() / "infinite-input.gltf";
    if (!write_text(input, fixture)) {
        return 47;
    }
    const auto original = elf3d::load_document(input.string());
    if (!original) {
        return 47;
    }
    const auto path = temporary.path() / "infinite-volume.gltf";
    const auto written = elf3d::save_document(path.string(), original.value().document.view());
    const auto json = read_text(path);
    const auto loaded = elf3d::load_document(path.string());
    if (!written || !json || json->find("attenuationDistance") != std::string::npos || !loaded) {
        return 48;
    }
    const auto material = loaded.value().document.material_at(0U).value();
    return material.description.volume && !material.description.volume->attenuation_distance &&
                   material.description.volume->thickness_factor == 1.0F
               ? 0
               : 49;
}

[[nodiscard]] bool clone_output_matches(const elf3d::Document& document)
{
    if (document.material_count() != 2U || !light_table_matches(document.root_metadata())) {
        return false;
    }
    for (std::size_t index = 0; index < 2U; ++index) {
        const auto material = document.material_at(index).value();
        if (!metadata_matches(material.metadata, "material") ||
            !material_light_matches(material.metadata) || !volume_matches(material.description)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] int verify_material_clones(const TemporaryDirectory& temporary)
{
    std::string fixture{metadata_fixture};
    const auto insertion = fixture.find("],\"extras\":{\"tag\":\"mesh\"}");
    if (insertion == std::string::npos) {
        return 50;
    }
    fixture.insert(insertion, R"json(,{"attributes":{"POSITION":0,"NORMAL":1},"material":0})json");
    const auto source = temporary.path() / "cloned.gltf";
    if (!write_text(source, fixture)) {
        return 51;
    }
    auto loaded = elf3d::load_document(source.string());
    if (!loaded || loaded.value().document.material_count() != 2U) {
        return 52;
    }
    auto& document = loaded.value().document;
    const auto node_id = document.node_at(0U).value().id;
    auto matrix = document.node_at(0U).value().local_matrix;
    matrix.elements[12] = 3.0F;
    if (!document.set_node_matrix(node_id, matrix)) {
        return 53;
    }
    const auto destination = temporary.path() / "cloned.glb";
    if (!elf3d::save_document(destination.string(), document.view())) {
        return 54;
    }
    const auto reloaded = elf3d::load_document(destination.string());
    if (!reloaded || !clone_output_matches(reloaded.value().document)) {
        return 55;
    }
    return 0;
}

[[nodiscard]] int verify_matrix_and_structure(const TemporaryDirectory& temporary,
                                              elf3d::Document& document)
{
    const int matrix = verify_matrix_edit(document);
    if (matrix != 0) {
        return matrix;
    }
    if (!round_trip_preserves(temporary.path() / "rotated.gltf", document) ||
        !round_trip_preserves(temporary.path() / "rotated.glb", document)) {
        return 32;
    }
    if (!document.create_node("Structural edit") || !document.preserved_metadata_stale()) {
        return 33;
    }
    return verify_stale_outputs(temporary, document);
}

[[nodiscard]] int verify_reverse_containers(const TemporaryDirectory& temporary)
{
    auto loaded = elf3d::load_document((temporary.path() / "preserved.glb").string());
    if (!loaded) {
        return 57;
    }
    const int matrix = verify_matrix_edit(loaded.value().document);
    if (matrix != 0) {
        return matrix;
    }
    return round_trip_preserves(temporary.path() / "glb-input.gltf", loaded.value().document) &&
                   round_trip_preserves(temporary.path() / "glb-input.glb", loaded.value().document)
               ? 0
               : 58;
}

[[nodiscard]] int verify_required_identity_pruning(const TemporaryDirectory& temporary)
{
    std::string fixture{metadata_fixture};
    constexpr std::string_view offset = "[0.25,0.5]";
    fixture.replace(fixture.find(offset), offset.size(), "[0,0]");
    const auto source = temporary.path() / "required-identity.gltf";
    if (!write_text(source, fixture)) {
        return 59;
    }
    const auto original = elf3d::load_document(source.string());
    if (!original || original.value().document.required_extensions().size() != 1U) {
        return 60;
    }
    const auto path = temporary.path() / "required-pruned.gltf";
    if (!elf3d::save_document(path.string(), original.value().document.view())) {
        return 61;
    }
    const auto json = read_text(path);
    const auto loaded = elf3d::load_document(path.string());
    return json && json->find("extensionsRequired") == std::string::npos &&
                   json->find("KHR_texture_transform") == std::string::npos && loaded &&
                   loaded.value().document.required_extensions().empty()
               ? 0
               : 62;
}

[[nodiscard]] int verify_invalid_names(const TemporaryDirectory& temporary)
{
    constexpr std::string_view name = R"json("Node \"quoted\" \\ path \u2600")json";
    for (const std::string_view invalid :
         {R"json("\uD800")json", R"json("\uDC00")json", R"json("bad\u0000name")json"}) {
        std::string fixture{metadata_fixture};
        fixture.replace(fixture.find(name), name.size(), invalid);
        const auto path = temporary.path() / "invalid-name.gltf";
        if (!write_text(path, fixture)) {
            return 63;
        }
        const auto loaded = elf3d::load_document(path.string());
        if (loaded || loaded.error().code() != elf3d::ErrorCode::gltf_validation_failed) {
            return 64;
        }
    }
    return 0;
}

} // namespace

int elf3d_gltf_metadata_round_trip_test()
{
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
    for (const int result :
         {verify_volume_validation(loaded.value().document), verify_infinite_volume(temporary),
          verify_material_clones(temporary), verify_reverse_containers(temporary),
          verify_required_identity_pruning(temporary), verify_invalid_names(temporary)}) {
        if (result != 0) {
            return result;
        }
    }

    const int stale_outputs = verify_matrix_and_structure(temporary, loaded.value().document);
    if (stale_outputs != 0) {
        return stale_outputs;
    }
    return verify_replacement_stales(source);
}
