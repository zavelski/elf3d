module;

#include <elf3d/clipping.h>
#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
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

constexpr char picking_vertex_shader_source[] = R"glsl(#version 410 core
layout(location = 0) in vec3 a_position;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_world_position;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_position = world_position.xyz;
    gl_Position = u_projection * u_view * world_position;
}
)glsl";

constexpr char picking_fragment_shader_source[] = R"glsl(#version 410 core
in vec3 v_world_position;

uniform uint u_pick_object_id;
uniform uint u_pick_primitive_index;
uniform bool u_clipping_section_plane_enabled;
uniform vec3 u_clipping_section_plane_normal;
uniform float u_clipping_section_plane_offset;
uniform bool u_clipping_retain_positive_half_space;
uniform int u_clipping_box_count;
uniform vec3 u_clipping_box_minimums[3];
uniform vec3 u_clipping_box_maximums[3];

layout(location = 0) out uvec4 pick_ids;

bool clipping_contains_point(vec3 world_position)
{
    const float tolerance = 0.00001;
    if (u_clipping_section_plane_enabled) {
        float signed_distance = dot(u_clipping_section_plane_normal, world_position) +
                                u_clipping_section_plane_offset;
        if (!u_clipping_retain_positive_half_space) {
            signed_distance = -signed_distance;
        }
        if (signed_distance < -tolerance) {
            return false;
        }
    }

    if (u_clipping_box_count > 0) {
        bool inside_box = false;
        for (int index = 0; index < 3; ++index) {
            if (index >= u_clipping_box_count) {
                break;
            }
            vec3 minimums = u_clipping_box_minimums[index] - vec3(tolerance);
            vec3 maximums = u_clipping_box_maximums[index] + vec3(tolerance);
            if (all(greaterThanEqual(world_position, minimums)) &&
                all(lessThanEqual(world_position, maximums))) {
                inside_box = true;
            }
        }
        if (!inside_box) {
            return false;
        }
    }
    return true;
}

void main()
{
    if (!clipping_contains_point(v_world_position)) {
        discard;
    }
    pick_ids = uvec4(u_pick_object_id, u_pick_primitive_index, uint(gl_PrimitiveID), 1u);
}
)glsl";

struct PickingDrawViews {
    PickingTargetView target;
    MeshView mesh;
};

struct PickingBatchStateCache {
    bool culling_valid{false};
    bool culling{false};
    bool front_face_valid{false};
    bool front_face_clockwise{false};
    bool vertex_array_valid{false};
    GLuint vertex_array{0};
};

[[nodiscard]] Result<PickingDrawViews> picking_draw_views(graphics::PickingTarget& target,
                                                          graphics::StaticMesh& mesh) noexcept {
    Result<PickingTargetView> target_result = picking_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    Result<MeshView> mesh_result = mesh_view(mesh);
    if (!mesh_result) {
        return mesh_result.error();
    }
    return PickingDrawViews{target_result.value(), mesh_result.value()};
}

void configure_picking_pass_state(const PickingTargetView& target, GLuint program) noexcept {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(target.extent.width),
               static_cast<GLsizei>(target.extent.height));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PRIMITIVE_RESTART);
    glDisable(GL_RASTERIZER_DISCARD);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDepthRange(0.0, 1.0);
    glCullFace(GL_BACK);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(program);
}

void upload_picking_frame_uniforms(const PickingResources& uniforms,
                                   const graphics::PickingDrawDescription& description) noexcept {
    glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, description.view_matrix.data());
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, description.projection_matrix.data());
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

void upload_picking_item_uniforms(const PickingResources& uniforms,
                                  const graphics::PickingDrawDescription& description) noexcept {
    glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, description.model_matrix.data());
    glUniform1ui(uniforms.object_id, description.object_id);
    glUniform1ui(uniforms.primitive_index, description.primitive_index);
}

void set_picking_capability(GLenum capability, bool enabled) noexcept {
    if (enabled) {
        glEnable(capability);
    } else {
        glDisable(capability);
    }
}

void configure_picking_culling(bool culling, PickingBatchStateCache& cache) noexcept {
    if (!cache.culling_valid || cache.culling != culling) {
        set_picking_capability(GL_CULL_FACE, culling);
        cache.culling_valid = true;
        cache.culling = culling;
    }
}

void configure_picking_front_face(bool clockwise, PickingBatchStateCache& cache) noexcept {
    if (!cache.front_face_valid || cache.front_face_clockwise != clockwise) {
        glFrontFace(clockwise ? GL_CW : GL_CCW);
        cache.front_face_valid = true;
        cache.front_face_clockwise = clockwise;
    }
}

