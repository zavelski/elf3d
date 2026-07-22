#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/model.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
import elf.assets;
import elf.graphics;
import elf.math;
import elf.model;
import elf.renderer;
import elf.scene;

#include "renderer_test_support.h"
namespace {
using elf3d::renderer::tests::FakeDevice;
using elf3d::renderer::tests::FakeDeviceState;
using elf3d::renderer::tests::FakePickingTarget;
using elf3d::renderer::tests::FakeRenderTarget;
[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.0001F) noexcept {
    return nearly_equal(left.x, right.x, tolerance) && nearly_equal(left.y, right.y, tolerance) &&
           nearly_equal(left.z, right.z, tolerance);
}
[[nodiscard]] double test_focus_depth_weight(elf3d::Extent2D extent, std::uint32_t x,
                                             std::uint32_t y) noexcept {
    const double sample_x =
        (static_cast<double>(x) + 0.5) * 2.0 / static_cast<double>(extent.width) - 1.0;
    const double sample_y =
        (static_cast<double>(y) + 0.5) * 2.0 / static_cast<double>(extent.height) - 1.0;
    const double radius_squared = (sample_x * sample_x + sample_y * sample_y) * 0.5;
    const double mass = 1.0 - std::min(radius_squared, 1.0);
    return mass * mass;
}
constexpr std::uint64_t engine_token = 11;
constexpr std::array<elf3d::VertexPositionNormal, 3> test_vertices{{
    {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
}};
constexpr std::array<std::uint32_t, 3> test_indices{{0, 1, 2}};
constexpr elf3d::RenderStatistics expected_highlighted{3, 3, 9, 9, 9, 0, 3, 0, 0};
[[nodiscard]] bool legacy_statistics_equal(const elf3d::RenderStatistics& actual,
                                           const elf3d::RenderStatistics& expected) noexcept {
    return actual.draw_calls == expected.draw_calls && actual.triangles == expected.triangles &&
           actual.vertices == expected.vertices && actual.indices == expected.indices &&
           actual.texture_bindings == expected.texture_bindings &&
           actual.gpu_texture_uploads == expected.gpu_texture_uploads &&
           actual.unique_gpu_textures == expected.unique_gpu_textures &&
           actual.overlay_lines == expected.overlay_lines &&
           actual.overlay_markers == expected.overlay_markers;
}
[[nodiscard]] elf3d::renderer::RenderRequest
render_request(elf3d::EntityId camera, elf3d::ViewportRenderOptions options = {}) {
    return {camera, {}, {}, options};
}
[[nodiscard]] bool position_test_camera(elf3d::scene::Storage& scene, elf3d::EntityId camera) {
    elf3d::Transform transform;
    transform.translation = {0.0F, 0.0F, 3.0F};
    return static_cast<bool>(scene.set_local_transform(camera, transform));
}
struct RendererContext {
    RendererContext()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 1)), scene(id) {}

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::MeshHandle mesh;
    elf3d::TextureAssetHandle texture;
    elf3d::TextureAssetHandle clamped_texture;
    elf3d::MaterialHandle material;
    elf3d::MaterialHandle double_sided;
    elf3d::EntityId model;
    elf3d::EntityId camera;
    elf3d::EntityId non_camera;
    std::unique_ptr<elf3d::renderer::Renderer> renderer;
    FakeRenderTarget target;

