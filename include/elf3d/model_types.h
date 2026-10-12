#ifndef ELF3D_MODEL_TYPES_H
#define ELF3D_MODEL_TYPES_H

#include <elf3d/math/value_types.h>

#include <cstdint>

namespace elf3d {

// Shared slot order for canonical/runtime material transport and graphics bindings.
enum class MaterialTextureSlot : std::uint8_t {
    base_color,
    metallic_roughness,
    normal,
    occlusion,
    emissive,
    specular,
    specular_color,
    transmission,
    thickness,
    clearcoat,
    clearcoat_roughness,
    clearcoat_normal,
};
inline constexpr std::size_t material_texture_slot_count = 12;

inline constexpr std::uint32_t maximum_texture_coordinate_sets = 2;

enum class AlphaMode {
    opaque,
    mask,
    blend,
};

enum class PixelFormat {
    rgba8_unorm,
};

struct PerspectiveCameraDescription {
    float vertical_field_of_view_radians = 1.0471975512F;
    float near_plane = 0.1F;
    float far_plane = 1000.0F;

    bool operator==(const PerspectiveCameraDescription&) const = default;
};

enum class TextureWrap {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

enum class TextureFilter {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};

struct TextureTransform {
    Float2 offset;
    Float2 scale{1.0F, 1.0F};
    float rotation_radians = 0.0F;

    bool operator==(const TextureTransform&) const = default;
};

struct TextureMapping {
    std::uint32_t texcoord_set = 0;
    TextureTransform transform;

    bool operator==(const TextureMapping&) const = default;
};

struct SamplerDescription {
    TextureWrap wrap_u = TextureWrap::repeat;
    TextureWrap wrap_v = TextureWrap::repeat;
    TextureFilter min_filter = TextureFilter::linear;
    TextureFilter mag_filter = TextureFilter::linear;

    bool operator==(const SamplerDescription&) const = default;
};

struct ModelLoadOptions {
    bool generate_missing_normals = true;
    bool import_node_names = true;
};

} // namespace elf3d

#endif
