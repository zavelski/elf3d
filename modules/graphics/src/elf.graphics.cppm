module;

#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

export module elf.graphics;

export namespace elf3d::graphics {

enum class VertexLayout {
    position_normal_float3,
    position_normal_float3_texcoord_float2,
    position_normal_float3_texcoord2_float2_color_float4,
};

enum class TextureFormat {
    rgba8_unorm,
    rgba8_srgb,
};

enum class TextureAddressMode {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

enum class TextureFilterMode {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};

struct Texture2DDescription {
    Extent2D extent;
    TextureFormat format = TextureFormat::rgba8_unorm;
    std::span<const std::byte> pixels;
    TextureAddressMode wrap_u = TextureAddressMode::repeat;
    TextureAddressMode wrap_v = TextureAddressMode::repeat;
    TextureFilterMode min_filter = TextureFilterMode::linear;
    TextureFilterMode mag_filter = TextureFilterMode::linear;
};

struct StaticMeshDescription {
    std::span<const std::byte> vertex_bytes;
    std::uint32_t vertex_count = 0;
    std::span<const std::uint32_t> indices;
    VertexLayout vertex_layout = VertexLayout::position_normal_float3;
};

struct GraphicsPipelineDescription {
    std::string_view vertex_shader_source;
    std::string_view fragment_shader_source;
    VertexLayout vertex_layout = VertexLayout::position_normal_float3;
};

struct DrawIndexedDescription {
    std::array<float, 16> model_matrix{};
    std::array<float, 16> view_matrix{};
    std::array<float, 16> projection_matrix{};
    std::array<float, 9> normal_matrix{};
    Color4 base_color;
    Float3 camera_world_position;
    Float3 light_direction;
    Color4 light_color;
    float ambient_intensity = 0.0F;
    float diffuse_intensity = 0.0F;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    Float3 emissive_factor;
    float occlusion_strength = 1.0F;
    float ior = 1.5F;
    float specular_factor = 1.0F;
    Float3 specular_color_factor{1.0F, 1.0F, 1.0F};
    Color4 highlight_color{1.0F, 0.55F, 0.05F, 1.0F};
    float highlight_strength = 0.0F;
    class Texture2D *base_color_texture = nullptr;
    class Texture2D *metallic_roughness_texture = nullptr;
    class Texture2D *occlusion_texture = nullptr;
    class Texture2D *emissive_texture = nullptr;
    std::array<TextureMapping, 4> texture_mappings{};
    AlphaMode alpha_mode = AlphaMode::opaque;
    float alpha_cutoff = 0.5F;
    bool unlit = false;
    bool double_sided = false;
    bool front_face_clockwise = false;
    bool clipping_section_plane_enabled = false;
    Float3 clipping_section_plane_normal{0.0F, 1.0F, 0.0F};
    float clipping_section_plane_offset = 0.0F;
    bool clipping_retain_positive_half_space = true;
    std::array<Bounds3, maximum_clipping_boxes> clipping_boxes{};
    std::uint32_t clipping_box_count = 0;
};

struct PickingDrawDescription {
    std::array<float, 16> model_matrix{};
    std::array<float, 16> view_matrix{};
    std::array<float, 16> projection_matrix{};
    std::uint32_t object_id = 0;
    std::uint32_t primitive_index = 0;
    bool double_sided = false;
    bool front_face_clockwise = false;
    bool clipping_section_plane_enabled = false;
    Float3 clipping_section_plane_normal{0.0F, 1.0F, 0.0F};
    float clipping_section_plane_offset = 0.0F;
    bool clipping_retain_positive_half_space = true;
    std::array<Bounds3, maximum_clipping_boxes> clipping_boxes{};
    std::uint32_t clipping_box_count = 0;
};

struct PickingPixel {
    std::uint32_t object_id = 0;
    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
    float depth = 1.0F;
    bool hit = false;
};

struct DrawOverlayDescription {
    std::array<float, 16> view_matrix{};
    std::array<float, 16> projection_matrix{};
    std::span<const OverlayLineSegment> lines;
    std::span<const OverlayPointMarker> markers;
};

class Texture2D {
  public:
    virtual ~Texture2D();

