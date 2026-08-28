#include <elf3d/assets.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

import elf.assets;
import elf.graphics;
import elf.renderer;
import elf.scene;

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

[[nodiscard]] elf3d::renderer::RenderRequest render_request(elf3d::EntityId camera) {
    return {camera};
}

[[nodiscard]] bool add_model(elf3d::scene::Storage& scene) {
    const auto mesh = scene.create_mesh({test_vertices, test_indices});
    const auto material = scene.create_material({});
    return mesh && material && scene.create_model(mesh.value(), material.value());
}

[[nodiscard]] bool prepare_context(EnvironmentContext& context, std::vector<std::byte> resource) {
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

[[nodiscard]] bool empty_render_defers_resource(EnvironmentContext& context) {
    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    const FakeDeviceState& state = context.device.front().state();
    return render && render.value().environment_preparations == 0 &&
           context.source.front().read_count() == 0 && state.cubemap_upload_count == 0 &&
           state.vertex_shader_source.empty() && state.fragment_shader_source.empty();
}

[[nodiscard]] bool unlit_render_defers_resource(EnvironmentContext& context) {
    auto request = render_request(context.camera);
    request.options.shading_mode = elf3d::RenderShadingMode::unlit;
    const auto render = context.renderer->render(context.scene, context.target, request);
    return render && render.value().environment_preparations == 0 &&
           context.source.front().read_count() == 0 &&
           context.device.front().state().cubemap_upload_count == 0 &&
           context.device.front().state().texture_upload_count == 0;
}

[[nodiscard]] bool standard_resource_is_shared(EnvironmentContext& context) {
    const auto first =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    FakeRenderTarget additional_target;
    const auto second =
        context.renderer->render(context.scene, additional_target, render_request(context.camera));
    return first && second && first.value().environment_preparations == 1 &&
           second.value().environment_preparations == 0 &&
           context.source.front().read_count() == 1 &&
           context.device.front().state().cubemap_upload_count == 2 &&
           context.device.front().state().texture_upload_count == 1;
}

[[nodiscard]] int verify_valid_resource_is_lazy_and_shared() {
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

[[nodiscard]] bool invalid_resource_is_rejected(std::vector<std::byte> resource) {
    EnvironmentContext context;
    if (!prepare_context(context, std::move(resource)) || !empty_render_defers_resource(context) ||
        !add_model(context.scene)) {
        return false;
    }
    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    return !render && render.error().code() == elf3d::ErrorCode::graphics_initialization_failed &&
           context.source.front().read_count() == 1 &&
           context.device.front().state().cubemap_upload_count == 0 &&
           context.device.front().state().texture_upload_count == 0;
}

[[nodiscard]] int verify_invalid_resources() {
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

[[nodiscard]] bool upload_failure_is_transactional(bool fail_brdf) {
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

} // namespace

int elf3d_renderer_environment_failure_test() {
    const int valid = verify_valid_resource_is_lazy_and_shared();
    if (valid != 0) {
        return valid;
    }
    const int invalid = verify_invalid_resources();
    if (invalid != 0) {
        return invalid;
    }
    return upload_failure_is_transactional(false) && upload_failure_is_transactional(true) ? 0 : 62;
}