    [[nodiscard]] FakeDeviceState& device_state() noexcept {
        return static_cast<FakeDevice&>(renderer->device()).state();
    }
};
[[nodiscard]] int prepare_mesh_and_textures(RendererContext& context) {
    const auto mesh = context.scene.create_mesh({test_vertices, test_indices});
    constexpr std::array<std::byte, 3U * 5U * 4U> pixels{};
    const auto image = context.scene.create_image({3, 5, elf3d::PixelFormat::rgba8_unorm, pixels});
    if (!mesh || !image) {
        return 1;
    }
    elf3d::SamplerDescription mipmapped_sampler;
    mipmapped_sampler.min_filter = elf3d::TextureFilter::linear_mipmap_linear;
    const auto texture = context.scene.create_texture({image.value(), mipmapped_sampler});
    elf3d::SamplerDescription clamp_sampler;
    clamp_sampler.wrap_u = elf3d::TextureWrap::clamp_to_edge;
    const auto clamped_texture = context.scene.create_texture({image.value(), clamp_sampler});
    if (!texture || !clamped_texture) {
        return 1;
    }
    context.mesh = mesh.value();
    context.texture = texture.value();
    context.clamped_texture = clamped_texture.value();
    return 0;
}
[[nodiscard]] int prepare_materials(RendererContext& context) {
    elf3d::MaterialDescription textured_description;
    textured_description.base_color = {0.5F, 0.5F, 0.5F, 1.0F};
    textured_description.base_color_texture = context.texture;
    textured_description.base_color_texture_mapping.texcoord_set = 1;
    textured_description.base_color_texture_mapping.transform.offset = {0.25F, 0.5F};
    textured_description.base_color_texture_mapping.transform.scale = {2.0F, 3.0F};
    textured_description.base_color_texture_mapping.transform.rotation_radians = 0.5F;
    textured_description.metallic_roughness_texture = context.texture;
    textured_description.occlusion_texture = context.texture;
    textured_description.occlusion_texture_mapping.texcoord_set = 1;
    textured_description.occlusion_strength = 0.6F;
    textured_description.emissive_texture = context.texture;
    textured_description.emissive_texture_mapping.texcoord_set = 1;
    textured_description.emissive_factor = {0.2F, 0.3F, 0.4F};
    textured_description.ior = 1.33F;
    textured_description.specular_factor = 0.75F;
    textured_description.specular_color_factor = {0.8F, 0.9F, 1.0F};
    textured_description.unlit = true;
    textured_description.alpha_mode = elf3d::AlphaMode::mask;
    textured_description.alpha_cutoff = 0.35F;
    const auto material = context.scene.create_material(textured_description);

    elf3d::MaterialDescription double_sided_description;
    double_sided_description.base_color = {0.8F, 0.2F, 0.2F, 1.0F};
    double_sided_description.double_sided = true;
    double_sided_description.base_color_texture = context.clamped_texture;
    double_sided_description.alpha_mode = elf3d::AlphaMode::blend;
    const auto double_sided = context.scene.create_material(double_sided_description);
    if (!material || !double_sided) {
        return 1;
    }
    context.material = material.value();
    context.double_sided = double_sided.value();
    return 0;
}
[[nodiscard]] int prepare_entities(RendererContext& context) {
    const auto model = context.scene.create_model(context.mesh, context.material);
    const auto camera =
        context.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera || !position_test_camera(context.scene, camera.value())) {
        return 1;
    }
    const std::array<elf3d::ModelPrimitiveBinding, 2> primitives{{
        {context.mesh, context.material},
        {context.mesh, context.double_sided},
    }};
    elf3d::Transform mirrored;
    mirrored.scale = {-1.0F, 2.0F, 1.0F};
    if (!context.scene.set_model_primitives(model.value(), primitives) ||
        !context.scene.set_local_transform(model.value(), mirrored)) {
        return 1;
    }
    const auto non_camera = context.scene.create_entity();
    if (!non_camera) {
        return 1;
    }
    context.model = model.value();
    context.camera = camera.value();
    context.non_camera = non_camera.value();
    return 0;
}
[[nodiscard]] int prepare_context(RendererContext& context) {
    const int resources = prepare_mesh_and_textures(context);
    if (resources != 0) {
        return resources;
    }
    const int materials = prepare_materials(context);
    if (materials != 0) {
        return materials;
    }
    return prepare_entities(context);
}
[[nodiscard]] bool has_expected_shader_sources(const FakeDeviceState& device) {
    return device.vertex_shader_source.find("a_texcoord1") != std::string::npos &&
           device.vertex_shader_source.find("a_color") != std::string::npos &&
           device.fragment_shader_source.find("mapped_uv") != std::string::npos &&
           device.fragment_shader_source.find("u_alpha_mode") != std::string::npos &&
           device.fragment_shader_source.find("u_emissive_texture") != std::string::npos;
}
[[nodiscard]] int verify_renderer_creation(RendererContext& context) {
    if (elf3d::renderer::build_render_list(context.scene, context.non_camera, {640, 360})
            .error()
            .code() != elf3d::ErrorCode::entity_has_no_camera) {
        return 2;
    }
    auto owned_device = std::make_unique<FakeDevice>();
    auto renderer = elf3d::renderer::Renderer::create(std::move(owned_device), engine_token);
    if (!renderer) {
        return 3;
    }
    context.renderer = std::move(renderer.value());
    if (!has_expected_shader_sources(context.device_state())) {
        return 31;
    }
    return 0;
}

