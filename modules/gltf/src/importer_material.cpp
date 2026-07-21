module;

#include <elf3d/core/error.h>
#include <elf3d/core/result.h>
#include <elf3d/model.h>

#include "importer_internal.hpp"

#include <cgltf.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module elf.gltf;

import elf.core;
import elf.math;
import elf.model;

namespace elf3d::gltf::importer_detail {

void generate_normals(std::span<const Float3> positions, std::vector<Float3>& normals,
                      std::span<const std::uint32_t> indices, std::uint64_t& degenerate_count,
                      std::uint64_t& fallback_count) {
    std::vector<Float3> accumulated(positions.size());
    for (std::size_t index = 0; index < indices.size(); index += 3) {
        const Float3 a = positions[indices[index]];
        const Float3 b = positions[indices[index + 1]];
        const Float3 c = positions[indices[index + 2]];
        const Float3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
        const Float3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
        const Float3 face{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z,
                          ab.x * ac.y - ab.y * ac.x};
        const float length_squared = face.x * face.x + face.y * face.y + face.z * face.z;
        if (!std::isfinite(length_squared) || length_squared <= 0.000000000001F) {
            ++degenerate_count;
            continue;
        }
        for (std::size_t offset = 0; offset < 3; ++offset) {
            Float3& normal = accumulated[indices[index + offset]];
            normal.x += face.x;
            normal.y += face.y;
            normal.z += face.z;
        }
    }

    normals.resize(positions.size());
    for (std::size_t index = 0; index < positions.size(); ++index) {
        const Float3 normal = accumulated[index];
        const float length_squared =
            normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (!std::isfinite(length_squared) || length_squared <= 0.000000000001F) {
            normals[index] = Float3{0.0F, 1.0F, 0.0F};
            ++fallback_count;
            continue;
        }
        const float inverse_length = 1.0F / std::sqrt(length_squared);
        normals[index] =
            Float3{normal.x * inverse_length, normal.y * inverse_length, normal.z * inverse_length};
    }
}

struct MaterialBuildState {
    ImportState& import_state;
    const cgltf_material& source;
    const TexcoordAvailability& available_texcoords;
    std::size_t index;
    std::string context;
    ModelMaterialDescription description;
    bool primitive_specific_fallback = false;
};

[[nodiscard]] Result<MaterialId> default_material_for(ImportState& state) {
    if (!state.default_material.has_value()) {
        const Result<MaterialId> created =
            state.builder.create_material(ModelMaterialDescription{});
        if (!created) {
            return created.error();
        }
        state.default_material = created.value();
    }
    return *state.default_material;
}

[[nodiscard]] Result<ImportedTextureView> import_material_texture(MaterialBuildState& state,
                                                                  const cgltf_texture_view& view,
                                                                  std::string_view slot,
                                                                  std::string_view context_suffix) {
    PrimitiveTextureState primitive_state{state.available_texcoords,
                                          state.primitive_specific_fallback};
    return import_texture_view(state.import_state, primitive_state, view, slot,
                               state.context + std::string{context_suffix});
}

[[nodiscard]] Result<void> apply_metallic_roughness(MaterialBuildState& state) {
    const cgltf_pbr_metallic_roughness& pbr = state.source.pbr_metallic_roughness;
    state.description.base_color = Color4{pbr.base_color_factor[0], pbr.base_color_factor[1],
                                          pbr.base_color_factor[2], pbr.base_color_factor[3]};
    state.description.metallic_factor = pbr.metallic_factor;
    state.description.roughness_factor = pbr.roughness_factor;
    if (pbr.base_color_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_material_texture(
            state, pbr.base_color_texture, "Base-color", " base-color texture");
        if (!texture) {
            return texture.error();
        }
        state.description.base_color_texture = texture.value().texture;
        state.description.base_color_texture_mapping = texture.value().mapping;
    }
    if (pbr.metallic_roughness_texture.texture != nullptr) {
        Result<ImportedTextureView> texture =
            import_material_texture(state, pbr.metallic_roughness_texture, "Metallic-roughness",
                                    " metallic-roughness texture");
        if (!texture) {
            return texture.error();
        }
        state.description.metallic_roughness_texture = texture.value().texture;
        state.description.metallic_roughness_texture_mapping = texture.value().mapping;
    }
    return {};
}

[[nodiscard]] Result<void> apply_specular_glossiness(MaterialBuildState& state) {
    const cgltf_pbr_specular_glossiness& pbr = state.source.pbr_specular_glossiness;
    state.description.base_color = Color4{pbr.diffuse_factor[0], pbr.diffuse_factor[1],
                                          pbr.diffuse_factor[2], pbr.diffuse_factor[3]};
    state.description.metallic_factor = 0.0F;
    state.description.roughness_factor = 1.0F - pbr.glossiness_factor;
    if (pbr.diffuse_texture.texture != nullptr) {
        Result<ImportedTextureView> texture =
            import_material_texture(state, pbr.diffuse_texture, "Specular-glossiness diffuse",
                                    " specular-glossiness diffuse texture");
        if (!texture) {
            return texture.error();
        }
        state.description.base_color_texture = texture.value().texture;
        state.description.base_color_texture_mapping = texture.value().mapping;
    }
    if (pbr.specular_glossiness_texture.texture != nullptr) {
        add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::material,
                       ModelLoadDiagnosticCode::material_fallback,
                       "KHR_materials_pbrSpecularGlossiness diffuse data was approximated, "
                       "but its specular-glossiness texture is ignored",
                       state.context);
    }
    return {};
}

