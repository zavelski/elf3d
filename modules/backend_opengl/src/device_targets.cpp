module;

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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

constexpr char display_resolve_vertex_shader_source[] = R"glsl(#version 410 core
out vec2 v_texcoord;

void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    const vec2 texcoords[3] = vec2[3](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_texcoord = texcoords[gl_VertexID];
}
)glsl";

constexpr char display_resolve_fragment_shader_source[] = R"glsl(#version 410 core
in vec2 v_texcoord;

uniform sampler2D u_linear_color_texture;
uniform float u_exposure_ev;
uniform int u_tone_mapping;

layout(location = 0) out vec4 fragment_color;

vec3 linear_to_srgb(vec3 linear_color)
{
    linear_color = max(linear_color, vec3(0.0));
    return mix(12.92 * linear_color,
               1.055 * pow(linear_color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), linear_color));
}

float sanitize_component(float value)
{
    return isnan(value) || isinf(value) || value < 0.0 ? 0.0 : value;
}

vec3 pbr_neutral_tone_mapping(vec3 color)
{
    const float start_compression = 0.76;
    const float desaturation = 0.15;
    float darkest = min(color.r, min(color.g, color.b));
    float offset = darkest < 0.08 ? darkest - 6.25 * darkest * darkest : 0.04;
    color -= vec3(offset);
    float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression) {
        return color;
    }
    float distance_to_white = 1.0 - start_compression;
    float compressed_peak =
        1.0 - distance_to_white * distance_to_white /
                  (peak + distance_to_white - start_compression);
    color *= compressed_peak / peak;
    float desaturation_weight =
        1.0 - 1.0 / (desaturation * (peak - compressed_peak) + 1.0);
    return mix(color, vec3(compressed_peak), desaturation_weight);
}

vec3 standard_tone_mapping(vec3 color)
{
    const float calibration = 1.590579;
    return vec3(1.0) - exp2(-calibration * color);
}

void main()
{
    vec4 linear_color = texture(u_linear_color_texture, v_texcoord);
    vec3 sanitized = vec3(sanitize_component(linear_color.r),
                          sanitize_component(linear_color.g),
                          sanitize_component(linear_color.b));
    vec3 exposed = sanitized * exp2(u_exposure_ev);
    vec3 display_linear = max(exposed, vec3(0.0));
    if (u_tone_mapping == 1) {
        display_linear = pbr_neutral_tone_mapping(exposed);
    } else if (u_tone_mapping == 2) {
        display_linear = standard_tone_mapping(exposed);
    }
    fragment_color = vec4(linear_to_srgb(display_linear), clamp(linear_color.a, 0.0, 1.0));
}
)glsl";

struct DisplayResolveResources final {
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLint texture_uniform = -1;
    GLint exposure_uniform = -1;
    GLint tone_mapping_uniform = -1;

    [[nodiscard]] bool valid() const noexcept {
        return program != 0 && vertex_array != 0 && texture_uniform >= 0 && exposure_uniform >= 0 &&
               tone_mapping_uniform >= 0;
    }
};

[[nodiscard]] Result<GLuint> create_display_resolve_program() {
    Result<GLuint> vertex_result =
        compile_shader(GL_VERTEX_SHADER, display_resolve_vertex_shader_source);
    if (!vertex_result) {
        return vertex_result.error();
    }
    const GLuint vertex_shader = vertex_result.value();
    Result<GLuint> fragment_result =
        compile_shader(GL_FRAGMENT_SHADER, display_resolve_fragment_shader_source);
    if (!fragment_result) {
        glDeleteShader(vertex_shader);
        return fragment_result.error();
    }
    Result<GLuint> program_result = link_program(vertex_shader, fragment_result.value());
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_result.value());
    return program_result;
}

