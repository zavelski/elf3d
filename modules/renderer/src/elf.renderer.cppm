module;

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>
#include <elf3d/viewport.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

export module elf.renderer;

import elf.assets;
import elf.clipping;
import elf.graphics;
import elf.math;
import elf.scene;

export namespace elf3d::renderer {

struct RenderItem {
    EntityId entity;
    MeshHandle mesh;
    MaterialHandle material;
    std::uint32_t primitive_index = 0;
    Float4x4 model_matrix{};
    math::Matrix3x3 normal_matrix{};
    bool orientation_reversed = false;
};

struct RenderList {
    Float4x4 view_matrix{};
    Float4x4 projection_matrix{};
    Float3 camera_world_position;
    std::vector<RenderItem> items;
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
    std::uint64_t picking_pass_time_microseconds = 0;
    std::uint64_t readback_time_microseconds = 0;
};

[[nodiscard]] Result<RenderList> build_render_list(const scene::Storage &scene, EntityId camera,
                                                   Extent2D extent);
[[nodiscard]] Result<RenderList> build_render_list(const scene::Storage &scene, EntityId camera,
                                                   Extent2D extent,
                                                   const scene::VisibilityFilter &visibility);
[[nodiscard]] Result<RenderList>
build_render_list(const scene::Storage &scene, EntityId camera, Extent2D extent,
                  const scene::VisibilityFilter &visibility,
                  const clipping::ClippingFilter &clipping_filter);

class Renderer final {
  public:
    [[nodiscard]] static Result<std::shared_ptr<Renderer>>
    create(std::shared_ptr<graphics::Device> device, std::uintptr_t engine_token) noexcept;

    ~Renderer() = default;
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    [[nodiscard]] Result<RenderStatistics> render(const scene::Storage &scene, EntityId camera,
                                                  graphics::RenderTarget &target,
                                                  Color4 clear_color, const BasicLighting &lighting,
                                                  const ViewportRenderOptions &options = {});
    [[nodiscard]] Result<RenderStatistics>
    render(const scene::Storage &scene, EntityId camera, graphics::RenderTarget &target,
           Color4 clear_color, const BasicLighting &lighting, const ViewportRenderOptions &options,
           const scene::VisibilityFilter &visibility);
    [[nodiscard]] Result<RenderStatistics>
    render(const scene::Storage &scene, EntityId camera, graphics::RenderTarget &target,
           Color4 clear_color, const BasicLighting &lighting, const ViewportRenderOptions &options,
           const scene::VisibilityFilter &visibility,
           const clipping::ClippingFilter &clipping_filter);
    [[nodiscard]] Result<GpuPickResult>
    gpu_pick(const scene::Storage &scene, EntityId camera, graphics::PickingTarget &target,
             Float2 position_pixels, const scene::VisibilityFilter &visibility,
             const clipping::ClippingFilter &clipping_filter);
    void release_scene(SceneId scene) noexcept;

  private:
    struct MeshCacheKey {
        std::uint64_t scene = 0;
        std::uint64_t mesh = 0;

        bool operator==(const MeshCacheKey &) const = default;
    };

    struct MeshCacheHash {
        [[nodiscard]] std::size_t operator()(const MeshCacheKey &key) const noexcept;
    };

    enum class TextureColorSpace : std::uint8_t {
        linear,
        srgb,
    };

    struct TextureCacheKey {
        std::uint64_t scene = 0;
        std::uint64_t image = 0;
        TextureColorSpace color_space = TextureColorSpace::linear;
        SamplerDescription sampler;

        bool operator==(const TextureCacheKey &) const = default;
    };

    struct TextureCacheHash {
        [[nodiscard]] std::size_t operator()(const TextureCacheKey &key) const noexcept;
    };

    Renderer(std::shared_ptr<graphics::Device> device, std::uintptr_t engine_token,
             std::unique_ptr<graphics::GraphicsPipeline> pipeline) noexcept;

    [[nodiscard]] Result<graphics::StaticMesh *> cached_mesh(SceneId scene_id, MeshHandle handle,
                                                             const assets::MeshAsset &mesh);
    [[nodiscard]] Result<graphics::Texture2D *>
    cached_texture(SceneId scene_id, TextureAssetHandle handle, TextureColorSpace color_space,
                   const assets::Storage &assets, std::uint64_t &upload_count);

    std::shared_ptr<graphics::Device> device_;
    std::uintptr_t engine_token_ = 0;
    std::unique_ptr<graphics::GraphicsPipeline> pipeline_;
    std::unordered_map<MeshCacheKey, std::unique_ptr<graphics::StaticMesh>, MeshCacheHash>
        mesh_cache_;
    std::unordered_map<TextureCacheKey, std::unique_ptr<graphics::Texture2D>, TextureCacheHash>
        texture_cache_;
};

} // namespace elf3d::renderer
