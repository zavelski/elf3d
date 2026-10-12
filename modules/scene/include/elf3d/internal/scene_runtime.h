#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <elf3d/assets.h>
#include <elf3d/model.h>
#include <elf3d/scene.h>
#include <span>

namespace elf3d::scene {

enum class RuntimeAlphaMode : std::uint8_t { opaque, mask, blend };
enum class RuntimeTextureWrap : std::uint8_t { repeat, mirrored_repeat, clamp_to_edge };
enum class RuntimeTextureFilter : std::uint8_t {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};
using RuntimeMaterialTextureSlot = MaterialTextureSlot;
enum class RuntimeVertexLayout : std::uint8_t { position_normal, textured, full, tangent };
struct RuntimeTextureTransform {
    Float2 offset;
    Float2 scale{1.0F, 1.0F};
    float rotation_radians = 0.0F;
    bool operator==(const RuntimeTextureTransform&) const = default;
};
struct RuntimeTextureMapping {
    std::uint32_t texcoord_set = 0;
    RuntimeTextureTransform transform;
    bool operator==(const RuntimeTextureMapping&) const = default;
};
struct RuntimeSamplerDescription {
    RuntimeTextureWrap wrap_u = RuntimeTextureWrap::repeat;
    RuntimeTextureWrap wrap_v = RuntimeTextureWrap::repeat;
    RuntimeTextureFilter min_filter = RuntimeTextureFilter::linear;
    RuntimeTextureFilter mag_filter = RuntimeTextureFilter::linear;
    bool operator==(const RuntimeSamplerDescription&) const = default;
};
struct RuntimeMaterialView {
    Color4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
    bool double_sided = false;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    bool unlit = false;
    RuntimeAlphaMode alpha_mode = RuntimeAlphaMode::opaque;
    float alpha_cutoff = 0.5F;
    Float3 emissive_factor;
    float normal_scale = 1.0F;
    float occlusion_strength = 1.0F;
    float ior = 1.5F;
    float specular_factor = 1.0F;
    Float3 specular_color_factor{1.0F, 1.0F, 1.0F};
    RuntimeTextureMapping base_color_texture_mapping;
    RuntimeTextureMapping metallic_roughness_texture_mapping;
    RuntimeTextureMapping normal_texture_mapping;
    RuntimeTextureMapping occlusion_texture_mapping;
    RuntimeTextureMapping emissive_texture_mapping;
    bool has_base_color_texture = false;
    bool has_metallic_roughness_texture = false;
    bool has_normal_texture = false;
    bool has_occlusion_texture = false;
    bool has_emissive_texture = false;
    float transmission_factor = 0.0F;
    float thickness_factor = 0.0F;
    std::optional<float> attenuation_distance;
    Float3 attenuation_color{1.0F, 1.0F, 1.0F};
    float clearcoat_factor = 0.0F;
    float clearcoat_roughness_factor = 0.0F;
    float clearcoat_normal_scale = 1.0F;
    RuntimeTextureMapping specular_texture_mapping;
    RuntimeTextureMapping specular_color_texture_mapping;
    RuntimeTextureMapping transmission_texture_mapping;
    RuntimeTextureMapping thickness_texture_mapping;
    RuntimeTextureMapping clearcoat_texture_mapping;
    RuntimeTextureMapping clearcoat_roughness_texture_mapping;
    RuntimeTextureMapping clearcoat_normal_texture_mapping;
    bool has_specular_texture = false;
    bool has_specular_color_texture = false;
    bool has_transmission_texture = false;
    bool has_thickness_texture = false;
    bool has_clearcoat_texture = false;
    bool has_clearcoat_roughness_texture = false;
    bool has_clearcoat_normal_texture = false;
    [[nodiscard]] bool has_texture(RuntimeMaterialTextureSlot slot) const noexcept;
};
struct RuntimeTextureView {
    std::uint64_t texture_identity = 0;
    std::uint64_t image_identity = 0;
    bool document_image = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::span<const std::byte> pixels;
    RuntimeSamplerDescription sampler;
};
class RuntimePrimitiveView final {
  public:
    MeshHandle mesh;
    PrimitiveId document_primitive;
    std::uint64_t geometry_identity = 0;
    std::uint64_t material_identity = 0;
    Bounds3 bounds;
    RuntimeMaterialView material_view;
    [[nodiscard]] std::size_t vertex_count() const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> indices() const noexcept;
    [[nodiscard]] Float3 position(std::size_t index) const noexcept;
    [[nodiscard]] Float3 normal(std::size_t index) const noexcept;
    [[nodiscard]] Float2 texcoord0(std::size_t index) const noexcept;
    [[nodiscard]] Float2 texcoord1(std::size_t index) const noexcept;
    [[nodiscard]] Color4 color(std::size_t index) const noexcept;
    [[nodiscard]] Float4 tangent(std::size_t index) const noexcept;
    [[nodiscard]] RuntimeVertexLayout vertex_layout() const noexcept;
    [[nodiscard]] bool has_tangent_basis() const noexcept;

  private:
    friend class Storage;
    std::span<const VertexPositionNormalTexCoord> compatibility_vertices_;
    std::span<const Float3> document_positions_;
    std::span<const Float3> document_normals_;
    std::span<const Float2> document_texcoord0_;
    std::span<const Float2> document_texcoord1_;
    std::span<const Color4> document_colors_;
    std::span<const Float4> document_tangents_;
    std::span<const std::uint32_t> indices_;
    std::array<TextureAssetHandle, material_texture_slot_count> compatibility_textures_;
    std::array<TextureId, material_texture_slot_count> document_textures_;
    RuntimeVertexLayout vertex_layout_ = RuntimeVertexLayout::position_normal;
};
} // namespace elf3d::scene
