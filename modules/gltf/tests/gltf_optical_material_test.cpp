#include "gltf_import_test_support.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
using namespace elf3d;
using namespace elf3d::gltf::import_tests;

constexpr std::string_view fixture = R"json({
"asset":{"version":"2.0"},
"extensionsUsed":["KHR_materials_transmission","KHR_materials_volume","KHR_materials_clearcoat","KHR_materials_specular","KHR_materials_ior","KHR_texture_transform","EXT_material_note"],
"extensionsRequired":["KHR_materials_transmission","KHR_materials_volume","KHR_materials_clearcoat","KHR_materials_specular","KHR_materials_ior"],
"buffers":[{"uri":"geometry.bin","byteLength":96}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],
"images":[{"uri":"image.png"}],"samplers":[{"magFilter":9728,"minFilter":9987,"wrapS":33648,"wrapT":33071}],"textures":[{"source":0,"sampler":0}],
"materials":[{"name":"optical","alphaMode":"OPAQUE","extras":{"note":"retained"},"normalTexture":{"index":0},"extensions":{
"KHR_materials_ior":{"ior":0},
"KHR_materials_specular":{"specularFactor":0.25,"specularColorFactor":[30,2,0.5],"specularTexture":{"index":0,"texCoord":1},"specularColorTexture":{"index":0}},
"KHR_materials_transmission":{"transmissionFactor":0.8,"transmissionTexture":{"index":0,"texCoord":1,"extensions":{"KHR_texture_transform":{"offset":[0.25,0.5],"scale":[-2,3],"rotation":0.5}}}},
"KHR_materials_volume":{"thicknessFactor":1,"thicknessTexture":{"index":0,"texCoord":1},"attenuationDistance":0.3,"attenuationColor":[0.0078431377,0.0078431377,0.0078431377]},
"KHR_materials_clearcoat":{"clearcoatFactor":0.7,"clearcoatRoughnessFactor":0.2,"clearcoatTexture":{"index":0},"clearcoatRoughnessTexture":{"index":0,"texCoord":1},"clearcoatNormalTexture":{"index":0,"texCoord":1,"scale":0.4}},
"EXT_material_note":{"value":9007199254740993}}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TEXCOORD_1":2},"material":0}]}],
"nodes":[{"mesh":0,"matrix":[0.3048,0,0,0,0.1,0.3048,0,0,0,0,-0.3048,0,0,0,0,1]}],"scenes":[{"nodes":[0]}],"scene":0
})json";

bool verify_optical_factors(const ModelMaterialDescription& m)
{
    const std::array<bool, 10> equal{m.ior == 0,
                                     m.alpha_mode == AlphaMode::opaque,
                                     m.specular_color_factor.x == 30,
                                     m.specular_factor == .25F,
                                     m.transmission->factor == .8F,
                                     m.volume->thickness_factor == 1,
                                     m.volume->attenuation_distance == .3F,
                                     m.clearcoat->factor == .7F,
                                     m.clearcoat->roughness_factor == .2F,
                                     m.clearcoat->normal_scale == .4F};
    return std::all_of(equal.begin(), equal.end(), [](bool value) { return value; });
}
bool verify_optical_mappings(const ModelMaterialDescription& m)
{
    const auto& mapping = m.transmission->texture_mapping;
    if (mapping.texcoord_set != 1 || mapping.transform.offset != Float2{0.25F, 0.5F} ||
        mapping.transform.scale != Float2{-2, 3} || mapping.transform.rotation_radians != 0.5F ||
        m.specular_texture_mapping.texcoord_set != 1 ||
        m.clearcoat->normal_texture_mapping.texcoord_set != 1)
        return false;
    return true;
}
bool verify_optical_textures(const Document& document, const ModelMaterialDescription& m)
{
    const std::array<TextureId, 7> slots{m.specular_texture,         m.specular_color_texture,
                                         m.transmission->texture,    m.volume->thickness_texture,
                                         m.clearcoat->texture,       m.clearcoat->roughness_texture,
                                         m.clearcoat->normal_texture};
    for (auto texture : slots) {
        if (!texture.is_valid() || texture != m.normal_texture || !document.texture(texture))
            return false;
    }
    return true;
}