void configure_picking_vertex_array(GLuint vertex_array, PickingBatchStateCache& cache) noexcept {
    if (!cache.vertex_array_valid || cache.vertex_array != vertex_array) {
        glBindVertexArray(vertex_array);
        cache.vertex_array_valid = true;
        cache.vertex_array = vertex_array;
    }
}

void configure_picking_item_state(const PickingDrawViews& views,
                                  const graphics::PickingDrawDescription& description,
                                  PickingBatchStateCache& cache) noexcept {
    configure_picking_culling(!description.double_sided, cache);
    configure_picking_front_face(description.front_face_clockwise, cache);
    configure_picking_vertex_array(views.mesh.vertex_array, cache);
}

[[nodiscard]] bool picking_position_in_bounds(Float2 position, Extent2D extent) noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) && position.x >= 0.0F &&
           position.y >= 0.0F && position.x < static_cast<float>(extent.width) &&
           position.y < static_cast<float>(extent.height);
}

[[nodiscard]] Result<void> ensure_picking_resources(PickingResources& resources) {
    if (resources.program != 0) {
        return {};
    }

    AllocationStateGuard allocation_guard;
    Result<GLuint> vertex_result = compile_shader(GL_VERTEX_SHADER, picking_vertex_shader_source);
    if (!vertex_result) {
        return vertex_result.error();
    }
    const GLuint vertex_shader = vertex_result.value();
    Result<GLuint> fragment_result =
        compile_shader(GL_FRAGMENT_SHADER, picking_fragment_shader_source);
    if (!fragment_result) {
        glDeleteShader(vertex_shader);
        return fragment_result.error();
    }
    const GLuint fragment_shader = fragment_result.value();
    Result<GLuint> program_result = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (!program_result) {
        return program_result.error();
    }

    const GLuint program = program_result.value();
    PickingResources candidate{
        program,
        glGetUniformLocation(program, "u_model"),
        glGetUniformLocation(program, "u_view"),
        glGetUniformLocation(program, "u_projection"),
        glGetUniformLocation(program, "u_pick_object_id"),
        glGetUniformLocation(program, "u_pick_primitive_index"),
        glGetUniformLocation(program, "u_clipping_section_plane_enabled"),
        glGetUniformLocation(program, "u_clipping_section_plane_normal"),
        glGetUniformLocation(program, "u_clipping_section_plane_offset"),
        glGetUniformLocation(program, "u_clipping_retain_positive_half_space"),
        glGetUniformLocation(program, "u_clipping_box_count"),
        glGetUniformLocation(program, "u_clipping_box_minimums[0]"),
        glGetUniformLocation(program, "u_clipping_box_maximums[0]")};
    if (!candidate.valid()) {
        glDeleteProgram(program);
        return Error{ErrorCode::shader_linking_failed,
                     "The picking shader is missing a required uniform"};
    }
    resources = candidate;
    return {};
}

[[nodiscard]] bool picking_draw_locations_valid(const PickingResources& resources) noexcept {
    return resources.model >= 0 && resources.view >= 0 && resources.projection >= 0 &&
           resources.object_id >= 0 && resources.primitive_index >= 0;
}

[[nodiscard]] bool picking_clipping_locations_valid(const PickingResources& resources) noexcept {
    return resources.clipping_section_plane_enabled >= 0 &&
           resources.clipping_section_plane_normal >= 0 &&
           resources.clipping_section_plane_offset >= 0 &&
           resources.clipping_retain_positive_half_space >= 0 &&
           resources.clipping_box_count >= 0 && resources.clipping_box_minimums >= 0 &&
           resources.clipping_box_maximums >= 0;
}

void submit_picking_draw(const PickingResources& resources, const PickingDrawViews& views,
                         const graphics::PickingDrawDescription& description,
                         PickingBatchStateCache& cache) noexcept {
    configure_picking_item_state(views, description, cache);
    upload_picking_item_uniforms(resources, description);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(views.mesh.index_count), GL_UNSIGNED_INT,
                   nullptr);
}

[[nodiscard]] Result<void>
submit_picking_batch_items(const PickingResources& resources, const PickingTargetView& target,
                           std::span<const graphics::PickingDrawBatchItem> items,
                           PickingBatchStateCache& cache) noexcept {
    for (const graphics::PickingDrawBatchItem& item : items) {
        if (item.mesh == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Picking draw batches require a mesh for every item"};
        }
        Result<MeshView> mesh_result = mesh_view(*item.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const PickingDrawViews views{target, mesh_result.value()};
        if (views.mesh.index_count != 0 && item.description.object_id != 0) {
            submit_picking_draw(resources, views, item.description, cache);
        }
    }
    return {};
}

} // namespace

