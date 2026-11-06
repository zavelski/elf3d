#include "device_pipeline_preparation.h"

#include <array>
#include <limits>
#include <new>
#include <utility>

namespace elf3d::backend::opengl::device_detail {
namespace {
using Phase = graphics::PipelinePreparationPhase;
[[nodiscard]] bool valid_source(std::string_view source) noexcept
{
    return !source.empty() &&
           source.size() <= static_cast<std::size_t>(std::numeric_limits<GLint>::max());
}
[[nodiscard]] Error compile_error(GLuint object, bool program)
{
    // Bound diagnostic storage independently of the driver's reported log size.
    std::array<char, 4096> text{};
    GLsizei written = 0;
    if (program)
        glGetProgramInfoLog(object, 4096, &written, text.data());
    else
        glGetShaderInfoLog(object, 4096, &written, text.data());
    const std::string diagnostic{text.data()};
    return {program ? ErrorCode::shader_linking_failed : ErrorCode::shader_compilation_failed,
            "OpenGL program preparation failed: " + diagnostic};
}
class OpenGLPipelinePreparation final : public graphics::GraphicsPipelinePreparation {
  public:
    OpenGLPipelinePreparation(std::shared_ptr<OpenGLDeviceState> state,
                              const graphics::GraphicsPipelineDescription& description,
                              ShaderUniformInterface uniforms)
        : state_(std::move(state)), vertex_source_(description.vertex_shader_source),
          fragment_source_(description.fragment_shader_source), interface_(uniforms)
    {
    }
    ~OpenGLPipelinePreparation() noexcept override;
    [[nodiscard]] Result<graphics::PipelinePreparationRequest> request() const noexcept override;
    [[nodiscard]] Result<graphics::PipelinePreparationStep> advance() noexcept override;
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return calls_;
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    advance_retirement(const graphics::ResourceRetirementBudget& budget) noexcept override;
    [[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>>
    take_pipeline() noexcept override;

  private:
    [[nodiscard]] Result<void> shader_unit() noexcept;
    [[nodiscard]] Result<void> program_unit() noexcept;
    [[nodiscard]] Result<void> uniform_unit() noexcept;
    [[nodiscard]] Result<void> check_status(GLuint object, bool program) noexcept;
    void next_phase() noexcept;
    [[nodiscard]] bool owns_objects() const noexcept;
    void delete_next_object() noexcept;
    std::shared_ptr<OpenGLDeviceState> state_;
    std::string vertex_source_;
    std::string fragment_source_;
    ShaderUniformInterface interface_;
    GLuint vertex_ = 0;
    GLuint fragment_ = 0;
    GLuint program_ = 0;
    UniformLocations uniforms_;
    std::size_t cursor_ = 0;
    Phase phase_ = Phase::vertex_object;
    std::uint64_t calls_ = 1; // Factory context validation.
};
OpenGLPipelinePreparation::~OpenGLPipelinePreparation() noexcept
{
    if (!owns_objects())
        return;
    if (!state_->can_destroy_objects())
        return;
    if (vertex_ != 0)
        glDeleteShader(vertex_);
    if (fragment_ != 0)
        glDeleteShader(fragment_);
    if (program_ != 0)
        glDeleteProgram(program_);
}
Result<graphics::PipelinePreparationRequest> OpenGLPipelinePreparation::request() const noexcept
{
    if (const auto valid = state_->validate_thread(); !valid)
        return valid.error();
    if (phase_ == Phase::failed || phase_ == Phase::consumed)
        return Error{ErrorCode::invalid_argument, "Pipeline preparation failed or was consumed"};
    const bool status = phase_ == Phase::vertex_status || phase_ == Phase::fragment_status ||
                        phase_ == Phase::link_status;
    return graphics::PipelinePreparationRequest{phase_, phase_ == Phase::ready ? 0U
                                                        : status               ? 3U
                                                                               : 2U};
}
void OpenGLPipelinePreparation::next_phase() noexcept
{
    // Called only for the closed, ordered phases preceding ready.
    phase_ = static_cast<Phase>(static_cast<std::uint8_t>(phase_) + 1U);
}
Result<void> OpenGLPipelinePreparation::check_status(GLuint object, bool program) noexcept
{
    GLint status = GL_FALSE;
    if (program)
        glGetProgramiv(object, GL_LINK_STATUS, &status);
    else
        glGetShaderiv(object, GL_COMPILE_STATUS, &status);
    ++calls_;
    if (status == GL_TRUE)
        return {};
    ++calls_; // One bounded info-log query on failure.
    return compile_error(object, program);
}
Result<void> OpenGLPipelinePreparation::shader_unit() noexcept
{
    const bool fragment = phase_ >= Phase::fragment_object;
    auto& object = fragment ? fragment_ : vertex_;
    const auto& source = fragment ? fragment_source_ : vertex_source_;
    const auto first = fragment ? Phase::fragment_object : Phase::vertex_object;
    const auto unit = static_cast<std::uint8_t>(phase_) - static_cast<std::uint8_t>(first);
    if (unit == 3)
        return check_status(object, false);
    ++calls_;
    if (unit == 0) {
        object = glCreateShader(fragment ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
        if (object == 0)
            return Error{ErrorCode::shader_compilation_failed, "OpenGL shader allocation failed"};
    } else if (unit == 1) {
        const GLchar* data = source.data();
        const auto length = static_cast<GLint>(source.size()); // Factory checked representability.
        glShaderSource(object, 1, &data, &length);
    } else {
        glCompileShader(object);
    }
    return {};
}
Result<void> OpenGLPipelinePreparation::program_unit() noexcept
{
    if (phase_ == Phase::link_status)
        return check_status(program_, true);
    ++calls_;
    switch (phase_) {
    case Phase::program_object:
        program_ = glCreateProgram();
        if (program_ == 0)
            return Error{ErrorCode::shader_linking_failed, "OpenGL program allocation failed"};
        break;
    case Phase::attach_vertex:
        glAttachShader(program_, vertex_);
        break;
    case Phase::attach_fragment:
        glAttachShader(program_, fragment_);
        break;
    case Phase::link:
        glLinkProgram(program_);
        break;
    case Phase::release_vertex:
        glDeleteShader(std::exchange(vertex_, 0));
        break;
    case Phase::release_fragment:
        glDeleteShader(std::exchange(fragment_, 0));
        break;
    default:
        return Error{ErrorCode::invalid_argument, "Unexpected program preparation phase"};
    }
    return {};
}
Result<void> OpenGLPipelinePreparation::uniform_unit() noexcept
{
    if (phase_ == Phase::uniforms) {
        query_pipeline_uniform(program_, uniforms_, cursor_++);
        ++calls_;
        if (cursor_ != pipeline_uniform_count())
            return {};
        cursor_ = 0;
        if (!uniforms_.valid())
            return Error{ErrorCode::shader_linking_failed,
                         "The linked shader program is missing a required renderer uniform"};
    } else if (phase_ == Phase::samplers) {
        set_pipeline_sampler(program_, uniforms_, cursor_++);
        ++calls_;
        if (cursor_ != 16)
            return {};
        cursor_ = 0;
    } else {
        ++calls_;
        if (glGetError() != GL_NO_ERROR)
            return Error{ErrorCode::shader_linking_failed, "OpenGL pipeline preparation failed"};
    }
    next_phase();
    return {};
}
Result<graphics::PipelinePreparationStep> OpenGLPipelinePreparation::advance() noexcept
{
    const auto next = request();
    if (!next)
        return next.error();
    if (phase_ == Phase::ready)
        return graphics::PipelinePreparationStep{0, true};
    const auto before = calls_;
    ++calls_;
    if (const auto valid = state_->validate_operation(); !valid)
        return valid.error();
    auto step = phase_ <= Phase::fragment_status    ? shader_unit()
                : phase_ <= Phase::release_fragment ? program_unit()
                                                    : uniform_unit();
    if (!step) {
        phase_ = Phase::failed;
        return step.error();
    }
    if (next.value().phase <= Phase::release_fragment)
        next_phase();
    if (interface_ == ShaderUniformInterface::unbound && phase_ == Phase::uniforms)
        phase_ = Phase::ready;
    return graphics::PipelinePreparationStep{static_cast<std::uint32_t>(calls_ - before),
                                             phase_ == Phase::ready};
}
bool OpenGLPipelinePreparation::owns_objects() const noexcept
{
    return vertex_ != 0 || fragment_ != 0 || program_ != 0;
}
void OpenGLPipelinePreparation::delete_next_object() noexcept
{
    if (vertex_ != 0)
        glDeleteShader(std::exchange(vertex_, 0));
    else if (fragment_ != 0)
        glDeleteShader(std::exchange(fragment_, 0));
    else
        glDeleteProgram(std::exchange(program_, 0));
}
Result<graphics::ResourceRetirementStep> OpenGLPipelinePreparation::advance_retirement(
    const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (const auto valid = state_->validate_thread(); !valid)
        return valid.error();
    if (!owns_objects()) {
        phase_ = Phase::consumed;
        return graphics::ResourceRetirementStep{0, 0, true, false};
    }
    if (budget.objects == 0 || budget.driver_calls < 3)
        return graphics::ResourceRetirementStep{};
    if (const auto valid = state_->validate_operation(); !valid)
        return valid.error();
    delete_next_object();
    phase_ = Phase::failed;
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::shader_linking_failed, "OpenGL pipeline retirement failed"};
    const bool complete = !owns_objects();
    if (complete)
        phase_ = Phase::consumed;
    return graphics::ResourceRetirementStep{1, 3, complete, true};
}
Result<std::unique_ptr<graphics::GraphicsPipeline>>
OpenGLPipelinePreparation::take_pipeline() noexcept
{
    if (phase_ != Phase::ready)
        return Error{ErrorCode::invalid_argument, "The graphics pipeline is not ready"};
    if (const auto valid = state_->validate_thread(); !valid)
        return valid.error();
    phase_ = Phase::consumed;
    return adopt_graphics_pipeline(state_, std::exchange(program_, 0), uniforms_);
}
} // namespace
Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
begin_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                        const graphics::GraphicsPipelineDescription& description) noexcept
{
    if (description.vertex_layout !=
        graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4)
        return Error{ErrorCode::unsupported_vertex_layout,
                     "The PBR pipeline requires the full vertex layout"};
    return begin_shader_program(std::move(state), description, ShaderUniformInterface::pbr);
}
Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
begin_shader_program(std::shared_ptr<OpenGLDeviceState> state,
                     const graphics::GraphicsPipelineDescription& description,
                     ShaderUniformInterface uniforms) noexcept
{
    if (const auto valid = state->validate_operation(); !valid)
        return valid.error();
    if (!valid_source(description.vertex_shader_source) ||
        !valid_source(description.fragment_shader_source))
        return Error{ErrorCode::shader_compilation_failed,
                     "Shader source is empty or exceeds the OpenGL source-length limit"};
    try {
        return std::unique_ptr<graphics::GraphicsPipelinePreparation>{
            std::make_unique<OpenGLPipelinePreparation>(std::move(state), description, uniforms)};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}
} // namespace elf3d::backend::opengl::device_detail