[[nodiscard]] bool has_expected_diagnostic_counts(const elf3d::RenderStatistics& first,
                                                  const elf3d::RenderStatistics& second) noexcept {
    return first.candidate_primitives == 2 && first.visible_primitives == 2 &&
           first.material_switches == 2 && second.material_switches == 2 &&
           first.gpu_buffer_uploads == 1 && second.gpu_buffer_uploads == 0 &&
           first.draw_packet_rebuilds == 2 && second.draw_packet_rebuilds == 0 &&
           std::array{first.estimated_resident_geometry_bytes,
                      first.estimated_resident_texture_bytes} ==
               std::array<std::uint64_t, 2>{84U, 204U} &&
           first.cpu_total_milliseconds >= 0.0;
}
[[nodiscard]] bool has_expected_compact_upload(const FakeDeviceState& device) {
    return device.upload_count == 1 && !device.mesh_layouts.empty() &&
           !device.mesh_uploaded_bytes.empty() &&
           device.mesh_layouts.front() == elf3d::graphics::VertexLayout::position_normal_float3 &&
           device.mesh_uploaded_bytes.front() == test_vertices.size() * 6U * sizeof(float);
}

[[nodiscard]] bool has_expected_render_counts(const elf3d::Result<elf3d::RenderStatistics>& first,
                                              const elf3d::Result<elf3d::RenderStatistics>& second,
                                              const FakeDeviceState& device) {
    const elf3d::RenderStatistics expected_first{2, 2, 6, 6, 5, 3, 3, 0, 0};
    const elf3d::RenderStatistics expected_second{2, 2, 6, 6, 5, 0, 3, 0, 0};
    if (!first || !second) {
        return false;
    }
    const bool legacy_counts = legacy_statistics_equal(first.value(), expected_first) &&
                               legacy_statistics_equal(second.value(), expected_second);
    const bool diagnostic_counts = has_expected_diagnostic_counts(first.value(), second.value());
    return legacy_counts && diagnostic_counts && has_expected_compact_upload(device) &&
           device.indexed_batch_count == 2 && device.draw_count == 4 && device.draws.size() == 4;
}

[[nodiscard]] bool has_expected_texture_uploads(const FakeDeviceState& device) {
    return device.texture_upload_count == 3 && device.texture_descriptions.size() == 3 &&
           device.texture_descriptions[0].format == elf3d::graphics::TextureFormat::rgba8_srgb &&
           device.texture_descriptions[0].min_filter ==
               elf3d::graphics::TextureFilterMode::linear_mipmap_linear &&
           device.texture_descriptions[1].format == elf3d::graphics::TextureFormat::rgba8_unorm &&
           device.texture_descriptions[2].wrap_u ==
               elf3d::graphics::TextureAddressMode::clamp_to_edge;
}

[[nodiscard]] bool has_expected_texture_mapping(const FakeDeviceState& device) {
    return !device.draws.empty() && device.draws[0].texture_mappings[0].texcoord_set == 1U &&
           device.draws[0].texture_mappings[0].transform.offset == elf3d::Float2{0.25F, 0.5F} &&
           device.draws[0].texture_mappings[0].transform.scale == elf3d::Float2{2.0F, 3.0F} &&
           nearly_equal(device.draws[0].texture_mappings[0].transform.rotation_radians, 0.5F);
}

[[nodiscard]] bool has_expected_material_parameters(const FakeDeviceState& device) {
    return !device.draws.empty() && !device.draw_texture_presence.empty() &&
           device.draw_texture_presence[0][2] && device.draw_texture_presence[0][3] &&
           device.draws[0].emissive_factor == elf3d::Float3{0.2F, 0.3F, 0.4F} &&
           nearly_equal(device.draws[0].ior, 1.33F) &&
           nearly_equal(device.draws[0].specular_factor, 0.75F) && device.draws[0].unlit &&
           device.draws[0].alpha_mode == elf3d::AlphaMode::mask &&
           nearly_equal(device.draws[0].alpha_cutoff, 0.35F);
}

[[nodiscard]] bool has_expected_raster_state(const FakeDeviceState& device) {
    return device.draws.size() >= 2 && device.draws[0].front_face_clockwise &&
           !device.draws[0].double_sided && device.draws[1].front_face_clockwise &&
           device.draws[1].double_sided && device.draws[1].alpha_mode == elf3d::AlphaMode::blend;
}

