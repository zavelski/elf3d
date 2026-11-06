#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <array>
#include <assets/assets.h>
#include <cstddef>
#include <cstdint>
#include <graphics/graphics.h>
#include <memory>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <span>
#include <utility>
#include <vector>

#include "renderer_test_support.h"

namespace {

using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeDeviceState;
using elf3d::renderer::tests::FakeRenderTarget;
using elf3d::renderer::tests::TestStudioEnvironmentSource;

constexpr std::uint64_t engine_token = 23;
constexpr std::array<elf3d::VertexPositionNormal, 3> test_vertices{{
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
}};
constexpr std::array<std::uint32_t, 3> test_indices{{0, 1, 2}};

struct EnvironmentContext final {
    elf3d::scene::Storage scene{elf3d::detail::SceneHandleAccess::create_scene(engine_token, 1)};
    elf3d::EntityId camera;
    std::span<FakeDevice> device;
    std::span<TestStudioEnvironmentSource> source;
    std::unique_ptr<elf3d::renderer::Renderer> renderer;
    FakeRenderTarget target;
};

[[nodiscard]] elf3d::renderer::RenderRequest render_request(elf3d::EntityId camera)
{
    return {camera};
}

[[nodiscard]] bool add_model(elf3d::scene::Storage& scene)
{
    const auto mesh = scene.create_mesh({test_vertices, test_indices});
    const auto material = scene.create_material({});
    return mesh && material && scene.create_model(mesh.value(), material.value());
}

[[nodiscard]] bool prepare_context(EnvironmentContext& context, std::vector<std::byte> resource)
{
    const auto camera = context.scene.create_perspective_camera({});
    if (!camera) {
        return false;
    }
    context.camera = camera.value();
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    if (!context.scene.set_local_transform(context.camera, transform)) {
        return false;
    }
    auto device = std::make_unique<FakeDevice>();
    context.device = std::span{device.get(), 1U};
    auto source = std::make_unique<TestStudioEnvironmentSource>(std::move(resource));
    context.source = std::span{source.get(), 1U};
    auto renderer =
        elf3d::renderer::Renderer::create(std::move(device), engine_token, std::move(source));
    if (!renderer) {
        return false;
    }
    context.renderer = std::move(renderer).value();
    return true;
}

[[nodiscard]] bool empty_render_defers_resource(EnvironmentContext& context)
{
    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    const FakeDeviceState& state = context.device.front().state();
    return render && render.value().environment_preparations == 0 &&
           context.source.front().read_count() == 0 && state.cubemap_upload_count == 0 &&
           state.vertex_shader_source.empty() && state.fragment_shader_source.empty();
}

[[nodiscard]] bool unlit_render_defers_resource(EnvironmentContext& context)
{
    auto request = render_request(context.camera);
    request.options.shading_mode = elf3d::RenderShadingMode::unlit;
    const auto render = context.renderer->render(context.scene, context.target, request);
    return render && render.value().environment_preparations == 0 &&
           context.source.front().read_count() == 0 &&
           context.device.front().state().cubemap_upload_count == 0 &&
           context.device.front().state().texture_upload_count == 0;
}

[[nodiscard]] bool standard_resource_is_shared(EnvironmentContext& context)
{
    const auto first =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    const auto borrows = context.source.front().read_count();
    FakeRenderTarget additional_target;
    const auto second =
        context.renderer->render(context.scene, additional_target, render_request(context.camera));
    return first && second && first.value().environment_preparations == 1 &&
           second.value().environment_preparations == 0 && borrows >= 26 &&
           context.source.front().read_count() == borrows &&
           context.device.front().state().cubemap_upload_count == 2 &&
           context.device.front().state().texture_upload_count == 1;
}

[[nodiscard]] int verify_valid_resource_is_lazy_and_shared()
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !empty_render_defers_resource(context) || !add_model(context.scene)) {
        return 58;
    }
    if (!unlit_render_defers_resource(context)) {
        return 59;
    }
    return standard_resource_is_shared(context) ? 0 : 60;
}

[[nodiscard]] bool invalid_resource_is_rejected(std::vector<std::byte> resource)
{
    EnvironmentContext context;
    if (!prepare_context(context, std::move(resource)) || !empty_render_defers_resource(context) ||
        !add_model(context.scene)) {
        return false;
    }

    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    return !render && render.error().code() == elf3d::ErrorCode::graphics_initialization_failed &&
           context.source.front().read_count() > 0 && context.source.front().read_count() <= 26 &&
           context.device.front().state().cubemap_upload_count == 0 &&
           context.device.front().state().texture_upload_count == 0;
}