[[nodiscard]] Result<void> apply_primary_material(MaterialBuildState& state) {
    if (state.source.has_pbr_metallic_roughness) {
        return apply_metallic_roughness(state);
    }
    if (state.source.has_pbr_specular_glossiness) {
        return apply_specular_glossiness(state);
    }
    return {};
}

void apply_material_factors(MaterialBuildState& state) {
    const cgltf_material& source = state.source;
    state.description.emissive_factor = {source.emissive_factor[0], source.emissive_factor[1],
                                         source.emissive_factor[2]};
    if (source.has_emissive_strength) {
        state.description.emissive_strength = source.emissive_strength.emissive_strength;
    }
    state.description.unlit = source.unlit != 0;
    state.description.ior = source.has_ior ? source.ior.ior : 1.5F;
    if (source.has_specular) {
        state.description.specular_factor = source.specular.specular_factor;
        state.description.specular_color_factor = {source.specular.specular_color_factor[0],
                                                   source.specular.specular_color_factor[1],
                                                   source.specular.specular_color_factor[2]};
        if (source.specular.specular_texture.texture != nullptr ||
            source.specular.specular_color_texture.texture != nullptr) {
            add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::material,
                           ModelLoadDiagnosticCode::material_fallback,
                           "KHR_materials_specular factors are rendered, but its optional "
                           "textures are ignored",
                           state.context);
        }
    }
}

[[nodiscard]] Result<void> apply_auxiliary_textures(MaterialBuildState& state) {
    const cgltf_material& source = state.source;
    if (source.normal_texture.texture != nullptr) {
        Result<ImportedTextureView> texture =
            import_material_texture(state, source.normal_texture, "Normal", " normal texture");
        if (!texture) {
            return texture.error();
        }
        state.description.normal_texture = texture.value().texture;
        state.description.normal_texture_mapping = texture.value().mapping;
        state.description.normal_scale = source.normal_texture.scale;
        if (state.description.normal_texture.is_valid()) {
            add_diagnostic(state.import_state.diagnostics, ModelLoadDiagnosticCategory::material,
                           ModelLoadDiagnosticCode::normal_map_fallback,
                           "Normal texture was imported and preserved but is not rendered because "
                           "Elf3D does not yet have a complete tangent-space path",
                           state.context);
        }
    }
    if (source.occlusion_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_material_texture(
            state, source.occlusion_texture, "Occlusion", " occlusion texture");
        if (!texture) {
            return texture.error();
        }
        state.description.occlusion_texture = texture.value().texture;
        state.description.occlusion_texture_mapping = texture.value().mapping;
        state.description.occlusion_strength = source.occlusion_texture.scale;
    }
    if (source.emissive_texture.texture != nullptr) {
        Result<ImportedTextureView> texture = import_material_texture(
            state, source.emissive_texture, "Emissive", " emissive texture");
        if (!texture) {
            return texture.error();
        }
        state.description.emissive_texture = texture.value().texture;
        state.description.emissive_texture_mapping = texture.value().mapping;
    }
    return {};
}

