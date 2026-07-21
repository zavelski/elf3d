module;

#include <elf3d/core/assert.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

module elf.scene;

import elf.assets;
import elf.model;

namespace elf3d::scene {
namespace {

[[nodiscard]] RuntimeAlphaMode runtime_alpha_mode(AlphaMode mode) noexcept {
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

[[nodiscard]] RuntimeTextureWrap runtime_texture_wrap(TextureWrap wrap) noexcept {
    switch (wrap) {
    case TextureWrap::repeat:
        return RuntimeTextureWrap::repeat;
    case TextureWrap::mirrored_repeat:
        return RuntimeTextureWrap::mirrored_repeat;
    case TextureWrap::clamp_to_edge:
        return RuntimeTextureWrap::clamp_to_edge;
    }
    return RuntimeTextureWrap::repeat;
}

[[nodiscard]] RuntimeTextureFilter runtime_texture_filter(TextureFilter filter) noexcept {
    switch (filter) {
    case TextureFilter::nearest:
        return RuntimeTextureFilter::nearest;
    case TextureFilter::linear:
        return RuntimeTextureFilter::linear;
    case TextureFilter::nearest_mipmap_nearest:
        return RuntimeTextureFilter::nearest_mipmap_nearest;
    case TextureFilter::linear_mipmap_nearest:
        return RuntimeTextureFilter::linear_mipmap_nearest;
    case TextureFilter::nearest_mipmap_linear:
        return RuntimeTextureFilter::nearest_mipmap_linear;
    case TextureFilter::linear_mipmap_linear:
        return RuntimeTextureFilter::linear_mipmap_linear;
    }
    return RuntimeTextureFilter::linear;
}

[[nodiscard]] RuntimeTextureMapping runtime_texture_mapping(TextureMapping mapping) noexcept {
    return RuntimeTextureMapping{mapping.texcoord_set,
                                 RuntimeTextureTransform{mapping.transform.offset,
                                                         mapping.transform.scale,
                                                         mapping.transform.rotation_radians}};
}

[[nodiscard]] RuntimeMaterialView runtime_material(MaterialDescription source) noexcept {
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

[[nodiscard]] RuntimeMaterialView runtime_material(ModelMaterialDescription source) noexcept {
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

[[nodiscard]] RuntimeSamplerDescription runtime_sampler(SamplerDescription sampler) noexcept {
    return RuntimeSamplerDescription{
        runtime_texture_wrap(sampler.wrap_u), runtime_texture_wrap(sampler.wrap_v),
        runtime_texture_filter(sampler.min_filter), runtime_texture_filter(sampler.mag_filter)};
}

[[nodiscard]] constexpr std::size_t
runtime_texture_slot_index(RuntimeMaterialTextureSlot slot) noexcept {
    return static_cast<std::size_t>(slot);
}

} // namespace

namespace {

struct CreatedSceneNode {
    EntityId entity;
    std::optional<NodeView> source;
};

struct PendingSceneNode {
    NodeId node;
    std::optional<EntityId> parent;
};

[[nodiscard]] Result<void> configure_scene_entity(DocumentView document, const NodeView& node,
                                                  EntityId entity, Storage& storage) {
    if (!node.name.empty()) {
        const Result<void> name_result = storage.set_entity_name(entity, node.name);
        if (!name_result) {
            return name_result.error();
        }
    }
    const Result<void> matrix_result = storage.set_local_matrix(entity, node.local_matrix);
    if (!matrix_result) {
        return matrix_result.error();
    }
    if (node.mesh.has_value()) {
        const Result<MeshView> mesh = document.mesh(*node.mesh);
        if (!mesh) {
            return mesh.error();
        }
        const Result<void> model_result =
            storage.set_model_document_primitives(entity, mesh.value().primitives);
        if (!model_result) {
            return model_result.error();
        }
    }
    if (node.perspective_camera.has_value()) {
        const Result<void> camera_result =
            storage.attach_perspective_camera(entity, *node.perspective_camera);
        if (!camera_result) {
            return camera_result.error();
        }
    }
    return {};
}

[[nodiscard]] Result<CreatedSceneNode>
create_scene_node(DocumentView document, NodeId node_id, Storage& storage,
                  std::vector<std::optional<EntityId>>& node_cache) {
    const std::size_t index = static_cast<std::size_t>(node_id.debug_value());
    if (index >= node_cache.size()) {
        node_cache.resize(index + 1U);
    }
    if (node_cache[index].has_value()) {
        return CreatedSceneNode{*node_cache[index], std::nullopt};
    }
    const Result<NodeView> node = document.node(node_id);
    if (!node) {
        return node.error();
    }
    const Result<EntityId> entity = storage.create_entity();
    if (!entity) {
        return entity.error();
    }
    node_cache[index] = entity.value();
    const Result<void> configured =
        configure_scene_entity(document, node.value(), entity.value(), storage);
    if (!configured) {
        return configured.error();
    }
    return CreatedSceneNode{entity.value(), node.value()};
}

} // namespace

Result<void> populate_from_document(Document&& document, DocumentSceneId default_scene,
                                    Storage& storage) {
    const Result<void> document_result = storage.set_document(std::move(document));
    if (!document_result) {
        return document_result.error();
    }
    const DocumentView document_view = storage.document();
    const Result<DocumentSceneView> scene = document_view.scene(default_scene);
    if (!scene) {
        return scene.error();
    }
    const DocumentStatistics statistics = document_view.statistics();
    std::vector<std::optional<EntityId>> node_cache(
        static_cast<std::size_t>(statistics.nodes + 1U));
    std::vector<PendingSceneNode> pending;
    pending.reserve(static_cast<std::size_t>(statistics.nodes));
    for (std::size_t index = scene.value().roots.size(); index != 0U; --index) {
        pending.push_back(PendingSceneNode{scene.value().roots[index - 1U], std::nullopt});
    }
    while (!pending.empty()) {
        const PendingSceneNode current = pending.back();
        pending.pop_back();
        const Result<CreatedSceneNode> created =
            create_scene_node(document_view, current.node, storage, node_cache);
        if (!created) {
            return created.error();
        }
        if (current.parent.has_value()) {
            const Result<void> parent_result =
                storage.set_parent(created.value().entity, *current.parent);
            if (!parent_result) {
                return parent_result.error();
            }
        }
        if (!created.value().source.has_value()) {
            continue;
        }
        const std::span<const NodeId> children = created.value().source->children;
        for (std::size_t index = children.size(); index != 0U; --index) {
            pending.push_back(PendingSceneNode{children[index - 1U], created.value().entity});
        }
    }
    return {};
}

std::size_t RuntimePrimitiveView::vertex_count() const noexcept {
    return document_positions_.empty() ? compatibility_vertices_.size()
                                       : document_positions_.size();
}

std::span<const std::uint32_t> RuntimePrimitiveView::indices() const noexcept {
    return indices_;
}

Float3 RuntimePrimitiveView::position(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < vertex_count());
    if (!document_positions_.empty()) {
        return document_positions_.first(index + 1U).back();
    }
    return compatibility_vertices_.first(index + 1U).back().position;
}

Float3 RuntimePrimitiveView::normal(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < vertex_count());
    if (!document_positions_.empty()) {
        return document_normals_.empty() ? Float3{0.0F, 1.0F, 0.0F}
                                         : document_normals_.first(index + 1U).back();
    }
    return compatibility_vertices_.first(index + 1U).back().normal;
}

Float2 RuntimePrimitiveView::texcoord0(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < vertex_count());
    if (!document_positions_.empty()) {
        return document_texcoord0_.empty() ? Float2{}
                                           : document_texcoord0_.first(index + 1U).back();
    }
    return compatibility_vertices_.first(index + 1U).back().texcoord0;
}

Float2 RuntimePrimitiveView::texcoord1(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < vertex_count());
    if (!document_positions_.empty()) {
        return document_texcoord1_.empty() ? Float2{}
                                           : document_texcoord1_.first(index + 1U).back();
    }
    return compatibility_vertices_.first(index + 1U).back().texcoord1;
}

