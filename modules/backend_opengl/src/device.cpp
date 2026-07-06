module;

#include <elf3d/clipping.h>
#include <elf3d/graphics.h>
#include <elf3d/math/value_types.h>
#include <elf3d/measurement.h>

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl {
namespace {

constexpr char missing_loader_message[] =
    "OpenGL initialization requires a graphics procedure loader";
constexpr char glad_failure_message[] = "GLAD failed to load the OpenGL procedure table";
constexpr char version_failure_message[] = "Elf3D requires an OpenGL 4.1 core-compatible context";
constexpr char context_failure_message[] =
    "A compatible OpenGL context must be current on the graphics thread";
constexpr char thread_failure_message[] =
    "The graphics operation must run on the engine owning graphics thread";

constinit const int opengl_resource_token_anchor = 0;

[[nodiscard]] std::uintptr_t opengl_resource_token() noexcept {
    return reinterpret_cast<std::uintptr_t>(&opengl_resource_token_anchor);
}

[[nodiscard]] bool is_opengl_texture(const graphics::Texture2D* texture) noexcept {
    return texture == nullptr || texture->backend_resource_token() == opengl_resource_token();
}

class ColorTextureResolver {
  public:
    virtual ~ColorTextureResolver() = default;
    [[nodiscard]] virtual Result<void> resolve_color_texture() = 0;
};

[[nodiscard]] Result<GLuint> compile_shader(GLenum type, std::string_view source);
[[nodiscard]] Result<GLuint> link_program(GLuint vertex_shader, GLuint fragment_shader);

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

layout(location = 0) out vec4 fragment_color;

vec3 linear_to_srgb(vec3 linear_color)
{
    linear_color = max(linear_color, vec3(0.0));
    return mix(12.92 * linear_color,
               1.055 * pow(linear_color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), linear_color));
}

void main()
{
    vec4 linear_color = texture(u_linear_color_texture, v_texcoord);
    fragment_color = vec4(linear_to_srgb(linear_color.rgb), linear_color.a);
}
)glsl";

struct TextureRecord {
    GLuint texture = 0;
    Extent2D extent;
    ColorTextureResolver* resolver = nullptr;
};

class OpenGLDeviceState final {
  public:
    explicit OpenGLDeviceState(GLint maximum_texture_size) noexcept
        : owner_thread_(std::this_thread::get_id()), maximum_texture_size_(maximum_texture_size) {}

    [[nodiscard]] Result<void> validate_operation() const noexcept {
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

    [[nodiscard]] bool can_destroy_objects() const noexcept {
        return operational_ && std::this_thread::get_id() == owner_thread_ &&
               glGetString(GL_VERSION) != nullptr;
    }

    [[nodiscard]] bool supports(Extent2D extent) const noexcept {
        const auto maximum = static_cast<std::uint32_t>(maximum_texture_size_);
        return extent.width <= maximum && extent.height <= maximum &&
               extent.width <= static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max()) &&
               extent.height <= static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max());
    }

    [[nodiscard]] Result<TextureHandle> register_texture(GLuint texture, Extent2D extent,
                                                         ColorTextureResolver* resolver = nullptr) {
        try {
            std::uint64_t candidate = next_texture_handle_++;
            if (candidate == 0) {
                candidate = next_texture_handle_++;
            }
            texture_records_.emplace(candidate, TextureRecord{texture, extent, resolver});
            return detail::TextureHandleAccess::create(candidate);
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "Failed to register the OpenGL color texture"};
        }
    }

    void unregister_texture(TextureHandle handle) noexcept {
        texture_records_.erase(detail::TextureHandleAccess::value(handle));
    }

    [[nodiscard]] Result<NativeTextureView> native_texture_view(TextureHandle handle) const {
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

    void shut_down() noexcept {
        operational_ = false;
    }

  private:
    std::thread::id owner_thread_;
    GLint maximum_texture_size_ = 0;
    bool operational_ = true;
    std::uint64_t next_texture_handle_ = 1;
    std::unordered_map<std::uint64_t, TextureRecord> texture_records_;
};

