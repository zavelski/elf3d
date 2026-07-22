module;

#include <elf3d/clipping.h>
#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl::device_detail {
namespace {

struct IndexedDrawResources {
    RenderTargetView target;
    PipelineView pipeline;
    MeshView mesh;
    std::array<GLuint, graphics::material_texture_count> textures{};
};

struct IndexedBatchStateCache {
    bool blending_valid{false};
    bool blending{false};
    bool culling_valid{false};
    bool culling{false};
    bool front_face_valid{false};
    bool front_face_clockwise{false};
    bool vertex_array_valid{false};
    GLuint vertex_array{0};
    bool textures_valid{false};
    std::array<GLuint, graphics::material_texture_count> textures{};
};

[[nodiscard]] Result<IndexedDrawResources>
indexed_draw_resources(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                       graphics::StaticMesh& mesh,
                       const graphics::DrawIndexedDescription& description) noexcept {
    if (description.textures.size() != graphics::material_texture_count) {
        return Error{ErrorCode::invalid_argument,
                     "Indexed drawing requires four ordered material texture observers"};
    }
    Result<RenderTargetView> target_result = render_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    Result<PipelineView> pipeline_result = pipeline_view(pipeline);
    if (!pipeline_result) {
        return pipeline_result.error();
    }
    Result<MeshView> mesh_result = mesh_view(mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }

    IndexedDrawResources resources{target_result.value(), pipeline_result.value(),
                                   mesh_result.value()};
    for (std::size_t index = 0; index < resources.textures.size(); ++index) {
        Result<GLuint> texture_result = texture_object(description.textures[index]);
        if (!texture_result) {
            return texture_result.error();
        }
        resources.textures[index] = texture_result.value();
    }
    return resources;
}

void configure_indexed_pass_state(const IndexedDrawResources& resources) noexcept {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.target.framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(resources.target.extent.width),
               static_cast<GLsizei>(resources.target.extent.height));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PRIMITIVE_RESTART);
    glDisable(GL_RASTERIZER_DISCARD);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthRange(0.0, 1.0);
    glCullFace(GL_BACK);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(resources.pipeline.program);
}

void upload_indexed_frame_uniforms(const UniformLocations& uniforms,
                                   const graphics::DrawIndexedDescription& description) noexcept {
    glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, description.view_matrix.data());
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, description.projection_matrix.data());
    glUniform3f(uniforms.camera_world_position, description.camera_world_position.x,
                description.camera_world_position.y, description.camera_world_position.z);
    glUniform3f(uniforms.light_direction, description.light_direction.x,
                description.light_direction.y, description.light_direction.z);
    glUniform4f(uniforms.light_color, description.light_color.red, description.light_color.green,
                description.light_color.blue, description.light_color.alpha);
    glUniform1f(uniforms.ambient_intensity, description.ambient_intensity);
    glUniform1f(uniforms.diffuse_intensity, description.diffuse_intensity);
}

