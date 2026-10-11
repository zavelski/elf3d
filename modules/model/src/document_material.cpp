#include <elf3d/model.h>

#include <elf3d/model/detail/document_storage.h>

namespace elf3d::model::detail {

[[nodiscard]] bool valid_mapping(TextureMapping mapping) noexcept
{
    return mapping.texcoord_set < maximum_texture_coordinate_sets &&
           finite(mapping.transform.offset) && finite(mapping.transform.scale) &&
           finite(mapping.transform.rotation_radians);
}

[[nodiscard]] bool
valid_material_color_factors(const ModelMaterialDescription& description) noexcept
{
    return finite(description.base_color) && finite(description.emissive_factor) &&
           finite(description.specular_color_factor);
}

[[nodiscard]] bool
valid_material_scalar_factors(const ModelMaterialDescription& description) noexcept
{
    return finite(description.metallic_factor) && finite(description.roughness_factor) &&
           finite(description.alpha_cutoff) && finite(description.emissive_strength) &&
           finite(description.normal_scale) && finite(description.occlusion_strength) &&
           finite(description.ior) && finite(description.specular_factor);
}

[[nodiscard]] bool valid_volume_factors(const ModelVolumeDescription& volume) noexcept
{
    const auto in_unit_range = [](float value) noexcept {
        return finite(value) && value >= 0.0F && value <= 1.0F;
    };
    const bool valid_distance =
        !volume.attenuation_distance ||
        (finite(*volume.attenuation_distance) && *volume.attenuation_distance > 0.0F);
    return finite(volume.thickness_factor) && volume.thickness_factor >= 0.0F && valid_distance &&
           in_unit_range(volume.attenuation_color.x) && in_unit_range(volume.attenuation_color.y) &&
           in_unit_range(volume.attenuation_color.z);
}

[[nodiscard]] bool valid_material_factors(const ModelMaterialDescription& description) noexcept
{
    return valid_material_color_factors(description) &&
           valid_material_scalar_factors(description) && valid_alpha(description.alpha_mode) &&
           (!description.volume || valid_volume_factors(*description.volume));
}

[[nodiscard]] bool valid_material_mappings(const ModelMaterialDescription& description) noexcept
{
    return valid_mapping(description.base_color_texture_mapping) &&
           valid_mapping(description.metallic_roughness_texture_mapping) &&
           valid_mapping(description.normal_texture_mapping) &&
           valid_mapping(description.occlusion_texture_mapping) &&
           valid_mapping(description.emissive_texture_mapping) &&
           (!description.volume || valid_mapping(description.volume->thickness_texture_mapping));
}

} // namespace elf3d::model::detail