class AllocationStateGuard final {
  public:
    AllocationStateGuard() noexcept {
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

    ~AllocationStateGuard() {
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

    AllocationStateGuard(const AllocationStateGuard&) = delete;
    AllocationStateGuard& operator=(const AllocationStateGuard&) = delete;

  private:
    GLint draw_framebuffer_ = 0;
    GLint read_framebuffer_ = 0;
    GLint renderbuffer_ = 0;
    GLint active_texture_ = GL_TEXTURE0;
    GLint texture_2d_ = 0;
    GLint pixel_unpack_buffer_ = 0;
    GLint unpack_alignment_ = 4;
    GLint vertex_array_ = 0;
    GLint array_buffer_ = 0;
};

class RenderStateGuard final {
  public:
    RenderStateGuard() noexcept {
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

    ~RenderStateGuard() {
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
        glBlendFuncSeparate(static_cast<GLenum>(blend_source_rgb_),
                            static_cast<GLenum>(blend_destination_rgb_),
                            static_cast<GLenum>(blend_source_alpha_),
                            static_cast<GLenum>(blend_destination_alpha_));
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

    RenderStateGuard(const RenderStateGuard&) = delete;
    RenderStateGuard& operator=(const RenderStateGuard&) = delete;

  private:
    static void set_enabled(GLenum capability, GLboolean enabled) noexcept {
        if (enabled == GL_TRUE) {
            glEnable(capability);
        } else {
            glDisable(capability);
        }
    }

    GLint draw_framebuffer_ = 0;
    GLint read_framebuffer_ = 0;
    GLint viewport_[4]{};
    GLfloat clear_color_[4]{};
    GLdouble depth_clear_value_ = 1.0;
    GLboolean color_mask_[4]{};
    GLboolean depth_mask_ = GL_TRUE;
    GLboolean scissor_enabled_ = GL_FALSE;
    GLboolean framebuffer_srgb_enabled_ = GL_FALSE;
    GLboolean depth_test_enabled_ = GL_FALSE;
    GLboolean cull_face_enabled_ = GL_FALSE;
    GLboolean blend_enabled_ = GL_FALSE;
    GLboolean stencil_test_enabled_ = GL_FALSE;
    GLboolean depth_clamp_enabled_ = GL_FALSE;
    GLboolean polygon_offset_fill_enabled_ = GL_FALSE;
    GLboolean primitive_restart_enabled_ = GL_FALSE;
    GLboolean rasterizer_discard_enabled_ = GL_FALSE;
    GLint program_ = 0;
    GLint vertex_array_ = 0;
    GLint array_buffer_ = 0;
    GLint active_texture_ = GL_TEXTURE0;
    std::array<GLint, 4> texture_units_{};
    GLint depth_function_ = GL_LESS;
    GLint blend_source_rgb_ = GL_ONE;
    GLint blend_destination_rgb_ = GL_ZERO;
    GLint blend_source_alpha_ = GL_ONE;
    GLint blend_destination_alpha_ = GL_ZERO;
    GLint blend_equation_rgb_ = GL_FUNC_ADD;
    GLint blend_equation_alpha_ = GL_FUNC_ADD;
    GLint cull_face_mode_ = GL_BACK;
    GLint front_face_ = GL_CCW;
    GLint polygon_mode_[2]{GL_FILL, GL_FILL};
    GLdouble depth_range_[2]{0.0, 1.0};
};

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

    [[nodiscard]] Result<void> resize(Extent2D extent) override {
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

        GLuint new_framebuffer = 0;
        GLuint new_linear_color_texture = 0;
        GLuint new_display_framebuffer = 0;
        GLuint new_display_texture = 0;
        GLuint new_depth_renderbuffer = 0;

        {
            AllocationStateGuard state_guard;

            glGenFramebuffers(1, &new_framebuffer);
            glGenFramebuffers(1, &new_display_framebuffer);
            GLuint color_textures[2]{};
            glGenTextures(2, color_textures);
            new_linear_color_texture = color_textures[0];
            new_display_texture = color_textures[1];
            glGenRenderbuffers(1, &new_depth_renderbuffer);
            if (new_framebuffer == 0 || new_linear_color_texture == 0 ||
                new_display_framebuffer == 0 || new_display_texture == 0 ||
                new_depth_renderbuffer == 0) {
                delete_render_target_objects(new_framebuffer, new_linear_color_texture,
                                             new_display_framebuffer, new_display_texture,
                                             new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_creation_failed,
                             "OpenGL failed to allocate viewport framebuffer objects"};
            }

            glBindTexture(GL_TEXTURE_2D, new_linear_color_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(extent.width),
                         static_cast<GLsizei>(extent.height), 0, GL_RGBA, GL_HALF_FLOAT, nullptr);

            glBindTexture(GL_TEXTURE_2D, new_display_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(extent.width),
                         static_cast<GLsizei>(extent.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         nullptr);

            glBindRenderbuffer(GL_RENDERBUFFER, new_depth_renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                  static_cast<GLsizei>(extent.width),
                                  static_cast<GLsizei>(extent.height));

            glBindFramebuffer(GL_FRAMEBUFFER, new_framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   new_linear_color_texture, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      new_depth_renderbuffer);
            constexpr GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &draw_buffer);

            const GLenum render_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (render_status != GL_FRAMEBUFFER_COMPLETE) {
                delete_render_target_objects(new_framebuffer, new_linear_color_texture,
                                             new_display_framebuffer, new_display_texture,
                                             new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_incomplete,
                             "The OpenGL viewport framebuffer is incomplete"};
            }

            glBindFramebuffer(GL_FRAMEBUFFER, new_display_framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   new_display_texture, 0);
            glDrawBuffers(1, &draw_buffer);

            const GLenum display_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (display_status != GL_FRAMEBUFFER_COMPLETE) {
                delete_render_target_objects(new_framebuffer, new_linear_color_texture,
                                             new_display_framebuffer, new_display_texture,
                                             new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_incomplete,
                             "The OpenGL viewport display framebuffer is incomplete"};
            }
        }

        Result<TextureHandle> handle_result =
            state_->register_texture(new_display_texture, extent, this);
        if (!handle_result) {
            delete_render_target_objects(new_framebuffer, new_linear_color_texture,
                                         new_display_framebuffer, new_display_texture,
                                         new_depth_renderbuffer);
            return handle_result.error();
        }

        release();
        framebuffer_ = new_framebuffer;
        linear_color_texture_ = new_linear_color_texture;
        display_framebuffer_ = new_display_framebuffer;
        display_texture_ = new_display_texture;
        depth_renderbuffer_ = new_depth_renderbuffer;
        color_texture_handle_ = std::move(handle_result).value();
        extent_ = extent;
        display_stale_ = true;
        return {};
    }

    [[nodiscard]] Result<void> clear(Color4 color) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (!is_valid()) {
            return {};
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, static_cast<GLsizei>(extent_.width), static_cast<GLsizei>(extent_.height));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClearColor(color.red, color.green, color.blue, color.alpha);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        display_stale_ = true;
        return {};
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
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::draw_submission_failed,
                         "OpenGL reported an error while resolving the viewport display texture"};
        }

        display_stale_ = false;
        return {};
    }

