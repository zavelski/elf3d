#include <elf3d/internal/gltf.h>

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "exporter_internal.hpp"
#include "exporter_json_internal.hpp"

#include <algorithm>
#include <limits>
namespace elf3d::gltf::exporter_detail {
namespace {

struct MaterialTextures {
    std::optional<std::uint32_t> base;
    std::optional<std::uint32_t> metallic_roughness;
    std::optional<std::uint32_t> normal;
    std::optional<std::uint32_t> occlusion;
    std::optional<std::uint32_t> emissive;
};

[[nodiscard]] Result<std::optional<std::uint32_t>> material_texture(const ExportData& document,
                                                                    TextureId id)
{
    if (!id.is_valid()) {
        return std::optional<std::uint32_t>{};
    }

    const std::optional<std::uint32_t> found = find_index(document.texture_indices, id);
    if (!found) {
        return Error{ErrorCode::invalid_argument, "Material texture is not in this document"};
    }
    return found;
}

[[nodiscard]] Result<MaterialTextures>
collect_material_textures(const ExportData& document, const ModelMaterialDescription& material)
{
    const Result<std::optional<std::uint32_t>> base =
        material_texture(document, material.base_color_texture);
    const Result<std::optional<std::uint32_t>> metallic =
        material_texture(document, material.metallic_roughness_texture);
    const Result<std::optional<std::uint32_t>> normal =
        material_texture(document, material.normal_texture);
    const Result<std::optional<std::uint32_t>> occlusion =
        material_texture(document, material.occlusion_texture);
    const Result<std::optional<std::uint32_t>> emissive =
        material_texture(document, material.emissive_texture);

    if (!base || !metallic || !normal || !occlusion || !emissive) {
        return Error{ErrorCode::invalid_argument, "Material texture is not in this document"};
    }
    return MaterialTextures{base.value(), metallic.value(), normal.value(), occlusion.value(),
                            emissive.value()};
}

void append_material_pbr(std::string& output, const ModelMaterialDescription& material,
                         const MaterialTextures& textures)
{
    output.append("{\"pbrMetallicRoughness\":{\"baseColorFactor\":");
    append_float4(output, material.base_color);
    output.append(",\"metallicFactor\":");
    append_float(output, material.metallic_factor);
    output.append(",\"roughnessFactor\":");
    append_float(output, material.roughness_factor);
    if (textures.base) {
        output.append(",\"baseColorTexture\":");
        append_texture_info(output, *textures.base, material.base_color_texture_mapping);
    }
    if (textures.metallic_roughness) {
        output.append(",\"metallicRoughnessTexture\":");
        append_texture_info(output, *textures.metallic_roughness,
                            material.metallic_roughness_texture_mapping);
    }
    output.push_back('}');
}

void append_material_texture_slots(std::string& output, const ModelMaterialDescription& material,
                                   const MaterialTextures& textures)
{
    if (textures.normal) {
        output.append(",\"normalTexture\":");
        append_texture_info(output, *textures.normal, material.normal_texture_mapping,
                            std::pair<std::string_view, float>{"scale", material.normal_scale});
    }
    if (textures.occlusion) {
        output.append(",\"occlusionTexture\":");
        append_texture_info(
            output, *textures.occlusion, material.occlusion_texture_mapping,
            std::pair<std::string_view, float>{"strength", material.occlusion_strength});
    }
    if (textures.emissive) {
        output.append(",\"emissiveTexture\":");
        append_texture_info(output, *textures.emissive, material.emissive_texture_mapping);
    }
}

void append_material_properties(std::string& output, const ModelMaterialDescription& material)
{
    output.append(",\"emissiveFactor\":");
    append_float3(output, material.emissive_factor);
    if (material.alpha_mode != AlphaMode::opaque) {
        output.append(",\"alphaMode\":");
        append_string(output, material.alpha_mode == AlphaMode::mask ? "MASK" : "BLEND");
    }
    if (material.alpha_mode == AlphaMode::mask) {
        output.append(",\"alphaCutoff\":");
        append_float(output, material.alpha_cutoff);
    }
    if (material.double_sided) {
        output.append(",\"doubleSided\":true");
    }
}

void append_standard_material_extensions(std::string& output, bool& first,
                                         std::vector<std::string_view>& emitted,
                                         const ModelMaterialDescription& material)
{
    if (material.unlit && begin_extension_member(output, first, emitted, "KHR_materials_unlit")) {
        output.append("{}");
    }
    if (material.ior != 1.5F &&
        begin_extension_member(output, first, emitted, "KHR_materials_ior")) {
        output.append("{\"ior\":");
        append_float(output, material.ior);
        output.push_back('}');
    }
    if (material.emissive_strength != 1.0F &&
        begin_extension_member(output, first, emitted, "KHR_materials_emissive_strength")) {
        output.append("{\"emissiveStrength\":");
        append_float(output, material.emissive_strength);
        output.push_back('}');
    }
}

void append_specular_extension(std::string& output, bool& first,
                               std::vector<std::string_view>& emitted,
                               const ModelMaterialDescription& material)
{
    if (material.specular_factor == 1.0F &&
        material.specular_color_factor == Float3{1.0F, 1.0F, 1.0F}) {
        return;
    }
    if (!begin_extension_member(output, first, emitted, "KHR_materials_specular")) {
        return;
    }

    output.append("{\"specularFactor\":");
    append_float(output, material.specular_factor);
    output.append(",\"specularColorFactor\":");
    append_float3(output, material.specular_color_factor);
    output.push_back('}');
}

void append_material_extensions(std::string& output, const ModelMaterialDescription& material,
                                ModelJsonMetadataView metadata)
{
    const bool required = material.unlit || material.ior != 1.5F ||
                          material.emissive_strength != 1.0F || material.specular_factor != 1.0F ||
                          material.specular_color_factor != Float3{1.0F, 1.0F, 1.0F} ||
                          !metadata.extensions.empty();
    if (!required) {
        return;
    }

    output.append(",\"extensions\":{");
    bool first = true;
    std::vector<std::string_view> emitted;
    emitted.reserve(metadata.extensions.size() + 4U);
    append_standard_material_extensions(output, first, emitted, material);
    append_specular_extension(output, first, emitted, material);
    append_preserved_extension_members(output, first, emitted, metadata.extensions);
    output.push_back('}');
}

[[nodiscard]] Result<void> append_material(std::string& output, const ExportData& document,
                                           const MaterialView& view)
{
    const ModelMaterialDescription& material = view.description;
    const Result<MaterialTextures> textures = collect_material_textures(document, material);
    if (!textures) {
        return textures.error();
    }

    append_material_pbr(output, material, textures.value());
    append_material_texture_slots(output, material, textures.value());
    append_material_properties(output, material);
    append_material_extensions(output, material, view.metadata);
    append_preserved_extras(output, view.metadata);
    output.push_back('}');
    return {};
}

} // namespace

[[nodiscard]] Result<void> append_materials(std::string& output, const ExportData& document)
{
    if (document.materials.empty()) {
        return {};
    }

    output.append(",\"materials\":[");
    for (std::size_t index = 0; index < document.materials.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        if (const Result<void> material =
                append_material(output, document, document.materials[index]);
            !material) {
            return material.error();
        }
    }
    output.push_back(']');
    return {};
}

} // namespace elf3d::gltf::exporter_detail