void upload_indexed_item_uniforms(const UniformLocations& uniforms,
                                  const graphics::DrawIndexedDescription& description,
                                  const IndexedDrawResources& resources) noexcept {
    glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, description.model_matrix.data());
    glUniformMatrix3fv(uniforms.normal, 1, GL_FALSE, description.normal_matrix.data());
    glUniform1i(uniforms.vertex_layout, static_cast<GLint>(resources.mesh.vertex_layout));
    glUniform4f(uniforms.base_color, description.base_color.red, description.base_color.green,
                description.base_color.blue, description.base_color.alpha);
    glUniform1f(uniforms.metallic_factor, description.metallic_factor);
    glUniform1f(uniforms.roughness_factor, description.roughness_factor);
    glUniform3f(uniforms.emissive_factor, description.emissive_factor.x,
                description.emissive_factor.y, description.emissive_factor.z);
    glUniform1f(uniforms.occlusion_strength, description.occlusion_strength);
    glUniform1f(uniforms.ior, description.ior);
    glUniform1f(uniforms.specular_factor, description.specular_factor);
    glUniform3f(uniforms.specular_color_factor, description.specular_color_factor.x,
                description.specular_color_factor.y, description.specular_color_factor.z);
    glUniform4f(uniforms.highlight_color, description.highlight_color.red,
                description.highlight_color.green, description.highlight_color.blue,
                description.highlight_color.alpha);
    glUniform1f(uniforms.highlight_strength, description.highlight_strength);
    glUniform1i(uniforms.has_base_color_texture, resources.textures[0] != 0 ? 1 : 0);
    glUniform1i(uniforms.has_metallic_roughness_texture, resources.textures[1] != 0 ? 1 : 0);
    glUniform1i(uniforms.has_occlusion_texture, resources.textures[2] != 0 ? 1 : 0);
    glUniform1i(uniforms.has_emissive_texture, resources.textures[3] != 0 ? 1 : 0);
    glUniform1i(uniforms.alpha_mode, static_cast<GLint>(description.alpha_mode));
    glUniform1f(uniforms.alpha_cutoff, description.alpha_cutoff);
    glUniform1i(uniforms.unlit, description.unlit ? 1 : 0);
}

void upload_texture_mapping_uniforms(const UniformLocations& uniforms,
                                     const graphics::DrawIndexedDescription& description) noexcept {
    std::array<GLint, graphics::material_texture_count> texcoord_sets{};
    std::array<float, graphics::material_texture_count * 2> texture_offsets{};
    std::array<float, graphics::material_texture_count * 2> texture_scales{};
    std::array<float, graphics::material_texture_count> texture_rotations{};
    for (std::size_t index = 0; index < description.texture_mappings.size(); ++index) {
        const TextureMapping& mapping = description.texture_mappings[index];
        texcoord_sets[index] = static_cast<GLint>(mapping.texcoord_set);
        texture_offsets[index * 2] = mapping.transform.offset.x;
        texture_offsets[index * 2 + 1] = mapping.transform.offset.y;
        texture_scales[index * 2] = mapping.transform.scale.x;
        texture_scales[index * 2 + 1] = mapping.transform.scale.y;
        texture_rotations[index] = mapping.transform.rotation_radians;
    }
    glUniform1iv(uniforms.texture_texcoord_sets, static_cast<GLsizei>(texcoord_sets.size()),
                 texcoord_sets.data());
    glUniform2fv(uniforms.texture_offsets,
                 static_cast<GLsizei>(description.texture_mappings.size()), texture_offsets.data());
    glUniform2fv(uniforms.texture_scales, static_cast<GLsizei>(description.texture_mappings.size()),
                 texture_scales.data());
    glUniform1fv(uniforms.texture_rotations, static_cast<GLsizei>(texture_rotations.size()),
                 texture_rotations.data());
}

void upload_indexed_clipping_uniforms(
    const UniformLocations& uniforms,
    const graphics::DrawIndexedDescription& description) noexcept {
    glUniform1i(uniforms.clipping_section_plane_enabled,
                description.clipping_section_plane_enabled ? 1 : 0);
    glUniform3f(uniforms.clipping_section_plane_normal, description.clipping_section_plane_normal.x,
                description.clipping_section_plane_normal.y,
                description.clipping_section_plane_normal.z);
    glUniform1f(uniforms.clipping_section_plane_offset, description.clipping_section_plane_offset);
    glUniform1i(uniforms.clipping_retain_positive_half_space,
                description.clipping_retain_positive_half_space ? 1 : 0);
    const std::uint32_t box_count =
        std::min(description.clipping_box_count, maximum_clipping_boxes);
    std::array<float, maximum_clipping_boxes * 3> minimums{};
    std::array<float, maximum_clipping_boxes * 3> maximums{};
    for (std::uint32_t index = 0; index < box_count; ++index) {
        const Bounds3& box = description.clipping_boxes[index];
        const std::size_t base = static_cast<std::size_t>(index) * 3U;
        minimums[base] = box.minimum.x;
        minimums[base + 1U] = box.minimum.y;
        minimums[base + 2U] = box.minimum.z;
        maximums[base] = box.maximum.x;
        maximums[base + 1U] = box.maximum.y;
        maximums[base + 2U] = box.maximum.z;
    }
    glUniform1i(uniforms.clipping_box_count, static_cast<GLint>(box_count));
    glUniform3fv(uniforms.clipping_box_minimums, static_cast<GLsizei>(maximum_clipping_boxes),
                 minimums.data());
    glUniform3fv(uniforms.clipping_box_maximums, static_cast<GLsizei>(maximum_clipping_boxes),
                 maximums.data());
}

