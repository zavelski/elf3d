module;

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/rendering.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

export module elf.renderer;

import elf.clipping;
import elf.core;
import elf.graphics;
import elf.math;
import elf.scene;

export namespace elf3d::renderer {

struct RenderItem {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    Float4x4 model_matrix{};
    math::Matrix3x3 normal_matrix{};
    bool orientation_reversed = false;
    std::uint64_t material_identity = 0;
    AlphaMode alpha_mode = AlphaMode::opaque;
    float camera_distance_squared = 0.0F;
};

struct RenderList {
    Float4x4 view_matrix{};
    Float4x4 projection_matrix{};
    Float3 camera_world_position;
    std::vector<RenderItem> items;
    std::uint64_t candidate_primitives = 0;
    std::uint64_t frustum_culled_primitives = 0;
    std::uint64_t clipping_bounds_tested = 0;
    std::uint64_t clipping_bounds_rejected = 0;
    std::uint64_t clipping_bounds_intersecting = 0;
};

struct GpuPickHit {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
    Float3 world_position;
    float depth = 1.0F;
    float world_distance = 0.0F;
};

struct GpuPickResult {
    std::optional<GpuPickHit> hit;
    std::uint64_t draw_calls = 0;
    std::uint64_t pixels_read = 0;
    double pass_milliseconds = 0.0;
    double readback_milliseconds = 0.0;
};

struct GpuFocusDepthAnchorResult {
    std::optional<Float3> world_position;
    std::uint64_t draw_calls = 0;
    std::uint64_t pixels_read = 0;
    double pass_milliseconds = 0.0;
    double readback_milliseconds = 0.0;
};

struct GpuPickRequest {
    EntityId camera;
    Float2 target_position_pixels;
    Extent2D viewport_extent;
    Float2 viewport_position_pixels;
};

struct GpuFocusDepthRequest {
    EntityId camera;
    Extent2D viewport_extent;
};

struct RenderRequest {
    EntityId camera;
    Color4 clear_color;
    BasicLighting lighting;
    ViewportRenderOptions options;
};

[[nodiscard]] Result<RenderList> build_render_list(const scene::Storage& scene, EntityId camera,
                                                   Extent2D extent);
[[nodiscard]] Result<RenderList> build_render_list(const scene::Storage& scene, EntityId camera,
                                                   Extent2D extent,
                                                   const scene::VisibilityFilter& visibility);
[[nodiscard]] Result<RenderList> build_render_list(const scene::Storage& scene, EntityId camera,
                                                   Extent2D extent,
                                                   const scene::VisibilityFilter& visibility,
                                                   const clipping::ClippingFilter& clipping_filter);

class Renderer final {
  private:
    struct ConstructionKey final {};
    struct CacheState;
    struct DrawPacket;
    struct Resources final {
        std::unique_ptr<graphics::Device> device;
        std::uint64_t engine_token = 0;
        std::unique_ptr<graphics::GraphicsPipeline> pipeline;
        std::unique_ptr<CacheState> cache;
    };

  public:
    [[nodiscard]] static Result<std::unique_ptr<Renderer>>
    create(std::unique_ptr<graphics::Device> device, std::uint64_t engine_token);