[[nodiscard]] Result<void> apply_alpha_mode(MaterialBuildState& state) {
    switch (state.source.alpha_mode) {
    case cgltf_alpha_mode_opaque:
        state.description.alpha_mode = AlphaMode::opaque;
        break;
    case cgltf_alpha_mode_mask:
        state.description.alpha_mode = AlphaMode::mask;
        break;
    case cgltf_alpha_mode_blend:
        state.description.alpha_mode = AlphaMode::blend;
        break;
    default:
        return Error{ErrorCode::scene_import_failed,
                     state.context + " contains an invalid alpha mode"};
    }
    state.description.alpha_cutoff = state.source.alpha_cutoff;
    return {};
}

[[nodiscard]] bool
base_material_factors_are_finite(const ModelMaterialDescription& description) noexcept {
    return std::isfinite(description.base_color.red) &&
           std::isfinite(description.base_color.green) &&
           std::isfinite(description.base_color.blue) &&
           std::isfinite(description.base_color.alpha) &&
           std::isfinite(description.metallic_factor) &&
           std::isfinite(description.roughness_factor);
}

[[nodiscard]] bool
extension_material_factors_are_finite(const ModelMaterialDescription& description) noexcept {
    return math::is_finite(description.emissive_factor) &&
           std::isfinite(description.normal_scale) &&
           std::isfinite(description.occlusion_strength) && std::isfinite(description.ior) &&
           std::isfinite(description.specular_factor) &&
           math::is_finite(description.specular_color_factor) &&
           std::isfinite(description.alpha_cutoff);
}

[[nodiscard]] bool
material_factors_are_finite(const ModelMaterialDescription& description) noexcept {
    return base_material_factors_are_finite(description) &&
           extension_material_factors_are_finite(description);
}

[[nodiscard]] Result<MaterialId> store_material(MaterialBuildState& state) {
    state.description.double_sided = state.source.double_sided != 0;
    const Result<MaterialId> created =
        state.import_state.builder.create_material(state.description);
    if (!created) {
        return created.error();
    }
    if (!state.primitive_specific_fallback) {
        state.import_state.ids.material_cache[state.index] = created.value();
    }
    state.import_state.ids.materials[state.index].push_back(created.value());
    return created.value();
}

[[nodiscard]] std::string material_context(const cgltf_material& material, std::size_t index) {
    return "material " +
           (material.name != nullptr ? std::string{material.name} : std::to_string(index));
}

[[nodiscard]] Result<MaterialId> material_for(ImportState& state, const cgltf_material* material,
                                              const TexcoordAvailability& available_texcoords) {
    const cgltf_data& data = state.data;
    auto& materials = state.ids.material_cache;
    if (material == nullptr) {
        return default_material_for(state);
    }

    const std::size_t index = static_cast<std::size_t>(material - data.materials);
    if (index >= materials.size()) {
        return Error{ErrorCode::scene_import_failed,
                     "A glTF primitive references a material outside the material table"};
    }
    const std::string context = material_context(*material, index);
    Result<bool> primitive_specific =
        material_uses_unavailable_texcoord(*material, available_texcoords, context);
    if (!primitive_specific) {
        return primitive_specific.error();
    }
    if (materials[index].has_value() && !primitive_specific.value()) {
        return materials[index].value();
    }
    MaterialBuildState build{state,   *material, available_texcoords,       index,
                             context, {},        primitive_specific.value()};
    if (const Result<void> primary = apply_primary_material(build); !primary) {
        return primary.error();
    }
    apply_material_factors(build);
    if (const Result<void> textures = apply_auxiliary_textures(build); !textures) {
        return textures.error();
    }
    if (const Result<void> alpha = apply_alpha_mode(build); !alpha) {
        return alpha.error();
    }
    if (!material_factors_are_finite(build.description)) {
        return Error{ErrorCode::scene_import_failed,
                     context + " contains a non-finite material factor"};
    }
    return store_material(build);
}

} // namespace elf3d::gltf::importer_detail