void bind_material_sampler_uniforms(const UniformLocations& uniforms) noexcept {
    const std::array<GLint, graphics::material_texture_count> sampler_locations{
        uniforms.base_color_texture, uniforms.metallic_roughness_texture,
        uniforms.occlusion_texture, uniforms.emissive_texture};
    for (std::size_t index = 0; index < sampler_locations.size(); ++index) {
        glUniform1i(sampler_locations[index], static_cast<GLint>(index));
    }
}

void set_capability(GLenum capability, bool enabled) noexcept {
    if (enabled) {
        glEnable(capability);
    } else {
        glDisable(capability);
    }
}

void configure_indexed_blending(bool blending, IndexedBatchStateCache& cache) noexcept {
    if (!cache.blending_valid || cache.blending != blending) {
        set_capability(GL_BLEND, blending);
        glDepthMask(blending ? GL_FALSE : GL_TRUE);
        cache.blending_valid = true;
        cache.blending = blending;
    }
}

void configure_indexed_culling(bool culling, IndexedBatchStateCache& cache) noexcept {
    if (!cache.culling_valid || cache.culling != culling) {
        set_capability(GL_CULL_FACE, culling);
        cache.culling_valid = true;
        cache.culling = culling;
    }
}

void configure_indexed_front_face(bool clockwise, IndexedBatchStateCache& cache) noexcept {
    if (!cache.front_face_valid || cache.front_face_clockwise != clockwise) {
        glFrontFace(clockwise ? GL_CW : GL_CCW);
        cache.front_face_valid = true;
        cache.front_face_clockwise = clockwise;
    }
}

void configure_indexed_vertex_array(GLuint vertex_array, IndexedBatchStateCache& cache) noexcept {
    if (!cache.vertex_array_valid || cache.vertex_array != vertex_array) {
        glBindVertexArray(vertex_array);
        cache.vertex_array_valid = true;
        cache.vertex_array = vertex_array;
    }
}

void configure_indexed_item_state(const IndexedDrawResources& resources,
                                  const graphics::DrawIndexedDescription& description,
                                  IndexedBatchStateCache& cache) noexcept {
    configure_indexed_blending(description.alpha_mode == AlphaMode::blend, cache);
    configure_indexed_culling(!description.double_sided, cache);
    configure_indexed_front_face(description.front_face_clockwise, cache);
    configure_indexed_vertex_array(resources.mesh.vertex_array, cache);
}

void bind_material_textures(const IndexedDrawResources& resources,
                            IndexedBatchStateCache& cache) noexcept {
    constexpr std::array<GLenum, graphics::material_texture_count> texture_units{
        GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2, GL_TEXTURE3};
    for (std::size_t index = 0; index < resources.textures.size(); ++index) {
        if (!cache.textures_valid || cache.textures[index] != resources.textures[index]) {
            glActiveTexture(texture_units[index]);
            glBindTexture(GL_TEXTURE_2D, resources.textures[index]);
            cache.textures[index] = resources.textures[index];
        }
    }
    cache.textures_valid = true;
}