    Texture2D(const Texture2D &) = delete;
    Texture2D &operator=(const Texture2D &) = delete;
    Texture2D(Texture2D &&) = delete;
    Texture2D &operator=(Texture2D &&) = delete;

    [[nodiscard]] virtual Extent2D extent() const noexcept = 0;

  protected:
    Texture2D() = default;
};

class StaticMesh {
  public:
    virtual ~StaticMesh();

    StaticMesh(const StaticMesh &) = delete;
    StaticMesh &operator=(const StaticMesh &) = delete;
    StaticMesh(StaticMesh &&) = delete;
    StaticMesh &operator=(StaticMesh &&) = delete;

    [[nodiscard]] virtual std::uint32_t vertex_count() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t index_count() const noexcept = 0;

  protected:
    StaticMesh() = default;
};

class GraphicsPipeline {
  public:
    virtual ~GraphicsPipeline();

    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
    GraphicsPipeline(GraphicsPipeline &&) = delete;
    GraphicsPipeline &operator=(GraphicsPipeline &&) = delete;

  protected:
    GraphicsPipeline() = default;
};

class RenderTarget {
  public:
    virtual ~RenderTarget();

    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&) = delete;
    RenderTarget &operator=(RenderTarget &&) = delete;

    [[nodiscard]] virtual Extent2D extent() const noexcept = 0;
    [[nodiscard]] virtual Result<void> resize(Extent2D extent) = 0;
    [[nodiscard]] virtual Result<void> clear(Color4 color) = 0;
    [[nodiscard]] virtual TextureHandle color_texture() const noexcept = 0;
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;

  protected:
    RenderTarget() = default;
};

class PickingTarget {
  public:
    virtual ~PickingTarget();

    PickingTarget(const PickingTarget &) = delete;
    PickingTarget &operator=(const PickingTarget &) = delete;
    PickingTarget(PickingTarget &&) = delete;
    PickingTarget &operator=(PickingTarget &&) = delete;

    [[nodiscard]] virtual Extent2D extent() const noexcept = 0;
    [[nodiscard]] virtual Result<void> resize(Extent2D extent) = 0;
    [[nodiscard]] virtual Result<void> clear() = 0;
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;

  protected:
    PickingTarget() = default;
};

class Device {
  public:
    virtual ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    [[nodiscard]] virtual GraphicsBackend backend() const noexcept = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<RenderTarget>>
    create_render_target(Extent2D initial_extent) = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<PickingTarget>>
    create_picking_target(Extent2D initial_extent) = 0;
    [[nodiscard]] virtual Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<StaticMesh>>
    create_static_mesh(const StaticMeshDescription &description) = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<Texture2D>>
    create_texture_2d(const Texture2DDescription &description) = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<GraphicsPipeline>>
    create_graphics_pipeline(const GraphicsPipelineDescription &description) = 0;
    [[nodiscard]] virtual Result<void> draw_indexed(RenderTarget &target,
                                                    GraphicsPipeline &pipeline, StaticMesh &mesh,
                                                    const DrawIndexedDescription &description) = 0;
    [[nodiscard]] virtual Result<void> draw_overlay(RenderTarget &target,
                                                    const DrawOverlayDescription &description) = 0;
    [[nodiscard]] virtual Result<void>
    draw_picking_indexed(PickingTarget &target, StaticMesh &mesh,
                         const PickingDrawDescription &description) = 0;
    [[nodiscard]] virtual Result<PickingPixel> read_picking_pixel(PickingTarget &target,
                                                                  Float2 position_pixels) = 0;

  protected:
    Device() = default;
};

} // namespace elf3d::graphics

export namespace elf3d::detail {

class TextureHandleAccess final {
  public:
    [[nodiscard]] static constexpr TextureHandle create(std::uint64_t value) noexcept {
        return TextureHandle{value};
    }

    [[nodiscard]] static constexpr std::uint64_t value(TextureHandle handle) noexcept {
        return handle.value_;
    }
};

} // namespace elf3d::detail
