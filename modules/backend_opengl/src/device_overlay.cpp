module;

#include <elf3d/graphics.h>
#include <elf3d/measurement.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl::device_detail {
namespace {

constexpr char overlay_vertex_shader_source[] = R"glsl(#version 410 core
layout(location = 0) in vec3 a_position_ndc;

void main()
{
    gl_Position = vec4(a_position_ndc, 1.0);
}
)glsl";

constexpr char overlay_fragment_shader_source[] = R"glsl(#version 410 core
uniform vec4 u_color;

layout(location = 0) out vec4 fragment_color;

void main()
{
    fragment_color = u_color;
}
)glsl";

struct OverlayVertex {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct OverlayProjectedPoint {
    float ndc_x = 0.0F;
    float ndc_y = 0.0F;
    float ndc_z = 0.0F;
    float screen_x = 0.0F;
    float screen_y = 0.0F;
};

[[nodiscard]] std::array<float, 4>
multiply_matrix_vector(const std::array<float, 16>& matrix,
                       const std::array<float, 4>& vector) noexcept {
    std::array<float, 4> result{};
    for (int row = 0; row < 4; ++row) {
        result[static_cast<std::size_t>(row)] =
            matrix[static_cast<std::size_t>(row)] * vector[0] +
            matrix[4 + static_cast<std::size_t>(row)] * vector[1] +
            matrix[8 + static_cast<std::size_t>(row)] * vector[2] +
            matrix[12 + static_cast<std::size_t>(row)] * vector[3];
    }
    return result;
}

[[nodiscard]] bool valid_clip_position(const std::array<float, 4>& clip) noexcept {
    return std::isfinite(clip[0]) && std::isfinite(clip[1]) && std::isfinite(clip[2]) &&
           std::isfinite(clip[3]) && clip[3] > 0.000001F;
}

[[nodiscard]] bool valid_ndc_position(float x, float y, float z) noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

[[nodiscard]] std::optional<OverlayProjectedPoint>
project_overlay_point(const graphics::DrawOverlayDescription& description, Extent2D extent,
                      Float3 world_position) noexcept {
    if (extent.width == 0 || extent.height == 0) {
        return std::nullopt;
    }
    const std::array<float, 4> world{world_position.x, world_position.y, world_position.z, 1.0F};
    const std::array<float, 4> view = multiply_matrix_vector(description.view_matrix, world);
    const std::array<float, 4> clip = multiply_matrix_vector(description.projection_matrix, view);
    if (!valid_clip_position(clip)) {
        return std::nullopt;
    }
    const float inverse_w = 1.0F / clip[3];
    const float ndc_x = clip[0] * inverse_w;
    const float ndc_y = clip[1] * inverse_w;
    const float ndc_z = clip[2] * inverse_w;
    if (!valid_ndc_position(ndc_x, ndc_y, ndc_z)) {
        return std::nullopt;
    }
    const float width = static_cast<float>(extent.width);
    const float height = static_cast<float>(extent.height);
    return OverlayProjectedPoint{ndc_x, ndc_y, ndc_z, (ndc_x * 0.5F + 0.5F) * width,
                                 (1.0F - (ndc_y * 0.5F + 0.5F)) * height};
}

[[nodiscard]] OverlayVertex vertex(float x, float y, float z) noexcept {
    return OverlayVertex{x, y, z};
}

[[nodiscard]] std::array<OverlayVertex, 6> quad(OverlayVertex a, OverlayVertex b, OverlayVertex c,
                                                OverlayVertex d) noexcept {
    return std::array<OverlayVertex, 6>{a, b, c, a, c, d};
}

[[nodiscard]] std::optional<std::array<OverlayVertex, 6>>
line_vertices(const OverlayProjectedPoint& start, const OverlayProjectedPoint& end, Extent2D extent,
              float thickness_pixels) noexcept {
    if (!std::isfinite(thickness_pixels) || thickness_pixels <= 0.0F || extent.width == 0 ||
        extent.height == 0) {
        return std::nullopt;
    }
    const float dx = end.screen_x - start.screen_x;
    const float dy = end.screen_y - start.screen_y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(length) || length <= 0.001F) {
        return std::nullopt;
    }
    const float half_thickness = thickness_pixels * 0.5F;
    const float normal_x = -dy / length * half_thickness;
    const float normal_y = dx / length * half_thickness;
    const float ndc_offset_x = normal_x * 2.0F / static_cast<float>(extent.width);
    const float ndc_offset_y = -normal_y * 2.0F / static_cast<float>(extent.height);
    return quad(vertex(start.ndc_x + ndc_offset_x, start.ndc_y + ndc_offset_y, start.ndc_z),
                vertex(start.ndc_x - ndc_offset_x, start.ndc_y - ndc_offset_y, start.ndc_z),
                vertex(end.ndc_x - ndc_offset_x, end.ndc_y - ndc_offset_y, end.ndc_z),
                vertex(end.ndc_x + ndc_offset_x, end.ndc_y + ndc_offset_y, end.ndc_z));
}

