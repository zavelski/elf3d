#ifndef ELF3D_MODEL_DETAIL_DOCUMENT_STORAGE_H
#define ELF3D_MODEL_DETAIL_DOCUMENT_STORAGE_H

#include <elf3d/model.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::model::detail {

[[nodiscard]] bool finite(const Float4x4& value) noexcept;
[[nodiscard]] bool valid_wrap(ModelTextureWrap wrap) noexcept;
[[nodiscard]] bool valid_filter(ModelTextureFilter filter) noexcept;
[[nodiscard]] bool valid_mag_filter(ModelTextureFilter filter) noexcept;
[[nodiscard]] bool
valid_perspective_camera(const ModelPerspectiveCameraDescription& description) noexcept;
[[nodiscard]] bool valid_material_factors(const ModelMaterialDescription& description) noexcept;
[[nodiscard]] bool valid_material_mappings(const ModelMaterialDescription& description) noexcept;
[[nodiscard]] Result<void>
validate_image_description(const ModelImageDescription& description) noexcept;
[[nodiscard]] Bounds3 merge(Bounds3 left, Bounds3 right) noexcept;
[[nodiscard]] Result<Bounds3> primitive_bounds(const PrimitiveDataView& data) noexcept;
[[nodiscard]] Result<void> validate_primitive_data(const PrimitiveDataView& data) noexcept;
[[nodiscard]] PrimitiveData copy_primitive_data(const PrimitiveDataView& view);
[[nodiscard]] ModelJsonMetadataView metadata_view(const ModelJsonMetadata& metadata) noexcept;
[[nodiscard]] bool has_metadata(const ModelJsonMetadata& metadata) noexcept;
[[nodiscard]] bool valid_metadata(const ModelJsonMetadata& metadata,
                                  std::size_t& total_bytes) noexcept;

} // namespace elf3d::model::detail

namespace elf3d {

class Document::Storage final {
  public:
    struct SceneRecord {
        DocumentSceneId id;
        std::string name;
        std::vector<NodeId> roots;
        ModelJsonMetadata metadata;
    };

    struct NodeRecord {
        NodeId id;
        std::string name;
        std::optional<NodeId> parent;
        std::vector<NodeId> children;
        Float4x4 local_matrix;
        std::optional<MeshId> mesh;
        std::optional<ModelPerspectiveCameraDescription> perspective_camera;
        ModelJsonMetadata metadata;
    };

    struct MeshRecord {
        MeshId id;
        std::string name;
        std::vector<PrimitiveId> primitives;
        std::optional<Bounds3> bounds;
        ModelJsonMetadata metadata;
    };

    struct PrimitiveRecord {
        PrimitiveId id;
        MeshId mesh;
        MaterialId material;
        Bounds3 bounds;
        PrimitiveData data;
        ModelJsonMetadata metadata;
    };

    struct MaterialRecord {
        MaterialId id;
        ModelMaterialDescription description;
        ModelJsonMetadata metadata;
    };

    struct ImageRecord {
        ImageId id;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        ModelPixelFormat format = ModelPixelFormat::rgba8_unorm;
        std::vector<std::byte> pixels;
        ModelImageMimeType source_mime_type = ModelImageMimeType::none;
        std::vector<std::byte> source_bytes;
        ModelJsonMetadata metadata;
    };

    struct TextureRecord {
        TextureId id;
        ModelTextureDescription description;
        ModelJsonMetadata metadata;
    };

    struct SamplerRecord {
        SamplerId id;
        ModelSamplerDescription description;
        ModelJsonMetadata metadata;
    };

    [[nodiscard]] std::uintptr_t token() const noexcept;

