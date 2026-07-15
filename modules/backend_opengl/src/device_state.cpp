module;

#include <elf3d/core/assert.h>
#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl::device_detail {
namespace {

constinit const int opengl_resource_token_anchor = 0;

[[nodiscard]] std::string shader_log(GLuint object, bool program) {
    GLint length = 0;
    if (program) {
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
    } else {
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
    }
    if (length <= 1) {
        return {};
    }

    std::vector<char> buffer(static_cast<std::size_t>(length));
    GLsizei written = 0;
    if (program) {
        glGetProgramInfoLog(object, length, &written, buffer.data());
    } else {
        glGetShaderInfoLog(object, length, &written, buffer.data());
    }
    return std::string(buffer.data(), static_cast<std::size_t>(std::max<GLsizei>(written, 0)));
}

} // namespace

[[noreturn]] void fatal_opengl_allocation_failure() noexcept {
    fatal_error("Elf3D OpenGL backend memory allocation failed");
}

[[noreturn]] void fatal_unexpected_opengl_boundary_exception() noexcept {
    fatal_error("Elf3D OpenGL backend encountered an unexpected exception");
}

std::uintptr_t opengl_resource_token() noexcept {
    return reinterpret_cast<std::uintptr_t>(&opengl_resource_token_anchor);
}

bool is_opengl_texture(const graphics::Texture2D* texture) noexcept {
    return texture == nullptr || texture->backend_resource_token() == opengl_resource_token();
}

OpenGLDeviceState::OpenGLDeviceState(GLint maximum_texture_size) noexcept
    : owner_thread_(std::this_thread::get_id()), maximum_texture_size_(maximum_texture_size) {}

Result<void> OpenGLDeviceState::validate_operation() const noexcept {
    if (!operational_) {
        return Error{ErrorCode::graphics_shutdown, "The OpenGL backend has shut down"};
    }
    if (std::this_thread::get_id() != owner_thread_) {
        return Error{ErrorCode::graphics_thread_violation, thread_failure_message};
    }
    if (glGetString(GL_VERSION) == nullptr) {
        return Error{ErrorCode::graphics_context_unavailable, context_failure_message};
    }
    return {};
}

bool OpenGLDeviceState::can_destroy_objects() const noexcept {
    return operational_ && std::this_thread::get_id() == owner_thread_ &&
           glGetString(GL_VERSION) != nullptr;
}

bool OpenGLDeviceState::supports(Extent2D extent) const noexcept {
    const auto maximum = static_cast<std::uint32_t>(maximum_texture_size_);
    return extent.width <= maximum && extent.height <= maximum &&
           extent.width <= static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) &&
           extent.height <= static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max());
}

Result<TextureHandle> OpenGLDeviceState::register_texture(GLuint texture, Extent2D extent,
                                                          ColorTextureResolver* resolver) {
    try {
        std::uint64_t candidate = next_texture_handle_++;
        if (candidate == 0) {
            candidate = next_texture_handle_++;
        }
        texture_records_.emplace(candidate, TextureRecord{texture, extent, resolver});
        return detail::TextureHandleAccess::create(candidate);
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

void OpenGLDeviceState::unregister_texture(TextureHandle handle) noexcept {
    texture_records_.erase(detail::TextureHandleAccess::value(handle));
}

Result<NativeTextureView> OpenGLDeviceState::native_texture_view(TextureHandle handle) const {
    const Result<void> validation = validate_operation();
    if (!validation) {
        return validation.error();
    }
    if (!handle.is_valid()) {
        return Error{ErrorCode::texture_unavailable, "The texture handle is invalid"};
    }

    const auto record = texture_records_.find(detail::TextureHandleAccess::value(handle));
    if (record == texture_records_.end()) {
        return Error{ErrorCode::texture_unavailable,
                     "The texture handle is stale or does not belong to this device"};
    }
    if (record->second.resolver != nullptr) {
        const Result<void> resolve_result = record->second.resolver->resolve_color_texture();
        if (!resolve_result) {
            return resolve_result.error();
        }
    }

    return NativeTextureView{NativeGraphicsApi::opengl,
                             static_cast<std::uintptr_t>(record->second.texture),
                             record->second.extent};
}

void OpenGLDeviceState::shut_down() noexcept {
    operational_ = false;
}

AllocationStateGuard::AllocationStateGuard() noexcept {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_2d_);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pixel_unpack_buffer_);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment_);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array_);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer_);
}

AllocationStateGuard::~AllocationStateGuard() {
    glActiveTexture(static_cast<GLenum>(active_texture_));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_2d_));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(pixel_unpack_buffer_));
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment_);
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(array_buffer_));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
}