[[nodiscard]] Result<DisplayResolveResources> create_display_resolve_resources() {
    AllocationStateGuard allocation_guard;
    Result<GLuint> program_result = create_display_resolve_program();
    if (!program_result) {
        return program_result.error();
    }
    DisplayResolveResources resources;
    resources.program = program_result.value();
    glGenVertexArrays(1, &resources.vertex_array);
    if (resources.vertex_array == 0) {
        glDeleteProgram(resources.program);
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "OpenGL failed to allocate viewport display resolve resources"};
    }
    resources.texture_uniform = glGetUniformLocation(resources.program, "u_linear_color_texture");
    resources.exposure_uniform = glGetUniformLocation(resources.program, "u_exposure_ev");
    resources.tone_mapping_uniform = glGetUniformLocation(resources.program, "u_tone_mapping");
    if (!resources.valid()) {
        glDeleteProgram(resources.program);
        glDeleteVertexArrays(1, &resources.vertex_array);
        return Error{ErrorCode::shader_linking_failed,
                     "The viewport display resolve shader is missing a required uniform"};
    }
    return resources;
}

void configure_framebuffer_clear_state(GLuint framebuffer, Extent2D extent) noexcept {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, static_cast<GLsizei>(extent.width), static_cast<GLsizei>(extent.height));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
}

void delete_render_target_objects(GLuint framebuffer, GLuint linear_color_texture,
                                  GLuint display_framebuffer, GLuint display_texture,
                                  GLuint depth_renderbuffer) noexcept {
    if (depth_renderbuffer != 0) {
        glDeleteRenderbuffers(1, &depth_renderbuffer);
    }
    if (display_texture != 0) {
        glDeleteTextures(1, &display_texture);
    }
    if (linear_color_texture != 0) {
        glDeleteTextures(1, &linear_color_texture);
    }
    if (display_framebuffer != 0) {
        glDeleteFramebuffers(1, &display_framebuffer);
    }
    if (framebuffer != 0) {
        glDeleteFramebuffers(1, &framebuffer);
    }
}

struct RenderTargetObjects {
    GLuint framebuffer = 0;
    GLuint linear_color_texture = 0;
    GLuint display_framebuffer = 0;
    GLuint display_texture = 0;
    GLuint depth_renderbuffer = 0;

    [[nodiscard]] bool valid() const noexcept {
        return framebuffer != 0 && linear_color_texture != 0 && display_framebuffer != 0 &&
               display_texture != 0 && depth_renderbuffer != 0;
    }
};

void delete_render_target_objects(const RenderTargetObjects& objects) noexcept {
    delete_render_target_objects(objects.framebuffer, objects.linear_color_texture,
                                 objects.display_framebuffer, objects.display_texture,
                                 objects.depth_renderbuffer);
}

void configure_render_target_textures(const RenderTargetObjects& objects,
                                      Extent2D extent) noexcept {
    glBindTexture(GL_TEXTURE_2D, objects.linear_color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(extent.width),
                 static_cast<GLsizei>(extent.height), 0, GL_RGBA, GL_HALF_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, objects.display_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(extent.width),
                 static_cast<GLsizei>(extent.height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glBindRenderbuffer(GL_RENDERBUFFER, objects.depth_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(extent.width),
                          static_cast<GLsizei>(extent.height));
}

[[nodiscard]] bool configure_render_framebuffer(const RenderTargetObjects& objects) noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, objects.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           objects.linear_color_texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              objects.depth_renderbuffer);
    constexpr GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &draw_buffer);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

[[nodiscard]] bool configure_display_framebuffer(const RenderTargetObjects& objects) noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, objects.display_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           objects.display_texture, 0);
    constexpr GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &draw_buffer);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

[[nodiscard]] Result<RenderTargetObjects> create_render_target_objects(Extent2D extent) noexcept {
    AllocationStateGuard state_guard;
    RenderTargetObjects objects;
    glGenFramebuffers(1, &objects.framebuffer);
    glGenFramebuffers(1, &objects.display_framebuffer);
    std::array<GLuint, 2> color_textures{};
    glGenTextures(static_cast<GLsizei>(color_textures.size()), color_textures.data());
    objects.linear_color_texture = color_textures[0];
    objects.display_texture = color_textures[1];
    glGenRenderbuffers(1, &objects.depth_renderbuffer);
    if (!objects.valid()) {
        delete_render_target_objects(objects);
        return Error{ErrorCode::framebuffer_creation_failed,
                     "OpenGL failed to allocate viewport framebuffer objects"};
    }

    configure_render_target_textures(objects, extent);
    if (!configure_render_framebuffer(objects)) {
        delete_render_target_objects(objects);
        return Error{ErrorCode::framebuffer_incomplete,
                     "The OpenGL viewport framebuffer is incomplete"};
    }
    if (!configure_display_framebuffer(objects)) {
        delete_render_target_objects(objects);
        return Error{ErrorCode::framebuffer_incomplete,
                     "The OpenGL viewport display framebuffer is incomplete"};
    }
    return objects;
}