bool verify_preserved_optical_metadata(ModelJsonMetadataView metadata)
{
    for (const auto& extension : metadata.extensions)
        if (extension.name == "EXT_material_note" &&
            extension.data.find("9007199254740993") != std::string::npos)
            return true;
    return false;
}

bool verify(const Document& document)
{
    if (validate_document(document.view()).has_errors())
        return false;
    auto view = document.material_at(0);
    if (!view)
        return false;
    const auto& m = view.value().description;
    if (!m.transmission || !m.volume || !m.clearcoat || !view.value().metadata.extras_json)
        return false;
    if (!verify_optical_factors(m))
        return false;
    if (!verify_optical_textures(document, m))
        return false;
    if (!verify_optical_mappings(m))
        return false;
    return verify_preserved_optical_metadata(view.value().metadata);
}
} // namespace

int verify_missing_optical_uv(const std::filesystem::path& input)
{
    // Both primitives share the source material. Only the second lacks UV1.
    std::string missing{fixture};
    constexpr std::string_view primitive = "{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,"
                                           "\"TEXCOORD_0\":2,\"TEXCOORD_1\":2},\"material\":0}";
    std::string fallback_primitive{primitive};
    constexpr std::string_view uv1 = ",\"TEXCOORD_1\":2";
    fallback_primitive.erase(fallback_primitive.find(uv1), uv1.size());
    missing.replace(missing.find(primitive), primitive.size(),
                    std::string{primitive} + "," + fallback_primitive);
    if (!write_text(input, missing))
        return 6;
    auto fallback = load_document(input.string());
    if (!fallback)
        return 7;
    if (!verify(fallback.value().document))
        return 7;
    const auto fallback_material = fallback.value().document.material_at(1);
    if (!fallback_material)
        return 7;
    const auto material = fallback_material.value().description;
    if (material.transmission->texture.is_valid() ||
        material.volume->thickness_texture.is_valid() || material.specular_texture.is_valid() ||
        material.clearcoat->normal_texture.is_valid() ||
        !material.specular_color_texture.is_valid())
        return 8;
    return 0;
}
int verify_optical_roundtrips(const Document& document, const std::filesystem::path& directory)
{
    for (auto suffix : {".gltf", ".glb"}) {
        auto output = directory / (std::string{"roundtrip"} + suffix);
        if (!save_document(output.string(), document.view()))
            return 4;
        auto reimported = load_document(output.string());
        if (!reimported || !verify(reimported.value().document))
            return 5;
    }
    return 0;
}

int verify_optical_defaults(const std::filesystem::path& input)
{
    std::string source{fixture};
    constexpr std::string_view normal =
        ",\"clearcoatNormalTexture\":{\"index\":0,\"texCoord\":1,\"scale\":0.4}";
    source.erase(source.find(normal), normal.size());
    if (!write_text(input, source))
        return 9;
    auto loaded = load_document(input.string());
    if (!loaded)
        return 9;
    const auto material = loaded.value().document.material_at(0);
    if (!material || !material.value().description.clearcoat)
        return 9;
    return material.value().description.clearcoat->normal_scale == 1.0F ? 0 : 9;
}

bool has_no_material_fallbacks(const ModelLoadReport& report)
{
    for (const auto& diagnostic : report.diagnostics)
        if (diagnostic.code == ModelLoadDiagnosticCode::material_fallback)
            return false;
    return true;
}

int elf3d_gltf_optical_material_test()
{
    TemporaryDirectory temporary;
    auto input = temporary.path() / "optical.gltf";
    if (!write_text(input, fixture) ||
        !write_bytes(temporary.path() / "geometry.bin", textured_geometry()) ||
        !write_bytes(temporary.path() / "image.png", std::as_bytes(std::span{asymmetric_png})))
        return 1;
    auto loaded = load_document(input.string());
    if (!loaded || !verify(loaded.value().document))
        return 2;
    if (!has_no_material_fallbacks(loaded.value().report))
        return 3;
    if (const auto roundtrip = verify_optical_roundtrips(loaded.value().document, temporary.path()))
        return roundtrip;
    if (const auto defaults = verify_optical_defaults(input))
        return defaults;
    const auto missing = verify_missing_optical_uv(input);
    if (missing)
        return missing;
    std::cout << "Optical material glTF/GLB persistence, required extensions, metadata and UV "
                 "fallback passed\n";
    return 0;
}
