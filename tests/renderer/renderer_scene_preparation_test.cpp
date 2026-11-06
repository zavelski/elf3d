#include "renderer_test_support.h"
#include <renderer/renderer.h>
#include <renderer/renderer_vertex_packing.h>
#include <scene/scene.h>

#include <array>

namespace {
using namespace elf3d;
using namespace elf3d::renderer;
using namespace elf3d::renderer::tests;
constexpr std::uint64_t owner = 11;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::invalid_argument, message};
}
[[nodiscard]] PrimitiveData triangle()
{
    PrimitiveData data;
    data.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
    data.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    data.texcoord0 = {{0, 0}, {1, 0}, {.5F, 1}};
    data.indices = {0, 1, 2};
    return data;
}
[[nodiscard]] Result<TextureId> create_texture(Document& document)
{
    const std::array<std::byte, 4> unused{std::byte{255}, std::byte{0}, std::byte{0},
                                          std::byte{255}};
    for (std::size_t i = 0; i < 512; ++i) {
        if (!document.create_image({1, 1, PixelFormat::rgba8_unorm, unused}))
            return failure("Cannot create sparse image-table fixture");
    }
    std::vector<std::byte> pixels(1025U * 769U * 4U, std::byte{192});
    const auto image = document.create_image({1025, 769, PixelFormat::rgba8_unorm, pixels});
    const auto sampler = document.create_sampler({});
    if (!image || !sampler)
        return failure("Cannot create large shared texture fixture");
    return document.create_texture({image.value(), sampler.value()});
}
struct Fixture {
    explicit Fixture(std::uint64_t ordinal = 9)
        : scene(detail::SceneHandleAccess::create_scene(owner, ordinal))
    {
    }
    scene::Storage scene;
    EntityId camera;
};
[[nodiscard]] Result<void> prepare(Fixture& f)
{
    Document document;
    const auto texture = create_texture(document);
    if (!texture)
        return texture.error();
    ModelMaterialDescription textured;
    textured.base_color_texture = texture.value();
    textured.metallic_roughness_texture = texture.value();
    textured.occlusion_texture = texture.value();
    const auto material = document.create_material(textured).value();
    const auto plain = document.create_material({}).value();
    const auto shared = document.create_mesh("Shared mesh").value();
    const auto hidden = document.create_mesh("Hidden mesh").value();
    const auto first = document.create_primitive(shared, material, triangle()).value();
    const auto second = document.create_primitive(hidden, plain, triangle()).value();
    if (!f.scene.set_document(std::move(document)))
        return failure("Cannot adopt preparation fixture Document");
    for (std::size_t i = 0; i < 3; ++i) {
        const auto entity = f.scene.create_entity().value();
        const std::array primitives{i == 2 ? second : first};
        if (!f.scene.set_model_document_primitives(entity, primitives))
            return failure("Cannot bind preparation fixture geometry");
        if (i == 2 && !f.scene.set_entity_visible(entity, false))
            return failure("Cannot hide preparation fixture geometry");
    }
    f.camera = f.scene.create_perspective_camera({}).value();
    Transform camera;
    camera.translation = {0, 0, 3};
    return f.scene.set_local_transform(f.camera, camera);
}
[[nodiscard]] Result<void> check_admission(Renderer& renderer, const Fixture& f)
{
    const auto idle = renderer.advance_scene_resources(f.scene, {.milliseconds = 0});
    if (!idle || idle.value().status.state != GraphicsPreparationState::cpu_ready)
        return failure("Zero time allowance admitted Scene preparation");
    const auto admitted = renderer.advance_scene_resources(f.scene, {.work_units = 1});
    if (!admitted || admitted.value().status.state != GraphicsPreparationState::uploading)
        return failure("Scene preparation did not retain its admission");
    FakeRenderTarget target;
    RenderRequest request;
    request.camera = f.camera;
    if (renderer.render(f.scene, target, request) || target.clear_count != 0)
        return failure("An incomplete Scene reached rendering");
    const scene::Storage foreign{detail::SceneHandleAccess::create_scene(owner + 1, 9)};
    if (renderer.advance_scene_resources(foreign, {}))
        return failure("A foreign Scene entered this Renderer preparation");
    return {};
}
[[nodiscard]] bool cube_coordinates_valid(const GraphicsPreparationActivity::Step& step)
{
    const auto levels = step.environment_texture == EnvironmentTextureKind::diffuse ? 1U : 8U;
    return step.mip_level && step.cube_face && *step.mip_level < levels && *step.cube_face < 6;
}
[[nodiscard]] bool environment_locator_valid(const GraphicsPreparationActivity::Step& step)
{
    if (step.phase != GraphicsPreparationPhase::environment_allocation &&
        step.phase != GraphicsPreparationPhase::environment_upload)
        return true;
    if (!step.environment_texture || step.entity.is_valid() || step.texture_slot)
        return false;
    if (step.environment_texture == EnvironmentTextureKind::brdf_lut)
        return !step.mip_level && !step.cube_face;
    return cube_coordinates_valid(step);
}
[[nodiscard]] bool shader_locator_valid(const GraphicsPreparationActivity::Step& step)
{
    if (step.driver_calls == 0)
        return true; // Metadata-only cache transitions have no native call.
    if (!step.shader_pipeline)
        return !step.shader_step;
    return step.shader_step && !step.entity.is_valid() && !step.texture_slot &&
           !step.environment_texture;
}
[[nodiscard]] Result<void> check_report(const SceneGraphicsPreparationReport& report,
                                        const GraphicsPreparationBudget& budget)
{
    const auto& work = report.activity;
    if (work.work_units > budget.work_units || work.driver_calls > budget.driver_calls ||
        work.uploaded_bytes > budget.upload_bytes || work.packed_bytes > maximum_vertex_pack_bytes)
        return failure("Scene graphics preparation exceeded a service allowance");
    if ((work.allocated_bytes != 0 || work.uploaded_bytes != 0) &&
        !environment_locator_valid(work.last_step))
        return failure("Environment allocation/upload activity lost its resource coordinates");
    if (!shader_locator_valid(work.last_step))
        return failure("Shader activity lost its program kind or exact native phase");
    return {};
}
struct ExpectedResources {
    std::uint64_t primitives = 3;
    std::uint64_t meshes = 2;
    std::uint64_t textures = 2;
};
[[nodiscard]] bool matches_resources(const SceneGraphicsPreparationStatus& status,
                                     const ExpectedResources& expected)
{
    return status.completed_primitives == expected.primitives &&
           status.completed_meshes == expected.meshes &&
           status.completed_textures == expected.textures && status.environment_ready &&
           status.pipelines_ready && status.auxiliary_graphics_ready &&
           status.shared_environment_bytes == studio_environment_payload_bytes;
}
[[nodiscard]] Result<void> complete(Renderer& renderer, const Fixture& f,
                                    const ExpectedResources& expected = {})
{
    const GraphicsPreparationBudget budget{10, 4096, 32, 1};
    for (std::size_t i = 0; i < 4096; ++i) {
        const auto report = renderer.advance_scene_resources(f.scene, budget);
        if (!report)
            return report.error();
        if (const auto bounds = check_report(report.value(), budget); !bounds)
            return bounds.error();
        if (report.value().status.state == GraphicsPreparationState::ready) {
            const auto& status = report.value().status;
            if (!matches_resources(status, expected))
                return failure("Preparation missed hidden geometry or duplicated shared resources");
            return {};
        }
        if (report.value().activity.work_units == 0)
            return failure("Preparation stopped despite sufficient service allowances");
    }
    return failure("Scene preparation did not complete bounded service calls");
}
[[nodiscard]] Result<void> verify_ready(Renderer& renderer, const Fixture& f)
{
    const auto repeated = renderer.advance_scene_resources(f.scene, {});
    if (!repeated || repeated.value().activity.work_units != 0 ||
        repeated.value().activity.driver_calls != 0)
        return failure("Ready Scene repeated preparation work");
    FakeRenderTarget target;
    RenderRequest request;
    request.camera = f.camera;
    request.options.shading_mode = RenderShadingMode::unlit;
    const auto rendered = renderer.render(f.scene, target, request);
    if (!rendered || rendered.value().gpu_buffer_uploads != 0 ||
        rendered.value().gpu_texture_uploads != 0)
        return failure("Ready Scene had a synchronous geometry/image cache miss");
    return {};
}
[[nodiscard]] Result<void> verify_reprepare(Renderer& renderer, Fixture& f)
{
    const auto data = triangle();
    const std::array vertices{VertexPositionNormal{data.positions[0], data.normals[0]},
                              VertexPositionNormal{data.positions[1], data.normals[1]},
                              VertexPositionNormal{data.positions[2], data.normals[2]}};
    const auto mesh = f.scene.create_mesh(MeshDataView{vertices, data.indices});
    if (!mesh)
        return mesh.error();
    const auto material = f.scene.create_material({});
    if (!material || !f.scene.create_model(mesh.value(), material.value()))
        return failure("Cannot extend the prepared Scene");
    if (renderer.validate_scene_resources(f.scene) ||
        renderer.scene_resource_status(f.scene).state == GraphicsPreparationState::ready)
        return failure("Changed Scene content retained stale readiness");
    if (const auto completed = complete(renderer, f, {4, 1, 0}); !completed)
        return completed.error();
    return verify_ready(renderer, f);
}
[[nodiscard]] bool clean_retirement_state(const GraphicsRetirementStatus& status,
                                          std::uint32_t deleted, std::uint32_t expected) noexcept
{
    return deleted == expected && status.mesh_entries == 0 && status.texture_entries == 0 &&
           status.estimated_geometry_bytes == 0 && status.estimated_texture_bytes == 0 &&
           status.staging_bytes == 0;
}
[[nodiscard]] Result<void> drain_retirement(Renderer& renderer, std::uint32_t expected_objects)
{
    std::uint32_t deleted = 0;
    for (std::size_t i = 0; i < 128; ++i) {
        const auto step = renderer.advance_retirement_unit({1, 3});
        if (!step)
            return step.error();
        if (step.value().driver_calls > 3 || step.value().deleted_objects > 1)
            return failure("Renderer retirement exceeded one object's allowance");
        deleted += step.value().deleted_objects;
        const auto status = renderer.retirement_status();
        if (status.empty()) {
            if (!clean_retirement_state(status, deleted, expected_objects))
                return failure("Renderer retirement did not return to its resource baseline");
            return {};
        }
        if (!step.value().progressed)
            return failure("Renderer retirement stalled with a valid allowance");
    }
    return failure("Renderer retirement did not complete bounded units");
}
[[nodiscard]] bool shared_cache_preserved(const FakeDevice& device)
{
    return device.live_texture_count() == 1 && device.live_cubemap_count() == 2;
}
[[nodiscard]] Result<void> verify_retirement_failure(Renderer& renderer, const Fixture& fixture,
                                                     FakeDevice& device)
{
    device.state().clock_step_milliseconds = 250;
    device.state().fail_next_mesh_retirement = true;
    const auto failed = renderer.advance_retirement_unit({1, 3});
    device.state().clock_step_milliseconds = 0.125;
    if (failed)
        return failure("Retirement fault was not propagated");
    const auto status = renderer.retirement_status();
    if (!status.last_failure)
        return failure("Retirement fault lost its diagnostics");
    const auto& trace = *status.last_failure;
    if (trace.phase != GraphicsRetirementPhase::mesh_object || trace.scene != fixture.scene.id() ||
        trace.milliseconds < 250 || trace.driver_call_limit != 3)
        return failure("Retirement fault lost its phase, owner, duration or call allowance");
    return {};
}
[[nodiscard]] bool new_uploads_blocked(Renderer& renderer, const Fixture& fixture,
                                       const FakeDevice& device, int uploads)
{
    const auto blocked = renderer.advance_scene_resources(fixture.scene, {});
    return blocked && blocked.value().status.state != GraphicsPreparationState::ready &&
           device.state().upload_count == uploads;
}
[[nodiscard]] Result<void> verify_retirement(Renderer& renderer, const Fixture& f)
{
    auto& device = static_cast<FakeDevice&>(renderer.device());
    const auto uploads = device.state().upload_count;
    renderer.release_scene(f.scene.id());
    const auto pending = renderer.retirement_status();
    if (pending.renderer_scenes != 1 || pending.mesh_entries != 3 || pending.texture_entries != 2 ||
        device.live_texture_count() != 3)
        return failure("Scene release did not retain detached GPU ownership");
    if (const auto traced = verify_retirement_failure(renderer, f, device); !traced)
        return traced.error();
    if (!new_uploads_blocked(renderer, f, device, uploads))
        return failure("New model uploads started before the retired generation drained");
    if (const auto drained = drain_retirement(renderer, 11); !drained)
        return drained.error();
    if (!shared_cache_preserved(device))
        return failure("Model retirement changed the independently cached environment");
    return {};
}
[[nodiscard]] Result<void> verify_pending_retirement(Renderer& renderer)
{
    {
        Fixture candidate{10};
        if (const auto prepared = prepare(candidate); !prepared)
            return prepared.error();
        bool packed = false;
        for (std::size_t i = 0; i < 64; ++i) {
            const auto step = renderer.advance_scene_resources(candidate.scene, {.work_units = 1});
            if (!step)
                return step.error();
            if (step.value().activity.packed_bytes != 0) {
                packed = true;
                break;
            }
        }
        if (!packed)
            return failure("Cancellation fixture did not reach owned mesh staging");
        renderer.release_scene(candidate.scene.id());
    } // The source Document/Storage dies before pending preparation retirement.
    if (renderer.retirement_status().staging_bytes == 0)
        return failure("Retirement lost its pending CPU staging allocation");
    const auto staged = renderer.advance_retirement_unit({0, 0});
    if (!staged || staged.value().freed_cpu_bytes == 0 || staged.value().driver_calls != 0)
        return failure("Mesh staging was not released as an explicit CPU unit");
    return drain_retirement(renderer, 3);
}
[[nodiscard]] bool preparation_failure_matches(const SceneGraphicsPreparationStatus& status,
                                               bool mesh)
{
    if (!status.failure || status.state != GraphicsPreparationState::failed)
        return false;
    const auto& trace = *status.failure;
    const auto phase =
        mesh ? GraphicsPreparationPhase::mesh_setup : GraphicsPreparationPhase::texture_setup;
    return trace.phase == phase && trace.entity.is_valid() && trace.milliseconds >= 250 &&
           trace.driver_call_limit == 1 && (mesh || trace.texture_slot.has_value());
}
[[nodiscard]] Result<void> verify_preparation_failure(bool mesh)
{
    Fixture fixture(47);
    if (const auto prepared = prepare(fixture); !prepared)
        return prepared.error();
    auto device = std::make_unique<FakeDevice>();
    device->state().clock_step_milliseconds = 250;
    if (mesh)
        device->state().fail_mesh_creation = true;
    else
        device->fail_texture_upload_at(1);
    auto renderer =
        Renderer::create(std::move(device), owner, make_test_studio_environment_source());
    if (!renderer)
        return renderer.error();
    const auto completed = complete(*renderer.value(), fixture);
    if (completed ||
        !preparation_failure_matches(renderer.value()->scene_resource_status(fixture.scene), mesh))
        return failure(
            "Failed preparation lost its resource, phase, duration or admission allowance");
    renderer.value()->release_scene(fixture.scene.id());
    return renderer.value()->complete_retirement();
}
[[nodiscard]] Result<void> run()
{
    Fixture f;
    if (const auto prepared = prepare(f); !prepared)
        return prepared.error();
    auto created = Renderer::create(std::make_unique<FakeDevice>(), owner,
                                    make_test_studio_environment_source());
    if (!created)
        return created.error();
    auto& renderer = *created.value();
    if (const auto admitted = check_admission(renderer, f); !admitted)
        return admitted.error();
    const auto revision = f.scene.document().revision();
    if (const auto completed = complete(renderer, f); !completed)
        return completed.error();
    if (f.scene.document().revision() != revision)
        return failure("GPU preparation mutated the canonical Document");
    if (const auto ready = verify_ready(renderer, f); !ready)
        return ready.error();
    if (const auto refreshed = verify_reprepare(renderer, f); !refreshed)
        return refreshed.error();
    if (const auto retired = verify_retirement(renderer, f); !retired)
        return retired.error();
    return verify_pending_retirement(renderer);
}
} // namespace
int elf3d_renderer_scene_preparation_test()
{
    if (!run())
        return 31;
    return verify_preparation_failure(true) && verify_preparation_failure(false) ? 0 : 32;
}
