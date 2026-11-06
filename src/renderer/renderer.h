#pragma once

#include "renderer_picking.h"

#include <elf3d/core/result.h>
#include <elf3d/graphics_preparation.h>
#include <elf3d/graphics_retirement.h>
#include <elf3d/math/value_types.h>
#include <elf3d/rendering.h>

#include <array>
#include <clipping/clipping.h>
#include <cstddef>
#include <cstdint>
#include <elf3d/core/diagnostics.h>
#include <graphics/graphics.h>
#include <math/math.h>
#include <memory>
#include <optional>
#include <scene/scene.h>
#include <span>
#include <vector>

namespace elf3d::renderer {
struct MeshWorkStep;
struct MeshWorkBudget;
struct TextureWorkStep;
struct TextureWorkBudget;
struct SceneResourcePreparation;
struct SceneResourceWork;
struct EnvironmentPreparation;
struct EnvironmentResources;
struct PipelineResources;

struct RenderItem {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    Float4x4 model_matrix{};
    math::Matrix3x3 normal_matrix{};
    bool orientation_reversed = false;
    std::uint64_t material_identity = 0;
    AlphaMode alpha_mode = AlphaMode::opaque;
    float camera_depth = 0.0F;
    bool transmission_active = false;
    std::optional<PrimitiveId> document_primitive;
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

struct RenderRequest {
    EntityId camera;
    Color4 clear_color;
    BasicLighting lighting;
    EnvironmentLighting environment_lighting;
    ViewportRenderOptions options;
    // Call-scoped IDs owned by the invoking Viewport's SelectionController.
    std::span<const PrimitiveInstance> selected_primitives;
    bool backface_culling_enabled = true;
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

class StudioEnvironmentSource {
  public:
    virtual ~StudioEnvironmentSource() = default;
    [[nodiscard]] virtual Result<std::span<const std::byte>> bytes() noexcept = 0;
};

class Renderer final {
  private:
    [[nodiscard]] SceneResourceWork clear_scene_draw_metadata(const scene::Storage& scene,
                                                              SceneResourcePreparation& state);
    [[nodiscard]] Result<void> advance_scene_resource_batch(const scene::Storage& scene,
                                                            SceneResourcePreparation& state,
                                                            const GraphicsPreparationBudget& budget,
                                                            SceneGraphicsPreparationReport& report);
    [[nodiscard]] Result<SceneResourceWork> finish_scene_primitive(const scene::Storage& scene,
                                                                   SceneResourcePreparation& state);
    struct ConstructionKey final {};
    struct CacheState;
    struct DrawPacket;
    struct Resources;

