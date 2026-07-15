#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <glad/gl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace elf3d::graphics {
class Device;
class GraphicsPipeline;
class PickingTarget;
class RenderTarget;
class StaticMesh;
class Texture2D;
struct DrawIndexedDescription;
struct DrawOverlayDescription;
struct GraphicsPipelineDescription;
struct PickingDrawDescription;
struct StaticMeshDescription;
struct Texture2DDescription;
} // namespace elf3d::graphics

namespace elf3d::backend::opengl::device_detail {

inline constexpr char missing_loader_message[] =
    "OpenGL initialization requires a graphics procedure loader";
inline constexpr char glad_failure_message[] = "GLAD failed to load the OpenGL procedure table";
inline constexpr char version_failure_message[] =
    "Elf3D requires an OpenGL 4.1 core-compatible context";
inline constexpr char context_failure_message[] =
    "A compatible OpenGL context must be current on the graphics thread";
inline constexpr char thread_failure_message[] =
    "The graphics operation must run on the engine owning graphics thread";

[[noreturn]] void fatal_opengl_allocation_failure() noexcept;
[[noreturn]] void fatal_unexpected_opengl_boundary_exception() noexcept;
[[nodiscard]] std::uintptr_t opengl_resource_token() noexcept;
[[nodiscard]] bool is_opengl_texture(const graphics::Texture2D* texture) noexcept;

class ColorTextureResolver {
  public:
    virtual ~ColorTextureResolver() = default;
    [[nodiscard]] virtual Result<void> resolve_color_texture() = 0;
};

struct TextureRecord {
    GLuint texture = 0;
    Extent2D extent;
    ColorTextureResolver* resolver = nullptr;
};

class OpenGLDeviceState final {
  public:
    explicit OpenGLDeviceState(GLint maximum_texture_size) noexcept;

    [[nodiscard]] Result<void> validate_operation() const noexcept;
    [[nodiscard]] bool can_destroy_objects() const noexcept;
    [[nodiscard]] bool supports(Extent2D extent) const noexcept;
    [[nodiscard]] Result<TextureHandle> register_texture(GLuint texture, Extent2D extent,
                                                         ColorTextureResolver* resolver = nullptr);
    void unregister_texture(TextureHandle handle) noexcept;
    [[nodiscard]] Result<NativeTextureView> native_texture_view(TextureHandle handle) const;
    void shut_down() noexcept;

  private:
    std::thread::id owner_thread_;
    GLint maximum_texture_size_ = 0;
    bool operational_ = true;
    std::uint64_t next_texture_handle_ = 1;
    std::unordered_map<std::uint64_t, TextureRecord> texture_records_;
};

class AllocationStateGuard final {
  public:
    AllocationStateGuard() noexcept;
    ~AllocationStateGuard();
    AllocationStateGuard(const AllocationStateGuard&) = delete;
    AllocationStateGuard& operator=(const AllocationStateGuard&) = delete;

  private:
    GLint draw_framebuffer_ = 0;
    GLint read_framebuffer_ = 0;
    GLint renderbuffer_ = 0;
    GLint active_texture_ = GL_TEXTURE0;
    GLint texture_2d_ = 0;
    GLint pixel_unpack_buffer_ = 0;
    GLint unpack_alignment_ = 4;
    GLint vertex_array_ = 0;
    GLint array_buffer_ = 0;
};

class RenderStateGuard final {
  public:
    RenderStateGuard() noexcept;
    ~RenderStateGuard();
    RenderStateGuard(const RenderStateGuard&) = delete;
    RenderStateGuard& operator=(const RenderStateGuard&) = delete;

  private:
    static void set_enabled(GLenum capability, GLboolean enabled) noexcept;