[[nodiscard]] int verify_material_render(RendererContext& context) {
    const auto first =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    const auto second =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    if (!has_expected_render_counts(first, second, context.device_state()) ||
        !has_expected_texture_uploads(context.device_state()) ||
        !has_expected_texture_mapping(context.device_state()) ||
        !has_expected_material_parameters(context.device_state()) ||
        !has_expected_raster_state(context.device_state())) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_camera_draw_packet_reuse(RendererContext& context) {
    context.target.extent_value = {640, 360};
    const auto warm_render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    if (!warm_render) {
        return 57;
    }
    const std::uint64_t content_revision = context.scene.render_content_revision();
    elf3d::Transform camera_transform;
    camera_transform.translation = {0.25F, 0.0F, 3.0F};
    if (!context.scene.set_local_transform(context.camera, camera_transform) ||
        context.scene.render_content_revision() != content_revision) {
        return 58;
    }
    const auto camera_render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    if (!camera_render || camera_render.value().draw_packet_rebuilds != 0 ||
        camera_render.value().gpu_buffer_uploads != 0) {
        return 59;
    }
    return 0;
}

[[nodiscard]] bool material_cache_reused(RendererContext& context,
                                         const elf3d::RenderStatistics& statistics,
                                         int mesh_uploads, int texture_uploads) {
    return statistics.gpu_buffer_uploads == 0 &&
           context.device_state().upload_count == mesh_uploads &&
           context.device_state().texture_upload_count == texture_uploads;
}

[[nodiscard]] bool latest_material_draw_matches(RendererContext& context,
                                                const elf3d::RenderStatistics& statistics,
                                                elf3d::Color4 base_color) {
    if (statistics.draw_calls == 0 || context.device_state().draws.size() < statistics.draw_calls) {
        return false;
    }
    const std::size_t first_draw =
        context.device_state().draws.size() - static_cast<std::size_t>(statistics.draw_calls);
    return context.device_state().draws[first_draw].base_color == base_color;
}

[[nodiscard]] int verify_material_draw_packet_invalidation(RendererContext& context) {
    const auto original = context.scene.material(context.material);
    if (!original) {
        return 60;
    }
    elf3d::MaterialDescription changed = original.value();
    changed.base_color = {0.1F, 0.2F, 0.3F, 1.0F};
    const int mesh_uploads = context.device_state().upload_count;
    const int texture_uploads = context.device_state().texture_upload_count;
    if (!context.scene.set_material(context.material, changed)) {
        return 61;
    }
    const auto material_render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    if (!material_render) {
        return 62;
    }
    if (material_render.value().draw_packet_rebuilds != material_render.value().draw_calls) {
        return 63;
    }
    if (!material_cache_reused(context, material_render.value(), mesh_uploads, texture_uploads)) {
        return 64;
    }
    if (!latest_material_draw_matches(context, material_render.value(), changed.base_color)) {
        return 65;
    }
    if (!context.scene.set_material(context.material, original.value())) {
        return 66;
    }
    return 0;
}

[[nodiscard]] int verify_draw_packet_invalidation(RendererContext& context) {
    const int camera_reuse = verify_camera_draw_packet_reuse(context);
    return camera_reuse == 0 ? verify_material_draw_packet_invalidation(context) : camera_reuse;
}

[[nodiscard]] bool
has_expected_gpu_pick_summary(const elf3d::Result<elf3d::renderer::GpuPickResult>& pick,
                              const FakePickingTarget& target, const FakeDeviceState& device) {
    return pick && pick.value().hit.has_value() && pick.value().draw_calls == 2 &&
           pick.value().pixels_read == 1 && target.clear_count == 1 &&
           device.picking_batch_count == 1 && device.picking_draw_count == 2 &&
           device.picking_draws.size() == 2;
}

[[nodiscard]] bool has_expected_gpu_pick_draws(const FakeDeviceState& device) {
    return device.picking_draws.size() >= 2 && device.picking_draws[0].object_id == 1U &&
           device.picking_draws[0].primitive_index == 0U &&
           device.picking_draws[1].object_id == 2U && device.picking_draws[1].primitive_index == 1U;
}

[[nodiscard]] bool has_expected_gpu_hit(const elf3d::renderer::GpuPickResult& pick,
                                        const RendererContext& context) {
    return pick.hit->entity == context.model && pick.hit->mesh == context.mesh &&
           pick.hit->primitive_index == 1U && pick.hit->triangle_index == 0U;
}

[[nodiscard]] int verify_gpu_pick(RendererContext& context) {
    FakePickingTarget picking_target;
    context.device_state().picking_pixel = elf3d::graphics::PickingPixel{2U, 1U, 0U, 0.5F};
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::renderer::GpuPickRequest request{
        context.camera, {319.5F, 179.5F}, picking_target.extent_value, {319.5F, 179.5F}};
    const auto gpu_pick = context.renderer->gpu_pick(context.scene, picking_target, visibility,
                                                     elf3d::clipping::disabled_filter(), request);
    if (!has_expected_gpu_pick_summary(gpu_pick, picking_target, context.device_state()) ||
        !has_expected_gpu_pick_draws(context.device_state()) ||
        !has_expected_gpu_hit(gpu_pick.value(), context)) {
        return 45;
    }
    return 0;
}

[[nodiscard]] bool has_expected_focus_anchor(
    const elf3d::Result<elf3d::renderer::GpuFocusDepthAnchorResult>& focus_anchor,
    const elf3d::Result<elf3d::Float3>& expected_anchor) {
    return expected_anchor && focus_anchor && focus_anchor.value().world_position.has_value() &&
           focus_anchor.value().draw_calls == 2 && focus_anchor.value().pixels_read == 16 &&
           nearly_equal(*focus_anchor.value().world_position, expected_anchor.value(), 0.0001F);
}

[[nodiscard]] int verify_focus_anchor(RendererContext& context) {
    FakePickingTarget anchor_target;
    anchor_target.extent_value = {4, 4};
    const elf3d::Extent2D viewport_extent{640, 360};
    context.device_state().picking_depths.assign(16U, 1.0F);
    context.device_state().picking_depths[static_cast<std::size_t>(1U * 4U + 1U)] = 0.25F;
    context.device_state().picking_depths[static_cast<std::size_t>(1U * 4U + 3U)] = 0.75F;
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::renderer::GpuFocusDepthRequest request{context.camera, viewport_extent};
    const auto focus_anchor = context.renderer->gpu_focus_depth_anchor(
        context.scene, anchor_target, visibility, elf3d::clipping::disabled_filter(), request);
    const auto render_list =
        elf3d::renderer::build_render_list(context.scene, context.camera, viewport_extent,
                                           visibility, elf3d::clipping::disabled_filter());
    if (!render_list) {
        return 46;
    }

    const double center_weight = test_focus_depth_weight(anchor_target.extent_value, 1U, 1U);
    const double edge_weight = test_focus_depth_weight(anchor_target.extent_value, 3U, 1U);
    const float expected_depth = static_cast<float>((0.25 * center_weight + 0.75 * edge_weight) /
                                                    (center_weight + edge_weight));
    const elf3d::Float2 expected_screen{static_cast<float>(viewport_extent.width) * 0.5F,
                                        static_cast<float>(viewport_extent.height) * 0.5F};
    const elf3d::Result<elf3d::Float3> expected_anchor = elf3d::math::unproject_viewport_point(
        render_list.value().view_matrix, render_list.value().projection_matrix, viewport_extent,
        expected_screen, expected_depth);
    if (!has_expected_focus_anchor(focus_anchor, expected_anchor)) {
        return 46;
    }
    const auto projected_anchor = elf3d::math::project_world_to_viewport_point(
        render_list.value().view_matrix, render_list.value().projection_matrix, viewport_extent,
        *focus_anchor.value().world_position);
    if (!projected_anchor ||
        !nearly_equal(projected_anchor.value().position_pixels.x, expected_screen.x, 0.001F) ||
        !nearly_equal(projected_anchor.value().position_pixels.y, expected_screen.y, 0.001F)) {
        return 47;
    }
    context.device_state().picking_depths.clear();
    return 0;
}

[[nodiscard]] bool
has_expected_highlighted_render(const elf3d::Result<elf3d::EntityId>& shared_model,
                                const elf3d::Result<elf3d::RenderStatistics>& render,
                                const FakeDeviceState& device) {
    return shared_model && render &&
           legacy_statistics_equal(render.value(), expected_highlighted) &&
           device.draws.size() == 7 && device.draws[4].highlight_strength == 0.6F &&
           device.draws[5].highlight_strength == 0.0F && device.draws[6].highlight_strength == 0.6F;
}

[[nodiscard]] bool has_expected_hidden_render(const elf3d::Result<void>& hidden,
                                              const elf3d::Result<elf3d::RenderStatistics>& render,
                                              const FakeDeviceState& device) {
    const elf3d::RenderStatistics expected_hidden{1, 1, 3, 3, 4, 0, 3, 0, 0};
    return hidden && render && legacy_statistics_equal(render.value(), expected_hidden) &&
           device.draws.size() == 8 && device.draws.back().highlight_strength == 0.0F;
}

[[nodiscard]] int verify_highlight_visibility(RendererContext& context) {
    const auto shared_model = context.scene.create_model(context.mesh, context.material);
    elf3d::ViewportRenderOptions options;
    options.highlight =
        elf3d::EntityHighlight{context.model, elf3d::Color4{1.0F, 0.2F, 0.0F, 1.0F}, 0.6F};
    const auto highlighted = context.renderer->render(context.scene, context.target,
                                                      render_request(context.camera, options));
    if (!has_expected_highlighted_render(shared_model, highlighted, context.device_state())) {
        return 41;
    }
    const auto hidden = context.scene.set_entity_visible(context.model, false);
    const auto hidden_render = context.renderer->render(context.scene, context.target,
                                                        render_request(context.camera, options));
    if (!has_expected_hidden_render(hidden, hidden_render, context.device_state())) {
        return 42;
    }
    const auto restored = context.scene.show_all_entities();
    const auto restored_render = context.renderer->render(context.scene, context.target,
                                                          render_request(context.camera, options));
    if (!restored || !restored_render ||
        !legacy_statistics_equal(restored_render.value(), expected_highlighted)) {
        return 43;
    }
    return 0;
}

[[nodiscard]] int verify_overlay(RendererContext& context) {
    const std::array<elf3d::OverlayLineSegment, 1> lines{
        elf3d::OverlayLineSegment{{0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F}}};
    const std::array<elf3d::OverlayPointMarker, 1> markers{
        elf3d::OverlayPointMarker{{0.0F, 0.0F, -1.0F}}};
    elf3d::ViewportRenderOptions options;
    options.overlay_lines = lines;
    options.overlay_markers = markers;
    const auto render = context.renderer->render(context.scene, context.target,
                                                 render_request(context.camera, options));
    if (!render || render.value().overlay_lines != 1 || render.value().overlay_markers != 1 ||
        context.device_state().overlay_draw_count != 1 ||
        context.device_state().overlay_line_count != 1 ||
        context.device_state().overlay_marker_count != 1) {
        return 44;
    }
    return 0;
}

[[nodiscard]] bool
has_expected_clipped_list(const elf3d::Result<elf3d::renderer::RenderList>& list) {
    return list && list.value().items.size() == 3 && list.value().clipping_bounds_tested == 3 &&
           list.value().clipping_bounds_rejected == 0 &&
           list.value().clipping_bounds_intersecting == 2;
}

[[nodiscard]] bool has_expected_clipped_render(const elf3d::Result<elf3d::RenderStatistics>& render,
                                               const FakeDeviceState& device,
                                               std::size_t previous_draw_count) {
    return render && render.value().draw_calls == 3 && render.value().clipping_bounds_tested == 3 &&
           render.value().clipping_bounds_rejected == 0 &&
           render.value().clipping_bounds_intersecting == 2 &&
           device.draws.size() == previous_draw_count + 3 &&
           device.draws[previous_draw_count].clipping_section_plane_enabled &&
           nearly_equal(device.draws[previous_draw_count].clipping_section_plane_normal,
                        {1.0F, 0.0F, 0.0F}) &&
           nearly_equal(device.draws[previous_draw_count].clipping_section_plane_offset, 0.5F) &&
           device.draws[previous_draw_count].clipping_retain_positive_half_space;
}

[[nodiscard]] elf3d::SectionPlane cutting_plane_at(float x) {
    elf3d::SectionPlane plane;
    plane.enabled = true;
    plane.point = {x, 0.0F, 0.0F};
    plane.normal = {2.0F, 0.0F, 0.0F};
    return plane;
}

[[nodiscard]] int verify_intersecting_clipping(RendererContext& context) {
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::clipping::ClippingFilter filter =
        elf3d::clipping::make_filter(cutting_plane_at(-0.5F), {}, 101).value();
    const auto list = elf3d::renderer::build_render_list(context.scene, context.camera, {640, 360},
                                                         visibility, filter);
    if (!has_expected_clipped_list(list)) {
        return 45;
    }
    const std::size_t previous_draw_count = context.device_state().draws.size();
    const auto render = context.renderer->render(
        context.scene, context.target, render_request(context.camera), visibility, filter);
    if (!has_expected_clipped_render(render, context.device_state(), previous_draw_count)) {
        return 46;
    }
    return 0;
}

[[nodiscard]] int verify_outside_clipping(RendererContext& context) {
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const elf3d::clipping::ClippingFilter filter =
        elf3d::clipping::make_filter(cutting_plane_at(10.0F), {}, 102).value();
    const std::size_t previous_draw_count = context.device_state().draws.size();
    const auto render = context.renderer->render(
        context.scene, context.target, render_request(context.camera), visibility, filter);
    if (!render || render.value().draw_calls != 0 || render.value().clipping_bounds_tested != 3 ||
        render.value().clipping_bounds_rejected != 3 || render.value().unique_gpu_textures != 3 ||
        context.device_state().draws.size() != previous_draw_count ||
        context.device_state().upload_count != 1 ||
        context.device_state().texture_upload_count != 3) {
        return 47;
    }
    return 0;
}

[[nodiscard]] int verify_box_clipping(RendererContext& context) {
    const std::array<elf3d::ClippingBox, 2> boxes{{
        {{20.0F, 20.0F, 20.0F}, {21.0F, 21.0F, 21.0F}, false},
        {{-2.0F, -1.0F, -1.0F}, {2.0F, 3.0F, 1.0F}, true},
    }};
    const elf3d::clipping::ClippingFilter filter =
        elf3d::clipping::make_filter(elf3d::SectionPlane{}, boxes, 103).value();
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(context.scene, std::nullopt).value();
    const std::size_t previous_draw_count = context.device_state().draws.size();
    const auto render = context.renderer->render(
        context.scene, context.target, render_request(context.camera), visibility, filter);
    if (!render || render.value().draw_calls != 3 || render.value().clipping_bounds_tested != 3 ||
        render.value().clipping_bounds_rejected != 0 ||
        render.value().clipping_bounds_intersecting != 0 ||
        context.device_state().draws.size() != previous_draw_count + 3 ||
        context.device_state().draws[previous_draw_count].clipping_box_count != 1 ||
        context.device_state().draws[previous_draw_count].clipping_boxes[0].minimum !=
            boxes[1].minimum ||
        context.device_state().draws[previous_draw_count].clipping_boxes[0].maximum !=
            boxes[1].maximum) {
        return 48;
    }
    return 0;
}

[[nodiscard]] bool has_upload_counts(const elf3d::Result<elf3d::RenderStatistics>& render,
                                     const FakeDeviceState& device, int meshes, int textures) {
    return render && device.upload_count == meshes && device.texture_upload_count == textures;
}

[[nodiscard]] int verify_cache_lifecycle(RendererContext& context) {
    const elf3d::SceneId second_id =
        elf3d::detail::SceneHandleAccess::create_scene(engine_token, 2);
    elf3d::scene::Storage second_scene{second_id};
    const auto mesh = second_scene.create_mesh({test_vertices, test_indices});
    const auto material = second_scene.create_material({elf3d::Color4{0.2F, 0.4F, 0.8F, 1.0F}});
    if (!mesh || !material) {
        return 5;
    }
    const auto model = second_scene.create_model(mesh.value(), material.value());
    const auto camera =
        second_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera || !position_test_camera(second_scene, camera.value())) {
        return 5;
    }
    const auto initial_render =
        context.renderer->render(second_scene, context.target, render_request(camera.value()));
    if (!has_upload_counts(initial_render, context.device_state(), 2, 3)) {
        return 5;
    }
    context.renderer->release_scene(context.id);
    const auto retained_render =
        context.renderer->render(second_scene, context.target, render_request(camera.value()));
    if (!has_upload_counts(retained_render, context.device_state(), 2, 3)) {
        return 6;
    }
    const auto reloaded_render =
        context.renderer->render(context.scene, context.target, render_request(context.camera));
    if (!has_upload_counts(reloaded_render, context.device_state(), 3, 6)) {
        return 6;
    }
    context.renderer->release_scene(second_id);
    context.renderer->release_scene(context.id);
    return 0;
}