Color4 RuntimePrimitiveView::color(std::size_t index) const noexcept {
    ELF3D_ASSERT(index < vertex_count());
    if (!document_positions_.empty()) {
        return document_colors_.empty() ? Color4{1.0F, 1.0F, 1.0F, 1.0F}
                                        : document_colors_.first(index + 1U).back();
    }
    return compatibility_vertices_.first(index + 1U).back().color;
}

bool RuntimeMaterialView::has_texture(RuntimeMaterialTextureSlot slot) const noexcept {
    switch (slot) {
    case RuntimeMaterialTextureSlot::base_color:
        return has_base_color_texture;
    case RuntimeMaterialTextureSlot::metallic_roughness:
        return has_metallic_roughness_texture;
    case RuntimeMaterialTextureSlot::normal:
        return has_normal_texture;
    case RuntimeMaterialTextureSlot::occlusion:
        return has_occlusion_texture;
    case RuntimeMaterialTextureSlot::emissive:
        return has_emissive_texture;
    }
    return false;
}

Result<RuntimePrimitiveView>
Storage::document_runtime_primitive(const ModelPrimitiveBinding& binding,
                                    PrimitiveId document_primitive) const noexcept {
    if (document_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "A document primitive binding requires a scene document"};
    }
    const Result<PrimitiveView> primitive_result = document_->primitive(document_primitive);
    if (!primitive_result) {
        return primitive_result.error();
    }
    const Result<MaterialView> material_result =
        document_->material(primitive_result.value().material);
    if (!material_result) {
        return material_result.error();
    }
    RuntimePrimitiveView result;
    result.mesh = binding.mesh;
    result.document_primitive = document_primitive;
    result.bounds = primitive_result.value().bounds;
    result.material_view = runtime_material(material_result.value().description);
    result.document_positions_ = primitive_result.value().data.positions;
    result.document_normals_ = primitive_result.value().data.normals;
    result.document_texcoord0_ = primitive_result.value().data.texcoord0;
    result.document_texcoord1_ = primitive_result.value().data.texcoord1;
    result.document_colors_ = primitive_result.value().data.colors;
    result.indices_ = primitive_result.value().data.indices;
    result.document_textures_ = {
        material_result.value().description.base_color_texture,
        material_result.value().description.metallic_roughness_texture,
        material_result.value().description.normal_texture,
        material_result.value().description.occlusion_texture,
        material_result.value().description.emissive_texture,
    };
    return result;
}