  private:
    [[nodiscard]] Result<void> ensure_display_resolve_resources() {
        if (resolve_program_ != 0 && resolve_vertex_array_ != 0 && resolve_texture_uniform_ >= 0) {
            return {};
        }

        AllocationStateGuard allocation_guard;
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
        const GLuint fragment_shader = fragment_result.value();
        Result<GLuint> program_result = link_program(vertex_shader, fragment_shader);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        if (!program_result) {
            return program_result.error();
        }

        GLuint vertex_array = 0;
        glGenVertexArrays(1, &vertex_array);
        if (vertex_array == 0) {
            glDeleteProgram(program_result.value());
            return Error{ErrorCode::gpu_buffer_creation_failed,
                         "OpenGL failed to allocate viewport display resolve resources"};
        }

        const GLuint program = program_result.value();
        const GLint texture_uniform = glGetUniformLocation(program, "u_linear_color_texture");
        if (texture_uniform < 0) {
            glDeleteProgram(program);
            glDeleteVertexArrays(1, &vertex_array);
            return Error{ErrorCode::shader_linking_failed,
                         "The viewport display resolve shader is missing a required uniform"};
        }

        resolve_program_ = program;
        resolve_vertex_array_ = vertex_array;
        resolve_texture_uniform_ = texture_uniform;
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
    TextureHandle color_texture_handle_;
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

    [[nodiscard]] Result<void> resize(Extent2D extent) override {
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

    [[nodiscard]] Result<void> clear() override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (!is_valid()) {
            return {};
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, static_cast<GLsizei>(extent_.width), static_cast<GLsizei>(extent_.height));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        const GLuint clear_ids[4]{0U, 0U, 0U, 0U};
        glClearBufferuiv(GL_COLOR, 0, clear_ids);
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

std::string shader_log(GLuint object, bool program) {
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
        const std::string message = "OpenGL shader compilation failed: " + diagnostic;
        return Error{ErrorCode::shader_compilation_failed, message};
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
        const std::string message = "OpenGL shader program linking failed: " + diagnostic;
        return Error{ErrorCode::shader_linking_failed, message};
    }
    return program;
}

struct UniformLocations {
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint normal = -1;
    GLint base_color = -1;
    GLint camera_world_position = -1;
    GLint light_direction = -1;
    GLint light_color = -1;
    GLint ambient_intensity = -1;
    GLint diffuse_intensity = -1;
    GLint metallic_factor = -1;
    GLint roughness_factor = -1;
    GLint emissive_factor = -1;
    GLint occlusion_strength = -1;
    GLint ior = -1;
    GLint specular_factor = -1;
    GLint specular_color_factor = -1;
    GLint highlight_color = -1;
    GLint highlight_strength = -1;
    GLint has_base_color_texture = -1;
    GLint has_metallic_roughness_texture = -1;
    GLint has_occlusion_texture = -1;
    GLint has_emissive_texture = -1;
    GLint base_color_texture = -1;
    GLint metallic_roughness_texture = -1;
    GLint occlusion_texture = -1;
    GLint emissive_texture = -1;
    GLint texture_texcoord_sets = -1;
    GLint texture_offsets = -1;
    GLint texture_scales = -1;
    GLint texture_rotations = -1;
    GLint alpha_mode = -1;
    GLint alpha_cutoff = -1;
    GLint unlit = -1;
    GLint clipping_section_plane_enabled = -1;
    GLint clipping_section_plane_normal = -1;
    GLint clipping_section_plane_offset = -1;
    GLint clipping_retain_positive_half_space = -1;
    GLint clipping_box_count = -1;
    GLint clipping_box_minimums = -1;
    GLint clipping_box_maximums = -1;

    [[nodiscard]] bool valid() const noexcept {
        return model >= 0 && view >= 0 && projection >= 0 && normal >= 0 && base_color >= 0 &&
               camera_world_position >= 0 && light_direction >= 0 && light_color >= 0 &&
               ambient_intensity >= 0 && diffuse_intensity >= 0 && metallic_factor >= 0 &&
               roughness_factor >= 0 && emissive_factor >= 0 && occlusion_strength >= 0 &&
               ior >= 0 && specular_factor >= 0 && specular_color_factor >= 0 &&
               highlight_color >= 0 && highlight_strength >= 0 && has_base_color_texture >= 0 &&
               has_metallic_roughness_texture >= 0 && has_occlusion_texture >= 0 &&
               has_emissive_texture >= 0 && base_color_texture >= 0 &&
               metallic_roughness_texture >= 0 && occlusion_texture >= 0 && emissive_texture >= 0 &&
               texture_texcoord_sets >= 0 && texture_offsets >= 0 && texture_scales >= 0 &&
               texture_rotations >= 0 && alpha_mode >= 0 && alpha_cutoff >= 0 && unlit >= 0 &&
               clipping_section_plane_enabled >= 0 && clipping_section_plane_normal >= 0 &&
               clipping_section_plane_offset >= 0 && clipping_retain_positive_half_space >= 0 &&
               clipping_box_count >= 0 && clipping_box_minimums >= 0 && clipping_box_maximums >= 0;
    }
};

class OpenGLTexture2D final : public graphics::Texture2D {
  public:
    OpenGLTexture2D(std::shared_ptr<OpenGLDeviceState> state, GLuint texture,
                    Extent2D extent) noexcept
        : state_(std::move(state)), texture_(texture), extent_(extent) {}

    ~OpenGLTexture2D() override {
        if (state_->can_destroy_objects() && texture_ != 0) {
            glDeleteTextures(1, &texture_);
        }
    }

    [[nodiscard]] Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint texture() const noexcept {
        return texture_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint texture_ = 0;
    Extent2D extent_;
};

GLenum texture_wrap(graphics::TextureAddressMode mode) noexcept {
    switch (mode) {
    case graphics::TextureAddressMode::repeat:
        return GL_REPEAT;
    case graphics::TextureAddressMode::mirrored_repeat:
        return GL_MIRRORED_REPEAT;
    case graphics::TextureAddressMode::clamp_to_edge:
        return GL_CLAMP_TO_EDGE;
    }
    return GL_REPEAT;
}

GLenum texture_filter(graphics::TextureFilterMode mode) noexcept {
    switch (mode) {
    case graphics::TextureFilterMode::nearest:
        return GL_NEAREST;
    case graphics::TextureFilterMode::linear:
        return GL_LINEAR;
    case graphics::TextureFilterMode::nearest_mipmap_nearest:
        return GL_NEAREST_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::linear_mipmap_nearest:
        return GL_LINEAR_MIPMAP_NEAREST;
    case graphics::TextureFilterMode::nearest_mipmap_linear:
        return GL_NEAREST_MIPMAP_LINEAR;
    case graphics::TextureFilterMode::linear_mipmap_linear:
        return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

bool uses_mipmaps(graphics::TextureFilterMode mode) noexcept {
    return mode != graphics::TextureFilterMode::nearest &&
           mode != graphics::TextureFilterMode::linear;
}

class OpenGLStaticMesh final : public graphics::StaticMesh {
  public:
    OpenGLStaticMesh(std::shared_ptr<OpenGLDeviceState> state, GLuint vertex_array,
                     GLuint vertex_buffer, GLuint index_buffer, std::uint32_t vertex_count,
                     std::uint32_t index_count) noexcept
        : state_(std::move(state)), vertex_array_(vertex_array), vertex_buffer_(vertex_buffer),
          index_buffer_(index_buffer), vertex_count_(vertex_count), index_count_(index_count) {}

    ~OpenGLStaticMesh() override {
        if (!state_->can_destroy_objects()) {
            return;
        }
        glDeleteVertexArrays(1, &vertex_array_);
        glDeleteBuffers(1, &vertex_buffer_);
        glDeleteBuffers(1, &index_buffer_);
    }

    [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
        return vertex_count_;
    }

    [[nodiscard]] std::uint32_t index_count() const noexcept override {
        return index_count_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] GLuint vertex_array() const noexcept {
        return vertex_array_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLuint index_buffer_ = 0;
    std::uint32_t vertex_count_ = 0;
    std::uint32_t index_count_ = 0;
};

class OpenGLGraphicsPipeline final : public graphics::GraphicsPipeline {
  public:
    OpenGLGraphicsPipeline(std::shared_ptr<OpenGLDeviceState> state, GLuint program,
                           UniformLocations uniforms) noexcept
        : state_(std::move(state)), program_(program), uniforms_(uniforms) {}

    ~OpenGLGraphicsPipeline() override {
        if (state_->can_destroy_objects() && program_ != 0) {
            glDeleteProgram(program_);
        }
    }

    [[nodiscard]] GLuint program() const noexcept {
        return program_;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return opengl_resource_token();
    }

    [[nodiscard]] const UniformLocations& uniforms() const noexcept {
        return uniforms_;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint program_ = 0;
    UniformLocations uniforms_;
};

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

struct OverlayUniformLocations {
    GLint color = -1;

    [[nodiscard]] bool valid() const noexcept {
        return color >= 0;
    }
};

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

struct PickingUniformLocations {
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint object_id = -1;
    GLint primitive_index = -1;
    GLint clipping_section_plane_enabled = -1;
    GLint clipping_section_plane_normal = -1;
    GLint clipping_section_plane_offset = -1;
    GLint clipping_retain_positive_half_space = -1;
    GLint clipping_box_count = -1;
    GLint clipping_box_minimums = -1;
    GLint clipping_box_maximums = -1;

    [[nodiscard]] bool valid() const noexcept {
        return model >= 0 && view >= 0 && projection >= 0 && object_id >= 0 &&
               primitive_index >= 0 && clipping_section_plane_enabled >= 0 &&
               clipping_section_plane_normal >= 0 && clipping_section_plane_offset >= 0 &&
               clipping_retain_positive_half_space >= 0 && clipping_box_count >= 0 &&
               clipping_box_minimums >= 0 && clipping_box_maximums >= 0;
    }
};

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

[[nodiscard]] std::optional<OverlayProjectedPoint>
project_overlay_point(const graphics::DrawOverlayDescription& description, Extent2D extent,
                      Float3 world_position) noexcept {
    if (extent.width == 0 || extent.height == 0) {
        return std::nullopt;
    }
    const std::array<float, 4> world{world_position.x, world_position.y, world_position.z, 1.0F};
    const std::array<float, 4> view = multiply_matrix_vector(description.view_matrix, world);
    const std::array<float, 4> clip = multiply_matrix_vector(description.projection_matrix, view);
    if (!std::isfinite(clip[0]) || !std::isfinite(clip[1]) || !std::isfinite(clip[2]) ||
        !std::isfinite(clip[3]) || clip[3] <= 0.000001F) {
        return std::nullopt;
    }
    const float inverse_w = 1.0F / clip[3];
    const float ndc_x = clip[0] * inverse_w;
    const float ndc_y = clip[1] * inverse_w;
    const float ndc_z = clip[2] * inverse_w;
    if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) {
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

class OpenGLDevice final : public graphics::Device {
  public:
    explicit OpenGLDevice(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLDevice() override {
        release_picking_resources();
        release_overlay_resources();
        state_->shut_down();
    }

    [[nodiscard]] GraphicsBackend backend() const noexcept override {
        return GraphicsBackend::opengl;
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
    create_render_target(Extent2D initial_extent) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        try {
            auto target = std::make_unique<OpenGLRenderTarget>(state_);
            const Result<void> resize_result = target->resize(initial_extent);
            if (!resize_result) {
                return resize_result.error();
            }
            return std::unique_ptr<graphics::RenderTarget>{std::move(target)};
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL viewport target creation threw an exception"};
        }
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::PickingTarget>>
    create_picking_target(Extent2D initial_extent) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        try {
            auto target = std::make_unique<OpenGLPickingTarget>(state_);
            const Result<void> resize_result = target->resize(initial_extent);
            if (!resize_result) {
                return resize_result.error();
            }
            return std::unique_ptr<graphics::PickingTarget>{std::move(target)};
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL picking target creation threw an exception"};
        }
    }

    [[nodiscard]] Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const override {
        return state_->native_texture_view(texture);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::StaticMesh>>
    create_static_mesh(const graphics::StaticMeshDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (description.vertex_layout != graphics::VertexLayout::position_normal_float3 &&
            description.vertex_layout !=
                graphics::VertexLayout::position_normal_float3_texcoord_float2 &&
            description.vertex_layout !=
                graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4) {
            return Error{ErrorCode::unsupported_vertex_layout,
                         "The OpenGL backend does not support the requested vertex layout"};
        }
        std::size_t vertex_stride = sizeof(float) * 6;
        if (description.vertex_layout ==
            graphics::VertexLayout::position_normal_float3_texcoord_float2) {
            vertex_stride = sizeof(float) * 8;
        } else if (description.vertex_layout ==
                   graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4) {
            vertex_stride = sizeof(float) * 14;
        }
        if (description.vertex_count == 0 || description.indices.empty() ||
            description.vertex_bytes.size() !=
                static_cast<std::size_t>(description.vertex_count) * vertex_stride ||
            description.indices.size() >
                static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            return Error{ErrorCode::gpu_buffer_creation_failed,
                         "Static mesh upload contains invalid vertex or index sizes"};
        }

        try {
            AllocationStateGuard state_guard;
            GLuint vertex_array = 0;
            GLuint vertex_buffer = 0;
            GLuint index_buffer = 0;
            glGenVertexArrays(1, &vertex_array);
            glGenBuffers(1, &vertex_buffer);
            glGenBuffers(1, &index_buffer);
            if (vertex_array == 0 || vertex_buffer == 0 || index_buffer == 0) {
                glDeleteVertexArrays(1, &vertex_array);
                glDeleteBuffers(1, &vertex_buffer);
                glDeleteBuffers(1, &index_buffer);
                return Error{ErrorCode::gpu_buffer_creation_failed,
                             "OpenGL failed to allocate static mesh objects"};
            }

            glBindVertexArray(vertex_array);
            glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(description.vertex_bytes.size()),
                         description.vertex_bytes.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(description.indices.size_bytes()),
                         description.indices.data(), GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertex_stride),
                                  nullptr);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertex_stride),
                                  reinterpret_cast<const void*>(sizeof(float) * 3));
            if (description.vertex_layout != graphics::VertexLayout::position_normal_float3) {
                glEnableVertexAttribArray(2);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertex_stride),
                                      reinterpret_cast<const void*>(sizeof(float) * 6));
            }
            if (description.vertex_layout ==
                graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4) {
                glEnableVertexAttribArray(3);
                glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertex_stride),
                                      reinterpret_cast<const void*>(sizeof(float) * 8));
                glEnableVertexAttribArray(4);
                glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vertex_stride),
                                      reinterpret_cast<const void*>(sizeof(float) * 10));
            }

            if (glGetError() != GL_NO_ERROR) {
                glDeleteVertexArrays(1, &vertex_array);
                glDeleteBuffers(1, &vertex_buffer);
                glDeleteBuffers(1, &index_buffer);
                return Error{ErrorCode::gpu_buffer_creation_failed,
                             "OpenGL reported an error while uploading static mesh buffers"};
            }

            return std::unique_ptr<graphics::StaticMesh>{std::make_unique<OpenGLStaticMesh>(
                state_, vertex_array, vertex_buffer, index_buffer, description.vertex_count,
                static_cast<std::uint32_t>(description.indices.size()))};
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL static mesh creation threw an exception"};
        }
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>>
    create_texture_2d(const graphics::Texture2DDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (description.extent.width == 0 || description.extent.height == 0 ||
            !state_->supports(description.extent)) {
            return Error{ErrorCode::gpu_texture_creation_failed,
                         "Texture dimensions are zero or exceed OpenGL limits"};
        }
        const std::size_t width = description.extent.width;
        const std::size_t height = description.extent.height;
        if (width > std::numeric_limits<std::size_t>::max() / 4 ||
            height > std::numeric_limits<std::size_t>::max() / (width * 4) ||
            description.pixels.size() != width * height * 4) {
            return Error{ErrorCode::gpu_texture_upload_failed,
                         "Texture upload does not contain tightly packed RGBA8 pixels"};
        }
        if (description.mag_filter != graphics::TextureFilterMode::nearest &&
            description.mag_filter != graphics::TextureFilterMode::linear) {
            return Error{ErrorCode::invalid_sampler_filter,
                         "Texture magnification supports only nearest or linear filtering"};
        }

        const GLint internal_format =
            description.format == graphics::TextureFormat::rgba8_srgb    ? GL_SRGB8_ALPHA8
            : description.format == graphics::TextureFormat::rgba8_unorm ? GL_RGBA8
                                                                         : 0;
        if (internal_format == 0) {
            return Error{ErrorCode::unsupported_texture_format,
                         "OpenGL texture upload supports only RGBA8 and sRGB RGBA8"};
        }

        try {
            AllocationStateGuard state_guard;
            GLuint texture = 0;
            glGenTextures(1, &texture);
            if (texture == 0) {
                return Error{ErrorCode::gpu_texture_creation_failed,
                             "OpenGL failed to allocate a 2D texture object"};
            }
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            static_cast<GLint>(texture_wrap(description.wrap_u)));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            static_cast<GLint>(texture_wrap(description.wrap_v)));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            static_cast<GLint>(texture_filter(description.min_filter)));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            static_cast<GLint>(texture_filter(description.mag_filter)));
            // Top-to-bottom image rows are uploaded unchanged. glTF v=0 therefore samples the
            // first encoded row; no UV or pixel flip is applied in the material path.
            glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                         static_cast<GLsizei>(description.extent.width),
                         static_cast<GLsizei>(description.extent.height), 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, description.pixels.data());
            if (uses_mipmaps(description.min_filter)) {
                glGenerateMipmap(GL_TEXTURE_2D);
            }
            if (glGetError() != GL_NO_ERROR) {
                glDeleteTextures(1, &texture);
                return Error{ErrorCode::gpu_texture_upload_failed,
                             "OpenGL reported an error while uploading a 2D texture"};
            }
            return std::unique_ptr<graphics::Texture2D>{
                std::make_unique<OpenGLTexture2D>(state_, texture, description.extent)};
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL texture creation threw an exception"};
        }
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>>
    create_graphics_pipeline(const graphics::GraphicsPipelineDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (description.vertex_layout !=
            graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4) {
            return Error{ErrorCode::unsupported_vertex_layout,
                         "The PBR pipeline requires position, normal, two UV sets, and color"};
        }

        try {
            Result<GLuint> vertex_result =
                compile_shader(GL_VERTEX_SHADER, description.vertex_shader_source);
            if (!vertex_result) {
                return vertex_result.error();
            }
            const GLuint vertex_shader = vertex_result.value();
            Result<GLuint> fragment_result =
                compile_shader(GL_FRAGMENT_SHADER, description.fragment_shader_source);
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
            const UniformLocations uniforms{
                glGetUniformLocation(program, "u_model"),
                glGetUniformLocation(program, "u_view"),
                glGetUniformLocation(program, "u_projection"),
                glGetUniformLocation(program, "u_normal_matrix"),
                glGetUniformLocation(program, "u_base_color"),
                glGetUniformLocation(program, "u_camera_world_position"),
                glGetUniformLocation(program, "u_light_direction"),
                glGetUniformLocation(program, "u_light_color"),
                glGetUniformLocation(program, "u_ambient_intensity"),
                glGetUniformLocation(program, "u_diffuse_intensity"),
                glGetUniformLocation(program, "u_metallic_factor"),
                glGetUniformLocation(program, "u_roughness_factor"),
                glGetUniformLocation(program, "u_emissive_factor"),
                glGetUniformLocation(program, "u_occlusion_strength"),
                glGetUniformLocation(program, "u_ior"),
                glGetUniformLocation(program, "u_specular_factor"),
                glGetUniformLocation(program, "u_specular_color_factor"),
                glGetUniformLocation(program, "u_highlight_color"),
                glGetUniformLocation(program, "u_highlight_strength"),
                glGetUniformLocation(program, "u_has_base_color_texture"),
                glGetUniformLocation(program, "u_has_metallic_roughness_texture"),
                glGetUniformLocation(program, "u_has_occlusion_texture"),
                glGetUniformLocation(program, "u_has_emissive_texture"),
                glGetUniformLocation(program, "u_base_color_texture"),
                glGetUniformLocation(program, "u_metallic_roughness_texture"),
                glGetUniformLocation(program, "u_occlusion_texture"),
                glGetUniformLocation(program, "u_emissive_texture"),
                glGetUniformLocation(program, "u_texture_texcoord_sets[0]"),
                glGetUniformLocation(program, "u_texture_offsets[0]"),
                glGetUniformLocation(program, "u_texture_scales[0]"),
                glGetUniformLocation(program, "u_texture_rotations[0]"),
                glGetUniformLocation(program, "u_alpha_mode"),
                glGetUniformLocation(program, "u_alpha_cutoff"),
                glGetUniformLocation(program, "u_unlit"),
                glGetUniformLocation(program, "u_clipping_section_plane_enabled"),
                glGetUniformLocation(program, "u_clipping_section_plane_normal"),
                glGetUniformLocation(program, "u_clipping_section_plane_offset"),
                glGetUniformLocation(program, "u_clipping_retain_positive_half_space"),
                glGetUniformLocation(program, "u_clipping_box_count"),
                glGetUniformLocation(program, "u_clipping_box_minimums[0]"),
                glGetUniformLocation(program, "u_clipping_box_maximums[0]")};
            if (!uniforms.valid()) {
                glDeleteProgram(program);
                return Error{ErrorCode::shader_linking_failed,
                             "The linked shader program is missing a required renderer uniform"};
            }

            return std::unique_ptr<graphics::GraphicsPipeline>{
                std::make_unique<OpenGLGraphicsPipeline>(state_, program, uniforms)};
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL graphics pipeline creation threw an exception"};
        }
    }

    [[nodiscard]] Result<void>
    draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                 graphics::StaticMesh& mesh,
                 const graphics::DrawIndexedDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (target.backend_resource_token() != opengl_resource_token() ||
            pipeline.backend_resource_token() != opengl_resource_token() ||
            mesh.backend_resource_token() != opengl_resource_token()) {
            return Error{ErrorCode::backend_mismatch,
                         "Indexed drawing requires resources created by the same OpenGL backend"};
        }
        if (description.textures.size() != graphics::material_texture_count) {
            return Error{ErrorCode::invalid_argument,
                         "Indexed drawing requires four ordered material texture observers"};
        }
        if (!is_opengl_texture(description.textures[0]) ||
            !is_opengl_texture(description.textures[1]) ||
            !is_opengl_texture(description.textures[2]) ||
            !is_opengl_texture(description.textures[3])) {
            return Error{ErrorCode::backend_mismatch,
                         "Material textures were created by a different graphics backend"};
        }
        auto* opengl_target = static_cast<OpenGLRenderTarget*>(&target);
        auto* opengl_pipeline = static_cast<OpenGLGraphicsPipeline*>(&pipeline);
        auto* opengl_mesh = static_cast<OpenGLStaticMesh*>(&mesh);
        auto* base_color_texture = static_cast<OpenGLTexture2D*>(description.textures[0]);
        auto* metallic_roughness_texture = static_cast<OpenGLTexture2D*>(description.textures[1]);
        auto* occlusion_texture = static_cast<OpenGLTexture2D*>(description.textures[2]);
        auto* emissive_texture = static_cast<OpenGLTexture2D*>(description.textures[3]);
        if (!opengl_target->is_valid() || opengl_mesh->index_count() == 0) {
            return {};
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, opengl_target->framebuffer());
        const Extent2D extent = opengl_target->extent();
        glViewport(0, 0, static_cast<GLsizei>(extent.width), static_cast<GLsizei>(extent.height));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        if (description.alpha_mode == AlphaMode::blend) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glDepthMask(GL_FALSE);
        } else {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_CLAMP);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_PRIMITIVE_RESTART);
        glDisable(GL_RASTERIZER_DISCARD);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthRange(0.0, 1.0);
        if (description.double_sided) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }
        glCullFace(GL_BACK);
        glFrontFace(description.front_face_clockwise ? GL_CW : GL_CCW);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(opengl_pipeline->program());
        glBindVertexArray(opengl_mesh->vertex_array());

        const UniformLocations& uniforms = opengl_pipeline->uniforms();
        glUniformMatrix4fv(uniforms.model, 1, GL_FALSE, description.model_matrix.data());
        glUniformMatrix4fv(uniforms.view, 1, GL_FALSE, description.view_matrix.data());
        glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, description.projection_matrix.data());
        glUniformMatrix3fv(uniforms.normal, 1, GL_FALSE, description.normal_matrix.data());
        glUniform4f(uniforms.base_color, description.base_color.red, description.base_color.green,
                    description.base_color.blue, description.base_color.alpha);
        glUniform3f(uniforms.camera_world_position, description.camera_world_position.x,
                    description.camera_world_position.y, description.camera_world_position.z);
        glUniform3f(uniforms.light_direction, description.light_direction.x,
                    description.light_direction.y, description.light_direction.z);
        glUniform4f(uniforms.light_color, description.light_color.red,
                    description.light_color.green, description.light_color.blue,
                    description.light_color.alpha);
        glUniform1f(uniforms.ambient_intensity, description.ambient_intensity);
        glUniform1f(uniforms.diffuse_intensity, description.diffuse_intensity);
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
        glUniform1i(uniforms.has_base_color_texture, base_color_texture != nullptr ? 1 : 0);
        glUniform1i(uniforms.has_metallic_roughness_texture,
                    metallic_roughness_texture != nullptr ? 1 : 0);
        glUniform1i(uniforms.has_occlusion_texture, occlusion_texture != nullptr ? 1 : 0);
        glUniform1i(uniforms.has_emissive_texture, emissive_texture != nullptr ? 1 : 0);
        std::array<GLint, 4> texcoord_sets{};
        std::array<float, 8> texture_offsets{};
        std::array<float, 8> texture_scales{};
        std::array<float, 4> texture_rotations{};
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
                     static_cast<GLsizei>(description.texture_mappings.size()),
                     texture_offsets.data());
        glUniform2fv(uniforms.texture_scales,
                     static_cast<GLsizei>(description.texture_mappings.size()),
                     texture_scales.data());
        glUniform1fv(uniforms.texture_rotations, static_cast<GLsizei>(texture_rotations.size()),
                     texture_rotations.data());
        glUniform1i(uniforms.alpha_mode, static_cast<GLint>(description.alpha_mode));
        glUniform1f(uniforms.alpha_cutoff, description.alpha_cutoff);
        glUniform1i(uniforms.unlit, description.unlit ? 1 : 0);
        glUniform1i(uniforms.clipping_section_plane_enabled,
                    description.clipping_section_plane_enabled ? 1 : 0);
        glUniform3f(uniforms.clipping_section_plane_normal,
                    description.clipping_section_plane_normal.x,
                    description.clipping_section_plane_normal.y,
                    description.clipping_section_plane_normal.z);
        glUniform1f(uniforms.clipping_section_plane_offset,
                    description.clipping_section_plane_offset);
        glUniform1i(uniforms.clipping_retain_positive_half_space,
                    description.clipping_retain_positive_half_space ? 1 : 0);
        const std::uint32_t box_count =
            std::min(description.clipping_box_count, maximum_clipping_boxes);
        std::array<float, maximum_clipping_boxes * 3> clipping_box_minimums{};
        std::array<float, maximum_clipping_boxes * 3> clipping_box_maximums{};
        for (std::uint32_t index = 0; index < box_count; ++index) {
            const Bounds3& box = description.clipping_boxes[index];
            const std::size_t base = static_cast<std::size_t>(index) * 3U;
            clipping_box_minimums[base] = box.minimum.x;
            clipping_box_minimums[base + 1U] = box.minimum.y;
            clipping_box_minimums[base + 2U] = box.minimum.z;
            clipping_box_maximums[base] = box.maximum.x;
            clipping_box_maximums[base + 1U] = box.maximum.y;
            clipping_box_maximums[base + 2U] = box.maximum.z;
        }
        glUniform1i(uniforms.clipping_box_count, static_cast<GLint>(box_count));
        glUniform3fv(uniforms.clipping_box_minimums, static_cast<GLsizei>(maximum_clipping_boxes),
                     clipping_box_minimums.data());
        glUniform3fv(uniforms.clipping_box_maximums, static_cast<GLsizei>(maximum_clipping_boxes),
                     clipping_box_maximums.data());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,
                      base_color_texture != nullptr ? base_color_texture->texture() : 0);
        glUniform1i(uniforms.base_color_texture, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, metallic_roughness_texture != nullptr
                                         ? metallic_roughness_texture->texture()
                                         : 0);
        glUniform1i(uniforms.metallic_roughness_texture, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D,
                      occlusion_texture != nullptr ? occlusion_texture->texture() : 0);
        glUniform1i(uniforms.occlusion_texture, 2);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, emissive_texture != nullptr ? emissive_texture->texture() : 0);
        glUniform1i(uniforms.emissive_texture, 3);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(opengl_mesh->index_count()),
                       GL_UNSIGNED_INT, nullptr);

        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::draw_submission_failed,
                         "OpenGL reported an error while submitting an indexed draw"};
        }
        opengl_target->mark_display_stale();
        return {};
    }

    [[nodiscard]] Result<void>
    draw_overlay(graphics::RenderTarget& target,
                 const graphics::DrawOverlayDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (target.backend_resource_token() != opengl_resource_token()) {
            return Error{ErrorCode::backend_mismatch,
                         "Overlay drawing requires an OpenGL render target"};
        }
        auto* opengl_target = static_cast<OpenGLRenderTarget*>(&target);
        if (!opengl_target->is_valid()) {
            return {};
        }

        const Extent2D extent = opengl_target->extent();
        if (extent.width == 0 || extent.height == 0 ||
            (description.lines.empty() && description.markers.empty())) {
            return {};
        }

        const Result<void> resources = ensure_overlay_resources();
        if (!resources) {
            return resources.error();
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, opengl_target->framebuffer());
        glViewport(0, 0, static_cast<GLsizei>(extent.width), static_cast<GLsizei>(extent.height));
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
        glUseProgram(overlay_program_);
        glBindVertexArray(overlay_vertex_array_);
        glBindBuffer(GL_ARRAY_BUFFER, overlay_vertex_buffer_);

        for (const OverlayLineSegment& line : description.lines) {
            const std::optional<OverlayProjectedPoint> start =
                project_overlay_point(description, extent, line.start_world);
            const std::optional<OverlayProjectedPoint> end =
                project_overlay_point(description, extent, line.end_world);
            if (!start.has_value() || !end.has_value()) {
                continue;
            }
            const std::optional<std::array<OverlayVertex, 6>> vertices =
                line_vertices(start.value(), end.value(), extent, line.thickness_pixels);
            if (!vertices.has_value()) {
                continue;
            }
            const Result<void> submit =
                submit_overlay_vertices(vertices.value(), line.color, line.depth_mode);
            if (!submit) {
                return submit.error();
            }
        }

        for (const OverlayPointMarker& marker : description.markers) {
            const std::optional<OverlayProjectedPoint> center =
                project_overlay_point(description, extent, marker.position_world);
            if (!center.has_value()) {
                continue;
            }
            const std::optional<std::array<OverlayVertex, 6>> vertices =
                marker_vertices(center.value(), extent, marker.radius_pixels);
            if (!vertices.has_value()) {
                continue;
            }
            const Result<void> submit =
                submit_overlay_vertices(vertices.value(), marker.color, marker.depth_mode);
            if (!submit) {
                return submit.error();
            }
        }

        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::draw_submission_failed,
                         "OpenGL reported an error while submitting overlay geometry"};
        }
        opengl_target->mark_display_stale();
        return {};
    }

    [[nodiscard]] Result<void>
    draw_picking_indexed(graphics::PickingTarget& target, graphics::StaticMesh& mesh,
                         const graphics::PickingDrawDescription& description) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (target.backend_resource_token() != opengl_resource_token() ||
            mesh.backend_resource_token() != opengl_resource_token()) {
            return Error{ErrorCode::backend_mismatch,
                         "Picking drawing requires resources created by the same OpenGL backend"};
        }
        auto* opengl_target = static_cast<OpenGLPickingTarget*>(&target);
        auto* opengl_mesh = static_cast<OpenGLStaticMesh*>(&mesh);
        if (!opengl_target->is_valid() || opengl_mesh->index_count() == 0 ||
            description.object_id == 0) {
            return {};
        }

        const Result<void> resources = ensure_picking_resources();
        if (!resources) {
            return resources.error();
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, opengl_target->framebuffer());
        const Extent2D extent = opengl_target->extent();
        glViewport(0, 0, static_cast<GLsizei>(extent.width), static_cast<GLsizei>(extent.height));
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
        if (description.double_sided) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }
        glCullFace(GL_BACK);
        glFrontFace(description.front_face_clockwise ? GL_CW : GL_CCW);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(picking_program_);
        glBindVertexArray(opengl_mesh->vertex_array());

        glUniformMatrix4fv(picking_uniforms_.model, 1, GL_FALSE, description.model_matrix.data());
        glUniformMatrix4fv(picking_uniforms_.view, 1, GL_FALSE, description.view_matrix.data());
        glUniformMatrix4fv(picking_uniforms_.projection, 1, GL_FALSE,
                           description.projection_matrix.data());
        glUniform1ui(picking_uniforms_.object_id, description.object_id);
        glUniform1ui(picking_uniforms_.primitive_index, description.primitive_index);
        glUniform1i(picking_uniforms_.clipping_section_plane_enabled,
                    description.clipping_section_plane_enabled ? 1 : 0);
        glUniform3f(picking_uniforms_.clipping_section_plane_normal,
                    description.clipping_section_plane_normal.x,
                    description.clipping_section_plane_normal.y,
                    description.clipping_section_plane_normal.z);
        glUniform1f(picking_uniforms_.clipping_section_plane_offset,
                    description.clipping_section_plane_offset);
        glUniform1i(picking_uniforms_.clipping_retain_positive_half_space,
                    description.clipping_retain_positive_half_space ? 1 : 0);
        const std::uint32_t box_count =
            std::min(description.clipping_box_count, maximum_clipping_boxes);
        std::array<float, maximum_clipping_boxes * 3> clipping_box_minimums{};
        std::array<float, maximum_clipping_boxes * 3> clipping_box_maximums{};
        for (std::uint32_t index = 0; index < box_count; ++index) {
            const Bounds3& box = description.clipping_boxes[index];
            const std::size_t base = static_cast<std::size_t>(index) * 3U;
            clipping_box_minimums[base] = box.minimum.x;
            clipping_box_minimums[base + 1U] = box.minimum.y;
            clipping_box_minimums[base + 2U] = box.minimum.z;
            clipping_box_maximums[base] = box.maximum.x;
            clipping_box_maximums[base + 1U] = box.maximum.y;
            clipping_box_maximums[base + 2U] = box.maximum.z;
        }
        glUniform1i(picking_uniforms_.clipping_box_count, static_cast<GLint>(box_count));
        glUniform3fv(picking_uniforms_.clipping_box_minimums,
                     static_cast<GLsizei>(maximum_clipping_boxes), clipping_box_minimums.data());
        glUniform3fv(picking_uniforms_.clipping_box_maximums,
                     static_cast<GLsizei>(maximum_clipping_boxes), clipping_box_maximums.data());
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(opengl_mesh->index_count()),
                       GL_UNSIGNED_INT, nullptr);

        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::draw_submission_failed,
                         "OpenGL reported an error while submitting a picking draw"};
        }
        return {};
    }

    [[nodiscard]] Result<std::optional<graphics::PickingPixel>>
    read_picking_pixel(graphics::PickingTarget& target, Float2 position_pixels) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (target.backend_resource_token() != opengl_resource_token()) {
            return Error{ErrorCode::backend_mismatch,
                         "Picking readback requires an OpenGL picking target"};
        }
        auto* opengl_target = static_cast<OpenGLPickingTarget*>(&target);
        if (!opengl_target->is_valid()) {
            return std::optional<graphics::PickingPixel>{};
        }

        const Extent2D extent = opengl_target->extent();
        if (!std::isfinite(position_pixels.x) || !std::isfinite(position_pixels.y) ||
            position_pixels.x < 0.0F || position_pixels.y < 0.0F ||
            position_pixels.x >= static_cast<float>(extent.width) ||
            position_pixels.y >= static_cast<float>(extent.height)) {
            return std::optional<graphics::PickingPixel>{};
        }

        RenderStateGuard state_guard;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, opengl_target->framebuffer());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        const GLint read_x = static_cast<GLint>(std::floor(position_pixels.x));
        const GLint read_y =
            static_cast<GLint>(static_cast<std::int64_t>(extent.height) - 1 -
                               static_cast<std::int64_t>(std::floor(position_pixels.y)));
        GLuint ids[4]{0U, 0U, 0U, 0U};
        GLfloat depth = 1.0F;
        glReadPixels(read_x, read_y, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, ids);
        glReadPixels(read_x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        if (glGetError() != GL_NO_ERROR) {
            return Error{ErrorCode::texture_unavailable,
                         "OpenGL reported an error while reading the picking pixel"};
        }

        if (ids[0] == 0U || ids[3] == 0U || depth >= 1.0F) {
            return std::optional<graphics::PickingPixel>{};
        }
        return std::optional<graphics::PickingPixel>{
            graphics::PickingPixel{ids[0], ids[1], ids[2], depth}};
    }

    [[nodiscard]] Result<std::vector<float>>
    read_picking_depths(graphics::PickingTarget& target) override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (target.backend_resource_token() != opengl_resource_token()) {
            return Error{ErrorCode::backend_mismatch,
                         "Picking readback requires an OpenGL picking target"};
        }
        auto* opengl_target = static_cast<OpenGLPickingTarget*>(&target);
        if (!opengl_target->is_valid()) {
            return std::vector<float>{};
        }

        const Extent2D extent = opengl_target->extent();
        const std::size_t width = static_cast<std::size_t>(extent.width);
        const std::size_t height = static_cast<std::size_t>(extent.height);
        if (width == 0U || height == 0U) {
            return std::vector<float>{};
        }
        if (width > std::numeric_limits<std::size_t>::max() / height) {
            return Error{ErrorCode::resource_limit_exceeded,
                         "Picking depth readback dimensions exceed addressable storage"};
        }
        const std::size_t pixel_count = width * height;

        try {
            std::vector<float> depths(pixel_count);

            RenderStateGuard state_guard;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, opengl_target->framebuffer());
            glReadPixels(0, 0, static_cast<GLsizei>(extent.width),
                         static_cast<GLsizei>(extent.height), GL_DEPTH_COMPONENT, GL_FLOAT,
                         depths.data());
            if (glGetError() != GL_NO_ERROR) {
                return Error{ErrorCode::texture_unavailable,
                             "OpenGL reported an error while reading picking depths"};
            }
            return depths;
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "OpenGL picking depth readback failed while allocating storage"};
        }
    }

  private:
    [[nodiscard]] Result<void> ensure_picking_resources() {
        if (picking_program_ != 0) {
            return {};
        }

        AllocationStateGuard allocation_guard;
        Result<GLuint> vertex_result =
            compile_shader(GL_VERTEX_SHADER, picking_vertex_shader_source);
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
        const PickingUniformLocations uniforms{
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
        if (!uniforms.valid()) {
            glDeleteProgram(program);
            return Error{ErrorCode::shader_linking_failed,
                         "The picking shader is missing a required uniform"};
        }
        picking_program_ = program;
        picking_uniforms_ = uniforms;
        return {};
    }

    [[nodiscard]] Result<void> ensure_overlay_resources() {
        if (overlay_program_ != 0 && overlay_vertex_array_ != 0 && overlay_vertex_buffer_ != 0) {
            return {};
        }

        AllocationStateGuard allocation_guard;
        Result<GLuint> vertex_result =
            compile_shader(GL_VERTEX_SHADER, overlay_vertex_shader_source);
        if (!vertex_result) {
            return vertex_result.error();
        }
        const GLuint vertex_shader = vertex_result.value();
        Result<GLuint> fragment_result =
            compile_shader(GL_FRAGMENT_SHADER, overlay_fragment_shader_source);
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
        const OverlayUniformLocations uniforms{glGetUniformLocation(program, "u_color")};
        if (!uniforms.valid()) {
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

        overlay_program_ = program;
        overlay_uniforms_ = uniforms;
        overlay_vertex_array_ = vertex_array;
        overlay_vertex_buffer_ = vertex_buffer;
        return {};
    }

    [[nodiscard]] Result<void> submit_overlay_vertices(const std::array<OverlayVertex, 6>& vertices,
                                                       Color4 color, OverlayDepthMode depth_mode) {
        if (depth_mode == OverlayDepthMode::depth_tested) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        const Color4 sanitized = sanitized_overlay_color(color);
        glUniform4f(overlay_uniforms_.color, sanitized.red, sanitized.green, sanitized.blue,
                    sanitized.alpha);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(sizeof(OverlayVertex) * vertices.size()),
                     vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
        return {};
    }

    void release_picking_resources() noexcept {
        if (!state_->can_destroy_objects()) {
            return;
        }
        if (picking_program_ != 0) {
            glDeleteProgram(picking_program_);
        }
        picking_program_ = 0;
        picking_uniforms_ = {};
    }

    void release_overlay_resources() noexcept {
        if (!state_->can_destroy_objects()) {
            return;
        }
        if (overlay_vertex_buffer_ != 0) {
            glDeleteBuffers(1, &overlay_vertex_buffer_);
        }
        if (overlay_vertex_array_ != 0) {
            glDeleteVertexArrays(1, &overlay_vertex_array_);
        }
        if (overlay_program_ != 0) {
            glDeleteProgram(overlay_program_);
        }
        overlay_vertex_buffer_ = 0;
        overlay_vertex_array_ = 0;
        overlay_program_ = 0;
        overlay_uniforms_ = {};
    }

    std::shared_ptr<OpenGLDeviceState> state_;
    GLuint picking_program_ = 0;
    PickingUniformLocations picking_uniforms_;
    GLuint overlay_program_ = 0;
    GLuint overlay_vertex_array_ = 0;
    GLuint overlay_vertex_buffer_ = 0;
    OverlayUniformLocations overlay_uniforms_;
};

} // namespace

