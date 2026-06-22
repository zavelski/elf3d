#include <elf3d/backend/opengl/device_factory.h>

#include <elf3d/graphics/device.h>
#include <elf3d/graphics/texture_handle_access.h>

#include <glad/gl.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

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

struct TextureRecord {
    GLuint texture = 0;
    Extent2D extent;
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

    [[nodiscard]] Result<TextureHandle> register_texture(GLuint texture, Extent2D extent) {
        try {
            std::uint64_t candidate = next_texture_handle_++;
            if (candidate == 0) {
                candidate = next_texture_handle_++;
            }
            texture_records_.emplace(candidate, TextureRecord{texture, extent});
            return detail::TextureHandleAccess::create(candidate);
        } catch (...) {
            return Error{ErrorCode::unexpected_exception,
                         "Failed to register the OpenGL color texture"};
        }
    }

    void unregister_texture(TextureHandle handle) noexcept {
        texture_records_.erase(detail::TextureHandleAccess::value(handle));
    }

    [[nodiscard]] Result<NativeTextureView>
    native_texture_view(TextureHandle handle) const noexcept {
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
    }

    ~AllocationStateGuard() {
        glActiveTexture(static_cast<GLenum>(active_texture_));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_2d_));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(pixel_unpack_buffer_));
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
    }

    AllocationStateGuard(const AllocationStateGuard &) = delete;
    AllocationStateGuard &operator=(const AllocationStateGuard &) = delete;

  private:
    GLint draw_framebuffer_ = 0;
    GLint read_framebuffer_ = 0;
    GLint renderbuffer_ = 0;
    GLint active_texture_ = GL_TEXTURE0;
    GLint texture_2d_ = 0;
    GLint pixel_unpack_buffer_ = 0;
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
    }

    ~RenderStateGuard() {
        set_enabled(GL_SCISSOR_TEST, scissor_enabled_);
        set_enabled(GL_FRAMEBUFFER_SRGB, framebuffer_srgb_enabled_);
        glColorMask(color_mask_[0], color_mask_[1], color_mask_[2], color_mask_[3]);
        glDepthMask(depth_mask_);
        glClearColor(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
        glClearDepth(depth_clear_value_);
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
    }

    RenderStateGuard(const RenderStateGuard &) = delete;
    RenderStateGuard &operator=(const RenderStateGuard &) = delete;

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
};

void delete_objects(GLuint framebuffer, GLuint color_texture, GLuint depth_renderbuffer) noexcept {
    if (depth_renderbuffer != 0) {
        glDeleteRenderbuffers(1, &depth_renderbuffer);
    }
    if (color_texture != 0) {
        glDeleteTextures(1, &color_texture);
    }
    if (framebuffer != 0) {
        glDeleteFramebuffers(1, &framebuffer);
    }
}

class OpenGLRenderTarget final : public graphics::RenderTarget {
  public:
    explicit OpenGLRenderTarget(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLRenderTarget() override {
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
                         "Viewport dimensions exceed the OpenGL texture-size limit"};
        }

        GLuint new_framebuffer = 0;
        GLuint new_color_texture = 0;
        GLuint new_depth_renderbuffer = 0;

        {
            AllocationStateGuard state_guard;

            glGenFramebuffers(1, &new_framebuffer);
            glGenTextures(1, &new_color_texture);
            glGenRenderbuffers(1, &new_depth_renderbuffer);
            if (new_framebuffer == 0 || new_color_texture == 0 || new_depth_renderbuffer == 0) {
                delete_objects(new_framebuffer, new_color_texture, new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_creation_failed,
                             "OpenGL failed to allocate viewport framebuffer objects"};
            }

            glBindTexture(GL_TEXTURE_2D, new_color_texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(extent.width),
                         static_cast<GLsizei>(extent.height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         nullptr);

            glBindRenderbuffer(GL_RENDERBUFFER, new_depth_renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                                  static_cast<GLsizei>(extent.width),
                                  static_cast<GLsizei>(extent.height));

            glBindFramebuffer(GL_FRAMEBUFFER, new_framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   new_color_texture, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      new_depth_renderbuffer);
            constexpr GLenum draw_buffer = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &draw_buffer);

            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                delete_objects(new_framebuffer, new_color_texture, new_depth_renderbuffer);
                return Error{ErrorCode::framebuffer_incomplete,
                             "The OpenGL viewport framebuffer is incomplete"};
            }
        }

        Result<TextureHandle> handle_result = state_->register_texture(new_color_texture, extent);
        if (!handle_result) {
            delete_objects(new_framebuffer, new_color_texture, new_depth_renderbuffer);
            return handle_result.error();
        }

        release();
        framebuffer_ = new_framebuffer;
        color_texture_ = new_color_texture;
        depth_renderbuffer_ = new_depth_renderbuffer;
        color_texture_handle_ = std::move(handle_result).value();
        extent_ = extent;
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
        return {};
    }

    [[nodiscard]] TextureHandle color_texture() const noexcept override {
        return color_texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return framebuffer_ != 0 && color_texture_ != 0 && depth_renderbuffer_ != 0 &&
               color_texture_handle_.is_valid() && extent_.width != 0 && extent_.height != 0;
    }

  private:
    void release() noexcept {
        if (color_texture_handle_.is_valid()) {
            state_->unregister_texture(color_texture_handle_);
        }
        if (state_->can_destroy_objects()) {
            delete_objects(framebuffer_, color_texture_, depth_renderbuffer_);
        }

        framebuffer_ = 0;
        color_texture_ = 0;
        depth_renderbuffer_ = 0;
        color_texture_handle_ = {};
    }

    std::shared_ptr<OpenGLDeviceState> state_;
    Extent2D extent_;
    GLuint framebuffer_ = 0;
    GLuint color_texture_ = 0;
    GLuint depth_renderbuffer_ = 0;
    TextureHandle color_texture_handle_;
};

class OpenGLDevice final : public graphics::Device {
  public:
    explicit OpenGLDevice(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLDevice() override {
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

    [[nodiscard]] Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const override {
        return state_->native_texture_view(texture);
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
};

} // namespace

Result<std::shared_ptr<graphics::Device>>
create_device(const OpenGLConfiguration &configuration) noexcept {
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
        return std::shared_ptr<graphics::Device>{std::make_shared<OpenGLDevice>(std::move(state))};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "OpenGL backend initialization threw an exception"};
    }
}

} // namespace elf3d::backend::opengl
