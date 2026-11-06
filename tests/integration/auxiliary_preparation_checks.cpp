#include "auxiliary_preparation_checks.h"
#include <backend_opengl/device_shared_preparation.h>

#include <array>
#include <cmath>
#include <memory>

namespace elf3d::backend::opengl::device_detail {
[[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
create_render_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D extent) noexcept;
}
namespace {
using namespace elf3d;
using namespace elf3d::graphics;
using namespace elf3d::backend::opengl::device_detail;
using Phase = ShaderPreparationPhase;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::shader_linking_failed, message};
}
class NativeState final {
  public:
    ~NativeState() noexcept
    {
        state_->shut_down();
    }
    [[nodiscard]] const std::shared_ptr<OpenGLDeviceState>& state() const noexcept
    {
        return state_;
    }
    [[nodiscard]] bool exclusive() const noexcept
    {
        return state_.use_count() == 1;
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_ = std::make_shared<OpenGLDeviceState>(4096);
};
class ForeignVertexState final {
  public:
    ForeignVertexState() noexcept
    {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_array_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &old_buffer_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &old_program_);
        glGenVertexArrays(1, &array_);
        glGenBuffers(1, &buffer_);
        glBindVertexArray(array_);
        glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    }
    ~ForeignVertexState() noexcept
    {
        glUseProgram(static_cast<GLuint>(old_program_));
        glBindVertexArray(static_cast<GLuint>(old_array_));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(old_buffer_));
        glDeleteVertexArrays(1, &array_);
        glDeleteBuffers(1, &buffer_);
    }
    ForeignVertexState(const ForeignVertexState&) = delete;
    ForeignVertexState& operator=(const ForeignVertexState&) = delete;
    [[nodiscard]] bool unchanged() const noexcept
    {
        GLint array = 0;
        GLint buffer = 0;
        GLint program = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &array);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        return static_cast<GLuint>(array) == array_ && static_cast<GLuint>(buffer) == buffer_ &&
               program == old_program_;
    }

  private:
    GLint old_array_ = 0;
    GLint old_buffer_ = 0;
    GLint old_program_ = 0;
    GLuint array_ = 0;
    GLuint buffer_ = 0;
};
[[nodiscard]] bool bounded_and_restored(const PipelinePreparationRequest& request,
                                        const PipelinePreparationStep& step,
                                        const ForeignVertexState& foreign)
{
    return step.driver_calls <= request.driver_call_limit && foreign.unchanged();
}
void count_phase(Phase phase, std::array<std::size_t, 3>& counts)
{
    if (phase == Phase::vertex_compile || phase == Phase::fragment_compile)
        ++counts[0];
    if (phase == Phase::link)
        ++counts[1];
    if (phase == Phase::uniforms)
        ++counts[2];
}
[[nodiscard]] Result<void> warm(Device& device, ShaderPipelineKind kind)
{
    std::array<std::size_t, 3> counts{};
    const ForeignVertexState foreign;
    for (unsigned unit = 0; unit < 128; ++unit) {
        const auto request = device.request_auxiliary_graphics(kind);
        if (!request)
            return request.error();
        const auto step = device.advance_auxiliary_graphics(kind);
        if (!step)
            return step.error();
        if (!bounded_and_restored(request.value(), step.value(), foreign))
            return failure(
                "Auxiliary shader exceeded its call allowance or changed foreign bindings");
        count_phase(request.value().phase, counts);
        if (step.value().ready) {
            if (counts != std::array<std::size_t, 3>{2, 1, auxiliary_uniform_count(kind)})
                return failure("Auxiliary shader compilation or bindings were not separate units");
            const auto repeated = device.advance_auxiliary_graphics(kind);
            return repeated && repeated.value().ready && repeated.value().driver_calls == 0
                       ? Result<void>{}
                       : Result<void>{failure("Ready auxiliary cache performed new work")};
        }
    }
    return failure("Auxiliary shader preparation did not finish");
}
[[nodiscard]] Result<void> retire(SharedGraphicsPreparation& preparation)
{
    const auto zero = preparation.advance_retirement({0, 0});
    if (!zero || zero.value().deleted_objects != 0 || zero.value().driver_calls != 0)
        return failure("Zero auxiliary retirement allowance deleted an object");
    for (unsigned unit = 0; unit < 12; ++unit) {
        const auto step = preparation.advance_retirement({1, 3});
        if (!step)
            return step.error();
        if (step.value().deleted_objects > 1 || step.value().driver_calls > 3)
            return failure("Auxiliary retirement exceeded its native object/call bound");
        if (step.value().complete)
            return {};
        if (!step.value().progressed)
            return failure("Auxiliary retirement failed to publish metadata progress");
    }
    return failure("Auxiliary preparation retirement did not finish");
}
[[nodiscard]] Result<void> abandoned(unsigned units)
{
    NativeState owner;
    {
        SharedGraphicsPreparation preparation{owner.state(), ShaderPipelineKind::overlay};
        for (unsigned unit = 0; unit < units; ++unit) {
            const auto step = preparation.advance();
            if (!step)
                return step.error();
            if (step.value().ready)
                return failure("Abandon fixture unexpectedly committed its cache");
        }
        if (const auto cleaned = retire(preparation); !cleaned)
            return cleaned.error();
        if (preparation.request())
            return failure("Retired auxiliary preparation remained usable");
    }
    return owner.exclusive() &&
                   !owner.state()->shared_resources().ready(ShaderPipelineKind::overlay)
               ? Result<void>{}
               : Result<void>{
                     failure("Abandoned auxiliary preparation retained an owner or cache")};
}
[[nodiscard]] Result<void> finish_preparation(SharedGraphicsPreparation& preparation)
{
    for (unsigned unit = 0; unit < 128; ++unit) {
        const auto request = preparation.request();
        if (!request)
            return request.error();
        const auto step = preparation.advance();
        if (!step)
            return step.error();
        if (step.value().driver_calls > request.value().driver_call_limit)
            return failure("Redundant shader cleanup exceeded its call allowance");
        if (step.value().ready)
            return {};
    }
    return failure("Redundant auxiliary candidate did not finish retirement");
}
[[nodiscard]] Result<void> superseded()
{
    NativeState owner;
    {
        SharedGraphicsPreparation preparation{owner.state(), ShaderPipelineKind::display_resolve};
        for (unsigned unit = 0; unit < 2; ++unit) {
            const auto step = preparation.advance();
            if (!step)
                return step.error();
        }
        auto target = create_render_target(owner.state(), {4, 4});
        if (!target)
            return target.error();
        if (const auto cleared = target.value()->clear({0.25F, 0.25F, 0.25F, 1}); !cleared)
            return cleared.error();
        std::array<std::uint8_t, 64> pixels{};
        if (const auto read = target.value()->read_color_pixels(pixels); !read)
            return read.error();
        const auto accepted = owner.state()->shared_resources().display_resolve.program;
        if (const auto finished = finish_preparation(preparation); !finished)
            return finished.error();
        if (owner.state()->shared_resources().display_resolve.program != accepted ||
            glIsProgram(accepted) != GL_TRUE)
            return failure(
                "Interleaved legacy resolve replaced or deleted the accepted shared program");
    }
    return owner.exclusive() ? Result<void>{}
                             : Result<void>{failure("Shared cache owns a teardown-state cycle")};
}
[[nodiscard]] Result<void> read_expected(RenderTarget& target, float exposure)
{
    DisplayTransform transform;
    transform.exposure_ev = exposure;
    transform.tone_mapping = ToneMappingMode::none;
    target.set_display_transform(transform);
    constexpr Color4 linear{0.125F, 0.125F, 0.125F, 1};
    if (const auto cleared = target.clear(linear); !cleared)
        return cleared.error();
    std::array<std::uint8_t, 64> pixels{};
    if (const auto read = target.read_color_pixels(pixels); !read)
        return read.error();
    const auto expected =
        static_cast<int>(std::lround(resolve_display_color(linear, transform).red * 255.0F));
    for (std::size_t pixel = 0; pixel < 16; ++pixel)
        if (std::abs(int{pixels[pixel * 4]} - expected) > 1)
            return failure("Shared display shader mixed independent target color/exposure state");
    return {};
}
[[nodiscard]] Result<void> independent_targets(Device& device)
{
    auto first = device.create_render_target({4, 4});
    auto second = device.create_render_target({4, 4});
    if (!first)
        return first.error();
    if (!second)
        return second.error();
    if (first.value()->color_texture() == second.value()->color_texture())
        return failure("Independent targets share their display texture");
    if (const auto read = read_expected(*first.value(), 0); !read)
        return read.error();
    if (const auto read = read_expected(*second.value(), 1); !read)
        return read.error();
    return read_expected(*first.value(), 0);
}
} // namespace
elf3d::Result<void> verify_auxiliary_preparation(elf3d::graphics::Device& device)
{
    for (const auto kind : {ShaderPipelineKind::picking, ShaderPipelineKind::overlay,
                            ShaderPipelineKind::display_resolve})
        if (const auto ready = warm(device, kind); !ready)
            return ready.error();
    if (!device.auxiliary_graphics_status().complete())
        return failure("Auxiliary cache is incomplete after warmup");
    for (const unsigned unit : {1U, 2U, 8U, 13U, 16U, 20U})
        if (const auto cleaned = abandoned(unit); !cleaned)
            return cleaned.error();
    if (const auto interleaved = superseded(); !interleaved)
        return interleaved.error();
    return independent_targets(device);
}