    [[nodiscard]] bool owns(DocumentSceneId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(NodeId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(MeshId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(PrimitiveId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(MaterialId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(ImageId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(TextureId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] bool owns(SamplerId id) const noexcept {
        return id.is_valid() && model::detail::DocumentHandleAccess::document(id) == token();
    }

    [[nodiscard]] Result<SceneRecord*> mutable_scene(DocumentSceneId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_argument,
                         "The scene identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= scenes.size()) {
            return Error{ErrorCode::invalid_argument, "The scene identifier is stale"};
        }
        return &scenes[index];
    }

    [[nodiscard]] Result<const SceneRecord*> scene(DocumentSceneId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_argument,
                         "The scene identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= scenes.size()) {
            return Error{ErrorCode::invalid_argument, "The scene identifier is stale"};
        }
        return &scenes[index];
    }

    [[nodiscard]] Result<NodeRecord*> mutable_node(NodeId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_entity,
                         "The node identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= nodes.size()) {
            return Error{ErrorCode::invalid_entity, "The node identifier is stale"};
        }
        return &nodes[index];
    }

    [[nodiscard]] Result<const NodeRecord*> node(NodeId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_entity,
                         "The node identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= nodes.size()) {
            return Error{ErrorCode::invalid_entity, "The node identifier is stale"};
        }
        return &nodes[index];
    }

    [[nodiscard]] Result<MeshRecord*> mutable_mesh(MeshId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The mesh identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= meshes.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The mesh identifier is stale"};
        }
        return &meshes[index];
    }

    [[nodiscard]] Result<const MeshRecord*> mesh(MeshId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The mesh identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= meshes.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The mesh identifier is stale"};
        }
        return &meshes[index];
    }

