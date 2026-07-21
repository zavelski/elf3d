#ifndef ELF3D_CORE_ERROR_H
#define ELF3D_CORE_ERROR_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace elf3d {

enum class ErrorCode {
    invalid_argument,
    missing_graphics_procedure_loader,
    graphics_initialization_failed,
    unsupported_graphics_version,
    graphics_context_unavailable,
    graphics_thread_violation,
    graphics_shutdown,
    invalid_viewport_dimensions,
    framebuffer_creation_failed,
    framebuffer_incomplete,
    texture_unavailable,
    backend_mismatch,
    invalid_entity,
    invalid_hierarchy_snapshot_index,
    no_selected_entity,
    invalid_mesh_handle,
    invalid_material_handle,
    invalid_image_handle,
    invalid_texture_asset_handle,
    invalid_parent_assignment,
    hierarchy_cycle,
    invalid_mesh_data,
    mesh_index_out_of_range,
    invalid_camera_configuration,
    entity_has_no_camera,
    invalid_viewport_input,
    invalid_viewport_position,
    invalid_navigation_settings,
    invalid_selection_settings,
    invalid_measurement_settings,
    invalid_section_plane,
    invalid_clipping_box,
    invalid_clipping_box_index,
    clipping_box_limit_exceeded,
    invalid_clipping_settings,
    invalid_measurement_hit,
    invalid_measurement_anchor,
    invalid_picking_ray,
    picking_acceleration_failed,
    projection_failed,
    scene_has_no_bounds,
    transform_requires_matrix_api,
    source_file_not_found,
    source_file_read_failed,
    source_file_write_failed,
    unsupported_scene_format,
    unsupported_model_format,
    malformed_gltf,
    malformed_glb,
    gltf_validation_failed,
    unsupported_required_extension,
    missing_external_buffer,
    unsupported_remote_uri,
    invalid_buffer_range,
    invalid_buffer_view,
    invalid_accessor,
    missing_position_accessor,
    mismatched_normal_count,
    invalid_texcoord,
    mismatched_texcoord_count,
    missing_normals,
    unsupported_primitive_mode,
    unsupported_index_type,
    imported_index_out_of_range,
    non_finite_position,
    unsupported_image_mime_type,
    unsupported_image_extension,
    malformed_data_uri,
    invalid_base64_payload,
    missing_external_image,
    external_image_read_failed,
    invalid_image_buffer_view,
    image_range_out_of_bounds,
    image_decode_failed,
    image_encode_failed,
    zero_image_dimensions,
    excessive_image_dimensions,
    decoded_image_size_overflow,
    image_resource_limit_exceeded,
    invalid_sampler_description,
    invalid_sampler_wrap,
    invalid_sampler_filter,
    size_overflow,
    resource_limit_exceeded,
    invalid_node_hierarchy,
    invalid_transform_matrix,
    scene_import_failed,
    empty_scene_geometry,
    shader_compilation_failed,
    shader_linking_failed,
    gpu_buffer_creation_failed,
    gpu_texture_creation_failed,
    gpu_texture_upload_failed,
    unsupported_texture_format,
    incomplete_mipmap_state,
    draw_submission_failed,
    unsupported_vertex_layout,
    foreign_engine_object,
    unexpected_exception,
    invalid_document_scene_id,
    invalid_node_id,
    invalid_mesh_id,
    invalid_primitive_id,
    invalid_material_id,
    invalid_material_description,
    invalid_image_id,
    invalid_texture_id,
    invalid_sampler_id,
};

class Error final {
  public:
    static constexpr std::size_t message_capacity = 239;

    constexpr Error(ErrorCode code, std::string_view message) noexcept : code_(code) {
        const std::size_t count = std::min(message.size(), message_capacity);
        for (std::size_t index = 0; index < count; ++index) {
            message_[index] = message[index];
        }
        message_[count] = '\0';
    }

    [[nodiscard]] constexpr ErrorCode code() const noexcept {
        return code_;
    }

    [[nodiscard]] constexpr const char* message() const noexcept {
        return message_.data();
    }

  private:
    ErrorCode code_;
    std::array<char, message_capacity + 1> message_{};
};

static_assert(sizeof(Error) <= 256);

} // namespace elf3d

#endif
