module;

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

module elf.assets;

import elf.math;

namespace elf3d::assets {
namespace {

// The high bit remains available to Scene for document-backed runtime mesh
// handles, so public convenience meshes occupy the lower half of the value range.
constexpr std::uint64_t maximum_compatibility_mesh_handle_value =
    std::numeric_limits<std::uint64_t>::max() >> 1U;

bool finite_vertex(const VertexPositionNormalTexCoord &vertex) noexcept {
    return math::is_finite(vertex.position) && math::is_finite(vertex.normal) &&
           std::isfinite(vertex.texcoord0.x) && std::isfinite(vertex.texcoord0.y) &&
           std::isfinite(vertex.texcoord1.x) && std::isfinite(vertex.texcoord1.y) &&
           std::isfinite(vertex.color.red) && std::isfinite(vertex.color.green) &&
           std::isfinite(vertex.color.blue) && std::isfinite(vertex.color.alpha);
}

bool finite_mapping(const TextureMapping &mapping) noexcept {
    return mapping.texcoord_set < 2U && std::isfinite(mapping.transform.offset.x) &&
           std::isfinite(mapping.transform.offset.y) && std::isfinite(mapping.transform.scale.x) &&
           std::isfinite(mapping.transform.scale.y) &&
           std::isfinite(mapping.transform.rotation_radians);
}

bool valid_alpha_mode(AlphaMode mode) noexcept {
    return mode == AlphaMode::opaque || mode == AlphaMode::mask || mode == AlphaMode::blend;
}

bool valid_texture_wrap(TextureWrap wrap) noexcept {
    return wrap == TextureWrap::repeat || wrap == TextureWrap::mirrored_repeat ||
           wrap == TextureWrap::clamp_to_edge;
}

bool valid_minification_filter(TextureFilter filter) noexcept {
    return filter == TextureFilter::nearest || filter == TextureFilter::linear ||
           filter == TextureFilter::nearest_mipmap_nearest ||
           filter == TextureFilter::linear_mipmap_nearest ||
           filter == TextureFilter::nearest_mipmap_linear ||
           filter == TextureFilter::linear_mipmap_linear;
}

bool valid_magnification_filter(TextureFilter filter) noexcept {
    return filter == TextureFilter::nearest || filter == TextureFilter::linear;
}

bool valid_sampler(const SamplerDescription &sampler) noexcept {
    return valid_texture_wrap(sampler.wrap_u) && valid_texture_wrap(sampler.wrap_v) &&
           valid_minification_filter(sampler.min_filter) &&
           valid_magnification_filter(sampler.mag_filter);
}

MaterialDescription sanitized_material(const MaterialDescription &description) noexcept {
    MaterialDescription result = description;
    result.base_color = math::clamp_color(description.base_color);
    result.metallic_factor = std::clamp(description.metallic_factor, 0.0F, 1.0F);
    result.roughness_factor = std::clamp(description.roughness_factor, 0.0F, 1.0F);
    result.emissive_factor.x = std::max(description.emissive_factor.x, 0.0F);
    result.emissive_factor.y = std::max(description.emissive_factor.y, 0.0F);
    result.emissive_factor.z = std::max(description.emissive_factor.z, 0.0F);
    result.occlusion_strength = std::clamp(description.occlusion_strength, 0.0F, 1.0F);
    result.ior = std::max(description.ior, 1.0F);
    result.specular_factor = std::clamp(description.specular_factor, 0.0F, 1.0F);
    result.specular_color_factor.x = std::clamp(description.specular_color_factor.x, 0.0F, 1.0F);
    result.specular_color_factor.y = std::clamp(description.specular_color_factor.y, 0.0F, 1.0F);
    result.specular_color_factor.z = std::clamp(description.specular_color_factor.z, 0.0F, 1.0F);
    return result;
}

bool has_valid_texture_handles(const MaterialDescription &description,
                               const Storage &storage) noexcept {
    const auto valid_texture = [&storage](TextureAssetHandle texture) {
        return !texture.is_valid() || static_cast<bool>(storage.texture(texture));
    };
    return valid_texture(description.base_color_texture) &&
           valid_texture(description.metallic_roughness_texture) &&
           valid_texture(description.normal_texture) &&
           valid_texture(description.occlusion_texture) &&
           valid_texture(description.emissive_texture);
}

bool has_valid_texture_mappings(const MaterialDescription &description) noexcept {
    return finite_mapping(description.base_color_texture_mapping) &&
           finite_mapping(description.metallic_roughness_texture_mapping) &&
           finite_mapping(description.normal_texture_mapping) &&
           finite_mapping(description.occlusion_texture_mapping) &&
           finite_mapping(description.emissive_texture_mapping);
}

bool has_valid_material_scalars(const MaterialDescription &description) noexcept {
    return valid_alpha_mode(description.alpha_mode) && std::isfinite(description.alpha_cutoff) &&
           std::isfinite(description.metallic_factor) && std::isfinite(description.roughness_factor) &&
           std::isfinite(description.normal_scale) && std::isfinite(description.occlusion_strength) &&
           std::isfinite(description.ior) && std::isfinite(description.specular_factor);
}

bool has_valid_material_colors(const MaterialDescription &description) noexcept {
    return math::is_finite(description.emissive_factor) &&
           math::is_finite(description.specular_color_factor);
}

Result<void> validate_material(const MaterialDescription &description,
                               const Storage &storage) noexcept {
    if (!has_valid_texture_handles(description, storage)) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "Material textures must belong to the same scene asset storage"};
    }
    if (!has_valid_texture_mappings(description)) {
        return Error{ErrorCode::invalid_texcoord,
                     "Material texture mappings require UV set 0 or 1 and finite transforms"};
    }
    if (!has_valid_material_scalars(description) || !has_valid_material_colors(description)) {
        return Error{ErrorCode::invalid_material_handle,
                     "Material factors, alpha values, and colors must be finite"};
    }
    return {};
}

} // namespace