Result<std::unique_ptr<graphics::Device>>
create_device(const OpenGLConfiguration& configuration) noexcept {
    if (configuration.load_procedure == nullptr) {
        return Error{ErrorCode::missing_graphics_procedure_loader, missing_loader_message};
    }

    try {
        static_assert(std::is_same_v<GraphicsProcedure, GLADapiproc>);
        static_assert(std::is_same_v<GraphicsProcedureLoader, GLADloadfunc>);

        const int loaded_version = gladLoadGL(configuration.load_procedure);
        if (loaded_version == 0) {
            return Error{ErrorCode::graphics_initialization_failed, glad_failure_message};
        }
        if (GLAD_GL_VERSION_4_1 == 0) {
            return Error{ErrorCode::unsupported_graphics_version, version_failure_message};
        }
        if (glGetString(GL_VERSION) == nullptr) {
            return Error{ErrorCode::graphics_context_unavailable, context_failure_message};
        }

        GLint profile_mask = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
        if ((profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
            return Error{ErrorCode::unsupported_graphics_version,
                         "Elf3D requires an OpenGL core-profile context"};
        }

        GLint maximum_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture_size);
        if (maximum_texture_size <= 0) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "OpenGL reported an invalid maximum texture size"};
        }

        auto state = std::make_shared<OpenGLDeviceState>(maximum_texture_size);
        return std::unique_ptr<graphics::Device>{std::make_unique<OpenGLDevice>(std::move(state))};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "OpenGL backend initialization threw an exception"};
    }
}

} // namespace elf3d::backend::opengl
