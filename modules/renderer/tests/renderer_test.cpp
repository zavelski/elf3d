#include <elf3d/assets/handle_access.h>
#include <elf3d/renderer/renderer.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

namespace {

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_value;
    }
    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) override {
        extent_value = extent;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4) override {
        ++clear_count;
        return {};
    }
    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override {
        return {};
    }
    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_value.width != 0 && extent_value.height != 0;
    }

    elf3d::Extent2D extent_value{640, 360};
    int clear_count = 0;
};

class FakePickingTarget final : public elf3d::graphics::PickingTarget {
  public:
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_value;
    }
    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) override {
        extent_value = extent;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> clear() override {
        ++clear_count;
        return {};
    }
    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_value.width != 0 && extent_value.height != 0;
    }

    elf3d::Extent2D extent_value{640, 360};
    int clear_count = 0;
};

class FakeMesh final : public elf3d::graphics::StaticMesh {
  public:
    FakeMesh(std::uint32_t vertices, std::uint32_t indices, int *destruction_count) noexcept
        : vertices_(vertices), indices_(indices), destruction_count_(destruction_count) {}
    ~FakeMesh() override {
        ++(*destruction_count_);
    }
    [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
        return vertices_;
    }
    [[nodiscard]] std::uint32_t index_count() const noexcept override {
        return indices_;
    }

  private:
    std::uint32_t vertices_ = 0;
    std::uint32_t indices_ = 0;
    int *destruction_count_ = nullptr;
};