Storage::Storage(SceneId scene) noexcept : scene_(scene) {}

SceneId Storage::scene_id() const noexcept {
    return scene_;
}

Result<MeshHandle> Storage::create_mesh(const MeshDataView &data) {
    std::vector<VertexPositionNormalTexCoord> vertices;
    vertices.reserve(data.vertices.size());
    for (const VertexPositionNormal &vertex : data.vertices) {
        vertices.push_back(VertexPositionNormalTexCoord{vertex.position, vertex.normal});
    }
    return create_mesh(TexturedMeshDataView{vertices, data.indices});
}

Result<MeshHandle> Storage::create_mesh(const TexturedMeshDataView &data) {
    if (data.vertices.empty()) {
        return Error{ErrorCode::invalid_mesh_data, "A mesh requires at least one vertex"};
    }
    if (data.indices.empty() || data.indices.size() % 3 != 0) {
        return Error{
            ErrorCode::invalid_mesh_data,
            "An indexed triangle-list mesh requires a non-empty index count divisible by three"};
    }

    Float3 minimum = data.vertices.front().position;
    Float3 maximum = minimum;
    for (const VertexPositionNormalTexCoord &vertex : data.vertices) {
        if (!finite_vertex(vertex)) {
            return Error{ErrorCode::invalid_mesh_data,
                         "Mesh vertex positions, normals, UVs, and colors must be finite"};
        }
        minimum.x = std::min(minimum.x, vertex.position.x);
        minimum.y = std::min(minimum.y, vertex.position.y);
        minimum.z = std::min(minimum.z, vertex.position.z);
        maximum.x = std::max(maximum.x, vertex.position.x);
        maximum.y = std::max(maximum.y, vertex.position.y);
        maximum.z = std::max(maximum.z, vertex.position.z);
    }

    for (const std::uint32_t index : data.indices) {
        if (static_cast<std::size_t>(index) >= data.vertices.size()) {
            return Error{ErrorCode::mesh_index_out_of_range,
                         "A mesh index is outside the submitted vertex range"};
        }
    }
    if (static_cast<std::uint64_t>(meshes_.size()) >= maximum_compatibility_mesh_handle_value) {
        return Error{ErrorCode::invalid_mesh_data, "The scene mesh identifier space is exhausted"};
    }

    MeshAsset asset;
    asset.vertices.assign(data.vertices.begin(), data.vertices.end());
    asset.indices.assign(data.indices.begin(), data.indices.end());
    asset.bounds = Bounds3{minimum, maximum};
    meshes_.push_back(std::move(asset));
    return detail::SceneHandleAccess::create_mesh(scene_, meshes_.size());
}

Result<ImageHandle> Storage::create_image(const ImageDescription &description) {
    if (description.width == 0 || description.height == 0) {
        return Error{ErrorCode::zero_image_dimensions,
                     "Image assets require positive width and height"};
    }
    if (description.format != PixelFormat::rgba8_unorm) {
        return Error{ErrorCode::unsupported_texture_format,
                     "Image assets currently support only RGBA8 UNORM pixels"};
    }
    constexpr std::size_t maximum_image_bytes = 256ULL * 1024ULL * 1024ULL;
    const std::size_t width = description.width;
    const std::size_t height = description.height;
    if (width > maximum_image_bytes / 4 || height > maximum_image_bytes / (width * 4)) {
        return Error{ErrorCode::decoded_image_size_overflow,
                     "Image asset dimensions overflow the decoded byte limit"};
    }
    const std::size_t expected_bytes = width * height * 4;
    if (description.pixels.size() != expected_bytes) {
        return Error{ErrorCode::invalid_argument,
                     "Image asset pixels must be tightly packed RGBA8 rows"};
    }
    if (images_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_image_handle,
                     "The scene image identifier space is exhausted"};
    }
    ImageAsset asset;
    asset.width = description.width;
    asset.height = description.height;
    asset.format = description.format;
    asset.pixels.assign(description.pixels.begin(), description.pixels.end());
    images_.push_back(std::move(asset));
    return detail::SceneHandleAccess::create_image(scene_, images_.size());
}