struct DocumentContext {
    DocumentContext()
        : id(elf3d::detail::SceneHandleAccess::create_scene(engine_token, 3)), scene(id) {}

    elf3d::SceneId id;
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
    elf3d::EntityId camera;
};

struct PreviousDrawCounts {
    int uploads = 0;
    std::size_t draws = 0;
};

[[nodiscard]] int prepare_document_scene(DocumentContext& context) {
    elf3d::Document document;
    const auto mesh = document.create_mesh("document-quad");
    elf3d::ModelMaterialDescription material_description;
    material_description.base_color = {0.1F, 0.2F, 0.3F, 0.4F};
    material_description.double_sided = true;
    material_description.alpha_mode = elf3d::AlphaMode::blend;
    const auto material = document.create_material(material_description);
    if (!mesh || !material) {
        return 49;
    }

    elf3d::PrimitiveData quad;
    quad.positions = {
        {-1.0F, -1.0F, 0.0F},
        {1.0F, -1.0F, 0.0F},
        {1.0F, 1.0F, 0.0F},
        {-1.0F, 1.0F, 0.0F},
    };
    quad.normals = {
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
        {0.0F, 0.0F, 1.0F},
    };
    quad.indices = {0, 1, 2, 0, 2, 3};
    const auto primitive =
        document.create_primitive(mesh.value(), material.value(), std::move(quad));
    if (!primitive || !context.scene.set_document(std::move(document))) {
        return 49;
    }
    const auto model = context.scene.create_entity();
    const auto camera =
        context.scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera || !position_test_camera(context.scene, camera.value())) {
        return 49;
    }
    const std::array<elf3d::PrimitiveId, 1> primitives{{primitive.value()}};
    if (!context.scene.set_model_document_primitives(model.value(), primitives)) {
        return 49;
    }
    context.model = model.value();
    context.camera = camera.value();
    return 0;
}

