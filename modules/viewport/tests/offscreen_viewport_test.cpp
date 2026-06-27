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
#include <limits>
#include <memory>
#include <optional>
#include <utility>

import elf.assets;
import elf.graphics;
import elf.renderer;
import elf.scene;
import elf.viewport;

namespace {

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    explicit FakeRenderTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {
        update_handle();
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        update_handle();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4 color) override {
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

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear() override {
        ++clear_count;
        return {};
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_.width != 0 && extent_.height != 0;
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
    create_render_target(elf3d::Extent2D initial_extent) override {
        auto target = std::make_unique<FakeRenderTarget>(initial_extent);
        last_target = target.get();
        return std::unique_ptr<elf3d::graphics::RenderTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) override {
        auto target = std::make_unique<FakePickingTarget>(initial_extent);
        last_picking_target = target.get();
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle texture) const override {
        if (!texture.is_valid()) {
            return elf3d::Error{elf3d::ErrorCode::texture_unavailable,
                                "Fake texture is unavailable"};
        }
        return elf3d::NativeTextureView{elf3d::NativeGraphicsApi::opengl, 1, last_target->extent()};
    }

    class FakeMesh final : public elf3d::graphics::StaticMesh {
      public:
        [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
            return 0;
        }
        [[nodiscard]] std::uint32_t index_count() const noexcept override {
            return 0;
        }
    };

    class FakePipeline final : public elf3d::graphics::GraphicsPipeline {};
    class FakeTexture final : public elf3d::graphics::Texture2D {
      public:
        [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
            return {1, 1};
        }
    };

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>>
    create_static_mesh(const elf3d::graphics::StaticMeshDescription &) override {
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription &) override {
        return std::unique_ptr<elf3d::graphics::Texture2D>{std::make_unique<FakeTexture>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(const elf3d::graphics::GraphicsPipelineDescription &) override {
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }

    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget &, elf3d::graphics::GraphicsPipeline &,
                 elf3d::graphics::StaticMesh &,
                 const elf3d::graphics::DrawIndexedDescription &) override {
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_overlay(elf3d::graphics::RenderTarget &,
                 const elf3d::graphics::DrawOverlayDescription &description) override {
        latest_overlay_lines = static_cast<int>(description.lines.size());
        latest_overlay_markers = static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_picking_indexed(elf3d::graphics::PickingTarget &, elf3d::graphics::StaticMesh &,
                         const elf3d::graphics::PickingDrawDescription &) override {
        return {};
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::PickingPixel>
    read_picking_pixel(elf3d::graphics::PickingTarget &, elf3d::Float2) override {
        return picking_pixel;
    }

    FakeRenderTarget *last_target = nullptr;
    FakePickingTarget *last_picking_target = nullptr;
    elf3d::graphics::PickingPixel picking_pixel;
    int latest_overlay_lines = 0;
    int latest_overlay_markers = 0;
};

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main() {
    auto device = std::make_shared<FakeDevice>();
    elf3d::Result<std::shared_ptr<elf3d::renderer::Renderer>> renderer_result =
        elf3d::renderer::Renderer::create(device, 1);
    if (!renderer_result) {
        return 1;
    }
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
        elf3d::viewport::OffscreenViewport::create(
            device, renderer_result.value(), std::make_shared<elf3d::picking::PickingService>(),
            elf3d::Extent2D{});
    if (!create_result) {
        return 3;
    }

    std::unique_ptr<elf3d::viewport::OffscreenViewport> viewport = std::move(create_result).value();
    if (viewport->framebuffer_valid() || viewport->color_texture().is_valid()) {
        return 4;
    }
    if (!viewport->render(scene, camera_result.value()) || device->last_target->clear_count != 0) {
        return 5;
    }

    if (!viewport->resize(elf3d::Extent2D{640, 360}) ||
        !viewport->resize(elf3d::Extent2D{640, 360}) || device->last_target->resize_count != 1 ||
        !viewport->framebuffer_valid()) {
        return 6;
    }

    viewport->set_clear_color(elf3d::Color4{-1.0F, 2.0F, std::numeric_limits<float>::quiet_NaN(),
                                            std::numeric_limits<float>::infinity()});
    const elf3d::Color4 expected{0.0F, 1.0F, 0.0F, 1.0F};
    if (viewport->clear_color() != expected || !viewport->render(scene, camera_result.value()) ||
        device->last_target->clear_count != 1 ||
        device->last_target->last_clear_color != expected) {
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

    elf3d::ViewportInput click_input;
    click_input.is_focused = true;
    click_input.is_hovered = true;
    click_input.pointer_position_pixels = {319.5F, 179.5F};
    device->picking_pixel = elf3d::graphics::PickingPixel{1U, 0U, 0U, 0.5F, true};
    click_input.left_button_down = true;
    if (!viewport->update_navigation(scene, camera_result.value(), click_input) ||
        viewport->has_selection()) {
        return 81;
    }
    click_input.control_down = true;
    click_input.left_button_down = false;
    if (!viewport->update_navigation(scene, camera_result.value(), click_input) ||
        !viewport->has_selection() || viewport->selected_entity() != model.value()) {
        return 82;
    }
    const elf3d::PickingStatistics pick_stats = viewport->picking_statistics();
    if (pick_stats.latest_gpu_requests != 1 || pick_stats.latest_gpu_hits != 1 ||
        pick_stats.latest_gpu_misses != 0 || pick_stats.latest_cpu_refinements != 1 ||
        pick_stats.latest_cpu_fallbacks != 0 || pick_stats.latest_triangle_tests != 1) {
        return 824;
    }
    click_input.control_down = false;
    if (!viewport->hide_selected(scene) || !viewport->has_selection() ||
        scene.entity_effective_visibility(model.value()).value()) {
        return 821;
    }
    if (!viewport->render(scene, camera_result.value()) || viewport->statistics().draw_calls != 0) {
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
    const elf3d::Result<elf3d::Bounds3> visible_bounds = viewport->visible_bounds(scene);
    if (!visible_bounds || !visible_bounds.value().is_valid) {
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
    const elf3d::Result<elf3d::Bounds3> plane_clipped_bounds = viewport->visible_bounds(scene);
    if (!plane_clipped_bounds || !plane_clipped_bounds.value().is_valid ||
        !nearly_equal(plane_clipped_bounds.value().minimum.x, 0.0F) ||
        !nearly_equal(plane_clipped_bounds.value().maximum.x, 0.5F)) {
        return 828;
    }

    auto second_device = std::make_shared<FakeDevice>();
    elf3d::Result<std::shared_ptr<elf3d::renderer::Renderer>> second_renderer =
        elf3d::renderer::Renderer::create(second_device, 1);
    if (!second_renderer) {
        return 829;
    }
    elf3d::Result<std::unique_ptr<elf3d::viewport::OffscreenViewport>> second_viewport_result =
        elf3d::viewport::OffscreenViewport::create(
            second_device, second_renderer.value(),
            std::make_shared<elf3d::picking::PickingService>(), elf3d::Extent2D{640, 360});
    if (!second_viewport_result) {
        return 829;
    }
    std::unique_ptr<elf3d::viewport::OffscreenViewport> second_viewport =
        std::move(second_viewport_result).value();
    const elf3d::Result<elf3d::Bounds3> second_unclipped_bounds =
        second_viewport->visible_bounds(scene);
    if (!second_unclipped_bounds || !second_unclipped_bounds.value().is_valid ||
        !nearly_equal(second_unclipped_bounds.value().minimum.x, -0.5F) ||
        !nearly_equal(second_unclipped_bounds.value().maximum.x, 0.5F)) {
        return 830;
    }
    const elf3d::ClippingBox centered_box{{-0.25F, -0.25F, -2.25F},
                                          {0.25F, 0.25F, -1.75F},
                                          true};
    if (second_viewport->add_clipping_box(centered_box).value() != 0 ||
        viewport->clipping_snapshot().box_count != 0 ||
        second_viewport->clipping_snapshot().box_count != 1) {
        return 831;
    }
    const elf3d::Result<elf3d::Bounds3> second_box_bounds =
        second_viewport->visible_bounds(scene);
    if (!second_box_bounds || !second_box_bounds.value().is_valid ||
        !nearly_equal(second_box_bounds.value().minimum.x, -0.25F) ||
        !nearly_equal(second_box_bounds.value().maximum.x, 0.25F)) {
        return 832;
    }
    viewport->clear_clipping();
    const elf3d::Result<elf3d::Bounds3> restored_bounds = viewport->visible_bounds(scene);
    if (!restored_bounds || !restored_bounds.value().is_valid ||
        !nearly_equal(restored_bounds.value().minimum.x, -0.5F) ||
        !nearly_equal(restored_bounds.value().maximum.x, 0.5F)) {
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
    if (!viewport->update_navigation(scene, camera_result.value(), click_input)) {
        return 83;
    }
    click_input.pointer_delta_pixels = {10.0F, 0.0F};
    click_input.pointer_position_pixels = {329.5F, 179.5F};
    if (!viewport->update_navigation(scene, camera_result.value(), click_input)) {
        return 84;
    }
    click_input.left_button_down = false;
    click_input.pointer_delta_pixels = {};
    if (!viewport->update_navigation(scene, camera_result.value(), click_input) ||
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
    if (!viewport->update_navigation(scene, camera_result.value(), click_input)) {
        return 87;
    }
    click_input.left_button_down = false;
    if (!viewport->update_navigation(scene, camera_result.value(), click_input) ||
        viewport->has_selection()) {
        return 88;
    }
    elf3d::DistanceMeasurementSnapshot measurement = viewport->distance_measurement_snapshot(scene);
    if (measurement.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        !measurement.first_point.has_value()) {
        return 89;
    }
    click_input.left_button_down = true;
    if (!viewport->update_navigation(scene, camera_result.value(), click_input)) {
        return 90;
    }
    click_input.left_button_down = false;
    if (!viewport->update_navigation(scene, camera_result.value(), click_input)) {
        return 91;
    }
    measurement = viewport->distance_measurement_snapshot(scene);
    if (measurement.state != elf3d::DistanceMeasurementState::complete ||
        !measurement.second_point.has_value() || measurement.distance_meters != 0.0) {
        return 92;
    }
    if (!viewport->render(scene, camera_result.value()) ||
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

    elf3d::viewport::OffscreenViewport moved{std::move(*viewport)};
    if (!moved.framebuffer_valid() || viewport->render(scene, camera_result.value())) {
        return 9;
    }

    if (!moved.resize(elf3d::Extent2D{0, 360}) || moved.framebuffer_valid() ||
        moved.color_texture().is_valid()) {
        return 10;
    }

    return 0;
}