void delete_picking_objects(GLuint framebuffer, GLuint id_texture,
                            GLuint depth_renderbuffer) noexcept {
    if (depth_renderbuffer != 0) {
        glDeleteRenderbuffers(1, &depth_renderbuffer);
    }
    if (id_texture != 0) {
        glDeleteTextures(1, &id_texture);
    }
    if (framebuffer != 0) {
        glDeleteFramebuffers(1, &framebuffer);
    }
}

class OpenGLRenderTarget final : public graphics::RenderTarget, public ColorTextureResolver {
  public:
    explicit OpenGLRenderTarget(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLRenderTarget() override {
        release();
        release_resolve_resources();
    }

    [[nodiscard]] Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] Result<void> resize(Extent2D extent) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (extent == extent_) {
            return {};
        }
        if (extent.width == 0 || extent.height == 0) {
            release();
            release_resolve_resources();
            extent_ = extent;
            return {};
        }
        if (!state_->supports(extent)) {
            return Error{ErrorCode::invalid_viewport_dimensions,
                         "Viewport dimensions exceed the OpenGL texture-size limit"};
        }

        Result<RenderTargetObjects> objects_result = create_render_target_objects(extent);
        if (!objects_result) {
            return objects_result.error();
        }
        const RenderTargetObjects objects = objects_result.value();

        Result<TextureHandle> handle_result =
            state_->register_texture(objects.display_texture, extent, this);
        if (!handle_result) {
            delete_render_target_objects(objects);
            return handle_result.error();
        }