[[nodiscard]] bool
has_expected_document_render(const DocumentContext& document,
                             const elf3d::Result<elf3d::renderer::RenderList>& list,
                             const elf3d::Result<elf3d::RenderStatistics>& render,
                             const FakeDeviceState& device, PreviousDrawCounts previous) {
    const elf3d::SceneStatistics expected_scene{2, 1, 1, 1, 1, 4, 6, 2};
    const elf3d::RenderStatistics expected_render{1, 2, 4, 6, 0, 0, 0, 0, 0};
    return list && list.value().items.size() == 1 && render &&
           document.scene.statistics() == expected_scene &&
           legacy_statistics_equal(render.value(), expected_render) &&
           device.upload_count == previous.uploads + 1 &&
           device.draws.size() == previous.draws + 1 && device.draws.back().double_sided &&
           device.draws.back().alpha_mode == elf3d::AlphaMode::blend;
}

[[nodiscard]] int verify_document_render(RendererContext& context) {
    DocumentContext document;
    const int prepared = prepare_document_scene(document);
    if (prepared != 0) {
        return prepared;
    }
    const auto list =
        elf3d::renderer::build_render_list(document.scene, document.camera, {640, 360});
    const PreviousDrawCounts previous{context.device_state().upload_count,
                                      context.device_state().draws.size()};
    const auto render =
        context.renderer->render(document.scene, context.target, render_request(document.camera));
    if (!has_expected_document_render(document, list, render, context.device_state(), previous)) {
        return 49;
    }
    return 0;
}

[[nodiscard]] int verify_zero_extent(RendererContext& context) {
    context.target.extent_value = {};
    const auto render =
        context.renderer->render(context.scene, context.target, render_request(context.non_camera));
    if (!render || render.value() != elf3d::RenderStatistics{}) {
        return 8;
    }
    return 0;
}

using RendererStep = int (*)(RendererContext&);

[[nodiscard]] int run_renderer_steps(RendererContext& context) {
    constexpr std::array<RendererStep, 13> steps{{
        verify_renderer_creation,
        verify_material_render,
        verify_gpu_pick,
        verify_focus_anchor,
        verify_highlight_visibility,
        verify_overlay,
        verify_intersecting_clipping,
        verify_outside_clipping,
        verify_box_clipping,
        verify_cache_lifecycle,
        verify_document_render,
        verify_zero_extent,
        verify_draw_packet_invalidation,
    }};
    for (const RendererStep step : steps) {
        const int result = step(context);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

} // namespace

int elf3d_renderer_test() {
    RendererContext context;
    const int prepared = prepare_context(context);
    if (prepared != 0) {
        return prepared;
    }
    return run_renderer_steps(context);
}