class FakePipeline final : public elf3d::graphics::GraphicsPipeline {};
class FakeTexture final : public elf3d::graphics::Texture2D {
  public:
    explicit FakeTexture(int *destruction_count) noexcept : destruction_count_(destruction_count) {}
    ~FakeTexture() override {
        ++(*destruction_count_);
    }
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return {1, 1};
    }

  private:
    int *destruction_count_ = nullptr;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::opengl;
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D) override {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Not used"};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) override {
        auto target = std::make_unique<FakePickingTarget>();
        target->extent_value = initial_extent;
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }
    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle) const override {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Not used"};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>>
    create_static_mesh(const elf3d::graphics::StaticMeshDescription &description) override {
        ++upload_count;
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>(
            description.vertex_count, static_cast<std::uint32_t>(description.indices.size()),
            &mesh_destruction_count)};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription &description) override {
        ++texture_upload_count;
        texture_descriptions.push_back(description);
        return std::unique_ptr<elf3d::graphics::Texture2D>{
            std::make_unique<FakeTexture>(&texture_destruction_count)};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(const elf3d::graphics::GraphicsPipelineDescription &) override {
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget &, elf3d::graphics::GraphicsPipeline &,
                 elf3d::graphics::StaticMesh &,
                 const elf3d::graphics::DrawIndexedDescription &description) override {
        ++draw_count;
        draws.push_back(description);
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_overlay(elf3d::graphics::RenderTarget &,
                 const elf3d::graphics::DrawOverlayDescription &description) override {
        ++overlay_draw_count;
        overlay_line_count += static_cast<int>(description.lines.size());
        overlay_marker_count += static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_picking_indexed(elf3d::graphics::PickingTarget &, elf3d::graphics::StaticMesh &,
                         const elf3d::graphics::PickingDrawDescription &description) override {
        ++picking_draw_count;
        picking_draws.push_back(description);
        return {};
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::PickingPixel>
    read_picking_pixel(elf3d::graphics::PickingTarget &, elf3d::Float2) override {
        return picking_pixel;
    }

    int upload_count = 0;
    int draw_count = 0;
    int overlay_draw_count = 0;
    int picking_draw_count = 0;
    int overlay_line_count = 0;
    int overlay_marker_count = 0;
    int mesh_destruction_count = 0;
    int texture_upload_count = 0;
    int texture_destruction_count = 0;
    elf3d::graphics::PickingPixel picking_pixel;
    std::vector<elf3d::graphics::Texture2DDescription> texture_descriptions;
    std::vector<elf3d::graphics::DrawIndexedDescription> draws;
    std::vector<elf3d::graphics::PickingDrawDescription> picking_draws;
};

[[nodiscard]] bool nearly_equal(float left, float right, float tolerance = 0.0001F) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.0001F) noexcept {
    return nearly_equal(left.x, right.x, tolerance) && nearly_equal(left.y, right.y, tolerance) &&
           nearly_equal(left.z, right.z, tolerance);
}

} // namespace

int main() {
    constexpr std::uintptr_t engine_token = 11;
    const elf3d::SceneId id = elf3d::detail::SceneHandleAccess::create_scene(engine_token, 1);
    elf3d::scene::Storage scene{id};
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = scene.create_mesh({vertices, indices});
    const std::array<std::byte, 4> pixel{
        {std::byte{255}, std::byte{128}, std::byte{64}, std::byte{255}}};
    const auto image = scene.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
    const auto texture = scene.create_texture({image.value(), {}});
    elf3d::SamplerDescription clamp_sampler;
    clamp_sampler.wrap_u = elf3d::TextureWrap::clamp_to_edge;
    const auto clamped_texture = scene.create_texture({image.value(), clamp_sampler});
    elf3d::MaterialDescription textured_description;
    textured_description.base_color = {0.5F, 0.5F, 0.5F, 1.0F};
    textured_description.base_color_texture = texture.value();
    textured_description.metallic_roughness_texture = texture.value();
    const auto material = scene.create_material(textured_description);
    elf3d::MaterialDescription double_sided_description;
    double_sided_description.base_color = {0.8F, 0.2F, 0.2F, 1.0F};
    double_sided_description.double_sided = true;
    double_sided_description.base_color_texture = clamped_texture.value();
    const auto double_sided = scene.create_material(double_sided_description);
    if (!mesh || !image || !texture || !clamped_texture || !material || !double_sided) {
        return 1;
    }
    const auto model = scene.create_model(mesh.value(), material.value());
    const auto camera = scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!model || !camera) {
        return 1;
    }
    const std::array<elf3d::ModelPrimitiveBinding, 2> primitives{{
        {mesh.value(), material.value()},
        {mesh.value(), double_sided.value()},
    }};
    elf3d::Transform mirrored;
    mirrored.scale = {-1.0F, 2.0F, 1.0F};
    if (!scene.set_model_primitives(model.value(), primitives) ||
        !scene.set_local_transform(model.value(), mirrored)) {
        return 1;
    }

    const auto non_camera = scene.create_entity();
    if (elf3d::renderer::build_render_list(scene, non_camera.value(), {640, 360}).error().code() !=
        elf3d::ErrorCode::entity_has_no_camera) {
        return 2;
    }

    auto device = std::make_shared<FakeDevice>();
    const auto renderer = elf3d::renderer::Renderer::create(device, engine_token);
    if (!renderer) {
        return 3;
    }
    FakeRenderTarget target;
    const auto first = renderer.value()->render(scene, camera.value(), target, {}, {});
    const auto second = renderer.value()->render(scene, camera.value(), target, {}, {});
    const elf3d::RenderStatistics expected_first{2, 2, 6, 6, 3, 3, 3, 0, 0};
    const elf3d::RenderStatistics expected_second{2, 2, 6, 6, 3, 0, 3, 0, 0};
    if (!first || !second || first.value() != expected_first || second.value() != expected_second ||
        device->upload_count != 1 || device->draw_count != 4 || device->draws.size() != 4 ||
        device->texture_upload_count != 3 || device->texture_descriptions.size() != 3 ||
        device->texture_descriptions[0].format != elf3d::graphics::TextureFormat::rgba8_srgb ||
        device->texture_descriptions[1].format != elf3d::graphics::TextureFormat::rgba8_unorm ||
        device->texture_descriptions[2].wrap_u !=
            elf3d::graphics::TextureAddressMode::clamp_to_edge ||
        !device->draws[0].front_face_clockwise || device->draws[0].double_sided ||
        !device->draws[1].front_face_clockwise || !device->draws[1].double_sided) {
        return 4;
    }

    FakePickingTarget picking_target;
    device->picking_pixel = elf3d::graphics::PickingPixel{2U, 1U, 0U, 0.5F, true};
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(scene, std::nullopt).value();
    const auto gpu_pick = renderer.value()->gpu_pick(
        scene, camera.value(), picking_target, {319.5F, 179.5F}, visibility,
        elf3d::clipping::disabled_filter());
    if (!gpu_pick || !gpu_pick.value().hit.has_value() || gpu_pick.value().draw_calls != 2 ||
        gpu_pick.value().pixels_read != 1 || picking_target.clear_count != 1 ||
        device->picking_draw_count != 2 || device->picking_draws.size() != 2 ||
        device->picking_draws[0].object_id != 1U || device->picking_draws[0].primitive_index != 0U ||
        device->picking_draws[1].object_id != 2U || device->picking_draws[1].primitive_index != 1U ||
        gpu_pick.value().hit->entity != model.value() ||
        gpu_pick.value().hit->mesh != mesh.value() ||
        gpu_pick.value().hit->primitive_index != 1U ||
        gpu_pick.value().hit->triangle_index != 0U) {
        return 45;
    }

    const auto shared_model = scene.create_model(mesh.value(), material.value());
    elf3d::ViewportRenderOptions highlight_options;
    highlight_options.highlight =
        elf3d::EntityHighlight{model.value(), elf3d::Color4{1.0F, 0.2F, 0.0F, 1.0F}, 0.6F};
    const auto highlighted =
        renderer.value()->render(scene, camera.value(), target, {}, {}, highlight_options);
    const elf3d::RenderStatistics expected_highlighted{3, 3, 9, 9, 5, 0, 3, 0, 0};
    if (!shared_model || !highlighted || highlighted.value() != expected_highlighted ||
        device->draws.size() != 7 || device->draws[4].highlight_strength != 0.6F ||
        device->draws[5].highlight_strength != 0.6F ||
        device->draws[6].highlight_strength != 0.0F) {
        return 41;
    }
    const auto hidden_visibility = scene.set_entity_visible(model.value(), false);
    const auto hidden_render =
        renderer.value()->render(scene, camera.value(), target, {}, {}, highlight_options);
    const elf3d::RenderStatistics expected_hidden{1, 1, 3, 3, 2, 0, 3, 0, 0};
    if (!hidden_visibility || !hidden_render || hidden_render.value() != expected_hidden ||
        device->draws.size() != 8 || device->draws.back().highlight_strength != 0.0F) {
        return 42;
    }
    const auto restored_visibility = scene.show_all_entities();
    const auto restored_render =
        renderer.value()->render(scene, camera.value(), target, {}, {}, highlight_options);
    if (!restored_visibility || !restored_render ||
        restored_render.value() != expected_highlighted) {
        return 43;
    }

    const std::array<elf3d::OverlayLineSegment, 1> overlay_lines{
        elf3d::OverlayLineSegment{{0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, -1.0F}}};
    const std::array<elf3d::OverlayPointMarker, 1> overlay_markers{
        elf3d::OverlayPointMarker{{0.0F, 0.0F, -1.0F}}};
    elf3d::ViewportRenderOptions overlay_options;
    overlay_options.overlay_lines = overlay_lines;
    overlay_options.overlay_markers = overlay_markers;
    const auto overlay_render =
        renderer.value()->render(scene, camera.value(), target, {}, {}, overlay_options);
    if (!overlay_render || overlay_render.value().overlay_lines != 1 ||
        overlay_render.value().overlay_markers != 1 || device->overlay_draw_count != 1 ||
        device->overlay_line_count != 1 || device->overlay_marker_count != 1) {
        return 44;
    }

    const elf3d::scene::VisibilityFilter clipping_visibility =
        elf3d::scene::make_visibility_filter(scene, std::nullopt).value();
    elf3d::SectionPlane cutting_plane;
    cutting_plane.enabled = true;
    cutting_plane.point = {-0.5F, 0.0F, 0.0F};
    cutting_plane.normal = {2.0F, 0.0F, 0.0F};
    const elf3d::clipping::ClippingFilter cutting_filter =
        elf3d::clipping::make_filter(cutting_plane, {}, 101).value();
    const elf3d::Result<elf3d::renderer::RenderList> clipped_list =
        elf3d::renderer::build_render_list(scene, camera.value(), {640, 360},
                                           clipping_visibility, cutting_filter);
    if (!clipped_list || clipped_list.value().items.size() != 3 ||
        clipped_list.value().clipping_bounds_tested != 3 ||
        clipped_list.value().clipping_bounds_rejected != 0 ||
        clipped_list.value().clipping_bounds_intersecting != 2) {
        return 45;
    }
    const std::size_t draw_count_before_cutting = device->draws.size();
    const elf3d::Result<elf3d::RenderStatistics> clipped_render = renderer.value()->render(
        scene, camera.value(), target, {}, {}, {}, clipping_visibility, cutting_filter);
    if (!clipped_render || clipped_render.value().draw_calls != 3 ||
        clipped_render.value().clipping_bounds_tested != 3 ||
        clipped_render.value().clipping_bounds_rejected != 0 ||
        clipped_render.value().clipping_bounds_intersecting != 2 ||
        device->draws.size() != draw_count_before_cutting + 3 ||
        !device->draws[draw_count_before_cutting].clipping_section_plane_enabled ||
        !nearly_equal(device->draws[draw_count_before_cutting].clipping_section_plane_normal,
                      {1.0F, 0.0F, 0.0F}) ||
        !nearly_equal(device->draws[draw_count_before_cutting].clipping_section_plane_offset,
                      0.5F) ||
        !device->draws[draw_count_before_cutting].clipping_retain_positive_half_space) {
        return 46;
    }

    cutting_plane.point = {10.0F, 0.0F, 0.0F};
    const elf3d::clipping::ClippingFilter outside_filter =
        elf3d::clipping::make_filter(cutting_plane, {}, 102).value();
    const std::size_t draw_count_before_outside = device->draws.size();
    const elf3d::Result<elf3d::RenderStatistics> outside_render = renderer.value()->render(
        scene, camera.value(), target, {}, {}, {}, clipping_visibility, outside_filter);
    if (!outside_render || outside_render.value().draw_calls != 0 ||
        outside_render.value().clipping_bounds_tested != 3 ||
        outside_render.value().clipping_bounds_rejected != 3 ||
        outside_render.value().unique_gpu_textures != 3 ||
        device->draws.size() != draw_count_before_outside || device->upload_count != 1 ||
        device->texture_upload_count != 3) {
        return 47;
    }

    const std::array<elf3d::ClippingBox, 2> clipping_boxes{{
        {{20.0F, 20.0F, 20.0F}, {21.0F, 21.0F, 21.0F}, false},
        {{-2.0F, -1.0F, -1.0F}, {2.0F, 3.0F, 1.0F}, true},
    }};
    const elf3d::clipping::ClippingFilter box_filter =
        elf3d::clipping::make_filter(elf3d::SectionPlane{}, clipping_boxes, 103).value();
    const std::size_t draw_count_before_boxes = device->draws.size();
    const elf3d::Result<elf3d::RenderStatistics> box_render = renderer.value()->render(
        scene, camera.value(), target, {}, {}, {}, clipping_visibility, box_filter);
    if (!box_render || box_render.value().draw_calls != 3 ||
        box_render.value().clipping_bounds_tested != 3 ||
        box_render.value().clipping_bounds_rejected != 0 ||
        box_render.value().clipping_bounds_intersecting != 0 ||
        device->draws.size() != draw_count_before_boxes + 3 ||
        device->draws[draw_count_before_boxes].clipping_box_count != 1 ||
        device->draws[draw_count_before_boxes].clipping_boxes[0].minimum !=
            clipping_boxes[1].minimum ||
        device->draws[draw_count_before_boxes].clipping_boxes[0].maximum !=
            clipping_boxes[1].maximum) {
        return 48;
    }

    const elf3d::SceneId second_id =
        elf3d::detail::SceneHandleAccess::create_scene(engine_token, 2);
    elf3d::scene::Storage second_scene{second_id};
    const auto second_mesh = second_scene.create_mesh({vertices, indices});
    const auto second_material =
        second_scene.create_material({elf3d::Color4{0.2F, 0.4F, 0.8F, 1.0F}});
    if (!second_mesh || !second_material) {
        return 5;
    }
    const auto second_model =
        second_scene.create_model(second_mesh.value(), second_material.value());
    const auto second_camera =
        second_scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{});
    if (!second_model || !second_camera ||
        !renderer.value()->render(second_scene, second_camera.value(), target, {}, {}) ||
        device->upload_count != 2) {
        return 5;
    }

    renderer.value()->release_scene(id);
    if (device->mesh_destruction_count != 1 || device->texture_destruction_count != 3 ||
        !renderer.value()->render(second_scene, second_camera.value(), target, {}, {}) ||
        device->upload_count != 2) {
        return 6;
    }
    renderer.value()->release_scene(second_id);
    if (device->mesh_destruction_count != 2) {
        return 7;
    }

    target.extent_value = {};
    const auto zero = renderer.value()->render(scene, non_camera.value(), target, {}, {});
    if (!zero || zero.value() != elf3d::RenderStatistics{}) {
        return 8;
    }
    return 0;
}