    [[nodiscard]] Result<PrimitiveRecord*> mutable_primitive(PrimitiveId id) noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The primitive identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= primitives.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The primitive identifier is stale"};
        }
        return &primitives[index];
    }

    [[nodiscard]] Result<const PrimitiveRecord*> primitive(PrimitiveId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_mesh_handle,
                         "The primitive identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= primitives.size()) {
            return Error{ErrorCode::invalid_mesh_handle, "The primitive identifier is stale"};
        }
        return &primitives[index];
    }

    [[nodiscard]] Result<const MaterialRecord*> material(MaterialId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_material_handle,
                         "The material identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= materials.size()) {
            return Error{ErrorCode::invalid_material_handle, "The material identifier is stale"};
        }
        return &materials[index];
    }

    [[nodiscard]] Result<const ImageRecord*> image(ImageId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_image_handle,
                         "The image identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= images.size()) {
            return Error{ErrorCode::invalid_image_handle, "The image identifier is stale"};
        }
        return &images[index];
    }

    [[nodiscard]] Result<const TextureRecord*> texture(TextureId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_texture_asset_handle,
                         "The texture identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= textures.size()) {
            return Error{ErrorCode::invalid_texture_asset_handle,
                         "The texture identifier is stale"};
        }
        return &textures[index];
    }

    [[nodiscard]] Result<const SamplerRecord*> sampler(SamplerId id) const noexcept {
        if (!owns(id)) {
            return Error{ErrorCode::invalid_sampler_description,
                         "The sampler identifier does not belong to this document"};
        }
        const std::size_t index =
            static_cast<std::size_t>(model::detail::DocumentHandleAccess::value(id) - 1);
        if (index >= samplers.size()) {
            return Error{ErrorCode::invalid_sampler_description, "The sampler identifier is stale"};
        }
        return &samplers[index];
    }

    [[nodiscard]] bool
    valid_material_textures(const ModelMaterialDescription& description) const noexcept {
        return valid_texture_or_empty(description.base_color_texture) &&
               valid_texture_or_empty(description.metallic_roughness_texture) &&
               valid_texture_or_empty(description.normal_texture) &&
               valid_texture_or_empty(description.occlusion_texture) &&
               valid_texture_or_empty(description.emissive_texture);
    }

    [[nodiscard]] Result<void> reject_parent_cycle(NodeId child_id,
                                                   NodeId parent_id) const noexcept {
        std::optional<NodeId> ancestor = parent_id;
        while (ancestor.has_value()) {
            if (*ancestor == child_id) {
                return Error{ErrorCode::hierarchy_cycle,
                             "The parent assignment would create a document node cycle"};
            }
            const Result<const NodeRecord*> ancestor_record = node(*ancestor);
            if (!ancestor_record) {
                return ancestor_record.error();
            }
            ancestor = ancestor_record.value()->parent;
        }
        return {};
    }

    void detach_from_existing_parent(NodeRecord& child, NodeId child_id) noexcept {
        if (!child.parent.has_value()) {
            return;
        }
        Result<NodeRecord*> old_parent = mutable_node(*child.parent);
        if (!old_parent) {
            return;
        }
        auto& siblings = old_parent.value()->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child_id), siblings.end());
    }

    void remove_scene_root_entries(NodeId node_id) noexcept {
        for (SceneRecord& scene_record : scenes) {
            scene_record.roots.erase(
                std::remove(scene_record.roots.begin(), scene_record.roots.end(), node_id),
                scene_record.roots.end());
        }
    }

    void update_mesh_bounds(MeshRecord& mesh) noexcept {
        mesh.bounds.reset();
        for (const PrimitiveId primitive_id : mesh.primitives) {
            const Result<const PrimitiveRecord*> primitive_result = primitive(primitive_id);
            if (!primitive_result) {
                continue;
            }
            mesh.bounds = mesh.bounds.has_value()
                              ? model::detail::merge(*mesh.bounds, primitive_result.value()->bounds)
                              : primitive_result.value()->bounds;
        }
    }

    [[nodiscard]] DocumentStatistics statistics() const noexcept {
        DocumentStatistics result;
        result.scenes = static_cast<std::uint64_t>(scenes.size());
        result.nodes = static_cast<std::uint64_t>(nodes.size());
        result.meshes = static_cast<std::uint64_t>(meshes.size());
        result.primitives = static_cast<std::uint64_t>(primitives.size());
        result.materials = static_cast<std::uint64_t>(materials.size());
        result.images = static_cast<std::uint64_t>(images.size());
        result.textures = static_cast<std::uint64_t>(textures.size());
        result.samplers = static_cast<std::uint64_t>(samplers.size());
        accumulate_node_statistics(result);
        accumulate_primitive_statistics(result);
        accumulate_image_statistics(result);
        accumulate_material_statistics(result);
        return result;
    }

    void note_mutation() noexcept {
        preserved_metadata_stale = preserved_metadata_stale || has_preserved_metadata;
    }

    ModelJsonMetadata root_metadata;
    ModelJsonMetadata asset_metadata;
    std::vector<SceneRecord> scenes;
    std::optional<DocumentSceneId> default_scene;
    std::vector<NodeRecord> nodes;
    std::vector<MeshRecord> meshes;
    std::vector<PrimitiveRecord> primitives;
    std::vector<MaterialRecord> materials;
    std::vector<ImageRecord> images;
    std::vector<TextureRecord> textures;
    std::vector<SamplerRecord> samplers;
    bool has_preserved_metadata = false;
    bool preserved_metadata_stale = false;

  private:
    void accumulate_node_statistics(DocumentStatistics& statistics) const noexcept {
        for (const NodeRecord& node : nodes) {
            if (node.perspective_camera.has_value()) {
                ++statistics.perspective_cameras;
            }
        }
    }

    void accumulate_primitive_statistics(DocumentStatistics& statistics) const noexcept {
        for (const PrimitiveRecord& primitive : primitives) {
            statistics.vertices += static_cast<std::uint64_t>(primitive.data.positions.size());
            statistics.indices += static_cast<std::uint64_t>(primitive.data.indices.size());
            statistics.triangles += static_cast<std::uint64_t>(primitive.data.indices.size() / 3);
        }
    }

    void accumulate_image_statistics(DocumentStatistics& statistics) const noexcept {
        for (const ImageRecord& image : images) {
            statistics.decoded_image_bytes += static_cast<std::uint64_t>(image.pixels.size());
        }
    }

    void accumulate_material_statistics(DocumentStatistics& statistics) const noexcept {
        for (const MaterialRecord& material : materials) {
            statistics.materials_with_base_color_textures +=
                material.description.base_color_texture.is_valid() ? 1U : 0U;
            statistics.materials_with_metallic_roughness_textures +=
                material.description.metallic_roughness_texture.is_valid() ? 1U : 0U;
            statistics.materials_with_normal_textures +=
                material.description.normal_texture.is_valid() ? 1U : 0U;
            statistics.materials_with_occlusion_textures +=
                material.description.occlusion_texture.is_valid() ? 1U : 0U;
            statistics.materials_with_emissive_textures +=
                material.description.emissive_texture.is_valid() ? 1U : 0U;
        }
    }

    [[nodiscard]] bool valid_texture_or_empty(TextureId texture_id) const noexcept {
        return !texture_id.is_valid() || static_cast<bool>(texture(texture_id));
    }
};

} // namespace elf3d

#endif
