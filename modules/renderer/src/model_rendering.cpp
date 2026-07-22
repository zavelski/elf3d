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

RuntimeVertexBuffer runtime_vertex_buffer(const scene::RuntimePrimitiveView& primitive) {
    RuntimeVertexBuffer buffer;
    std::size_t stride = 6U;
    if (primitive.vertex_layout() == scene::RuntimeVertexLayout::position_normal_texcoord) {
        buffer.layout = graphics::VertexLayout::position_normal_float3_texcoord_float2;
        stride = 8U;
    } else if (primitive.vertex_layout() == scene::RuntimeVertexLayout::full) {
        buffer.layout =
            graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4;
        stride = 14U;
    }
    buffer.vertex_count = static_cast<std::uint32_t>(primitive.vertex_count());
    buffer.values.resize(primitive.vertex_count() * stride);
    for (std::size_t index = 0; index < primitive.vertex_count(); ++index) {
        const Float3 position = primitive.position(index);
        const Float3 normal = primitive.normal(index);
        const std::size_t base = index * stride;
        buffer.values[base] = position.x;
        buffer.values[base + 1U] = position.y;
        buffer.values[base + 2U] = position.z;
        buffer.values[base + 3U] = normal.x;
        buffer.values[base + 4U] = normal.y;
        buffer.values[base + 5U] = normal.z;
        if (stride >= 8U) {
            const Float2 texcoord = primitive.texcoord0(index);
            buffer.values[base + 6U] = texcoord.x;
            buffer.values[base + 7U] = texcoord.y;
        }
        if (stride == 14U) {
            const Float2 texcoord = primitive.texcoord1(index);
            const Color4 color = primitive.color(index);
            buffer.values[base + 8U] = texcoord.x;
            buffer.values[base + 9U] = texcoord.y;
            buffer.values[base + 10U] = color.red;
            buffer.values[base + 11U] = color.green;
            buffer.values[base + 12U] = color.blue;
            buffer.values[base + 13U] = color.alpha;
        }
    }
    return buffer;
}

} // namespace elf3d::renderer