[[nodiscard]] std::optional<std::array<OverlayVertex, 6>>
marker_vertices(const OverlayProjectedPoint& center, Extent2D extent,
                float radius_pixels) noexcept {
    if (!std::isfinite(radius_pixels) || radius_pixels <= 0.0F || extent.width == 0 ||
        extent.height == 0) {
        return std::nullopt;
    }
    const float ndc_radius_x = radius_pixels * 2.0F / static_cast<float>(extent.width);
    const float ndc_radius_y = radius_pixels * 2.0F / static_cast<float>(extent.height);
    return quad(vertex(center.ndc_x - ndc_radius_x, center.ndc_y + ndc_radius_y, center.ndc_z),
                vertex(center.ndc_x - ndc_radius_x, center.ndc_y - ndc_radius_y, center.ndc_z),
                vertex(center.ndc_x + ndc_radius_x, center.ndc_y - ndc_radius_y, center.ndc_z),
                vertex(center.ndc_x + ndc_radius_x, center.ndc_y + ndc_radius_y, center.ndc_z));
}

[[nodiscard]] Color4 sanitized_overlay_color(Color4 color) noexcept {
    const auto channel = [](float value, float fallback) noexcept {
        return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
    };
    return Color4{channel(color.red, 1.0F), channel(color.green, 1.0F), channel(color.blue, 1.0F),
                  channel(color.alpha, 1.0F)};
}

[[nodiscard]] bool
overlay_has_geometry(Extent2D extent,
                     const graphics::DrawOverlayDescription& description) noexcept {
    return extent.width != 0 && extent.height != 0 &&
           (!description.lines.empty() || !description.markers.empty());
}

void configure_overlay_draw_state(const RenderTargetView& target,
                                  const OverlayResources& resources) noexcept {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(target.extent.width),
               static_cast<GLsizei>(target.extent.height));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_CLAMP);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PRIMITIVE_RESTART);
    glDisable(GL_RASTERIZER_DISCARD);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(resources.program);
    glBindVertexArray(resources.vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vertex_buffer);
}

[[nodiscard]] Result<void> ensure_overlay_resources(OverlayResources& resources) {
    if (resources.program != 0 && resources.vertex_array != 0 && resources.vertex_buffer != 0) {
        return {};
    }

    AllocationStateGuard allocation_guard;
    Result<GLuint> program_result =
        create_program_from_sources(overlay_vertex_shader_source, overlay_fragment_shader_source);
    if (!program_result) {
        return program_result.error();
    }

    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    if (vertex_array == 0 || vertex_buffer == 0) {
        glDeleteProgram(program_result.value());
        glDeleteVertexArrays(1, &vertex_array);
        glDeleteBuffers(1, &vertex_buffer);
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "OpenGL failed to allocate overlay vertex objects"};
    }

    const GLuint program = program_result.value();
    const GLint color_uniform = glGetUniformLocation(program, "u_color");
    if (color_uniform < 0) {
        glDeleteProgram(program);
        glDeleteVertexArrays(1, &vertex_array);
        glDeleteBuffers(1, &vertex_buffer);
        return Error{ErrorCode::shader_linking_failed,
                     "The overlay shader is missing a required uniform"};
    }

    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), nullptr);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteProgram(program);
        glDeleteVertexArrays(1, &vertex_array);
        glDeleteBuffers(1, &vertex_buffer);
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "OpenGL reported an error while creating overlay resources"};
    }

    resources = OverlayResources{program, vertex_array, vertex_buffer, color_uniform};
    return {};
}