        release();
        framebuffer_ = objects.framebuffer;
        linear_color_texture_ = objects.linear_color_texture;
        display_framebuffer_ = objects.display_framebuffer;
        display_texture_ = objects.display_texture;
        depth_renderbuffer_ = objects.depth_renderbuffer;
        color_texture_handle_ = std::move(handle_result).value();
        extent_ = extent;
        display_stale_ = true;
        return {};
    }

    [[nodiscard]] Result<void> clear(Color4 color) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (!is_valid()) {
            return {};
        }

        RenderStateGuard state_guard;
        configure_framebuffer_clear_state(framebuffer_, extent_);
        glClearColor(color.red, color.green, color.blue, color.alpha);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        display_stale_ = true;
        return {};
    }

    void set_display_transform(const DisplayTransform& transform) noexcept override {
        if (display_transform_ != transform) {
            display_transform_ = transform;
            display_stale_ = true;
        }
    }

    [[nodiscard]] TextureHandle color_texture() const noexcept override {
        return color_texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return framebuffer_ != 0 && linear_color_texture_ != 0 && display_framebuffer_ != 0 &&
               display_texture_ != 0 && depth_renderbuffer_ != 0 &&
               color_texture_handle_.is_valid() && extent_.width != 0 && extent_.height != 0;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint framebuffer() const noexcept {
        return framebuffer_;
    }

    void mark_display_stale() noexcept {
        display_stale_ = true;
    }

    [[nodiscard]] Result<void> resolve_color_texture() override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (!is_valid() || !display_stale_) {
            return {};
        }

        const Result<void> resources = ensure_display_resolve_resources();
        if (!resources) {
            return resources.error();
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, display_framebuffer_);
        glViewport(0, 0, static_cast<GLsizei>(extent_.width), static_cast<GLsizei>(extent_.height));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_CLAMP);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_PRIMITIVE_RESTART);
        glDisable(GL_RASTERIZER_DISCARD);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
        glUseProgram(resolve_program_);
        glBindVertexArray(resolve_vertex_array_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, linear_color_texture_);
        glUniform1i(resolve_texture_uniform_, 0);
        glUniform1f(resolve_exposure_uniform_, display_transform_.exposure_ev);
        int tone_mapping = 0;
        if (display_transform_.tone_mapping == ToneMappingMode::pbr_neutral) {
            tone_mapping = 1;
        } else if (display_transform_.tone_mapping == ToneMappingMode::standard) {
            tone_mapping = 2;
        }
        glUniform1i(resolve_tone_mapping_uniform_, tone_mapping);
        {
            const GpuTimingScope timing{*state_, GpuTimingKind::resolve};
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::draw_submission_failed,
                         "OpenGL reported an error while resolving the viewport display texture"};
        }

        display_stale_ = false;
        return {};
    }

  private:
    [[nodiscard]] Result<void> ensure_display_resolve_resources() {
        if (resolve_program_ != 0 && resolve_vertex_array_ != 0 && resolve_texture_uniform_ >= 0 &&
            resolve_exposure_uniform_ >= 0 && resolve_tone_mapping_uniform_ >= 0) {
            return {};
        }

        Result<DisplayResolveResources> resources = create_display_resolve_resources();
        if (!resources) {
            return resources.error();
        }
        resolve_program_ = resources.value().program;
        resolve_vertex_array_ = resources.value().vertex_array;
        resolve_texture_uniform_ = resources.value().texture_uniform;
        resolve_exposure_uniform_ = resources.value().exposure_uniform;
        resolve_tone_mapping_uniform_ = resources.value().tone_mapping_uniform;
        return {};
    }

    void release_resolve_resources() noexcept {
        if (!state_->can_destroy_objects()) {
            return;
        }
        if (resolve_vertex_array_ != 0) {
            glDeleteVertexArrays(1, &resolve_vertex_array_);
        }
        if (resolve_program_ != 0) {
            glDeleteProgram(resolve_program_);
        }
        resolve_vertex_array_ = 0;
        resolve_program_ = 0;
        resolve_texture_uniform_ = -1;
        resolve_exposure_uniform_ = -1;
        resolve_tone_mapping_uniform_ = -1;
    }

    void release() noexcept {
        if (color_texture_handle_.is_valid()) {
            state_->unregister_texture(color_texture_handle_);
        }
        if (state_->can_destroy_objects()) {
            delete_render_target_objects(framebuffer_, linear_color_texture_, display_framebuffer_,
                                         display_texture_, depth_renderbuffer_);
        }

        framebuffer_ = 0;
        linear_color_texture_ = 0;
        display_framebuffer_ = 0;
        display_texture_ = 0;
        depth_renderbuffer_ = 0;
        color_texture_handle_ = {};
        display_stale_ = false;
    }

    std::shared_ptr<OpenGLDeviceState> state_;
    Extent2D extent_;
    GLuint framebuffer_ = 0;
    GLuint linear_color_texture_ = 0;
    GLuint display_framebuffer_ = 0;
    GLuint display_texture_ = 0;
    GLuint depth_renderbuffer_ = 0;
    GLuint resolve_program_ = 0;
    GLuint resolve_vertex_array_ = 0;
    GLint resolve_texture_uniform_ = -1;
    GLint resolve_exposure_uniform_ = -1;
    GLint resolve_tone_mapping_uniform_ = -1;
    TextureHandle color_texture_handle_;
    DisplayTransform display_transform_;
    bool display_stale_ = false;
};

class OpenGLPickingTarget final : public graphics::PickingTarget {
  public:
    explicit OpenGLPickingTarget(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLPickingTarget() override {
        release();
    }

    [[nodiscard]] Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] Result<void> resize(Extent2D extent) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (extent == extent_) {
            return {};
        }
        if (extent.width == 0 || extent.height == 0) {
            release();
            extent_ = extent;
            return {};
        }
        if (!state_->supports(extent)) {
            return Error{ErrorCode::invalid_viewport_dimensions,
                         "Picking dimensions exceed the OpenGL texture-size limit"};
        }

        GLuint new_framebuffer = 0;
        GLuint new_id_texture = 0;
        GLuint new_depth_renderbuffer = 0;