[[nodiscard]] int verify_invalid_resources()
{
    std::vector<std::byte> invalid_magic = elf3d::renderer::tests::valid_studio_environment_bytes();
    invalid_magic.front() = std::byte{0};
    std::vector<std::byte> invalid_version =
        elf3d::renderer::tests::valid_studio_environment_bytes();
    elf3d::renderer::tests::write_studio_u32(invalid_version, 8, 1);
    std::vector<std::byte> invalid_checksum =
        elf3d::renderer::tests::valid_studio_environment_bytes();
    invalid_checksum.back() = std::byte{1};
    std::vector<std::byte> truncated = elf3d::renderer::tests::valid_studio_environment_bytes();
    truncated.pop_back();
    std::vector<std::byte> oversized = elf3d::renderer::tests::valid_studio_environment_bytes();
    elf3d::renderer::tests::write_studio_u64(oversized, 48, 2U * 1024U * 1024U + 1U);
    return invalid_resource_is_rejected(std::move(invalid_magic)) &&
                   invalid_resource_is_rejected(std::move(invalid_version)) &&
                   invalid_resource_is_rejected(std::move(invalid_checksum)) &&
                   invalid_resource_is_rejected(std::move(truncated)) &&
                   invalid_resource_is_rejected(std::move(oversized))
               ? 0
               : 61;
}

[[nodiscard]] bool upload_failure_is_transactional(bool fail_brdf)
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !empty_render_defers_resource(context) || !add_model(context.scene)) {
        return false;
    }
    if (fail_brdf) {
        context.device.front().fail_texture_upload_at(1);
    } else {
        context.device.front().fail_cubemap_upload_at(2);
    }

    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    return !render && render.error().code() == elf3d::ErrorCode::graphics_initialization_failed &&
           context.device.front().live_cubemap_count() == 0 &&
           context.device.front().live_texture_count() == 0;
}