    Renderer(ConstructionKey, Resources resources) noexcept;

    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] Result<RenderStatistics> render(const scene::Storage& scene,
                                                  graphics::RenderTarget& target,
                                                  const RenderRequest& request);
    [[nodiscard]] Result<RenderStatistics> render(const scene::Storage& scene,
                                                  graphics::RenderTarget& target,
                                                  const RenderRequest& request,
                                                  const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<RenderStatistics> render(const scene::Storage& scene,
                                                  graphics::RenderTarget& target,
                                                  const RenderRequest& request,
                                                  const scene::VisibilityFilter& visibility,
                                                  const clipping::ClippingFilter& clipping_filter);
    [[nodiscard]] Result<GpuPickResult> gpu_pick(const scene::Storage& scene,
                                                 graphics::PickingTarget& target,
                                                 const scene::VisibilityFilter& visibility,
                                                 const clipping::ClippingFilter& clipping_filter,
                                                 const GpuPickRequest& request);
    [[nodiscard]] Result<GpuFocusDepthAnchorResult>
    gpu_focus_depth_anchor(const scene::Storage& scene, graphics::PickingTarget& target,
                           const scene::VisibilityFilter& visibility,
                           const clipping::ClippingFilter& clipping_filter,
                           const GpuFocusDepthRequest& request);
    [[nodiscard]] graphics::Device& device() noexcept;
    [[nodiscard]] const graphics::Device& device() const noexcept;
    void release_scene(SceneId scene) noexcept;

  private:
    enum class TextureColorSpace : std::uint8_t {
        linear,
        srgb,
    };

    struct RenderPass final {
        RenderRequest request;
        RenderList list;
        clipping::ClippingFilter clipping_filter;
        RenderStatistics statistics;
    };

    struct PreparedDraw final {
        graphics::StaticMesh* mesh = nullptr;
        std::array<graphics::Texture2D*, graphics::material_texture_count> textures{};
        std::uint64_t material_identity = 0;
        graphics::DrawIndexedDescription description;
    };

    [[nodiscard]] Result<void> draw_render_items(const scene::Storage& scene,
                                                 graphics::RenderTarget& target, RenderPass& pass);
    [[nodiscard]] Result<void> prepare_render_item(const scene::Storage& scene,
                                                   const RenderItem& item, RenderPass& pass,
                                                   std::vector<PreparedDraw>& prepared);
    [[nodiscard]] std::uint64_t
    count_material_switches(const std::vector<PreparedDraw>& prepared) const noexcept;
    [[nodiscard]] Result<const DrawPacket*> cached_draw_packet(const scene::Storage& scene,
                                                               const RenderItem& item,
                                                               RenderStatistics& statistics);
    void synchronize_draw_packet_cache(const scene::Storage& scene);
    [[nodiscard]] Result<void> draw_render_overlay(graphics::RenderTarget& target,
                                                   RenderPass& pass);
    [[nodiscard]] Result<void> prepare_draw_textures(
        const scene::Storage& scene, const scene::RuntimePrimitiveView& primitive,
        std::array<graphics::Texture2D*, graphics::material_texture_count>& textures,
        std::uint64_t& upload_count, std::uint64_t& texture_bindings);
    [[nodiscard]] Result<graphics::StaticMesh*>
    cached_mesh(SceneId scene_id, const scene::RuntimePrimitiveView& primitive,
                RenderStatistics& statistics);
    [[nodiscard]] Result<graphics::Texture2D*>
    cached_texture(SceneId scene_id, const scene::RuntimeTextureView& texture,
                   TextureColorSpace color_space, std::uint64_t& upload_count);
    [[nodiscard]] Result<void>
    validate_gpu_picking_context(const scene::Storage& scene) const noexcept;
    [[nodiscard]] Result<std::uint64_t>
    draw_picking_items(const scene::Storage& scene, graphics::PickingTarget& target,
                       const RenderList& list, const clipping::ClippingFilter& clipping_filter);

    std::unique_ptr<graphics::Device> device_;
    std::uint64_t engine_token_ = 0;
    std::unique_ptr<graphics::GraphicsPipeline> pipeline_;
    std::unique_ptr<CacheState> cache_;
};

} // namespace elf3d::renderer

namespace elf3d::renderer {

[[nodiscard]] graphics::TextureAddressMode
runtime_address_mode(scene::RuntimeTextureWrap wrap) noexcept;
[[nodiscard]] graphics::TextureFilterMode
runtime_filter_mode(scene::RuntimeTextureFilter filter) noexcept;
[[nodiscard]] MaterialDescription
runtime_material_description(const scene::RuntimeMaterialView& source) noexcept;
struct RuntimeVertexBuffer {
    std::vector<float> values;
    std::uint32_t vertex_count = 0;
    graphics::VertexLayout layout = graphics::VertexLayout::position_normal_float3;
};
[[nodiscard]] RuntimeVertexBuffer
runtime_vertex_buffer(const scene::RuntimePrimitiveView& primitive);

} // namespace elf3d::renderer