Result<TextureAssetHandle> Storage::create_texture(const TextureDescription &description) {
    const Result<const ImageAsset *> image_result = image(description.image);
    if (!image_result) {
        return image_result.error();
    }
    if (!valid_sampler(description.sampler)) {
        return Error{ErrorCode::invalid_sampler_description,
                     "Texture sampler contains an invalid wrap or filter value"};
    }
    if (textures_.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "The scene texture identifier space is exhausted"};
    }
    textures_.push_back(TextureAsset{description});
    return detail::SceneHandleAccess::create_texture(scene_, textures_.size());
}

Result<MaterialHandle> Storage::create_material(const MaterialDescription &description) {
    if (materials_.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_material_handle,
                     "The scene material identifier space is exhausted"};
    }
    const Result<void> validation = validate_material(description, *this);
    if (!validation) {
        return validation.error();
    }
    materials_.push_back(MaterialAsset{sanitized_material(description)});
    return detail::SceneHandleAccess::create_material(scene_, materials_.size());
}

Result<void> Storage::set_material(MaterialHandle material,
                                   const MaterialDescription &description) {
    const Result<const MaterialAsset *> existing = this->material(material);
    if (!existing) {
        return existing.error();
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(material) - 1);
    const Result<void> validation = validate_material(description, *this);
    if (!validation) {
        return validation.error();
    }
    materials_[index].description = sanitized_material(description);
    return {};
}

Result<const MeshAsset *> Storage::mesh(MeshHandle mesh) const noexcept {
    if (!owns(mesh)) {
        return Error{ErrorCode::invalid_mesh_handle,
                     "The mesh handle is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index = static_cast<std::size_t>(detail::SceneHandleAccess::value(mesh) - 1);
    if (index >= meshes_.size()) {
        return Error{ErrorCode::invalid_mesh_handle, "The mesh handle is stale"};
    }
    return &meshes_[index];
}

Result<const MaterialAsset *> Storage::material(MaterialHandle material) const noexcept {
    if (!owns(material)) {
        return Error{ErrorCode::invalid_material_handle,
                     "The material handle is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(material) - 1);
    if (index >= materials_.size()) {
        return Error{ErrorCode::invalid_material_handle, "The material handle is stale"};
    }
    return &materials_[index];
}

Result<const ImageAsset *> Storage::image(ImageHandle image_handle) const noexcept {
    if (!owns(image_handle)) {
        return Error{ErrorCode::invalid_image_handle,
                     "The image handle is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(image_handle) - 1);
    if (index >= images_.size()) {
        return Error{ErrorCode::invalid_image_handle, "The image handle is stale"};
    }
    return &images_[index];
}

Result<const TextureAsset *> Storage::texture(TextureAssetHandle texture_handle) const noexcept {
    if (!owns(texture_handle)) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "The texture handle is invalid, stale, or belongs to another scene"};
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(texture_handle) - 1);
    if (index >= textures_.size()) {
        return Error{ErrorCode::invalid_texture_asset_handle, "The texture handle is stale"};
    }
    return &textures_[index];
}

std::span<const MeshAsset> Storage::meshes() const noexcept {
    return meshes_;
}

std::span<const MaterialAsset> Storage::materials() const noexcept {
    return materials_;
}

std::span<const ImageAsset> Storage::images() const noexcept {
    return images_;
}

std::span<const TextureAsset> Storage::textures() const noexcept {
    return textures_;
}

bool Storage::owns(MeshHandle mesh) const noexcept {
    return mesh.is_valid() && detail::SceneHandleAccess::scene(mesh) == scene_;
}

bool Storage::owns(MaterialHandle material) const noexcept {
    return material.is_valid() && detail::SceneHandleAccess::scene(material) == scene_;
}

bool Storage::owns(ImageHandle image_handle) const noexcept {
    return image_handle.is_valid() && detail::SceneHandleAccess::scene(image_handle) == scene_;
}

bool Storage::owns(TextureAssetHandle texture_handle) const noexcept {
    return texture_handle.is_valid() && detail::SceneHandleAccess::scene(texture_handle) == scene_;
}

} // namespace elf3d::assets
