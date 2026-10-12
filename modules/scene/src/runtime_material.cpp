#include <elf3d/assets.h>
#include <elf3d/internal/scene_runtime.h>
#include <elf3d/model.h>

namespace elf3d::scene {
[[nodiscard]] RuntimeAlphaMode runtime_alpha_mode(AlphaMode mode) noexcept
{
    switch (mode) {
    case AlphaMode::opaque:
        return RuntimeAlphaMode::opaque;
    case AlphaMode::mask:
        return RuntimeAlphaMode::mask;
    case AlphaMode::blend:
        return RuntimeAlphaMode::blend;
    }
    return RuntimeAlphaMode::opaque;
}

[[nodiscard]] RuntimeTextureMapping runtime_texture_mapping(TextureMapping mapping) noexcept
{
    return RuntimeTextureMapping{mapping.texcoord_set,
                                 RuntimeTextureTransform{mapping.transform.offset,
                                                         mapping.transform.scale,
                                                         mapping.transform.rotation_radians}};
}

[[nodiscard]] RuntimeMaterialView runtime_material(const MaterialDescription& source) noexcept
{
    RuntimeMaterialView result;
    result.base_color = source.base_color;
    result.double_sided = source.double_sided;
    result.metallic_factor = source.metallic_factor;
    result.roughness_factor = source.roughness_factor;
    result.unlit = source.unlit;
    result.alpha_mode = runtime_alpha_mode(source.alpha_mode);
    result.alpha_cutoff = source.alpha_cutoff;
    result.emissive_factor = source.emissive_factor;
    result.normal_scale = source.normal_scale;
    result.occlusion_strength = source.occlusion_strength;
    result.ior = source.ior;
    result.specular_factor = source.specular_factor;
    result.specular_color_factor = source.specular_color_factor;
    result.specular_texture_mapping = runtime_texture_mapping(source.specular_texture_mapping);
    result.specular_color_texture_mapping =
        runtime_texture_mapping(source.specular_color_texture_mapping);
    result.has_specular_texture = source.specular_texture.is_valid();
    result.has_specular_color_texture = source.specular_color_texture.is_valid();
    if (source.transmission) {
        result.transmission_factor = source.transmission->factor;
        result.transmission_texture_mapping =
            runtime_texture_mapping(source.transmission->texture_mapping);
        result.has_transmission_texture = source.transmission->texture.is_valid();
    }
    if (source.volume) {
        result.thickness_factor = source.volume->thickness_factor;
        result.attenuation_distance = source.volume->attenuation_distance;
        result.attenuation_color = source.volume->attenuation_color;
        result.thickness_texture_mapping =
            runtime_texture_mapping(source.volume->thickness_texture_mapping);
        result.has_thickness_texture = source.volume->thickness_texture.is_valid();
    }
    if (source.clearcoat) {
        result.clearcoat_factor = source.clearcoat->factor;
        result.clearcoat_roughness_factor = source.clearcoat->roughness_factor;
        result.clearcoat_normal_scale = source.clearcoat->normal_scale;
        result.clearcoat_texture_mapping =
            runtime_texture_mapping(source.clearcoat->texture_mapping);
        result.clearcoat_roughness_texture_mapping =
            runtime_texture_mapping(source.clearcoat->roughness_texture_mapping);
        result.clearcoat_normal_texture_mapping =
            runtime_texture_mapping(source.clearcoat->normal_texture_mapping);
        result.has_clearcoat_texture = source.clearcoat->texture.is_valid();
        result.has_clearcoat_roughness_texture = source.clearcoat->roughness_texture.is_valid();
        result.has_clearcoat_normal_texture = source.clearcoat->normal_texture.is_valid();
    }

    result.base_color_texture_mapping = runtime_texture_mapping(source.base_color_texture_mapping);
    result.metallic_roughness_texture_mapping =
        runtime_texture_mapping(source.metallic_roughness_texture_mapping);
    result.normal_texture_mapping = runtime_texture_mapping(source.normal_texture_mapping);
    result.occlusion_texture_mapping = runtime_texture_mapping(source.occlusion_texture_mapping);
    result.emissive_texture_mapping = runtime_texture_mapping(source.emissive_texture_mapping);
    result.has_base_color_texture = source.base_color_texture.is_valid();
    result.has_metallic_roughness_texture = source.metallic_roughness_texture.is_valid();
    result.has_normal_texture = source.normal_texture.is_valid();
    result.has_occlusion_texture = source.occlusion_texture.is_valid();
    result.has_emissive_texture = source.emissive_texture.is_valid();
    return result;
}

[[nodiscard]] RuntimeMaterialView runtime_material(const ModelMaterialDescription& source) noexcept
{
    RuntimeMaterialView result;
    result.base_color = source.base_color;
    result.double_sided = source.double_sided;
    result.metallic_factor = source.metallic_factor;
    result.roughness_factor = source.roughness_factor;
    result.unlit = source.unlit;
    result.alpha_mode = runtime_alpha_mode(source.alpha_mode);
    result.alpha_cutoff = source.alpha_cutoff;
    result.emissive_factor = {source.emissive_factor.x * source.emissive_strength,
                              source.emissive_factor.y * source.emissive_strength,
                              source.emissive_factor.z * source.emissive_strength};
    result.normal_scale = source.normal_scale;
    result.occlusion_strength = source.occlusion_strength;
    result.ior = source.ior;
    result.specular_factor = source.specular_factor;
    result.specular_color_factor = source.specular_color_factor;
    result.specular_texture_mapping = runtime_texture_mapping(source.specular_texture_mapping);
    result.specular_color_texture_mapping =
        runtime_texture_mapping(source.specular_color_texture_mapping);
    result.has_specular_texture = source.specular_texture.is_valid();
    result.has_specular_color_texture = source.specular_color_texture.is_valid();
    if (source.transmission) {
        result.transmission_factor = source.transmission->factor;
        result.transmission_texture_mapping =
            runtime_texture_mapping(source.transmission->texture_mapping);
        result.has_transmission_texture = source.transmission->texture.is_valid();
    }
    if (source.volume) {
        result.thickness_factor = source.volume->thickness_factor;
        result.attenuation_distance = source.volume->attenuation_distance;
        result.attenuation_color = source.volume->attenuation_color;
        result.thickness_texture_mapping =
            runtime_texture_mapping(source.volume->thickness_texture_mapping);
        result.has_thickness_texture = source.volume->thickness_texture.is_valid();
    }
    if (source.clearcoat) {
        result.clearcoat_factor = source.clearcoat->factor;
        result.clearcoat_roughness_factor = source.clearcoat->roughness_factor;
        result.clearcoat_normal_scale = source.clearcoat->normal_scale;
        result.clearcoat_texture_mapping =
            runtime_texture_mapping(source.clearcoat->texture_mapping);
        result.clearcoat_roughness_texture_mapping =
            runtime_texture_mapping(source.clearcoat->roughness_texture_mapping);
        result.clearcoat_normal_texture_mapping =
            runtime_texture_mapping(source.clearcoat->normal_texture_mapping);
        result.has_clearcoat_texture = source.clearcoat->texture.is_valid();
        result.has_clearcoat_roughness_texture = source.clearcoat->roughness_texture.is_valid();
        result.has_clearcoat_normal_texture = source.clearcoat->normal_texture.is_valid();
    }

    result.base_color_texture_mapping = runtime_texture_mapping(source.base_color_texture_mapping);
    result.metallic_roughness_texture_mapping =
        runtime_texture_mapping(source.metallic_roughness_texture_mapping);
    result.normal_texture_mapping = runtime_texture_mapping(source.normal_texture_mapping);
    result.occlusion_texture_mapping = runtime_texture_mapping(source.occlusion_texture_mapping);
    result.emissive_texture_mapping = runtime_texture_mapping(source.emissive_texture_mapping);
    result.has_base_color_texture = source.base_color_texture.is_valid();
    result.has_metallic_roughness_texture = source.metallic_roughness_texture.is_valid();
    result.has_normal_texture = source.normal_texture.is_valid();
    result.has_occlusion_texture = source.occlusion_texture.is_valid();
    result.has_emissive_texture = source.emissive_texture.is_valid();
    return result;
}

} // namespace elf3d::scene