[[nodiscard]] bool ready_status(const elf3d::SceneGraphicsPreparationStatus& status,
                                unsigned checksum_units, bool expect_failure,
                                unsigned minimum_checksum_units)
{
    return !expect_failure && checksum_units >= minimum_checksum_units &&
           status.environment_ready &&
           status.shared_environment_bytes ==
               elf3d::renderer::tests::studio_environment_payload_bytes;
}
[[nodiscard]] bool prepare_explicit(EnvironmentContext& context, bool expect_failure,
                                    unsigned minimum_checksum_units = 1)
{
    const elf3d::GraphicsPreparationBudget budget{10, 4096, 32, 1};
    unsigned checksum_units = 0;
    for (unsigned units = 0; units < 4096; ++units) {
        const auto advanced = context.renderer->advance_scene_resources(context.scene, budget);
        if (!advanced)
            return expect_failure &&
                   advanced.error().code() == elf3d::ErrorCode::graphics_initialization_failed;
        const auto& report = advanced.value();
        if (report.activity.work_units > 1 || report.activity.driver_calls > 32 ||
            report.activity.uploaded_bytes > 4096)
            return false;
        checksum_units += report.activity.last_step.phase ==
                          elf3d::GraphicsPreparationPhase::environment_checksum;
        if (report.status.state == elf3d::GraphicsPreparationState::ready)
            return ready_status(report.status, checksum_units, expect_failure,
                                minimum_checksum_units);
    }
    return false;
}
[[nodiscard]] bool coherent_before_draw(EnvironmentContext& context)
{
    const auto borrows = context.source.front().read_count();
    const auto rendered =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    return rendered && rendered.value().environment_preparations == 0 &&
           rendered.value().gpu_buffer_uploads == 0 && rendered.value().gpu_texture_uploads == 0 &&
           context.source.front().read_count() == borrows;
}
[[nodiscard]] bool shared_failure_cleared(const EnvironmentContext& context, std::uint32_t deleted,
                                          std::uint32_t expected)
{
    return deleted == expected && context.renderer->retirement_status().shared_texture_bytes == 0 &&
           context.device.front().live_cubemap_count() == 0 &&
           context.device.front().live_texture_count() == 0;
}
[[nodiscard]] bool drain_shared_failure(EnvironmentContext& context, std::uint32_t expected)
{
    const auto pending = context.renderer->retirement_status();
    if (pending.shared_preparations != 1 || pending.shared_texture_bytes == 0)
        return false;
    const auto idle = context.renderer->advance_retirement_unit({0, 0});
    if (!idle || idle.value().progressed)
        return false;
    std::uint32_t deleted = 0;
    for (unsigned units = 0; units < 16; ++units) {
        const auto step = context.renderer->advance_retirement_unit({1, 3});
        if (!step || step.value().driver_calls > 3 || step.value().deleted_objects > 1)
            return false;
        deleted += step.value().deleted_objects;
        if (context.renderer->retirement_status().empty())
            return shared_failure_cleared(context, deleted, expected);
    }
    return false;
}
[[nodiscard]] bool explicit_failure_retries(bool fail_brdf)
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !add_model(context.scene))
        return false;
    if (fail_brdf)
        context.device.front().fail_texture_upload_at(1);
    else
        context.device.front().fail_cubemap_upload_at(2);
    if (!prepare_explicit(context, true) || !drain_shared_failure(context, fail_brdf ? 2 : 1))
        return false;
    context.renderer->release_scene(context.scene.id());
    if (!context.renderer->complete_retirement())
        return false;
    return prepare_explicit(context, false) && coherent_before_draw(context);
}
[[nodiscard]] bool legacy_scene_cleans_shared_failure()
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !add_model(context.scene))
        return false;
    context.device.front().fail_cubemap_upload_at(2);
    if (!prepare_explicit(context, true))
        return false;
    elf3d::scene::Storage other{elf3d::detail::SceneHandleAccess::create_scene(engine_token, 2)};
    const auto camera = other.create_perspective_camera({});
    if (!camera || !add_model(other))
        return false;
    elf3d::Transform transform;
    transform.translation = {0, 0, 3};
    if (!other.set_local_transform(camera.value(), transform))
        return false;
    const auto rendered =
        context.renderer->render(other, context.target, render_request(camera.value()));
    return rendered && context.renderer->retirement_status().empty() &&
           rendered.value().environment_preparations == 1;
}
[[nodiscard]] bool retire_shader_failure(EnvironmentContext& context)
{
    const auto pending = context.renderer->retirement_status();
    if (pending.shared_preparations != 1 || pending.shared_texture_bytes != 0)
        return false;
    const auto idle = context.renderer->advance_retirement_unit({0, 0});
    if (!idle || idle.value().progressed)
        return false;
    const auto retired = context.renderer->advance_retirement_unit({1, 3});
    return retired && retired.value().phase == elf3d::GraphicsRetirementPhase::shader_object &&
           retired.value().deleted_objects == 1 && context.renderer->retirement_status().empty();
}
[[nodiscard]] bool observable_shader_failure(const EnvironmentContext& context,
                                             elf3d::ShaderPipelineKind kind)
{
    const auto status = context.renderer->scene_resource_status(context.scene);
    if (!status.failure)
        return false;
    const auto& failure = *status.failure;
    return failure.shader_pipeline == kind &&
           failure.shader_step == elf3d::ShaderPreparationPhase::vertex_compile &&
           failure.phase == elf3d::GraphicsPreparationPhase::shader_vertex_compile &&
           failure.milliseconds >= 250 && !failure.entity.is_valid() && !failure.texture_slot;
}
[[nodiscard]] bool retained_main_failure(const EnvironmentContext& context, int attempt)
{
    const auto status = context.renderer->scene_resource_status(context.scene);
    const auto kind =
        attempt == 1 ? elf3d::ShaderPipelineKind::core : elf3d::ShaderPipelineKind::extended;
    return status.environment_ready && !status.pipelines_ready &&
           observable_shader_failure(context, kind);
}
[[nodiscard]] bool retained_auxiliary_failure(const EnvironmentContext& context,
                                              elf3d::ShaderPipelineKind kind)
{
    const auto status = context.renderer->scene_resource_status(context.scene);
    return status.environment_ready && status.pipelines_ready && !status.auxiliary_graphics_ready &&
           observable_shader_failure(context, kind);
}
[[nodiscard]] bool shader_failure_retries(int attempt)
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !add_model(context.scene))
        return false;
    context.device.front().state().clock_step_milliseconds = 250;
    context.device.front().fail_pipeline_at(attempt);
    if (!prepare_explicit(context, true) || !retire_shader_failure(context))
        return false;
    if (!retained_main_failure(context, attempt))
        return false;
    context.renderer->release_scene(context.scene.id());
    if (!context.renderer->complete_retirement())
        return false;
    return prepare_explicit(context, false, 0) && coherent_before_draw(context);
}
[[nodiscard]] bool auxiliary_failure_retries(elf3d::ShaderPipelineKind kind)
{
    EnvironmentContext context;
    if (!prepare_context(context, elf3d::renderer::tests::valid_studio_environment_bytes()) ||
        !add_model(context.scene))
        return false;
    context.device.front().state().clock_step_milliseconds = 250;
    context.device.front().fail_auxiliary_at(kind);
    if (!prepare_explicit(context, true) || !retire_shader_failure(context))
        return false;
    if (!retained_auxiliary_failure(context, kind))
        return false;
    context.renderer->release_scene(context.scene.id());
    if (!context.renderer->complete_retirement())
        return false;
    return prepare_explicit(context, false, 0) && coherent_before_draw(context);
}
[[nodiscard]] bool all_auxiliary_failures_retry()
{
    for (const auto kind : {elf3d::ShaderPipelineKind::picking, elf3d::ShaderPipelineKind::overlay,
                            elf3d::ShaderPipelineKind::display_resolve})
        if (!auxiliary_failure_retries(kind))
            return false;
    return true;
}
[[nodiscard]] int verify_staged_resource_failures()
{
    if (!explicit_failure_retries(false) || !explicit_failure_retries(true))
        return 63;
    if (!legacy_scene_cleans_shared_failure())
        return 64;
    if (!shader_failure_retries(1) || !shader_failure_retries(2))
        return 65;
    return all_auxiliary_failures_retry() ? 0 : 66;
}
} // namespace

int elf3d_renderer_environment_failure_test()
{
    const int valid = verify_valid_resource_is_lazy_and_shared();
    if (valid != 0) {
        return valid;
    }

    const int invalid = verify_invalid_resources();
    if (invalid != 0) {
        return invalid;
    }
    if (!upload_failure_is_transactional(false) || !upload_failure_is_transactional(true))
        return 62;
    return verify_staged_resource_failures();
}
