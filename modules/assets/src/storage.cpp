module;

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

module elf.assets;

import elf.math;

namespace elf3d::assets {
namespace {

bool finite_vertex(const VertexPositionNormalTexCoord &vertex) noexcept {
    return math::is_finite(vertex.position) && math::is_finite(vertex.normal) &&
           std::isfinite(vertex.texcoord0.x) && std::isfinite(vertex.texcoord0.y);
}

bool valid_sampler(const SamplerDescription &sampler) noexcept {
    const auto valid_wrap = [](TextureWrap wrap) {
        return wrap == TextureWrap::repeat || wrap == TextureWrap::mirrored_repeat ||
               wrap == TextureWrap::clamp_to_edge;
    };
    const auto valid_filter = [](TextureFilter filter) {
        return filter == TextureFilter::nearest || filter == TextureFilter::linear ||
               filter == TextureFilter::nearest_mipmap_nearest ||
               filter == TextureFilter::linear_mipmap_nearest ||
               filter == TextureFilter::nearest_mipmap_linear ||
               filter == TextureFilter::linear_mipmap_linear;
    };
    return valid_wrap(sampler.wrap_u) && valid_wrap(sampler.wrap_v) &&
           valid_filter(sampler.min_filter) &&
           (sampler.mag_filter == TextureFilter::nearest ||
            sampler.mag_filter == TextureFilter::linear);
}

MaterialDescription sanitized_material(const MaterialDescription &description) noexcept {
    MaterialDescription result = description;
    result.base_color = math::clamp_color(description.base_color);
    result.metallic_factor = std::clamp(description.metallic_factor, 0.0F, 1.0F);
    result.roughness_factor = std::clamp(description.roughness_factor, 0.0F, 1.0F);
    return result;
}

} // namespace

Storage::Storage(SceneId scene) noexcept : scene_(scene) {}

SceneId Storage::scene_id() const noexcept {
    return scene_;
}

Result<MeshHandle> Storage::create_mesh(const MeshDataView &data) {
    try {
        std::vector<VertexPositionNormalTexCoord> vertices;
        vertices.reserve(data.vertices.size());
        for (const VertexPositionNormal &vertex : data.vertices) {
            vertices.push_back(VertexPositionNormalTexCoord{vertex.position, vertex.normal, {}});
        }
        return create_mesh(TexturedMeshDataView{vertices, data.indices});
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Mesh creation failed while normalizing caller-owned vertices"};
    }
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
                         "Mesh positions and normals must contain only finite values"};
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
    if (meshes_.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_mesh_data, "The scene mesh identifier space is exhausted"};
    }

    try {
        MeshAsset asset;
        asset.vertices.assign(data.vertices.begin(), data.vertices.end());
        asset.indices.assign(data.indices.begin(), data.indices.end());
        asset.bounds = Bounds3{minimum, maximum, true};
        meshes_.push_back(std::move(asset));
        return detail::SceneHandleAccess::create_mesh(scene_, meshes_.size());
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Mesh creation failed while copying caller-owned data"};
    }
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
        return Error{ErrorCode::invalid_image_handle, "The scene image identifier space is exhausted"};
    }
    try {
        ImageAsset asset;
        asset.width = description.width;
        asset.height = description.height;
        asset.format = description.format;
        asset.pixels.assign(description.pixels.begin(), description.pixels.end());
        images_.push_back(std::move(asset));
        return detail::SceneHandleAccess::create_image(scene_, images_.size());
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Image creation failed while copying caller-owned pixels"};
    }
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
    try {
        textures_.push_back(TextureAsset{description});
        return detail::SceneHandleAccess::create_texture(scene_, textures_.size());
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Texture creation failed while allocating storage"};
    }
}

Result<MaterialHandle> Storage::create_material(const MaterialDescription &description) {
    if (materials_.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - 1)) {
        return Error{ErrorCode::invalid_material_handle,
                     "The scene material identifier space is exhausted"};
    }
    if ((description.base_color_texture.is_valid() &&
         !texture(description.base_color_texture)) ||
        (description.metallic_roughness_texture.is_valid() &&
         !texture(description.metallic_roughness_texture))) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "Material textures must belong to the same scene asset storage"};
    }
    if (!std::isfinite(description.metallic_factor) ||
        !std::isfinite(description.roughness_factor)) {
        return Error{ErrorCode::invalid_material_handle,
                     "Material metallic and roughness factors must be finite"};
    }
    try {
        materials_.push_back(MaterialAsset{sanitized_material(description)});
        return detail::SceneHandleAccess::create_material(scene_, materials_.size());
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Material creation failed while allocating storage"};
    }
}

Result<void> Storage::set_material(MaterialHandle material,
                                   const MaterialDescription &description) {
    const Result<const MaterialAsset *> existing = this->material(material);
    if (!existing) {
        return existing.error();
    }
    const std::size_t index =
        static_cast<std::size_t>(detail::SceneHandleAccess::value(material) - 1);
    if ((description.base_color_texture.is_valid() &&
         !texture(description.base_color_texture)) ||
        (description.metallic_roughness_texture.is_valid() &&
         !texture(description.metallic_roughness_texture))) {
        return Error{ErrorCode::invalid_texture_asset_handle,
                     "Material textures must belong to the same scene asset storage"};
    }
    if (!std::isfinite(description.metallic_factor) ||
        !std::isfinite(description.roughness_factor)) {
        return Error{ErrorCode::invalid_material_handle,
                     "Material metallic and roughness factors must be finite"};
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