RenderStateGuard::RenderStateGuard() noexcept {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
    glGetIntegerv(GL_VIEWPORT, viewport_);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear_color_);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &depth_clear_value_);
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask_);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask_);
    scissor_enabled_ = glIsEnabled(GL_SCISSOR_TEST);
    framebuffer_srgb_enabled_ = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    depth_test_enabled_ = glIsEnabled(GL_DEPTH_TEST);
    cull_face_enabled_ = glIsEnabled(GL_CULL_FACE);
    blend_enabled_ = glIsEnabled(GL_BLEND);
    stencil_test_enabled_ = glIsEnabled(GL_STENCIL_TEST);
    depth_clamp_enabled_ = glIsEnabled(GL_DEPTH_CLAMP);
    polygon_offset_fill_enabled_ = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    primitive_restart_enabled_ = glIsEnabled(GL_PRIMITIVE_RESTART);
    rasterizer_discard_enabled_ = glIsEnabled(GL_RASTERIZER_DISCARD);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array_);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer_);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_);
    for (std::size_t index = 0; index < texture_units_.size(); ++index) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_units_[index]);
    }
    glActiveTexture(static_cast<GLenum>(active_texture_));
    glGetIntegerv(GL_DEPTH_FUNC, &depth_function_);
    glGetIntegerv(GL_BLEND_SRC_RGB, &blend_source_rgb_);
    glGetIntegerv(GL_BLEND_DST_RGB, &blend_destination_rgb_);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_source_alpha_);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_destination_alpha_);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_equation_rgb_);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_equation_alpha_);
    glGetIntegerv(GL_CULL_FACE_MODE, &cull_face_mode_);
    glGetIntegerv(GL_FRONT_FACE, &front_face_);
    glGetIntegerv(GL_POLYGON_MODE, polygon_mode_);
    glGetDoublev(GL_DEPTH_RANGE, depth_range_);
}

RenderStateGuard::~RenderStateGuard() {
    set_enabled(GL_SCISSOR_TEST, scissor_enabled_);
    set_enabled(GL_FRAMEBUFFER_SRGB, framebuffer_srgb_enabled_);
    set_enabled(GL_DEPTH_TEST, depth_test_enabled_);
    set_enabled(GL_CULL_FACE, cull_face_enabled_);
    set_enabled(GL_BLEND, blend_enabled_);
    set_enabled(GL_STENCIL_TEST, stencil_test_enabled_);
    set_enabled(GL_DEPTH_CLAMP, depth_clamp_enabled_);
    set_enabled(GL_POLYGON_OFFSET_FILL, polygon_offset_fill_enabled_);
    set_enabled(GL_PRIMITIVE_RESTART, primitive_restart_enabled_);
    set_enabled(GL_RASTERIZER_DISCARD, rasterizer_discard_enabled_);
    glColorMask(color_mask_[0], color_mask_[1], color_mask_[2], color_mask_[3]);
    glDepthMask(depth_mask_);
    glDepthFunc(static_cast<GLenum>(depth_function_));
    glBlendFuncSeparate(
        static_cast<GLenum>(blend_source_rgb_), static_cast<GLenum>(blend_destination_rgb_),
        static_cast<GLenum>(blend_source_alpha_), static_cast<GLenum>(blend_destination_alpha_));
    glBlendEquationSeparate(static_cast<GLenum>(blend_equation_rgb_),
                            static_cast<GLenum>(blend_equation_alpha_));
    glCullFace(static_cast<GLenum>(cull_face_mode_));
    glFrontFace(static_cast<GLenum>(front_face_));
    glPolygonMode(GL_FRONT, static_cast<GLenum>(polygon_mode_[0]));
    glPolygonMode(GL_BACK, static_cast<GLenum>(polygon_mode_[1]));
    glDepthRange(depth_range_[0], depth_range_[1]);
    glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
    glClearDepth(depth_clear_value_);
    glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
    glUseProgram(static_cast<GLuint>(program_));
    glBindVertexArray(static_cast<GLuint>(vertex_array_));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(array_buffer_));
    for (std::size_t index = 0; index < texture_units_.size(); ++index) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_units_[index]));
    }
    glActiveTexture(static_cast<GLenum>(active_texture_));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
}

void RenderStateGuard::set_enabled(GLenum capability, GLboolean enabled) noexcept {
    if (enabled == GL_TRUE) {
        glEnable(capability);
    } else {
        glDisable(capability);
    }
}

Result<GLuint> compile_shader(GLenum type, std::string_view source) {
    if (source.empty() ||
        source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
        return Error{ErrorCode::shader_compilation_failed,
                     "Shader source is empty or exceeds the OpenGL source-length limit"};
    }

    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        return Error{ErrorCode::shader_compilation_failed,
                     "OpenGL failed to create a shader object"};
    }
    const GLchar* source_pointer = source.data();
    const GLint source_length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &source_pointer, &source_length);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        const std::string diagnostic = shader_log(shader, false);
        glDeleteShader(shader);
        return Error{ErrorCode::shader_compilation_failed,
                     "OpenGL shader compilation failed: " + diagnostic};
    }
    return shader;
}

Result<GLuint> link_program(GLuint vertex_shader, GLuint fragment_shader) {
    const GLuint program = glCreateProgram();
    if (program == 0) {
        return Error{ErrorCode::shader_linking_failed, "OpenGL failed to create a shader program"};
    }
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        const std::string diagnostic = shader_log(program, true);
        glDeleteProgram(program);
        return Error{ErrorCode::shader_linking_failed,
                     "OpenGL shader program linking failed: " + diagnostic};
    }
    return program;
}

Result<GLuint> create_program_from_sources(std::string_view vertex_source,
                                           std::string_view fragment_source) {
    Result<GLuint> vertex_result = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex_result) {
        return vertex_result.error();
    }
    const GLuint vertex_shader = vertex_result.value();
    Result<GLuint> fragment_result = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment_result) {
        glDeleteShader(vertex_shader);
        return fragment_result.error();
    }
    const GLuint fragment_shader = fragment_result.value();
    Result<GLuint> program_result = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program_result;
}

} // namespace elf3d::backend::opengl::device_detail
