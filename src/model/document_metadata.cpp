#include <elf3d/model.h>

#include <core/exception_boundary.h>
#include <model/document_storage.h>
#include <model/imported_metadata.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace elf3d::model::detail {

bool DocumentMetadataAccess::target_metadata_valid(bool target_exists,
                                                   const ModelJsonMetadata& metadata,
                                                   std::size_t& total_bytes) noexcept
{
    return target_exists && valid_metadata(metadata, total_bytes);
}

Result<void>
DocumentMetadataAccess::validate_document_metadata(const ImportedDocumentMetadata& metadata,
                                                   std::size_t& total_bytes) noexcept
{
    if (!valid_metadata(metadata.root, total_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Imported JSON metadata is malformed or exceeds its resource budget"};
    }
    if (!valid_metadata(metadata.asset, total_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Imported JSON metadata is malformed or exceeds its resource budget"};
    }
    constexpr std::size_t maximum_names = 256U;
    if (metadata.required_extensions.size() > maximum_names) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "Imported required-extension declarations exceed the limit"};
    }
    for (std::size_t index = 0; index < metadata.required_extensions.size(); ++index) {
        const auto& name = metadata.required_extensions[index];
        const auto previous_end = metadata.required_extensions.begin() + index;
        if (name.empty() || name.size() > 256U ||
            std::find(metadata.required_extensions.begin(), previous_end, name) != previous_end) {
            return Error{ErrorCode::invalid_argument,
                         "Imported required-extension declarations are invalid"};
        }
    }
    return {};
}

