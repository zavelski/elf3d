#include <elf3d/model.h>

#include <core/exception_boundary.h>
#include <model/document_storage.h>

namespace elf3d {
namespace {

using model::detail::DocumentHandleAccess;
using model::detail::DocumentOwnerToken;

// Storage is append-only and clone preserves every record ordinal. Rebind only
// typed canonical IDs, never numbers embedded in preserved opaque metadata.
[[nodiscard]] DocumentSceneId remap(DocumentSceneId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_scene(owner, DocumentHandleAccess::value(id))
               : DocumentSceneId{};
}

[[nodiscard]] NodeId remap(NodeId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid() ? DocumentHandleAccess::create_node(owner, DocumentHandleAccess::value(id))
                         : NodeId{};
}

[[nodiscard]] MeshId remap(MeshId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid() ? DocumentHandleAccess::create_mesh(owner, DocumentHandleAccess::value(id))
                         : MeshId{};
}

[[nodiscard]] PrimitiveId remap(PrimitiveId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_primitive(owner, DocumentHandleAccess::value(id))
               : PrimitiveId{};
}

[[nodiscard]] MaterialId remap(MaterialId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_material(owner, DocumentHandleAccess::value(id))
               : MaterialId{};
}

[[nodiscard]] ImageId remap(ImageId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_image(owner, DocumentHandleAccess::value(id))
               : ImageId{};
}

[[nodiscard]] TextureId remap(TextureId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_texture(owner, DocumentHandleAccess::value(id))
               : TextureId{};
}

[[nodiscard]] SamplerId remap(SamplerId id, DocumentOwnerToken owner) noexcept
{
    return id.is_valid()
               ? DocumentHandleAccess::create_sampler(owner, DocumentHandleAccess::value(id))
               : SamplerId{};
}

void remap_material(ModelMaterialDescription& material, DocumentOwnerToken owner) noexcept
{
    material.base_color_texture = remap(material.base_color_texture, owner);
    material.metallic_roughness_texture = remap(material.metallic_roughness_texture, owner);
    material.normal_texture = remap(material.normal_texture, owner);
    material.occlusion_texture = remap(material.occlusion_texture, owner);
    material.emissive_texture = remap(material.emissive_texture, owner);
    material.specular_texture = remap(material.specular_texture, owner);
    material.specular_color_texture = remap(material.specular_color_texture, owner);
    if (material.transmission) {
        material.transmission->texture = remap(material.transmission->texture, owner);
    }
    if (material.volume) {
        material.volume->thickness_texture = remap(material.volume->thickness_texture, owner);
    }
    if (material.clearcoat) {
        material.clearcoat->texture = remap(material.clearcoat->texture, owner);
        material.clearcoat->roughness_texture = remap(material.clearcoat->roughness_texture, owner);
        material.clearcoat->normal_texture = remap(material.clearcoat->normal_texture, owner);
    }
    // MaterialRecord owns its name; material views supply that borrow on demand.
    material.name = {};
}

} // namespace

void Document::Storage::clone_contents_from(const Storage& source,
                                            model::detail::ImageCopyPolicy image_policy)
{
    // Tables, geometry and metadata always have independent storage. Only the
    // explicit snapshot operation may retain immutable image blocks.
    root_metadata = source.root_metadata;
    asset_metadata = source.asset_metadata;
    required_extensions = source.required_extensions;
    scenes = source.scenes;
    default_scene = source.default_scene;
    nodes = source.nodes;
    meshes = source.meshes;
    primitives = source.primitives;
    materials = source.materials;
    images = source.images;
    if (image_policy == model::detail::ImageCopyPolicy::independent) {
        for (auto& image : images) {
            image.payload = image.payload.independent_copy();
        }
    }
    textures = source.textures;
    samplers = source.samplers;
    has_preserved_metadata = source.has_preserved_metadata;
    preserved_metadata_stale = source.preserved_metadata_stale;
    remap_cloned_hierarchy();
    remap_cloned_assets();
}

void Document::Storage::remap_cloned_hierarchy() noexcept
{
    const auto owner = token();
    if (default_scene) {
        default_scene = remap(*default_scene, owner);
    }
    for (auto& scene : scenes) {
        scene.id = remap(scene.id, owner);
        for (auto& root : scene.roots) {
            root = remap(root, owner);
        }
    }
    for (auto& node : nodes) {
        node.id = remap(node.id, owner);
        if (node.parent) {
            node.parent = remap(*node.parent, owner);
        }
        for (auto& child : node.children) {
            child = remap(child, owner);
        }
        if (node.mesh) {
            node.mesh = remap(*node.mesh, owner);
        }
        if (node.camera_properties.image_based_lighting) {
            auto& lighting = *node.camera_properties.image_based_lighting;
            lighting.image = remap(lighting.image, owner);
        }
    }
}

void Document::Storage::remap_cloned_assets() noexcept
{
    const auto owner = token();
    for (auto& mesh : meshes) {
        mesh.id = remap(mesh.id, owner);
        for (auto& primitive : mesh.primitives) {
            primitive = remap(primitive, owner);
        }
    }
    for (auto& primitive : primitives) {
        primitive.id = remap(primitive.id, owner);
        primitive.mesh = remap(primitive.mesh, owner);
        primitive.material = remap(primitive.material, owner);
    }
    for (auto& material : materials) {
        material.id = remap(material.id, owner);
        remap_material(material.description, owner);
    }
    for (auto& image : images) {
        image.id = remap(image.id, owner);
    }
    for (auto& texture : textures) {
        texture.id = remap(texture.id, owner);
        texture.description.image = remap(texture.description.image, owner);
        texture.description.sampler = remap(texture.description.sampler, owner);
    }
    for (auto& sampler : samplers) {
        sampler.id = remap(sampler.id, owner);
    }
}

Result<Document> clone_document(DocumentView document) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<Document> {
        const auto validation = validate_document(document);
        for (const auto& diagnostic : validation.diagnostics) {
            if (diagnostic.severity == DocumentDiagnosticSeverity::error) {
                return Error{ErrorCode::invalid_argument, diagnostic.message};
            }
        }
        // Validation rejects null/moved-from views; a live empty owner is valid.
        Document copy;
        copy.storage_->clone_contents_from(*document.document_->storage_,
                                           model::detail::ImageCopyPolicy::independent);
        return copy;
    });
}

Result<Document> snapshot_document(DocumentView document) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<Document> {
        const auto validation = validate_document(document);
        for (const auto& diagnostic : validation.diagnostics) {
            if (diagnostic.severity == DocumentDiagnosticSeverity::error) {
                return Error{ErrorCode::invalid_argument, diagnostic.message};
            }
        }
        Document copy;
        copy.storage_->clone_contents_from(*document.document_->storage_,
                                           model::detail::ImageCopyPolicy::share_immutable);
        return copy;
    });
}

Result<ImageId> Document::create_image_snapshot(DocumentView source, ImageId image) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<ImageId> {
        const auto input = source.image(image);
        if (!input) {
            return input.error();
        }
        // Copy the record before append: source may be this same Document.
        auto record = *source.document_->storage_->image(image).value();
        record.metadata = {};
        if (!storage_) {
            storage_ = std::make_unique<Storage>();
        }
        record.id = DocumentHandleAccess::create_image(
            storage_->token(), static_cast<std::uint64_t>(storage_->images.size() + 1U));
        const auto id = record.id;
        storage_->images.push_back(std::move(record));
        storage_->note_mutation();
        return id;
    });
}

} // namespace elf3d
