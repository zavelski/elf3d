#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>
#include <elf3d/viewport.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

import elf.assets;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

namespace {

constexpr std::uintptr_t fake_resource_token = 1;

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    explicit FakeRenderTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {
        update_handle();
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        update_handle();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4 color) noexcept override {
        last_clear_color = color;
        ++clear_count;
        return {};
    }

    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override {
        return texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return texture_handle_.is_valid();
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;
    elf3d::Color4 last_clear_color;

  private:
    void update_handle() noexcept {
        texture_handle_ = extent_.width != 0 && extent_.height != 0
                              ? elf3d::detail::TextureHandleAccess::create(1)
                              : elf3d::TextureHandle{};
    }

    elf3d::Extent2D extent_;
    elf3d::TextureHandle texture_handle_;
};

class FakePickingTarget final : public elf3d::graphics::PickingTarget {
  public:
    explicit FakePickingTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {}

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear() noexcept override {
        ++clear_count;
        return {};
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_.width != 0 && extent_.height != 0;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;

  private:
    elf3d::Extent2D extent_;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::opengl;
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D initial_extent) noexcept override {
        latest_render_target_extent = initial_extent;
        auto target = std::make_unique<FakeRenderTarget>(initial_extent);
        return std::unique_ptr<elf3d::graphics::RenderTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) noexcept override {
        auto target = std::make_unique<FakePickingTarget>(initial_extent);
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle texture) const noexcept override {
        if (!texture.is_valid()) {
            return elf3d::Error{elf3d::ErrorCode::texture_unavailable,
                                "Fake texture is unavailable"};
        }
        return elf3d::NativeTextureView{elf3d::NativeGraphicsApi::opengl, 1,
                                        latest_render_target_extent};
    }

    class FakeMesh final : public elf3d::graphics::StaticMesh {
      public:
        [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
            return 0;
        }
        [[nodiscard]] std::uint32_t index_count() const noexcept override {
            return 0;
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
            return fake_resource_token;
        }
    };

