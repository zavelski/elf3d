#include "renderer_detail.h"

#include <renderer/renderer.h>

#include <elf3d/rendering.h>

#include <cstddef>
#include <graphics/graphics.h>
#include <scene/scene.h>
#include <vector>

namespace elf3d::renderer {
namespace {

[[nodiscard]] TextureWrap runtime_texture_wrap(scene::RuntimeTextureWrap wrap) noexcept
{
    switch (wrap) {
    case scene::RuntimeTextureWrap::repeat:
        return TextureWrap::repeat;
    case scene::RuntimeTextureWrap::mirrored_repeat:
        return TextureWrap::mirrored_repeat;
    case scene::RuntimeTextureWrap::clamp_to_edge:
        return TextureWrap::clamp_to_edge;
    }
    return TextureWrap::repeat;
}

[[nodiscard]] TextureFilter runtime_texture_filter(scene::RuntimeTextureFilter filter) noexcept
{
    switch (filter) {
    case scene::RuntimeTextureFilter::nearest:
        return TextureFilter::nearest;
    case scene::RuntimeTextureFilter::linear:
        return TextureFilter::linear;
    case scene::RuntimeTextureFilter::nearest_mipmap_nearest:
        return TextureFilter::nearest_mipmap_nearest;
    case scene::RuntimeTextureFilter::linear_mipmap_nearest:
        return TextureFilter::linear_mipmap_nearest;
    case scene::RuntimeTextureFilter::nearest_mipmap_linear:
        return TextureFilter::nearest_mipmap_linear;
    case scene::RuntimeTextureFilter::linear_mipmap_linear:
        return TextureFilter::linear_mipmap_linear;
    }
    return TextureFilter::linear;
}

[[nodiscard]] TextureMapping runtime_texture_mapping(scene::RuntimeTextureMapping mapping) noexcept
{
    return TextureMapping{mapping.texcoord_set,
                          TextureTransform{mapping.transform.offset, mapping.transform.scale,
                                           mapping.transform.rotation_radians}};
}

} // namespace

[[nodiscard]] AlphaMode runtime_alpha_mode(scene::RuntimeAlphaMode mode) noexcept
{
    switch (mode) {
    case scene::RuntimeAlphaMode::opaque:
        return AlphaMode::opaque;
    case scene::RuntimeAlphaMode::mask:
        return AlphaMode::mask;
    case scene::RuntimeAlphaMode::blend:
        return AlphaMode::blend;
    }
    return AlphaMode::opaque;
}

graphics::TextureAddressMode runtime_address_mode(scene::RuntimeTextureWrap wrap) noexcept
{
    switch (wrap) {
    case scene::RuntimeTextureWrap::repeat:
        return graphics::TextureAddressMode::repeat;
    case scene::RuntimeTextureWrap::mirrored_repeat:
        return graphics::TextureAddressMode::mirrored_repeat;
    case scene::RuntimeTextureWrap::clamp_to_edge:
        return graphics::TextureAddressMode::clamp_to_edge;
    }
    return graphics::TextureAddressMode::repeat;
}

graphics::TextureFilterMode runtime_filter_mode(scene::RuntimeTextureFilter filter) noexcept
{
    switch (filter) {
    case scene::RuntimeTextureFilter::nearest:
        return graphics::TextureFilterMode::nearest;
    case scene::RuntimeTextureFilter::linear:
        return graphics::TextureFilterMode::linear;
    case scene::RuntimeTextureFilter::nearest_mipmap_nearest:
        return graphics::TextureFilterMode::nearest_mipmap_nearest;
    case scene::RuntimeTextureFilter::linear_mipmap_nearest:
        return graphics::TextureFilterMode::linear_mipmap_nearest;
    case scene::RuntimeTextureFilter::nearest_mipmap_linear:
        return graphics::TextureFilterMode::nearest_mipmap_linear;
    case scene::RuntimeTextureFilter::linear_mipmap_linear:
        return graphics::TextureFilterMode::linear_mipmap_linear;
    }
    return graphics::TextureFilterMode::linear;
}

MaterialDescription runtime_material_description(const scene::RuntimeMaterialView& source) noexcept
{
    MaterialDescription target;
    target.base_color = source.base_color;
    target.double_sided = source.double_sided;
    target.metallic_factor = source.metallic_factor;
    target.roughness_factor = source.roughness_factor;
    target.unlit = source.unlit;
    target.alpha_mode = runtime_alpha_mode(source.alpha_mode);
    target.alpha_cutoff = source.alpha_cutoff;
    target.emissive_factor = source.emissive_factor;
    target.normal_scale = source.normal_scale;
    target.occlusion_strength = source.occlusion_strength;
    target.ior = source.ior;
    target.specular_factor = source.specular_factor;
    target.specular_color_factor = source.specular_color_factor;
    target.base_color_texture_mapping = runtime_texture_mapping(source.base_color_texture_mapping);
    target.metallic_roughness_texture_mapping =
        runtime_texture_mapping(source.metallic_roughness_texture_mapping);
    target.normal_texture_mapping = runtime_texture_mapping(source.normal_texture_mapping);
    target.occlusion_texture_mapping = runtime_texture_mapping(source.occlusion_texture_mapping);
    target.emissive_texture_mapping = runtime_texture_mapping(source.emissive_texture_mapping);
    target.specular_texture_mapping = runtime_texture_mapping(source.specular_texture_mapping);
    target.specular_color_texture_mapping =
        runtime_texture_mapping(source.specular_color_texture_mapping);
    if (source.transmission_factor > 0) {
        target.transmission = TransmissionDescription{};
        target.transmission->factor = source.transmission_factor;
        target.transmission->texture_mapping =
            runtime_texture_mapping(source.transmission_texture_mapping);
    }
    if (source.transmission_factor > 0 && source.thickness_factor > 0) {
        target.volume = VolumeDescription{};
        target.volume->thickness_factor = source.thickness_factor;
        target.volume->attenuation_distance = source.attenuation_distance;
        target.volume->attenuation_color = source.attenuation_color;
        target.volume->thickness_texture_mapping =
            runtime_texture_mapping(source.thickness_texture_mapping);
    }
    if (source.has_clearcoat) {
        target.clearcoat = ClearcoatDescription{};
        target.clearcoat->factor = source.clearcoat_factor;
        target.clearcoat->roughness_factor = source.clearcoat_roughness_factor;
        target.clearcoat->normal_scale = source.clearcoat_normal_scale;
        target.clearcoat->texture_mapping =
            runtime_texture_mapping(source.clearcoat_texture_mapping);
        target.clearcoat->roughness_texture_mapping =
            runtime_texture_mapping(source.clearcoat_roughness_texture_mapping);
        target.clearcoat->normal_texture_mapping =
            runtime_texture_mapping(source.clearcoat_normal_texture_mapping);
    }
    return target;
}

} // namespace elf3d::renderer