Result<RuntimePrimitiveView>
Storage::compatibility_runtime_primitive(const ModelPrimitiveBinding& binding) const noexcept {
    const Result<const assets::MeshAsset*> mesh_result = assets_.mesh(binding.mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }
    const Result<const assets::MaterialAsset*> material_result = assets_.material(binding.material);
    if (!material_result) {
        return material_result.error();
    }
    RuntimePrimitiveView result;
    result.mesh = binding.mesh;
    result.bounds = mesh_result.value()->bounds;
    result.material_view = runtime_material(material_result.value()->description);
    result.compatibility_vertices_ = mesh_result.value()->vertices;
    result.indices_ = mesh_result.value()->indices;
    result.compatibility_textures_ = {
        material_result.value()->description.base_color_texture,
        material_result.value()->description.metallic_roughness_texture,
        material_result.value()->description.normal_texture,
        material_result.value()->description.occlusion_texture,
        material_result.value()->description.emissive_texture,
    };
    return result;
}

Result<RuntimeTextureView>
Storage::runtime_texture(const RuntimePrimitiveView& primitive,
                         RuntimeMaterialTextureSlot slot) const noexcept {
    if (!primitive.material_view.has_texture(slot)) {
        return Error{ErrorCode::invalid_argument,
                     "Runtime texture access requires a material-bound texture slot"};
    }
    const std::size_t slot_index = runtime_texture_slot_index(slot);
    if (primitive.document_primitive.is_valid()) {
        if (document_ == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "A document texture binding requires a scene document"};
        }
        const TextureId texture_id = primitive.document_textures_[slot_index];
        const Result<TextureView> texture_result = document_->texture(texture_id);
        if (!texture_result) {
            return texture_result.error();
        }
        const Result<ImageView> image_result =
            document_->image(texture_result.value().description.image);
        if (!image_result) {
            return image_result.error();
        }
        const Result<SamplerView> sampler_result =
            document_->sampler(texture_result.value().description.sampler);
        if (!sampler_result) {
            return sampler_result.error();
        }
        return RuntimeTextureView{texture_result.value().id.debug_value(),
                                  image_result.value().id.debug_value(),
                                  true,
                                  image_result.value().width,
                                  image_result.value().height,
                                  image_result.value().pixels,
                                  runtime_sampler(sampler_result.value().description)};
    }

    const Result<const assets::TextureAsset*> texture_result =
        assets_.texture(primitive.compatibility_textures_[slot_index]);
    if (!texture_result) {
        return texture_result.error();
    }
    const Result<const assets::ImageAsset*> image_result =
        assets_.image(texture_result.value()->description.image);
    if (!image_result) {
        return image_result.error();
    }
    const assets::ImageAsset& image = *image_result.value();
    return RuntimeTextureView{primitive.compatibility_textures_[slot_index].debug_value(),
                              texture_result.value()->description.image.debug_value(),
                              false,
                              image.width,
                              image.height,
                              image.pixels,
                              runtime_sampler(texture_result.value()->description.sampler)};
}