    class FakePipeline final : public elf3d::graphics::GraphicsPipeline {
      public:
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
            return fake_resource_token;
        }
    };
    class FakeTexture final : public elf3d::graphics::Texture2D {
      public:
        [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
            return {1, 1};
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
            return fake_resource_token;
        }
    };

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>>
    create_static_mesh(const elf3d::graphics::StaticMeshDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::Texture2D>{std::make_unique<FakeTexture>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(const elf3d::graphics::GraphicsPipelineDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }

    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget&, elf3d::graphics::GraphicsPipeline&,
                 elf3d::graphics::StaticMesh&,
                 const elf3d::graphics::DrawIndexedDescription&) noexcept override {
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_overlay(elf3d::graphics::RenderTarget&,
                 const elf3d::graphics::DrawOverlayDescription& description) noexcept override {
        latest_overlay_lines = static_cast<int>(description.lines.size());
        latest_overlay_markers = static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_picking_indexed(elf3d::graphics::PickingTarget&, elf3d::graphics::StaticMesh&,
                         const elf3d::graphics::PickingDrawDescription&) noexcept override {
        return {};
    }
    [[nodiscard]] elf3d::Result<std::optional<elf3d::graphics::PickingPixel>>
    read_picking_pixel(elf3d::graphics::PickingTarget&, elf3d::Float2 position_pixels) noexcept override {
        last_picking_read_position = position_pixels;
        ++picking_pixel_read_count;
        return picking_pixel;
    }
    [[nodiscard]] elf3d::Result<std::vector<float>>
    read_picking_depths(elf3d::graphics::PickingTarget& target) noexcept override {
        ++picking_depths_read_count;
        last_picking_read_extent = target.extent();
        if (!picking_depths.empty()) {
            return picking_depths;
        }
        const elf3d::Extent2D target_extent = target.extent();
        return std::vector<float>(static_cast<std::size_t>(target_extent.width) *
                                      static_cast<std::size_t>(target_extent.height),
                                  picking_pixel.has_value() ? picking_pixel->depth : 1.0F);
    }

    elf3d::Extent2D latest_render_target_extent;
    elf3d::Extent2D last_picking_read_extent;
    std::optional<elf3d::graphics::PickingPixel> picking_pixel;
    std::vector<float> picking_depths;
    elf3d::Float2 last_picking_read_position;
    int picking_pixel_read_count = 0;
    int picking_depths_read_count = 0;
    int latest_overlay_lines = 0;
    int latest_overlay_markers = 0;
};

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int elf3d_viewport_lifetime_test() {
    auto owned_device = std::make_unique<FakeDevice>();
    FakeDevice* device = owned_device.get();
    elf3d::Result<std::unique_ptr<elf3d::renderer::Renderer>> renderer_result =
        elf3d::renderer::Renderer::create(std::move(owned_device), 1);
    if (!renderer_result) {
        return 1;
    }
    elf3d::picking::PickingService picking_service;
    const elf3d::SceneId scene_id = elf3d::detail::SceneHandleAccess::create_scene(1, 1);
    elf3d::scene::Storage scene{scene_id};
    elf3d::Result<elf3d::EntityId> camera_result =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-0.5F, -0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.5F, -0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 0.5F, -2.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::Result<elf3d::MeshHandle> mesh = scene.create_mesh({vertices, indices});
    const elf3d::Result<elf3d::MaterialHandle> material = scene.create_material({});
    elf3d::Result<elf3d::EntityId> model{
        elf3d::Error{elf3d::ErrorCode::invalid_argument, "Test mesh or material creation failed"}};
    if (mesh && material) {
        model = scene.create_model(mesh.value(), material.value());
    }
    if (!camera_result) {
        return 2;
    }
    if (!mesh || !material || !model) {
        return 21;
    }
    elf3d::Result<std::unique_ptr<elf3d::viewport::OffscreenViewport>> create_result =
        elf3d::viewport::OffscreenViewport::create(renderer_result.value()->device(),
                                                   elf3d::Extent2D{});
    if (!create_result) {
        return 3;
    }

    std::unique_ptr<elf3d::viewport::OffscreenViewport> viewport = std::move(create_result).value();
    if (viewport->framebuffer_valid() || viewport->color_texture().is_valid()) {
        return 4;
    }
    if (!viewport->render(*renderer_result.value(), scene, camera_result.value())) {
        return 5;
    }

    if (!viewport->resize(elf3d::Extent2D{640, 360}) ||
        !viewport->resize(elf3d::Extent2D{640, 360}) || !viewport->framebuffer_valid() ||
        viewport->extent() != elf3d::Extent2D{640, 360}) {
        return 6;
    }

    viewport->set_clear_color(elf3d::Color4{-1.0F, 2.0F, std::numeric_limits<float>::quiet_NaN(),
                                            std::numeric_limits<float>::infinity()});
    const elf3d::Color4 expected{0.0F, 1.0F, 0.0F, 1.0F};
    if (viewport->clear_color() != expected ||
        !viewport->render(*renderer_result.value(), scene, camera_result.value())) {
        return 7;
    }

    elf3d::BasicLighting lighting;
    lighting.direction = {0.0F, 3.0F, 4.0F};
    lighting.ambient_intensity = 5.0F;
    lighting.diffuse_intensity = 20.0F;
    viewport->set_basic_lighting(lighting);
    const elf3d::BasicLighting sanitized = viewport->basic_lighting();
    if (sanitized.direction != elf3d::Float3{0.0F, 0.6F, 0.8F} ||
        sanitized.ambient_intensity != 2.0F || sanitized.diffuse_intensity != 10.0F) {
        return 8;
    }

    if (!viewport->reset_view(scene, camera_result.value())) {
        return 840;
    }
    const elf3d::Extent2D focus_anchor_target_extent{256U, 144U};
    const std::size_t focus_anchor_pixel_count =
        static_cast<std::size_t>(focus_anchor_target_extent.width) *
        static_cast<std::size_t>(focus_anchor_target_extent.height);
    device->picking_pixel = elf3d::graphics::PickingPixel{1U, 0U, 0U, 0.5F};
    device->picking_depths.assign(focus_anchor_pixel_count, 0.5F);
    FakePickingTarget dynamic_anchor_reference_target{focus_anchor_target_extent};
    const elf3d::scene::VisibilityFilter dynamic_anchor_visibility =
        elf3d::scene::make_visibility_filter(scene, std::nullopt).value();
    const elf3d::Result<elf3d::renderer::GpuFocusDepthAnchorResult> dynamic_anchor_result =
        renderer_result.value()->gpu_focus_depth_anchor(
            scene, camera_result.value(), dynamic_anchor_reference_target,
            elf3d::Extent2D{640, 360}, dynamic_anchor_visibility,
            elf3d::clipping::disabled_filter());
    if (!dynamic_anchor_result || !dynamic_anchor_result.value().world_position.has_value() ||
        dynamic_anchor_result.value().pixels_read != focus_anchor_pixel_count) {
        return 841;
    }
    const elf3d::Float3 dynamic_anchor = dynamic_anchor_result.value().world_position.value();
    const elf3d::ProjectedViewportPoint dynamic_anchor_before =
        viewport->project_world_to_viewport(scene, camera_result.value(), dynamic_anchor).value();
    if (!dynamic_anchor_before.is_inside_viewport ||
        !nearly_equal(dynamic_anchor_before.position_pixels.x, 320.0F, 0.5F) ||
        !nearly_equal(dynamic_anchor_before.position_pixels.y, 180.0F, 0.5F)) {
        return 842;
    }
    device->picking_depths_read_count = 0;
    device->picking_pixel_read_count = 0;
    elf3d::ViewportInput orbit_start_input;
    orbit_start_input.is_focused = true;
    orbit_start_input.is_hovered = true;
    orbit_start_input.pointer_position_pixels = {16.0F, 16.0F};
    orbit_start_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), orbit_start_input)) {
        return 843;
    }
    orbit_start_input.pointer_position_pixels = {32.0F, 16.0F};
    orbit_start_input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), orbit_start_input)) {
        return 844;
    }
    const elf3d::PickingStatistics focus_stats = viewport->picking_statistics(picking_service);
    if (device->picking_depths_read_count != 1 || device->picking_pixel_read_count != 0 ||
        device->last_picking_read_extent != focus_anchor_target_extent ||
        focus_stats.latest_gpu_pixels_read > 65536U) {
        return 845;
    }
    orbit_start_input.pointer_position_pixels = {48.0F, 16.0F};
    orbit_start_input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), orbit_start_input)) {
        return 846;
    }
    const elf3d::ProjectedViewportPoint dynamic_anchor_after =
        viewport->project_world_to_viewport(scene, camera_result.value(), dynamic_anchor).value();
    if (!nearly_equal(dynamic_anchor_after.position_pixels.x,
                      dynamic_anchor_before.position_pixels.x, 0.1F) ||
        !nearly_equal(dynamic_anchor_after.position_pixels.y,
                      dynamic_anchor_before.position_pixels.y, 0.1F)) {
        return 848;
    }
    orbit_start_input.left_button_down = false;
    orbit_start_input.pointer_delta_pixels = {};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), orbit_start_input)) {
        return 849;
    }

    if (!viewport->reset_view(scene, camera_result.value())) {
        return 867;
    }
    device->picking_depths.assign(focus_anchor_pixel_count, 0.5F);
    device->picking_depths_read_count = 0;
    device->picking_pixel_read_count = 0;
    elf3d::ViewportInput eye_orbit_input;
    eye_orbit_input.is_focused = true;
    eye_orbit_input.is_hovered = true;
    eye_orbit_input.space_down = true;
    eye_orbit_input.left_button_down = true;
    eye_orbit_input.pointer_position_pixels = {16.0F, 16.0F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), eye_orbit_input)) {
        return 868;
    }
    eye_orbit_input.pointer_position_pixels = {32.0F, 16.0F};
    eye_orbit_input.pointer_delta_pixels = {16.0F, 0.0F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), eye_orbit_input)) {
        return 869;
    }
    std::optional<elf3d::NavigationSnapshot> eye_orbit_snapshot =
        viewport->navigation_snapshot();
    if (device->picking_depths_read_count != 0 || device->picking_pixel_read_count != 0 ||
        !eye_orbit_snapshot.has_value() || !eye_orbit_snapshot->is_pointer_captured ||
        eye_orbit_snapshot->interaction_mode != elf3d::NavigationInteractionMode::orbit) {
        return 870;
    }
    const float eye_orbit_yaw = eye_orbit_snapshot->yaw_radians;
    eye_orbit_input.space_down = false;
    eye_orbit_input.pointer_position_pixels = {64.0F, 16.0F};
    eye_orbit_input.pointer_delta_pixels = {32.0F, 0.0F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), eye_orbit_input)) {
        return 871;
    }
    eye_orbit_snapshot = viewport->navigation_snapshot();
    if (device->picking_depths_read_count != 0 || device->picking_pixel_read_count != 0 ||
        !eye_orbit_snapshot.has_value() ||
        nearly_equal(eye_orbit_snapshot->yaw_radians, eye_orbit_yaw)) {
        return 872;
    }
    eye_orbit_input.left_button_down = false;
    eye_orbit_input.pointer_delta_pixels = {};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), eye_orbit_input) ||
        viewport->navigation_snapshot()->is_pointer_captured) {
        return 873;
    }

    if (!viewport->reset_view(scene, camera_result.value())) {
        return 858;
    }
    device->picking_depths_read_count = 0;
    device->picking_pixel_read_count = 0;
    elf3d::ViewportInput quick_click_input;
    quick_click_input.is_focused = true;
    quick_click_input.is_hovered = true;
    quick_click_input.pointer_position_pixels = {319.5F, 179.5F};
    const elf3d::Result<std::optional<elf3d::PickHit>> quick_click_hit =
        viewport->pick(*renderer_result.value(), picking_service, scene, camera_result.value(),
                       quick_click_input.pointer_position_pixels, elf3d::PickOptions{});
    if (!quick_click_hit || !quick_click_hit.value().has_value()) {
        return 859;
    }
    const elf3d::Float3 quick_click_anchor = quick_click_hit.value()->world_position;
    device->picking_depths_read_count = 0;
    device->picking_pixel_read_count = 0;
    quick_click_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), quick_click_input)) {
        return 860;
    }
    quick_click_input.left_button_down = false;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), quick_click_input)) {
        return 861;
    }
    if (device->picking_depths_read_count != 0 || device->picking_pixel_read_count == 0) {
        return 862;
    }
    const elf3d::ProjectedViewportPoint quick_click_anchor_before =
        viewport->project_world_to_viewport(scene, camera_result.value(), quick_click_anchor)
            .value();
    quick_click_input.pointer_delta_pixels = {};
    quick_click_input.wheel_delta = 1.0F;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), quick_click_input)) {
        return 864;
    }
    const elf3d::ProjectedViewportPoint quick_click_anchor_after =
        viewport->project_world_to_viewport(scene, camera_result.value(), quick_click_anchor)
            .value();
    if (!nearly_equal(quick_click_anchor_after.position_pixels.x,
                      quick_click_anchor_before.position_pixels.x, 0.05F) ||
        !nearly_equal(quick_click_anchor_after.position_pixels.y,
                      quick_click_anchor_before.position_pixels.y, 0.05F)) {
        return 866;
    }

    if (!viewport->reset_view(scene, camera_result.value())) {
        return 850;
    }
    elf3d::ViewportInput miss_click_input;
    miss_click_input.is_focused = true;
    miss_click_input.is_hovered = true;
    miss_click_input.pointer_position_pixels = {319.5F, 179.5F};
    device->picking_pixel.reset();
    device->picking_depths.clear();
    device->picking_depths_read_count = 0;
    device->picking_pixel_read_count = 0;
    miss_click_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), miss_click_input)) {
        return 852;
    }
    miss_click_input.left_button_down = false;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), miss_click_input)) {
        return 853;
    }
    if (device->picking_depths_read_count != 0 || device->picking_pixel_read_count == 0) {
        return 856;
    }
    if (!viewport->reset_view(scene, camera_result.value())) {
        return 857;
    }

    elf3d::ViewportInput click_input;
    click_input.is_focused = true;
    click_input.is_hovered = true;
    click_input.pointer_position_pixels = {319.5F, 179.5F};
    device->picking_pixel = elf3d::graphics::PickingPixel{1U, 0U, 0U, 0.5F};
    click_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input) ||
        viewport->has_selection()) {
        return 81;
    }
    click_input.control_down = true;
    click_input.left_button_down = false;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input) ||
        !viewport->has_selection() || viewport->selected_entity() != model.value()) {
        return 82;
    }
    const elf3d::PickingStatistics pick_stats = viewport->picking_statistics(picking_service);
    if (pick_stats.latest_gpu_requests != 1 || pick_stats.latest_gpu_hits != 1 ||
        pick_stats.latest_gpu_misses != 0 || pick_stats.latest_cpu_refinements != 1 ||
        pick_stats.latest_cpu_fallbacks != 0 || pick_stats.latest_triangle_tests != 1) {
        return 824;
    }
    if (!nearly_equal(device->last_picking_read_position.x, 159.5F, 0.001F) ||
        !nearly_equal(device->last_picking_read_position.y, 89.5F, 0.001F)) {
        return 865;
    }
    click_input.control_down = false;
    if (!viewport->hide_selected(scene) || !viewport->has_selection() ||
        scene.entity_effective_visibility(model.value()).value()) {
        return 821;
    }
    if (!viewport->render(*renderer_result.value(), scene, camera_result.value()) ||
        viewport->statistics().draw_calls != 0) {
        return 822;
    }
    if (!viewport->show_selected(scene) ||
        !scene.entity_effective_visibility(model.value()).value()) {
        return 823;
    }
    if (!viewport->isolate_selected(scene) || !viewport->is_isolating() ||
        viewport->isolated_entity() != model.value()) {
        return 824;
    }
    const elf3d::Result<std::optional<elf3d::Bounds3>> visible_bounds =
        viewport->visible_bounds(scene);
    if (!visible_bounds || !visible_bounds.value().has_value()) {
        return 825;
    }
    elf3d::SectionPlane clip_plane;
    clip_plane.enabled = true;
    clip_plane.point = {0.0F, 0.0F, -2.0F};
    clip_plane.normal = {1.0F, 0.0F, 0.0F};
    const std::uint64_t clipping_revision = viewport->clipping_snapshot().revision;
    if (!viewport->set_section_plane(clip_plane) ||
        viewport->clipping_snapshot().revision != clipping_revision + 1U) {
        return 827;
    }
    const elf3d::Result<std::optional<elf3d::Bounds3>> plane_clipped_bounds =
        viewport->visible_bounds(scene);
    if (!plane_clipped_bounds || !plane_clipped_bounds.value().has_value() ||
        !nearly_equal(plane_clipped_bounds.value()->minimum.x, 0.0F) ||
        !nearly_equal(plane_clipped_bounds.value()->maximum.x, 0.5F)) {
        return 828;
    }

    auto second_owned_device = std::make_unique<FakeDevice>();
    elf3d::Result<std::unique_ptr<elf3d::renderer::Renderer>> second_renderer =
        elf3d::renderer::Renderer::create(std::move(second_owned_device), 1);
    if (!second_renderer) {
        return 829;
    }
    elf3d::picking::PickingService second_picking_service;
    elf3d::Result<std::unique_ptr<elf3d::viewport::OffscreenViewport>> second_viewport_result =
        elf3d::viewport::OffscreenViewport::create(second_renderer.value()->device(),
                                                   elf3d::Extent2D{640, 360});
    if (!second_viewport_result) {
        return 829;
    }
    std::unique_ptr<elf3d::viewport::OffscreenViewport> second_viewport =
        std::move(second_viewport_result).value();
    const elf3d::Result<std::optional<elf3d::Bounds3>> second_unclipped_bounds =
        second_viewport->visible_bounds(scene);
    if (!second_unclipped_bounds || !second_unclipped_bounds.value().has_value() ||
        !nearly_equal(second_unclipped_bounds.value()->minimum.x, -0.5F) ||
        !nearly_equal(second_unclipped_bounds.value()->maximum.x, 0.5F)) {
        return 830;
    }
    const elf3d::ClippingBox centered_box{{-0.25F, -0.25F, -2.25F}, {0.25F, 0.25F, -1.75F}, true};
    if (second_viewport->add_clipping_box(centered_box).value() != 0 ||
        viewport->clipping_snapshot().box_count != 0 ||
        second_viewport->clipping_snapshot().box_count != 1) {
        return 831;
    }
    const elf3d::Result<std::optional<elf3d::Bounds3>> second_box_bounds =
        second_viewport->visible_bounds(scene);
    if (!second_box_bounds || !second_box_bounds.value().has_value() ||
        !nearly_equal(second_box_bounds.value()->minimum.x, -0.25F) ||
        !nearly_equal(second_box_bounds.value()->maximum.x, 0.25F)) {
        return 832;
    }
    viewport->clear_clipping();
    const elf3d::Result<std::optional<elf3d::Bounds3>> restored_bounds =
        viewport->visible_bounds(scene);
    if (!restored_bounds || !restored_bounds.value().has_value() ||
        !nearly_equal(restored_bounds.value()->minimum.x, -0.5F) ||
        !nearly_equal(restored_bounds.value()->maximum.x, 0.5F)) {
        return 833;
    }
    viewport->clear_isolation();
    if (viewport->is_isolating()) {
        return 826;
    }

    viewport->clear_selection();
    click_input.left_button_down = true;
    click_input.pointer_delta_pixels = {};
    click_input.pointer_position_pixels = {319.5F, 179.5F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input)) {
        return 83;
    }
    click_input.pointer_delta_pixels = {10.0F, 0.0F};
    click_input.pointer_position_pixels = {329.5F, 179.5F};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input)) {
        return 84;
    }
    click_input.left_button_down = false;
    click_input.pointer_delta_pixels = {};
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input) ||
        viewport->has_selection()) {
        return 85;
    }

    viewport->set_active_tool(elf3d::ViewportTool::distance_measurement);
    if (viewport->active_tool() != elf3d::ViewportTool::distance_measurement) {
        return 86;
    }
    click_input.pointer_position_pixels = {319.5F, 179.5F};
    click_input.pointer_delta_pixels = {};
    click_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input)) {
        return 87;
    }
    click_input.left_button_down = false;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input) ||
        viewport->has_selection()) {
        return 88;
    }
    elf3d::DistanceMeasurementSnapshot measurement = viewport->distance_measurement_snapshot(scene);
    if (measurement.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        !measurement.first_point.has_value()) {
        return 89;
    }
    click_input.left_button_down = true;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input)) {
        return 90;
    }
    click_input.left_button_down = false;
    if (!viewport->update_navigation(*renderer_result.value(), picking_service, scene,
                                     camera_result.value(), click_input)) {
        return 91;
    }
    measurement = viewport->distance_measurement_snapshot(scene);
    if (measurement.state != elf3d::DistanceMeasurementState::complete ||
        !measurement.second_point.has_value() || measurement.distance_meters != 0.0) {
        return 92;
    }
    if (!viewport->render(*renderer_result.value(), scene, camera_result.value()) ||
        viewport->statistics().overlay_lines != 1 || viewport->statistics().overlay_markers != 2 ||
        device->latest_overlay_lines != 1 || device->latest_overlay_markers != 2) {
        return 93;
    }
    viewport->set_active_tool(elf3d::ViewportTool::selection);
    if (viewport->active_tool() != elf3d::ViewportTool::selection ||
        viewport->distance_measurement_snapshot(scene).state !=
            elf3d::DistanceMeasurementState::complete) {
        return 94;
    }
    viewport->clear_distance_measurement();
    if (viewport->distance_measurement_snapshot(scene).state !=
        elf3d::DistanceMeasurementState::empty) {
        return 95;
    }

    if (!viewport->resize(elf3d::Extent2D{0, 360}) || viewport->framebuffer_valid() ||
        viewport->color_texture().is_valid()) {
        return 10;
    }

    return 0;
}