Result<void>
DocumentMetadataAccess::validate_structural_targets(const Document::Storage& storage,
                                                    const ImportedDocumentMetadata& metadata,
                                                    std::size_t& total_bytes) noexcept
{
    for (const auto& entry : metadata.scenes) {
        if (!target_metadata_valid(static_cast<bool>(storage.scene(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported scene metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.nodes) {
        if (!target_metadata_valid(static_cast<bool>(storage.node(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported node metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.meshes) {
        if (!target_metadata_valid(static_cast<bool>(storage.mesh(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported mesh metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.primitives) {
        if (!target_metadata_valid(static_cast<bool>(storage.primitive(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported primitive metadata does not match the document"};
        }
    }
    return validate_camera_targets(storage, metadata, total_bytes);
}

Result<void>
DocumentMetadataAccess::validate_camera_targets(const Document::Storage& storage,
                                                const ImportedDocumentMetadata& metadata,
                                                std::size_t& total_bytes) noexcept
{
    for (const auto& entry : metadata.cameras) {
        const auto node = storage.node(entry.first);
        if (!target_metadata_valid(node && node.value()->perspective_camera.has_value(),
                                   entry.second, total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported camera metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.camera_projections) {
        const auto node = storage.node(entry.first);
        if (!target_metadata_valid(node && node.value()->perspective_camera.has_value(),
                                   entry.second, total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported camera projection metadata does not match the document"};
        }
    }
    return {};
}

Result<void>
DocumentMetadataAccess::validate_resource_targets(const Document::Storage& storage,
                                                  const ImportedDocumentMetadata& metadata,
                                                  std::size_t& total_bytes) noexcept
{
    for (const auto& entry : metadata.materials) {
        if (!target_metadata_valid(static_cast<bool>(storage.material(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported material metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.images) {
        if (!target_metadata_valid(static_cast<bool>(storage.image(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported image metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.textures) {
        if (!target_metadata_valid(static_cast<bool>(storage.texture(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported texture metadata does not match the document"};
        }
    }
    for (const auto& entry : metadata.samplers) {
        if (!target_metadata_valid(static_cast<bool>(storage.sampler(entry.first)), entry.second,
                                   total_bytes)) {
            return Error{ErrorCode::invalid_argument,
                         "Imported sampler metadata does not match the document"};
        }
    }
    return {};
}

void DocumentMetadataAccess::attach_structural_metadata(Document::Storage& storage,
                                                        ImportedDocumentMetadata& metadata,
                                                        bool& any_metadata) noexcept
{
    for (auto& entry : metadata.scenes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.scenes[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.nodes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.nodes[index].metadata = std::move(entry.second);
    }
    attach_camera_metadata(storage, metadata, any_metadata);
    for (auto& entry : metadata.meshes) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.meshes[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.primitives) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.primitives[index].metadata = std::move(entry.second);
    }
}

void DocumentMetadataAccess::attach_camera_metadata(Document::Storage& storage,
                                                    ImportedDocumentMetadata& metadata,
                                                    bool& any_metadata) noexcept
{
    for (auto& entry : metadata.cameras) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.nodes[index].camera_metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.camera_projections) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.nodes[index].camera_projection_metadata = std::move(entry.second);
    }
}

void DocumentMetadataAccess::attach_resource_metadata(Document::Storage& storage,
                                                      ImportedDocumentMetadata& metadata,
                                                      bool& any_metadata) noexcept
{
    for (auto& entry : metadata.materials) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.materials[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.images) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.images[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.textures) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.textures[index].metadata = std::move(entry.second);
    }
    for (auto& entry : metadata.samplers) {
        const std::size_t index =
            static_cast<std::size_t>(DocumentHandleAccess::value(entry.first) - 1U);
        any_metadata = any_metadata || has_metadata(entry.second);
        storage.samplers[index].metadata = std::move(entry.second);
    }
}

Result<void> DocumentMetadataAccess::attach_import_metadata(Document& document,
                                                            ImportedDocumentMetadata&& metadata)
{
    if (document.storage_ == nullptr) {
        return Error{ErrorCode::invalid_argument,
                     "Imported metadata requires a live model document"};
    }
    Document::Storage& storage = *document.storage_;
    std::size_t total_bytes = 0;
    Result<void> document_validation = validate_document_metadata(metadata, total_bytes);
    if (!document_validation) {
        return document_validation.error();
    }

    Result<void> structural_validation =
        validate_structural_targets(storage, metadata, total_bytes);
    if (!structural_validation) {
        return structural_validation.error();
    }

    Result<void> resource_validation = validate_resource_targets(storage, metadata, total_bytes);
    if (!resource_validation) {
        return resource_validation.error();
    }

    bool any_metadata = has_metadata(metadata.root) || has_metadata(metadata.asset);
    storage.root_metadata = std::move(metadata.root);
    storage.asset_metadata = std::move(metadata.asset);
    storage.required_extensions = std::move(metadata.required_extensions);
    attach_structural_metadata(storage, metadata, any_metadata);
    attach_resource_metadata(storage, metadata, any_metadata);
    storage.has_preserved_metadata = any_metadata;
    storage.preserved_metadata_stale = false;
    storage.advance_revision();
    return {};
}

} // namespace elf3d::model::detail

namespace elf3d {
namespace {

ModelJsonMetadata copy_metadata(ModelJsonMetadataView view)
{
    ModelJsonMetadata result;
    if (view.extras_json) {
        result.extras_json = std::string{*view.extras_json};
    }
    result.extensions.assign(view.extensions.begin(), view.extensions.end());
    return result;
}

template <class Id, class ReadSource, class ReadTarget>
Result<void> copy_mapped_metadata(std::span<const std::pair<Id, Id>> mapping,
                                  ReadSource read_source, ReadTarget read_target,
                                  std::vector<std::pair<Id, ModelJsonMetadata>>& output)
{
    for (const auto& [source, target] : mapping) {
        const auto a = read_source(source);
        const auto b = read_target(target);
        if (!a || !b || std::any_of(output.begin(), output.end(), [&](const auto& entry) {
                return entry.first == target;
            })) {
            return Error{ErrorCode::invalid_argument,
                         "Metadata mapping has a foreign ID or duplicate destination"};
        }
        output.emplace_back(target, copy_metadata(a.value().metadata));
    }
    return {};
}

Result<void> copy_camera_metadata(DocumentView source, DocumentView target,
                                  std::span<const std::pair<NodeId, NodeId>> mapping,
                                  model::detail::ImportedDocumentMetadata& output)
{
    for (const auto& [from, to] : mapping) {
        const auto a = source.node(from).value();
        const auto b = target.node(to).value();
        if (a.perspective_camera.has_value() != b.perspective_camera.has_value()) {
            return Error{ErrorCode::invalid_argument,
                         "Mapped nodes must agree about camera presence"};
        }
        if (a.perspective_camera) {
            output.cameras.emplace_back(to, copy_metadata(a.camera_properties.metadata));
            output.camera_projections.emplace_back(
                to, copy_metadata(a.camera_properties.projection_metadata));
        }
    }
    return {};
}
bool metadata_transfer_valid(DocumentView source, DocumentView target) noexcept
{
    return !target.preserved_metadata_stale() && target.required_extensions().empty() &&
           !source.preserved_metadata_stale() && !validate_document(source).has_errors() &&
           !validate_document(target).has_errors();
}

template <class Id>
bool unique_metadata_targets(std::vector<std::pair<Id, ModelJsonMetadata>>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return model::detail::DocumentHandleAccess::value(left.first) <
               model::detail::DocumentHandleAccess::value(right.first);
    });
    return std::adjacent_find(entries.begin(), entries.end(),
                              [](const auto& left, const auto& right) {
                                  return left.first == right.first;
                              }) == entries.end();
}

bool metadata_targets_unique(ModelDocumentMetadata& metadata)
{
    const std::array checks{unique_metadata_targets(metadata.scenes),
                            unique_metadata_targets(metadata.nodes),
                            unique_metadata_targets(metadata.cameras),
                            unique_metadata_targets(metadata.camera_projections),
                            unique_metadata_targets(metadata.meshes),
                            unique_metadata_targets(metadata.primitives),
                            unique_metadata_targets(metadata.materials),
                            unique_metadata_targets(metadata.images),
                            unique_metadata_targets(metadata.textures),
                            unique_metadata_targets(metadata.samplers)};
    return std::all_of(checks.begin(), checks.end(), [](bool value) { return value; });
}
} // namespace

Result<void> Document::attach_verified_metadata(ModelDocumentMetadata metadata) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (!storage_ || storage_->has_preserved_metadata || preserved_metadata_stale() ||
            !required_extensions().empty() || validate_document(view()).has_errors()) {
            return Error{ErrorCode::invalid_argument,
                         "Metadata construction requires a valid unannotated target"};
        }
        if (!metadata_targets_unique(metadata)) {
            return Error{ErrorCode::invalid_argument,
                         "Metadata construction contains duplicate destinations"};
        }
        return model::detail::DocumentMetadataAccess::attach_import_metadata(*this,
                                                                             std::move(metadata));
    });
}

Result<void> Document::copy_verified_metadata(DocumentView source,
                                              const ModelMetadataMapping& mapping) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (!storage_ || storage_->has_preserved_metadata ||
            !metadata_transfer_valid(source, view())) {
            return Error{ErrorCode::invalid_argument,
                         "Metadata transfer requires valid source and unannotated target"};
        }
        model::detail::ImportedDocumentMetadata metadata;
        metadata.root = copy_metadata(source.root_metadata());
        metadata.asset = copy_metadata(source.asset_metadata());
        const auto names = source.required_extensions();
        metadata.required_extensions.assign(names.begin(), names.end());
        auto scenes = copy_mapped_metadata(
            mapping.scenes, [&](DocumentSceneId id) { return source.scene(id); },
            [&](DocumentSceneId id) { return scene(id); }, metadata.scenes);
        auto nodes = copy_mapped_metadata(
            mapping.nodes, [&](NodeId id) { return source.node(id); },
            [&](NodeId id) { return node(id); }, metadata.nodes);
        auto meshes = copy_mapped_metadata(
            mapping.meshes, [&](MeshId id) { return source.mesh(id); },
            [&](MeshId id) { return mesh(id); }, metadata.meshes);
        auto primitives = copy_mapped_metadata(
            mapping.primitives, [&](PrimitiveId id) { return source.primitive(id); },
            [&](PrimitiveId id) { return primitive(id); }, metadata.primitives);
        auto materials = copy_mapped_metadata(
            mapping.materials, [&](MaterialId id) { return source.material(id); },
            [&](MaterialId id) { return material(id); }, metadata.materials);
        auto images = copy_mapped_metadata(
            mapping.images, [&](ImageId id) { return source.image(id); },
            [&](ImageId id) { return image(id); }, metadata.images);
        auto textures = copy_mapped_metadata(
            mapping.textures, [&](TextureId id) { return source.texture(id); },
            [&](TextureId id) { return texture(id); }, metadata.textures);
        auto samplers = copy_mapped_metadata(
            mapping.samplers, [&](SamplerId id) { return source.sampler(id); },
            [&](SamplerId id) { return sampler(id); }, metadata.samplers);
        const std::array checks{std::move(scenes),     std::move(nodes),     std::move(meshes),
                                std::move(primitives), std::move(materials), std::move(images),
                                std::move(textures),   std::move(samplers)};
        for (const auto& check : checks) {
            if (!check) {
                return check.error();
            }
        }
        const auto cameras = copy_camera_metadata(source, view(), mapping.nodes, metadata);
        if (!cameras) {
            return cameras.error();
        }
        return model::detail::DocumentMetadataAccess::attach_import_metadata(*this,
                                                                             std::move(metadata));
    });
}
} // namespace elf3d