        {
            AllocationStateGuard state_guard;

            glGenFramebuffers(1, &new_framebuffer);
            glGenTextures(1, &new_id_texture);
            glGenRenderbuffers(1, &new_depth_renderbuffer);
            if (new_framebuffer == 0 || new_id_texture == 0 || new_depth_renderbuffer == 0) {
                delete_picking_objects(new_framebuffer, new_id_texture, new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_creation_failed,
                             "OpenGL failed to allocate picking framebuffer objects"};
            }

            glBindTexture(GL_TEXTURE_2D, new_id_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, static_cast<GLsizei>(extent.width),
                         static_cast<GLsizei>(extent.height), 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                         nullptr);

            glBindRenderbuffer(GL_RENDERBUFFER, new_depth_renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                  static_cast<GLsizei>(extent.width),
                                  static_cast<GLsizei>(extent.height));

            glBindFramebuffer(GL_FRAMEBUFFER, new_framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   new_id_texture, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      new_depth_renderbuffer);
            constexpr GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &draw_buffer);

            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                delete_picking_objects(new_framebuffer, new_id_texture, new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_incomplete,
                             "The OpenGL picking framebuffer is incomplete"};
            }
        }

        release();
        framebuffer_ = new_framebuffer;
        id_texture_ = new_id_texture;
        depth_renderbuffer_ = new_depth_renderbuffer;
        extent_ = extent;
        return {};
    }

    [[nodiscard]] Result<void> clear() noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (!is_valid()) {
            return {};
        }

        RenderStateGuard state_guard;
        configure_framebuffer_clear_state(framebuffer_, extent_);
        constexpr std::array<GLuint, 4> clear_ids{};
        glClearBufferuiv(GL_COLOR, 0, clear_ids.data());
        const GLfloat clear_depth = 1.0F;
        glClearBufferfv(GL_DEPTH, 0, &clear_depth);
        return {};
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return framebuffer_ != 0 && id_texture_ != 0 && depth_renderbuffer_ != 0 &&
               extent_.width != 0 && extent_.height != 0;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint framebuffer() const noexcept {
        return framebuffer_;
    }

  private:
    void release() noexcept {
        if (state_->can_destroy_objects()) {
            delete_picking_objects(framebuffer_, id_texture_, depth_renderbuffer_);
        }
        framebuffer_ = 0;
        id_texture_ = 0;
        depth_renderbuffer_ = 0;
    }

    std::shared_ptr<OpenGLDeviceState> state_;
    Extent2D extent_;
    GLuint framebuffer_ = 0;
    GLuint id_texture_ = 0;
    GLuint depth_renderbuffer_ = 0;
};

} // namespace

Result<std::unique_ptr<graphics::RenderTarget>>
create_render_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept {
    const Result<void> validation = state->validate_operation();
    if (!validation) {
        return validation.error();
    }

    try {
        auto target = std::make_unique<OpenGLRenderTarget>(std::move(state));
        const Result<void> resize_result = target->resize(initial_extent);
        if (!resize_result) {
            return resize_result.error();
        }
        return std::unique_ptr<graphics::RenderTarget>{std::move(target)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<std::unique_ptr<graphics::PickingTarget>>
create_picking_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept {
    const Result<void> validation = state->validate_operation();
    if (!validation) {
        return validation.error();
    }

    try {
        auto target = std::make_unique<OpenGLPickingTarget>(std::move(state));
        const Result<void> resize_result = target->resize(initial_extent);
        if (!resize_result) {
            return resize_result.error();
        }
        return std::unique_ptr<graphics::PickingTarget>{std::move(target)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

Result<RenderTargetView> render_target_view(graphics::RenderTarget& target) noexcept {
    if (target.backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch, "The render target does not belong to OpenGL"};
    }
    auto& opengl_target = static_cast<OpenGLRenderTarget&>(target);
    return RenderTargetView{opengl_target.framebuffer(), opengl_target.extent(),
                            opengl_target.is_valid()};
}

Result<PickingTargetView> picking_target_view(graphics::PickingTarget& target) noexcept {
    if (target.backend_resource_token() != opengl_resource_token()) {
        return Error{ErrorCode::backend_mismatch, "The picking target does not belong to OpenGL"};
    }
    auto& opengl_target = static_cast<OpenGLPickingTarget&>(target);
    return PickingTargetView{opengl_target.framebuffer(), opengl_target.extent(),
                             opengl_target.is_valid()};
}

void mark_render_target_stale(graphics::RenderTarget& target) noexcept {
    static_cast<OpenGLRenderTarget&>(target).mark_display_stale();
}

} // namespace elf3d::backend::opengl::device_detail
