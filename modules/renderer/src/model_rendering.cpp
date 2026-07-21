module;

#include <elf3d/rendering.h>

#include <cstddef>
#include <vector>

module elf.renderer;

import elf.graphics;
import elf.scene;

namespace elf3d::renderer {
namespace {

[[nodiscard]] TextureWrap runtime_texture_wrap(scene::RuntimeTextureWrap wrap) noexcept {
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

[[nodiscard]] TextureFilter runtime_texture_filter(scene::RuntimeTextureFilter filter) noexcept {
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

[[nodiscard]] AlphaMode runtime_alpha_mode(scene::RuntimeAlphaMode mode) noexcept {
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

[[nodiscard]] TextureMapping
runtime_texture_mapping(scene::RuntimeTextureMapping mapping) noexcept {
    return TextureMapping{mapping.texcoord_set,
                          TextureTransform{mapping.transform.offset, mapping.transform.scale,
                                           mapping.transform.rotation_radians}};
}

} // namespace

graphics::TextureAddressMode runtime_address_mode(scene::RuntimeTextureWrap wrap) noexcept {
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

graphics::TextureFilterMode runtime_filter_mode(scene::RuntimeTextureFilter filter) noexcept {
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

MaterialDescription
runtime_material_description(const scene::RuntimeMaterialView& source) noexcept {
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
    return target;
}

std::vector<VertexPositionNormalTexCoord>
vertices_from_runtime_primitive(const scene::RuntimePrimitiveView& primitive) {
    std::vector<VertexPositionNormalTexCoord> vertices(primitive.vertex_count());
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        vertices[index].position = primitive.position(index);
        vertices[index].normal = primitive.normal(index);
        vertices[index].texcoord0 = primitive.texcoord0(index);
        vertices[index].texcoord1 = primitive.texcoord1(index);
        vertices[index].color = primitive.color(index);
    }
    return vertices;
}

} // namespace elf3d::renderer
