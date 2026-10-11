#include <elf3d/model.h>

#include <elf3d/core/detail/exception_boundary.h>

#include <elf3d/model/detail/document_storage.h>

#include <cmath>
#include <memory>

namespace elf3d {

using model::detail::finite;

Result<void> Document::Storage::validate_camera_properties(
    const ModelCameraPropertiesDescription& description) const noexcept
{
    if (description.aspect_ratio &&
        (!std::isfinite(*description.aspect_ratio) || *description.aspect_ratio <= 0.0F)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "Authored camera aspect ratio must be finite and positive"};
    }
    if (description.image_based_lighting) {
        const auto& lighting = *description.image_based_lighting;
        if (!image(lighting.image) || !finite(lighting.rotation) || !finite(lighting.scale)) {
            return Error{ErrorCode::invalid_camera_configuration,
                         "Camera lighting requires a document image and finite transforms"};
        }
    }
    return {};
}

Result<void>
Document::set_node_camera_properties(NodeId node_id,
                                     const ModelCameraPropertiesDescription& description) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<void> {
        if (storage_ == nullptr) {
            return Error{ErrorCode::invalid_node_id, "The document is empty"};
        }
        auto node = storage_->mutable_node(node_id);
        if (!node) {
            return node.error();
        }
        if (!node.value()->perspective_camera) {
            return Error{ErrorCode::invalid_camera_configuration,
                         "Camera properties require an attached perspective camera"};
        }
        if (const auto valid = storage_->validate_camera_properties(description); !valid) {
            return valid.error();
        }
        if (node.value()->camera_properties != description) {
            node.value()->camera_properties = description;
            storage_->note_mutation();
        }
        return {};
    });
}

} // namespace elf3d