void submit_overlay_vertices(const OverlayResources& resources,
                             const std::array<OverlayVertex, 6>& vertices, Color4 color,
                             OverlayDepthMode depth_mode) noexcept {
    if (depth_mode == OverlayDepthMode::depth_tested) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    const Color4 sanitized = sanitized_overlay_color(color);
    glUniform4f(resources.color_uniform, sanitized.red, sanitized.green, sanitized.blue,
                sanitized.alpha);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(OverlayVertex) * vertices.size()),
                 vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
}

[[nodiscard]] Result<void> submit_overlay_lines(const OverlayResources& resources,
                                                const graphics::DrawOverlayDescription& description,
                                                Extent2D extent) {
    for (const OverlayLineSegment& line : description.lines) {
        const std::optional<OverlayProjectedPoint> start =
            project_overlay_point(description, extent, line.start_world);
        const std::optional<OverlayProjectedPoint> end =
            project_overlay_point(description, extent, line.end_world);
        if (!start.has_value() || !end.has_value()) {
            continue;
        }
        const std::optional<std::array<OverlayVertex, 6>> vertices =
            line_vertices(*start, *end, extent, line.thickness_pixels);
        if (vertices.has_value()) {
            submit_overlay_vertices(resources, *vertices, line.color, line.depth_mode);
        }
    }
    return {};
}

[[nodiscard]] Result<void>
submit_overlay_markers(const OverlayResources& resources,
                       const graphics::DrawOverlayDescription& description, Extent2D extent) {
    for (const OverlayPointMarker& marker : description.markers) {
        const std::optional<OverlayProjectedPoint> center =
            project_overlay_point(description, extent, marker.position_world);
        if (!center.has_value()) {
            continue;
        }
        const std::optional<std::array<OverlayVertex, 6>> vertices =
            marker_vertices(*center, extent, marker.radius_pixels);
        if (vertices.has_value()) {
            submit_overlay_vertices(resources, *vertices, marker.color, marker.depth_mode);
        }
    }
    return {};
}

} // namespace

Result<void> draw_overlay(OverlayResources& resources, graphics::RenderTarget& target,
                          const graphics::DrawOverlayDescription& description) noexcept {
    Result<RenderTargetView> target_result = render_target_view(target);
    if (!target_result) {
        return target_result.error();
    }
    const RenderTargetView& target_view = target_result.value();
    if (!target_view.valid || !overlay_has_geometry(target_view.extent, description)) {
        return {};
    }
    const Result<void> resource_result = ensure_overlay_resources(resources);
    if (!resource_result) {
        return resource_result.error();
    }

    RenderStateGuard state_guard;
    configure_overlay_draw_state(target_view, resources);
    const Result<void> lines = submit_overlay_lines(resources, description, target_view.extent);
    if (!lines) {
        return lines.error();
    }
    const Result<void> markers = submit_overlay_markers(resources, description, target_view.extent);
    if (!markers) {
        return markers.error();
    }
    if (glGetError() != GL_NO_ERROR) {
        return Error{ErrorCode::draw_submission_failed,
                     "OpenGL reported an error while submitting overlay geometry"};
    }
    mark_render_target_stale(target);
    return {};
}

void release_overlay_resources(OverlayResources& resources) noexcept {
    if (resources.vertex_buffer != 0) {
        glDeleteBuffers(1, &resources.vertex_buffer);
    }
    if (resources.vertex_array != 0) {
        glDeleteVertexArrays(1, &resources.vertex_array);
    }
    if (resources.program != 0) {
        glDeleteProgram(resources.program);
    }
    resources = {};
}

} // namespace elf3d::backend::opengl::device_detail