    GLint draw_framebuffer_ = 0;
    GLint read_framebuffer_ = 0;
    GLint viewport_[4]{};
    GLfloat clear_color_[4]{};
    GLdouble depth_clear_value_ = 1.0;
    GLboolean color_mask_[4]{};
    GLboolean depth_mask_ = GL_TRUE;
    GLboolean scissor_enabled_ = GL_FALSE;
    GLboolean framebuffer_srgb_enabled_ = GL_FALSE;
    GLboolean depth_test_enabled_ = GL_FALSE;
    GLboolean cull_face_enabled_ = GL_FALSE;
    GLboolean blend_enabled_ = GL_FALSE;
    GLboolean stencil_test_enabled_ = GL_FALSE;
    GLboolean depth_clamp_enabled_ = GL_FALSE;
    GLboolean polygon_offset_fill_enabled_ = GL_FALSE;
    GLboolean primitive_restart_enabled_ = GL_FALSE;
    GLboolean rasterizer_discard_enabled_ = GL_FALSE;
    GLint program_ = 0;
    GLint vertex_array_ = 0;
    GLint array_buffer_ = 0;
    GLint active_texture_ = GL_TEXTURE0;
    std::array<GLint, 4> texture_units_{};
    GLint depth_function_ = GL_LESS;
    GLint blend_source_rgb_ = GL_ONE;
    GLint blend_destination_rgb_ = GL_ZERO;
    GLint blend_source_alpha_ = GL_ONE;
    GLint blend_destination_alpha_ = GL_ZERO;
    GLint blend_equation_rgb_ = GL_FUNC_ADD;
    GLint blend_equation_alpha_ = GL_FUNC_ADD;
    GLint cull_face_mode_ = GL_BACK;
    GLint front_face_ = GL_CCW;
    GLint polygon_mode_[2]{GL_FILL, GL_FILL};
    GLdouble depth_range_[2]{0.0, 1.0};
};

[[nodiscard]] Result<GLuint> compile_shader(GLenum type, std::string_view source);
[[nodiscard]] Result<GLuint> link_program(GLuint vertex_shader, GLuint fragment_shader);
[[nodiscard]] Result<GLuint> create_program_from_sources(std::string_view vertex_source,
                                                         std::string_view fragment_source);

struct UniformLocations {
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint normal = -1;
    GLint base_color = -1;
    GLint camera_world_position = -1;
    GLint light_direction = -1;
    GLint light_color = -1;
    GLint ambient_intensity = -1;
    GLint diffuse_intensity = -1;
    GLint metallic_factor = -1;
    GLint roughness_factor = -1;
    GLint emissive_factor = -1;
    GLint occlusion_strength = -1;
    GLint ior = -1;
    GLint specular_factor = -1;
    GLint specular_color_factor = -1;
    GLint highlight_color = -1;
    GLint highlight_strength = -1;
    GLint has_base_color_texture = -1;
    GLint has_metallic_roughness_texture = -1;
    GLint has_occlusion_texture = -1;
    GLint has_emissive_texture = -1;
    GLint base_color_texture = -1;
    GLint metallic_roughness_texture = -1;
    GLint occlusion_texture = -1;
    GLint emissive_texture = -1;
    GLint texture_texcoord_sets = -1;
    GLint texture_offsets = -1;
    GLint texture_scales = -1;
    GLint texture_rotations = -1;
    GLint alpha_mode = -1;
    GLint alpha_cutoff = -1;
    GLint unlit = -1;
    GLint clipping_section_plane_enabled = -1;
    GLint clipping_section_plane_normal = -1;
    GLint clipping_section_plane_offset = -1;
    GLint clipping_retain_positive_half_space = -1;
    GLint clipping_box_count = -1;
    GLint clipping_box_minimums = -1;
    GLint clipping_box_maximums = -1;

    [[nodiscard]] bool valid() const noexcept;
};

struct RenderTargetView {
    GLuint framebuffer = 0;
    Extent2D extent;
    bool valid = false;
};

struct PickingTargetView {
    GLuint framebuffer = 0;
    Extent2D extent;
    bool valid = false;
};

struct MeshView {
    GLuint vertex_array = 0;
    std::uint32_t index_count = 0;
};

struct PipelineView {
    GLuint program = 0;
    UniformLocations uniforms;
};

[[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
create_render_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::PickingTarget>>
create_picking_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept;
[[nodiscard]] Result<RenderTargetView> render_target_view(graphics::RenderTarget& target) noexcept;
[[nodiscard]] Result<PickingTargetView>
picking_target_view(graphics::PickingTarget& target) noexcept;
void mark_render_target_stale(graphics::RenderTarget& target) noexcept;

[[nodiscard]] Result<std::unique_ptr<graphics::StaticMesh>>
create_static_mesh(std::shared_ptr<OpenGLDeviceState> state,
                   const graphics::StaticMeshDescription& description) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>>
create_texture_2d(std::shared_ptr<OpenGLDeviceState> state,
                  const graphics::Texture2DDescription& description) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>>
create_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                         const graphics::GraphicsPipelineDescription& description) noexcept;
[[nodiscard]] Result<MeshView> mesh_view(graphics::StaticMesh& mesh) noexcept;
[[nodiscard]] Result<PipelineView> pipeline_view(graphics::GraphicsPipeline& pipeline) noexcept;
[[nodiscard]] Result<GLuint> texture_object(const graphics::Texture2D* texture) noexcept;

[[nodiscard]] Result<void>
draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
             graphics::StaticMesh& mesh,
             const graphics::DrawIndexedDescription& description) noexcept;

struct OverlayResources {
    GLuint program = 0;
    GLuint vertex_array = 0;
    GLuint vertex_buffer = 0;
    GLint color_uniform = -1;
};

[[nodiscard]] Result<void>
draw_overlay(OverlayResources& resources, graphics::RenderTarget& target,
             const graphics::DrawOverlayDescription& description) noexcept;
void release_overlay_resources(OverlayResources& resources) noexcept;

struct PickingResources {
    GLuint program = 0;
    GLint model = -1;
    GLint view = -1;
    GLint projection = -1;
    GLint object_id = -1;
    GLint primitive_index = -1;
    GLint clipping_section_plane_enabled = -1;
    GLint clipping_section_plane_normal = -1;
    GLint clipping_section_plane_offset = -1;
    GLint clipping_retain_positive_half_space = -1;
    GLint clipping_box_count = -1;
    GLint clipping_box_minimums = -1;
    GLint clipping_box_maximums = -1;

    [[nodiscard]] bool valid() const noexcept;
};

struct PickingReadback {
    std::uint32_t object_id = 0;
    std::uint32_t primitive_index = 0;
    std::uint32_t instance_index = 0;
    float depth = 1.0F;
};

[[nodiscard]] Result<void>
draw_picking_indexed(PickingResources& resources, graphics::PickingTarget& target,
                     graphics::StaticMesh& mesh,
                     const graphics::PickingDrawDescription& description) noexcept;
[[nodiscard]] Result<std::optional<PickingReadback>>
read_picking_pixel(graphics::PickingTarget& target, Float2 position_pixels) noexcept;
[[nodiscard]] Result<std::vector<float>>
read_picking_depths(graphics::PickingTarget& target) noexcept;
void release_picking_resources(PickingResources& resources) noexcept;

} // namespace elf3d::backend::opengl::device_detail