  public:
    [[nodiscard]] static Result<std::unique_ptr<Renderer>>
    create(std::unique_ptr<graphics::Device> device, std::uint64_t engine_token,
           std::unique_ptr<StudioEnvironmentSource> environment_source);

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
    [[nodiscard]] GraphicsRetirementStatus retirement_status() const noexcept;
    [[nodiscard]] Result<GraphicsRetirementStep>
    advance_retirement_unit(const graphics::ResourceRetirementBudget& budget);
    // Blocking compatibility driver, used only by synchronous cache-miss paths.
    [[nodiscard]] Result<void> complete_retirement();
    // Driven explicitly on the context thread; all Scene views expire per call.
    [[nodiscard]] Result<SceneGraphicsPreparationReport>
    advance_scene_resources(const scene::Storage& scene, const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<void> admit_scene_resources(const scene::Storage& scene);
    [[nodiscard]] SceneGraphicsPreparationStatus
    scene_resource_status(const scene::Storage& scene) const noexcept;
    [[nodiscard]] Result<void> validate_scene_resources(const scene::Storage& scene) const;

  private:
    [[nodiscard]] Result<SceneResourceWork>
    advance_scene_resource_unit(const scene::Storage& scene, SceneResourcePreparation& state,
                                const GraphicsPreparationBudget& remaining);
    [[nodiscard]] Result<SceneResourceWork>
    advance_scene_texture(const scene::Storage& scene, SceneResourcePreparation& state,
                          const GraphicsPreparationBudget& remaining);
    enum class TextureColorSpace : std::uint8_t {
        linear,
        srgb,
    };
    [[nodiscard]] static TextureColorSpace texture_color_space(std::size_t slot) noexcept;

    struct RenderExecutionContext final {
        std::span<const scene::VisibilityFilter> visibility;
        std::span<const clipping::ClippingFilter> clipping_filter;
        double total_begin = 0.0;
    };

    struct RenderPass final {
        RenderRequest request;
        RenderList list;
        clipping::ClippingFilter clipping_filter;
        RenderStatistics statistics;
    };

    struct PreparedDraw final {
        std::size_t packet_index = 0;
        std::array<std::size_t, graphics::material_texture_count> texture_indices{};
        std::array<std::size_t, graphics::material_texture_count> image_indices{};
        std::array<bool, graphics::material_texture_count> document_images{};
        std::array<bool, graphics::material_texture_count> has_textures{};
        std::uint64_t material_identity = 0;
        graphics::DrawIndexedDescription description;
    };

    [[nodiscard]] Result<void> draw_render_items(const scene::Storage& scene,
                                                 graphics::RenderTarget& target, RenderPass& pass);
    [[nodiscard]] Result<bool> ensure_pipeline_resources(bool extended_materials);
    [[nodiscard]] Result<bool> ensure_environment_resources();
    [[nodiscard]] Result<SceneResourceWork>
    advance_environment_resources(const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<SceneResourceWork>
    advance_scene_environment(SceneResourcePreparation& state,
                              const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<bool> prepare_environment(const RenderRequest& request);
    [[nodiscard]] Result<RenderStatistics>
    execute_render_pass(const scene::Storage& scene, graphics::RenderTarget& target,
                        const RenderRequest& request, const RenderExecutionContext& execution);
    [[nodiscard]] Result<void> prepare_render_item(const scene::Storage& scene,
                                                   const RenderItem& item, RenderPass& pass,
                                                   std::vector<PreparedDraw>& prepared);
    [[nodiscard]] Result<void> prepare_render_item_textures(const scene::Storage& scene,
                                                            const RenderItem& item,
                                                            DrawPacket& packet, RenderPass& pass,
                                                            PreparedDraw& prepared);
    void configure_draw_description(const DrawPacket& packet, const RenderItem& item,
                                    const RenderPass& pass, PreparedDraw& prepared) const;
    [[nodiscard]] std::uint64_t
    count_material_switches(const std::vector<PreparedDraw>& prepared) const noexcept;
    [[nodiscard]] Result<std::size_t> cached_draw_packet_index(const scene::Storage& scene,
                                                               const RenderItem& item,
                                                               RenderStatistics& statistics);
    [[nodiscard]] DrawPacket& mutable_draw_packet(const scene::Storage& scene,
                                                  const RenderItem& item,
                                                  std::size_t packet_index) noexcept;
    [[nodiscard]] const DrawPacket& draw_packet(const scene::Storage& scene, const RenderItem& item,
                                                std::size_t packet_index) const noexcept;
    void synchronize_draw_packet_cache(const scene::Storage& scene);
    [[nodiscard]] Result<void> draw_render_overlay(graphics::RenderTarget& target,
                                                   RenderPass& pass);
    [[nodiscard]] Result<void> prepare_draw_textures(const scene::Storage& scene,
                                                     const scene::RuntimePrimitiveView& primitive,
                                                     DrawPacket& packet,
                                                     std::uint64_t& upload_count,
                                                     bool include_normal_texture);
    [[nodiscard]] Result<std::size_t> cached_mesh(SceneId scene_id,
                                                  const scene::RuntimePrimitiveView& primitive,
                                                  RenderStatistics& statistics);
    [[nodiscard]] Result<MeshWorkStep>
    prepare_mesh_step(SceneId scene_id, const scene::RuntimePrimitiveView& primitive,
                      const MeshWorkBudget& budget);
    [[nodiscard]] Result<TextureWorkStep>
    prepare_texture_step(SceneId scene_id, const scene::RuntimeTextureView& source,
                         TextureColorSpace color_space, const TextureWorkBudget& budget);
    [[nodiscard]] Result<std::size_t> cached_texture(SceneId scene_id,
                                                     const scene::RuntimeTextureView& texture,
                                                     TextureColorSpace color_space,
                                                     std::uint64_t& upload_count);
    [[nodiscard]] graphics::StaticMesh& mesh(SceneId scene_id, bool document_primitive,
                                             std::size_t index) const noexcept;
    [[nodiscard]] graphics::Texture2D& texture(SceneId scene_id, bool document_image,
                                               std::size_t image_index,
                                               std::size_t variant_index) const noexcept;
    [[nodiscard]] Result<void>
    validate_gpu_picking_context(const scene::Storage& scene) const noexcept;
    [[nodiscard]] Result<std::uint64_t>
    draw_picking_items(const scene::Storage& scene, graphics::PickingTarget& target,
                       const RenderList& list, const clipping::ClippingFilter& clipping_filter,
                       PickingRasterOptions raster);

    std::unique_ptr<graphics::Device> device_;
    std::uint64_t engine_token_ = 0;
    std::unique_ptr<StudioEnvironmentSource> environment_source_;
    std::unique_ptr<PipelineResources> pipelines_;
    std::unique_ptr<CacheState> cache_;
    std::unique_ptr<EnvironmentResources> environment_;
    std::unique_ptr<EnvironmentPreparation> environment_preparation_;
};

} // namespace elf3d::renderer
