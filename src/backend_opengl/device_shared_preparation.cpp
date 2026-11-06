#include "device_shared_preparation.h"

#include <elf3d/core/assert.h>
#include <new>
#include <utility>

namespace elf3d::backend::opengl::device_detail {
namespace {
using Phase = ShaderPreparationPhase;
class VertexBinding final {
  public:
    VertexBinding(GLuint array, GLuint buffer) noexcept
    {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &array_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer_);
        glBindVertexArray(array);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
    }
    VertexBinding(const VertexBinding&) = delete;
    VertexBinding& operator=(const VertexBinding&) = delete;
    ~VertexBinding() noexcept
    {
        glBindVertexArray(static_cast<GLuint>(array_));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer_));
    }

  private:
    GLint array_ = 0;
    GLint buffer_ = 0;
};
[[nodiscard]] graphics::GraphicsPipelineDescription sources(ShaderPipelineKind kind) noexcept
{
    switch (kind) {
    case ShaderPipelineKind::picking:
        return picking_program_sources();
    case ShaderPipelineKind::overlay:
        return overlay_program_sources();
    case ShaderPipelineKind::display_resolve:
        return display_program_sources();
    default:
        return {};
    }
}
[[nodiscard]] bool auxiliary_kind(ShaderPipelineKind kind) noexcept
{
    return kind == ShaderPipelineKind::picking || kind == ShaderPipelineKind::overlay ||
           kind == ShaderPipelineKind::display_resolve;
}
[[nodiscard]] Result<void> finish_retirement(SharedGraphicsPreparation& preparation) noexcept
{
    for (;;) {
        const auto step = preparation.advance_retirement({});
        if (!step)
            return step.error();
        if (step.value().complete)
            return {};
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Auxiliary shader retirement did not advance"};
    }
}
} // namespace
SharedGraphicsPreparation::SharedGraphicsPreparation(std::shared_ptr<OpenGLDeviceState> state,
                                                     ShaderPipelineKind kind) noexcept
    : state_(std::move(state)), kind_(kind)
{
}
SharedGraphicsPreparation::~SharedGraphicsPreparation() noexcept
{
    if (!candidate_objects())
        return;
    if (!state_->can_destroy_objects())
        return;
    while (candidate_objects())
        delete_candidate_object(); // At most two fixed vertex objects.
}
bool SharedGraphicsPreparation::complete() const noexcept
{
    return phase_ == Phase::consumed;
}
bool SharedGraphicsPreparation::retired() const noexcept
{
    return complete() && !state_->shared_resources().ready(kind_);
}
Result<graphics::PipelinePreparationRequest> SharedGraphicsPreparation::request() const noexcept
{
    if (const auto valid = state_->validate_thread(); !valid)
        return valid.error();
    if (!auxiliary_kind(kind_))
        return Error{ErrorCode::invalid_argument,
                     "Expected a picking, overlay or display resolve shader"};
    if (failed_ || retired())
        return Error{ErrorCode::invalid_argument,
                     "Auxiliary shader preparation has failed or was retired"};
    if (program_)
        return program_->request();
    std::uint32_t calls = 2;
    if (phase_ == Phase::factory)
        calls = 1;
    if (phase_ == Phase::vertex_attributes)
        calls = 9;
    if (phase_ == Phase::object_retirement)
        calls = 3;
    if (complete())
        calls = 0;
    return graphics::PipelinePreparationRequest{phase_, calls};
}
GLuint SharedGraphicsPreparation::program_name() const noexcept
{
    ELF3D_ASSERT(linked_);
    const auto view = pipeline_view(*linked_);
    ELF3D_ASSERT(view);
    return view.value().program;
}
Result<graphics::PipelinePreparationStep> SharedGraphicsPreparation::advance_program() noexcept
{
    if (!program_) {
        auto created =
            begin_shader_program(state_, sources(kind_), ShaderUniformInterface::unbound);
        if (!created)
            return created.error();
        program_ = std::move(created).value();
        phase_ = Phase::vertex_object;
        return graphics::PipelinePreparationStep{
            static_cast<std::uint32_t>(program_->driver_calls()), false};
    }
    auto step = program_->advance();
    if (!step)
        return step.error();
    if (step.value().ready) {
        auto linked = program_->take_pipeline();
        if (!linked)
            return linked.error();
        linked_ = std::move(linked).value();
        program_.reset();
        const auto name = program_name();
        // Temporary native aliases used only for validation; linked_ still owns the program.
        if (kind_ == ShaderPipelineKind::picking)
            candidate_.picking.program = name;
        else if (kind_ == ShaderPipelineKind::overlay)
            candidate_.overlay.program = name;
        else
            candidate_.display_resolve.program = name;
        phase_ = Phase::uniforms;
        step.value().ready = false;
    }
    return step;
}
void SharedGraphicsPreparation::configure_overlay_attributes() noexcept
{
    const VertexBinding binding{candidate_.overlay.vertex_array, candidate_.overlay.vertex_buffer};
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(OverlayVertex), nullptr);
}
void SharedGraphicsPreparation::advance_uniform_bindings() noexcept
{
    query_auxiliary_uniform(candidate_, kind_, cursor_++, program_name());
    if (cursor_ == auxiliary_uniform_count(kind_))
        phase_ = kind_ == ShaderPipelineKind::picking ? Phase::interface_validation
                                                      : Phase::vertex_array;
}
Result<void> SharedGraphicsPreparation::allocate_vertex_array() noexcept
{
    auto& object = kind_ == ShaderPipelineKind::overlay ? candidate_.overlay.vertex_array
                                                        : candidate_.display_resolve.vertex_array;
    glGenVertexArrays(1, &object);
    if (object == 0)
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "Auxiliary vertex array allocation failed"};
    phase_ =
        kind_ == ShaderPipelineKind::overlay ? Phase::vertex_buffer : Phase::interface_validation;
    return {};
}
Result<void> SharedGraphicsPreparation::execute_object_unit() noexcept
{
    if (phase_ == Phase::uniforms) {
        advance_uniform_bindings();
    } else if (phase_ == Phase::vertex_array) {
        return allocate_vertex_array();
    } else if (phase_ == Phase::vertex_buffer) {
        glGenBuffers(1, &candidate_.overlay.vertex_buffer);
        if (candidate_.overlay.vertex_buffer == 0)
            return Error{ErrorCode::gpu_buffer_creation_failed,
                         "Overlay vertex buffer allocation failed"};
        phase_ = Phase::vertex_attributes;
    } else if (phase_ == Phase::vertex_attributes) {
        configure_overlay_attributes();
        phase_ = Phase::interface_validation;
    } else {
        return validate_and_commit();
    }
    return {};
}
Result<void> SharedGraphicsPreparation::validate_and_commit() noexcept
{
    const auto error = glGetError();
    if (error != GL_NO_ERROR)
        return Error{ErrorCode::shader_linking_failed,
                     "OpenGL auxiliary shader preparation failed"};
    if (!candidate_.ready(kind_))
        return Error{ErrorCode::shader_linking_failed,
                     "Auxiliary shader is missing required uniforms or vertex objects"};
    return commit();
}
Result<void> SharedGraphicsPreparation::commit() noexcept
{
    auto& destination = state_->shared_resources();
    if (destination.ready(kind_)) {
        // A legacy target may have completed the same fixed cache between units.
        // Retire this candidate incrementally without replacing the accepted one.
        phase_ = Phase::object_retirement;
        return {};
    }
    const auto expected = program_name();
    const auto released = release_graphics_program(*linked_);
    if (!released)
        return released.error();
    ELF3D_ASSERT(released.value() == expected);
    if (kind_ == ShaderPipelineKind::picking)
        destination.picking = candidate_.picking;
    else if (kind_ == ShaderPipelineKind::overlay)
        destination.overlay = candidate_.overlay;
    else
        destination.display_resolve = candidate_.display_resolve;
    linked_.reset(); // program ownership has moved to the context's scalar cache.
    candidate_ = {};
    phase_ = Phase::consumed;
    return {};
}
Result<graphics::PipelinePreparationStep> SharedGraphicsPreparation::advance_objects() noexcept
{
    if (const auto valid = state_->validate_operation(); !valid)
        return valid.error();
    const auto calls = phase_ == Phase::vertex_attributes ? 9U : 2U;
    const auto step = execute_object_unit();
    if (!step)
        return step.error();
    return graphics::PipelinePreparationStep{calls, complete()};
}
Result<graphics::PipelinePreparationStep> SharedGraphicsPreparation::advance() noexcept
{
    const auto next = request();
    if (!next)
        return next.error();
    if (complete())
        return graphics::PipelinePreparationStep{0, true};
    if (phase_ == Phase::object_retirement) {
        const auto retired = advance_retirement({});
        if (!retired)
            return retired.error();
        return graphics::PipelinePreparationStep{retired.value().driver_calls,
                                                 retired.value().complete};
    }
    auto step = (phase_ == Phase::factory || program_) ? advance_program() : advance_objects();
    if (!step)
        failed_ = true;
    return step;
}
bool SharedGraphicsPreparation::candidate_objects() const noexcept
{
    return candidate_.overlay.vertex_buffer != 0 || candidate_.overlay.vertex_array != 0 ||
           candidate_.display_resolve.vertex_array != 0;
}
void SharedGraphicsPreparation::delete_candidate_object() noexcept
{
    if (candidate_.overlay.vertex_buffer != 0)
        glDeleteBuffers(1, &candidate_.overlay.vertex_buffer);
    else if (candidate_.overlay.vertex_array != 0)
        glDeleteVertexArrays(1, &candidate_.overlay.vertex_array);
    else
        glDeleteVertexArrays(1, &candidate_.display_resolve.vertex_array);
    if (candidate_.overlay.vertex_buffer != 0)
        candidate_.overlay.vertex_buffer = 0;
    else if (candidate_.overlay.vertex_array != 0)
        candidate_.overlay.vertex_array = 0;
    else
        candidate_.display_resolve.vertex_array = 0;
}
Result<graphics::ResourceRetirementStep> SharedGraphicsPreparation::retire_candidate(
    const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (!candidate_objects()) {
        candidate_ = {};
        failed_ = false;
        phase_ = Phase::consumed;
        return graphics::ResourceRetirementStep{0, 0, true, false};
    }
    if (budget.objects == 0 || budget.driver_calls < 3)
        return graphics::ResourceRetirementStep{};
    if (const auto valid = state_->validate_operation(); !valid)
        return valid.error();
    delete_candidate_object();
    if (glGetError() != GL_NO_ERROR)
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "Auxiliary vertex object retirement failed"};
    return graphics::ResourceRetirementStep{1, 3, false, true};
}
Result<graphics::ResourceRetirementStep> SharedGraphicsPreparation::advance_retirement(
    const graphics::ResourceRetirementBudget& budget) noexcept
{
    if (const auto valid = state_->validate_thread(); !valid)
        return valid.error();
    if (program_) {
        auto step = program_->advance_retirement(budget);
        if (step && step.value().complete) {
            program_.reset();
            step.value().progressed = true;
        }
        if (step)
            step.value().complete = false;
        return step;
    }
    if (linked_) {
        auto step = retire_graphics_pipeline(*linked_, budget);
        if (step && step.value().complete) {
            linked_.reset();
            step.value().progressed = true;
        }
        if (step)
            step.value().complete = false;
        return step;
    }
    return retire_candidate(budget);
}
Result<std::unique_ptr<SharedGraphicsPreparation>>
begin_shared_graphics(std::shared_ptr<OpenGLDeviceState> state, ShaderPipelineKind kind) noexcept
{
    if (const auto valid = state->validate_thread(); !valid)
        return valid.error();
    if (!auxiliary_kind(kind))
        return Error{ErrorCode::invalid_argument, "Expected an auxiliary graphics program"};
    try {
        return std::make_unique<SharedGraphicsPreparation>(std::move(state), kind);
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}
Result<void> ensure_auxiliary_graphics(std::shared_ptr<OpenGLDeviceState> state,
                                       ShaderPipelineKind kind) noexcept
{
    if (state->shared_resources().ready(kind))
        return {};
    SharedGraphicsPreparation preparation{std::move(state), kind};
    for (;;) {
        const auto step = preparation.advance();
        if (!step) {
            const auto error = step.error();
            const auto cleaned = finish_retirement(preparation);
            return cleaned ? Result<void>{error} : Result<void>{cleaned.error()};
        }
        if (step.value().ready)
            return {};
    }
}
} // namespace elf3d::backend::opengl::device_detail