bool PickingResources::valid() const noexcept {
    return program != 0 && picking_draw_locations_valid(*this) &&
           picking_clipping_locations_valid(*this);
}

Result<void> draw_picking_indexed(PickingResources& resources, graphics::PickingTarget& target,
                                  graphics::StaticMesh& mesh,
                                  const graphics::PickingDrawDescription& description) noexcept {
    Result<PickingDrawViews> views_result = picking_draw_views(target, mesh);
    if (!views_result) {
        return views_result.error();
    }
    const PickingDrawViews& views = views_result.value();
    if (!views.target.valid || views.mesh.index_count == 0 || description.object_id == 0) {
        return {};
    }
    const Result<void> resource_result = ensure_picking_resources(resources);
    if (!resource_result) {
        return resource_result.error();
    }

    RenderStateGuard state_guard;
    configure_picking_pass_state(views.target, resources.program);
    upload_picking_frame_uniforms(resources, description);
    PickingBatchStateCache cache;
    submit_picking_draw(resources, views, description, cache);
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::draw_submission_failed,
                     "OpenGL reported an error while submitting a picking draw"};
    }
    return {};
}

Result<void> draw_picking_batch(PickingResources& resources, graphics::PickingTarget& target,
                                std::span<const graphics::PickingDrawBatchItem> items) noexcept {
    Result<PickingTargetView> target_result = picking_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    if (!target_result.value().valid || items.empty()) {
        return {};
    }
    const Result<void> resource_result = ensure_picking_resources(resources);
    if (!resource_result) {
        return resource_result.error();
    }

    RenderStateGuard state_guard;
    configure_picking_pass_state(target_result.value(), resources.program);
    upload_picking_frame_uniforms(resources, items.front().description);
    PickingBatchStateCache cache;
    const Result<void> submission =
        submit_picking_batch_items(resources, target_result.value(), items, cache);
    if (!submission) {
        return submission.error();
    }
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::draw_submission_failed,
                     "OpenGL reported an error while submitting a picking draw batch"};
    }
    return {};
}

Result<std::optional<PickingReadback>> read_picking_pixel(graphics::PickingTarget& target,
                                                          Float2 position_pixels) noexcept {
    Result<PickingTargetView> target_result = picking_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    const PickingTargetView& view = target_result.value();
    if (!view.valid || !picking_position_in_bounds(position_pixels, view.extent)) {
        return std::optional<PickingReadback>{};
    }

    RenderStateGuard state_guard;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, view.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLint read_x = static_cast<GLint>(std::floor(position_pixels.x));
    const GLint read_y =
        static_cast<GLint>(static_cast<std::int64_t>(view.extent.height) - 1 -
                           static_cast<std::int64_t>(std::floor(position_pixels.y)));
    std::array<GLuint, 4> ids{};
    GLfloat depth = 1.0F;
    glReadPixels(read_x, read_y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, ids.data());
    glReadPixels(read_x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::texture_unavailable,
                     "OpenGL reported an error while reading the picking pixel"};
    }
    if (ids[0] == 0U || ids[3] == 0U || depth >= 1.0F) {
        return std::optional<PickingReadback>{};
    }
    return std::optional<PickingReadback>{PickingReadback{ids[0], ids[1], ids[2], depth}};
}

Result<std::vector<float>> read_picking_depths(graphics::PickingTarget& target) noexcept {
    Result<PickingTargetView> target_result = picking_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    const PickingTargetView& view = target_result.value();
    if (!view.valid) {
        return std::vector<float>{};
    }

    const std::size_t width = static_cast<std::size_t>(view.extent.width);
    const std::size_t height = static_cast<std::size_t>(view.extent.height);
    if (width == 0U || height == 0U) {
        return std::vector<float>{};
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "Picking depth readback dimensions exceed addressable storage"};
    }

    try {
        std::vector<float> depths(width * height);
        RenderStateGuard state_guard;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, view.framebuffer);
        glReadPixels(0, 0, static_cast<GLsizei>(view.extent.width),
                     static_cast<GLsizei>(view.extent.height), GL_DEPTH_COMPONENT, GL_FLOAT,
                     depths.data());
        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::texture_unavailable,
                         "OpenGL reported an error while reading picking depths"};
        }
        return depths;
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

void release_picking_resources(PickingResources& resources) noexcept {
    if (resources.program != 0) {
        glDeleteProgram(resources.program);
    }
    resources = {};
}

} // namespace elf3d::backend::opengl::device_detail
