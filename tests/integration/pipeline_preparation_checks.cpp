#include "pipeline_preparation_checks.h"
#include "auxiliary_preparation_checks.h"
#include <backend_opengl/device_pipeline_preparation.h>
#include <renderer/renderer_detail.h>

#include <array>
#include <string>

namespace {
using namespace elf3d;
using namespace elf3d::graphics;
using Phase = PipelinePreparationPhase;
constexpr auto layout =
    VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4;
[[nodiscard]] Error failure(const char* text)
{
    return {ErrorCode::shader_linking_failed, text};
}
[[nodiscard]] bool retirement_repeats(GraphicsPipelinePreparation& preparation)
{
    const auto repeated = preparation.advance_retirement({0, 0});
    return repeated && repeated.value().complete && repeated.value().deleted_objects == 0;
}
[[nodiscard]] Result<void> retire(GraphicsPipelinePreparation& preparation)
{
    const auto before = preparation.driver_calls();
    const auto zero = preparation.advance_retirement({0, 0});
    if (!zero || zero.value().deleted_objects != 0 || preparation.driver_calls() != before)
        return failure("Zero pipeline retirement allowance performed native deletion");
    for (std::size_t unit = 0; unit < 4; ++unit) {
        auto step = preparation.advance_retirement({1, 3});
        if (!step)
            return step.error();
        if (step.value().deleted_objects > 1 || step.value().driver_calls > 3)
            return failure("Pipeline retirement exceeded one-object allowance");
        if (step.value().complete) {
            return retirement_repeats(preparation)
                       ? Result<void>{}
                       : Result<void>{failure("Pipeline retirement is not idempotent")};
        }
    }
    return failure("Pipeline retirement did not release its three native owners");
}
[[nodiscard]] Result<void> check_failure(Device& device, std::string_view vertex,
                                         std::string_view fragment, Phase expected)
{
    auto created = device.begin_graphics_pipeline({vertex, fragment, layout});
    if (!created)
        return created.error();
    auto& preparation = *created.value();
    if (preparation.take_pipeline())
        return failure("Incomplete native pipeline was adopted");
    for (std::size_t unit = 0; unit < 128; ++unit) {
        const auto request = preparation.request();
        if (!request)
            return request.error();
        const auto before = preparation.driver_calls();
        const auto step = preparation.advance();
        if (preparation.driver_calls() - before > request.value().driver_call_limit)
            return failure("Failed shader step exceeded its native call bound");
        if (!step) {
            if (request.value().phase != expected || preparation.request())
                return failure("Pipeline error occurred in an unexpected phase or allowed a retry");
            return retire(preparation);
        }
    }
    return failure("Invalid shader unexpectedly completed or never failed");
}
void count_phase(Phase phase, std::array<std::size_t, 3>& counts)
{
    if (phase == Phase::vertex_compile || phase == Phase::fragment_compile)
        ++counts[0];
    if (phase == Phase::uniforms)
        ++counts[1];
    if (phase == Phase::samplers)
        ++counts[2];
}
[[nodiscard]] Result<void> adopt_ready(GraphicsPipelinePreparation& preparation,
                                       const std::array<std::size_t, 3>& counts)
{
    if (counts !=
        std::array<std::size_t, 3>{2, backend::opengl::device_detail::pipeline_uniform_count(), 16})
        return failure("Shader compilation/uniform work was not split into individual units");
    auto pipeline = preparation.take_pipeline();
    if (!pipeline || preparation.take_pipeline())
        return failure("Ready shader owner transfer failed or was repeatable");
    const auto view = backend::opengl::device_detail::pipeline_view(*pipeline.value());
    if (!view || glIsProgram(view.value().program) != GL_TRUE)
        return failure("Prepared pipeline did not own a linked OpenGL program");
    return {};
}
[[nodiscard]] Result<void> check_success(Device& device)
{
    Result<std::unique_ptr<GraphicsPipelinePreparation>> created = [&]() {
        // Both strings expire before the first native work unit.
        std::string vertex{renderer::main_vertex_shader_source()};
        std::string fragment{renderer::main_fragment_shader_source()};
        return device.begin_graphics_pipeline({vertex, fragment, layout});
    }();
    if (!created)
        return created.error();
    auto& preparation = *created.value();
    std::array<std::size_t, 3> counts{};
    for (std::size_t unit = 0; unit < 128; ++unit) {
        const auto request = preparation.request();
        if (!request)
            return request.error();
        auto step = preparation.advance();
        if (!step)
            return step.error();
        if (step.value().driver_calls > request.value().driver_call_limit)
            return failure("Shader step exceeded its native call bound");
        count_phase(request.value().phase, counts);
        if (step.value().ready)
            return adopt_ready(preparation, counts);
    }
    return failure("Native pipeline preparation did not finish");
}
} // namespace
elf3d::Result<void> verify_staged_pipeline(elf3d::graphics::Device& device)
{
    const auto vertex = renderer::main_vertex_shader_source();
    const auto fragment = renderer::main_fragment_shader_source();
    if (const auto valid = check_success(device); !valid)
        return valid.error();
    if (const auto bad =
            check_failure(device, "#version 410 core\ninvalid", fragment, Phase::vertex_status);
        !bad)
        return bad.error();
    if (const auto bad =
            check_failure(device, vertex, "#version 410 core\ninvalid", Phase::fragment_status);
        !bad)
        return bad.error();
    constexpr auto simple_vertex = "#version 410 core\nout vec3 varying_value;\nvoid "
                                   "main(){varying_value=vec3(1);gl_Position=vec4(0,0,0,1);}";
    constexpr auto mismatch = "#version 410 core\nin vec4 varying_value;\nout vec4 color;\nvoid "
                              "main(){color=varying_value;}";
    if (const auto bad = check_failure(device, simple_vertex, mismatch, Phase::link_status); !bad)
        return bad.error();
    constexpr auto missing = "#version 410 core\nin vec3 varying_value;\nout vec4 color;\nvoid "
                             "main(){color=vec4(varying_value,1);}";
    if (const auto bad = check_failure(device, simple_vertex, missing, Phase::uniforms); !bad)
        return bad.error();
    return verify_auxiliary_preparation(device);
}