void submit_indexed_draw(const IndexedDrawResources& resources,
                         const graphics::DrawIndexedDescription& description,
                         IndexedBatchStateCache& cache) noexcept {
    configure_indexed_item_state(resources, description, cache);
    upload_indexed_item_uniforms(resources.pipeline.uniforms, description, resources);
    upload_texture_mapping_uniforms(resources.pipeline.uniforms, description);
    bind_material_textures(resources, cache);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(resources.mesh.index_count), GL_UNSIGNED_INT,
                   nullptr);
}

[[nodiscard]] Result<IndexedDrawResources>
batch_item_resources(const RenderTargetView& target, const PipelineView& pipeline,
                     const graphics::IndexedDrawBatchItem& item) noexcept {
    if (item.mesh == nullptr ||
        item.description.textures.size() != graphics::material_texture_count) {
        return Error{ErrorCode::invalid_argument,
                     "Indexed draw batches require a mesh and four ordered textures per item"};
    }
    Result<MeshView> mesh_result = mesh_view(*item.mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }
    IndexedDrawResources resources{target, pipeline, mesh_result.value()};
    for (std::size_t index = 0; index < resources.textures.size(); ++index) {
        Result<GLuint> texture_result = texture_object(item.description.textures[index]);
        if (!texture_result) {
            return texture_result.error();
        }
        resources.textures[index] = texture_result.value();
    }
    return resources;
}

} // namespace

Result<void> draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                          graphics::StaticMesh& mesh,
                          const graphics::DrawIndexedDescription& description) noexcept {
    Result<IndexedDrawResources> resources_result =
        indexed_draw_resources(target, pipeline, mesh, description);
    if (!resources_result) {
        return resources_result.error();
    }
    const IndexedDrawResources& resources = resources_result.value();
    if (!resources.target.valid || resources.mesh.index_count == 0) {
        return {};
    }

    RenderStateGuard state_guard;
    configure_indexed_pass_state(resources);
    upload_indexed_frame_uniforms(resources.pipeline.uniforms, description);
    upload_indexed_clipping_uniforms(resources.pipeline.uniforms, description);
    bind_material_sampler_uniforms(resources.pipeline.uniforms);
    IndexedBatchStateCache cache;
    submit_indexed_draw(resources, description, cache);

    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::draw_submission_failed,
                     "OpenGL reported an error while submitting an indexed draw"};
    }
    mark_render_target_stale(target);
    return {};
}

Result<void> draw_indexed_batch(graphics::RenderTarget& target,
                                graphics::GraphicsPipeline& pipeline,
                                std::span<const graphics::IndexedDrawBatchItem> items) noexcept {
    Result<RenderTargetView> target_result = render_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    Result<PipelineView> pipeline_result = pipeline_view(pipeline);
    if (!pipeline_result) {
        return pipeline_result.error();
    }
    if (!target_result.value().valid || items.empty()) {
        return {};
    }

    RenderStateGuard state_guard;
    const IndexedDrawResources batch_resources{target_result.value(), pipeline_result.value(), {}};
    configure_indexed_pass_state(batch_resources);
    upload_indexed_frame_uniforms(pipeline_result.value().uniforms, items.front().description);
    upload_indexed_clipping_uniforms(pipeline_result.value().uniforms, items.front().description);
    bind_material_sampler_uniforms(pipeline_result.value().uniforms);
    IndexedBatchStateCache cache;
    for (const graphics::IndexedDrawBatchItem& item : items) {
        Result<IndexedDrawResources> resources =
            batch_item_resources(target_result.value(), pipeline_result.value(), item);
        if (!resources) {
            return resources.error();
        }
        if (resources.value().mesh.index_count != 0) {
            submit_indexed_draw(resources.value(), item.description, cache);
        }
    }
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::draw_submission_failed,
                     "OpenGL reported an error while submitting an indexed draw batch"};
    }
    mark_render_target_stale(target);
    return {};
}

} // namespace elf3d::backend::opengl::device_detail