namespace {

void add_compatibility_geometry_statistics(SceneStatistics& result,
                                           const assets::Storage& assets_storage) noexcept {
    result.mesh_assets = static_cast<std::uint64_t>(assets_storage.meshes().size());
    result.primitives = result.mesh_assets;
    for (const assets::MeshAsset& mesh : assets_storage.meshes()) {
        result.vertices += static_cast<std::uint64_t>(mesh.vertices.size());
        result.indices += static_cast<std::uint64_t>(mesh.indices.size());
        result.triangles += static_cast<std::uint64_t>(mesh.indices.size() / 3U);
    }
}

[[nodiscard]] std::uint64_t
compatibility_sampler_description_count(std::span<const assets::TextureAsset> textures) noexcept {
    std::uint64_t result = 0;
    for (std::size_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
        bool is_first = true;
        for (std::size_t earlier_index = 0; earlier_index < texture_index; ++earlier_index) {
            if (textures[earlier_index].description.sampler ==
                textures[texture_index].description.sampler) {
                is_first = false;
                break;
            }
        }
        result += is_first ? 1U : 0U;
    }
    return result;
}

void add_compatibility_image_statistics(SceneStatistics& result,
                                        const assets::Storage& assets_storage) noexcept {
    result.image_assets = static_cast<std::uint64_t>(assets_storage.images().size());
    result.texture_assets = static_cast<std::uint64_t>(assets_storage.textures().size());
    result.sampler_descriptions =
        compatibility_sampler_description_count(assets_storage.textures());
    for (const assets::ImageAsset& image : assets_storage.images()) {
        result.decoded_image_bytes += static_cast<std::uint64_t>(image.pixels.size());
    }
}

void add_compatibility_material_statistics(SceneStatistics& result,
                                           const assets::Storage& assets_storage) noexcept {
    result.material_assets = static_cast<std::uint64_t>(assets_storage.materials().size());
    for (const assets::MaterialAsset& material : assets_storage.materials()) {
        result.materials_with_base_color_textures +=
            material.description.base_color_texture.is_valid() ? 1U : 0U;
        result.materials_with_metallic_roughness_textures +=
            material.description.metallic_roughness_texture.is_valid() ? 1U : 0U;
        result.materials_with_normal_textures +=
            material.description.normal_texture.is_valid() ? 1U : 0U;
        result.materials_with_occlusion_textures +=
            material.description.occlusion_texture.is_valid() ? 1U : 0U;
        result.materials_with_emissive_textures +=
            material.description.emissive_texture.is_valid() ? 1U : 0U;
    }
}

[[nodiscard]] SceneStatistics
compatibility_statistics(const assets::Storage& assets_storage) noexcept {
    SceneStatistics result;
    add_compatibility_geometry_statistics(result, assets_storage);
    add_compatibility_image_statistics(result, assets_storage);
    add_compatibility_material_statistics(result, assets_storage);
    return result;
}

[[nodiscard]] SceneStatistics document_statistics(DocumentStatistics source) noexcept {
    SceneStatistics result;
    result.mesh_assets = source.primitives;
    result.material_assets = source.materials;
    result.primitives = source.primitives;
    result.vertices = source.vertices;
    result.indices = source.indices;
    result.triangles = source.triangles;
    result.image_assets = source.images;
    result.texture_assets = source.textures;
    result.sampler_descriptions = source.samplers;
    result.decoded_image_bytes = source.decoded_image_bytes;
    result.materials_with_base_color_textures = source.materials_with_base_color_textures;
    result.materials_with_metallic_roughness_textures =
        source.materials_with_metallic_roughness_textures;
    result.materials_with_normal_textures = source.materials_with_normal_textures;
    result.materials_with_occlusion_textures = source.materials_with_occlusion_textures;
    result.materials_with_emissive_textures = source.materials_with_emissive_textures;
    return result;
}

void add_scene_entity_statistics(SceneStatistics& result,
                                 std::span<const std::optional<EntityRecord>> records) noexcept {
    for (const std::optional<EntityRecord>& record : records) {
        if (!record.has_value()) {
            continue;
        }
        ++result.entities;
        result.model_entities += record->model.has_value() ? 1U : 0U;
    }
}

} // namespace

SceneStatistics Storage::statistics() const noexcept {
    SceneStatistics result = document_ != nullptr ? document_statistics(document_->statistics())
                                                  : compatibility_statistics(assets_);
    add_scene_entity_statistics(result, entities_);
    return result;
}

} // namespace elf3d::scene
