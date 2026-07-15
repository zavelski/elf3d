#include <elf3d/model/detail/document_storage.h>
#include <elf3d/model/detail/imported_metadata.h>

#include <cstddef>
#include <utility>

namespace elf3d::model::detail {

bool DocumentMetadataAccess::target_metadata_valid(bool target_exists,
                                                   const ModelJsonMetadata& metadata,
                                                   std::size_t& total_bytes) noexcept {
    return target_exists && valid_metadata(metadata, total_bytes);
}

Result<void>
DocumentMetadataAccess::validate_document_metadata(const ImportedDocumentMetadata& metadata,
                                                   std::size_t& total_bytes) noexcept {
    if (!valid_metadata(metadata.root, total_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Imported JSON metadata is malformed or exceeds its resource budget"};
    }
    if (!valid_metadata(metadata.asset, total_bytes)) {
        return Error{ErrorCode::invalid_argument,
                     "Imported JSON metadata is malformed or exceeds its resource budget"};
    }
    return {};
}

Result<void>
DocumentMetadataAccess::validate_structural_targets(const Document::Storage& storage,
                                                    const ImportedDocumentMetadata& metadata,
                                                    std::size_t& total_bytes) noexcept {
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
    return {};
}

Result<void>
DocumentMetadataAccess::validate_resource_targets(const Document::Storage& storage,
                                                  const ImportedDocumentMetadata& metadata,
                                                  std::size_t& total_bytes) noexcept {
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
                                                        bool& any_metadata) noexcept {
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

void DocumentMetadataAccess::attach_resource_metadata(Document::Storage& storage,
                                                      ImportedDocumentMetadata& metadata,
                                                      bool& any_metadata) noexcept {
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
                                                            ImportedDocumentMetadata&& metadata) {
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
    attach_structural_metadata(storage, metadata, any_metadata);
    attach_resource_metadata(storage, metadata, any_metadata);
    storage.has_preserved_metadata = any_metadata;
    storage.preserved_metadata_stale = false;
    return {};
}

} // namespace elf3d::model::detail
