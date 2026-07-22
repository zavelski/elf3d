module;

#include <elf3d/rendering.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

module elf.renderer;

import elf.clipping;
import elf.graphics;
import elf.math;
import elf.scene;

namespace elf3d::renderer {

struct Renderer::DrawPacket {
    graphics::StaticMesh* mesh = nullptr;
    std::array<graphics::Texture2D*, graphics::material_texture_count> textures{};
    MaterialDescription material;
};

struct Renderer::CacheState {
    struct MeshEntry {
        std::unique_ptr<graphics::StaticMesh> mesh;
        std::uint64_t resident_bytes = 0;
    };

    struct TextureVariant {
        TextureColorSpace color_space = TextureColorSpace::linear;
        scene::RuntimeSamplerDescription sampler;
        std::unique_ptr<graphics::Texture2D> texture;
        std::uint64_t resident_bytes = 0;
    };

    struct ImageEntry {
        std::vector<TextureVariant> variants;
    };

    struct EntityPackets {
        std::vector<std::optional<DrawPacket>> primitives;
    };

    struct SceneEntry {
        std::vector<std::optional<MeshEntry>> meshes;
        std::vector<std::optional<MeshEntry>> document_meshes;
        std::vector<std::optional<ImageEntry>> images;
        std::vector<std::optional<ImageEntry>> document_images;
        std::vector<std::optional<EntityPackets>> draw_packets;
        std::uint64_t draw_packet_revision = 0;
        std::uint64_t resident_geometry_bytes = 0;
        std::uint64_t resident_texture_bytes = 0;
        std::uint64_t texture_count = 0;
    };

    [[nodiscard]] SceneEntry& scene(SceneId id) {
        const std::size_t index = static_cast<std::size_t>(id.debug_value());
        if (index >= scenes.size()) {
            scenes.resize(index + 1U);
        }
        if (!scenes[index].has_value()) {
            scenes[index].emplace();
        }
        return *scenes[index];
    }

    std::vector<std::optional<SceneEntry>> scenes;
    std::uint64_t resident_geometry_bytes = 0;
    std::uint64_t resident_texture_bytes = 0;
    std::uint64_t texture_count = 0;
};

namespace {

[[nodiscard]] bool has_zero_component(Extent2D extent) noexcept {
    return extent.width == 0 || extent.height == 0;
}

constexpr char vertex_shader_source[] = R"glsl(#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec4 a_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normal_matrix;
uniform int u_vertex_layout;

out vec3 v_world_normal;
out vec3 v_world_position;
out vec2 v_texcoord0;
out vec2 v_texcoord1;
out vec4 v_color;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_normal = normalize(u_normal_matrix * a_normal);
    v_world_position = world_position.xyz;
    v_texcoord0 = u_vertex_layout >= 1 ? a_texcoord0 : vec2(0.0);
    v_texcoord1 = u_vertex_layout >= 2 ? a_texcoord1 : vec2(0.0);
    v_color = u_vertex_layout >= 2 ? a_color : vec4(1.0);
    gl_Position = u_projection * u_view * world_position;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(#version 410 core
in vec3 v_world_normal;
in vec3 v_world_position;
in vec2 v_texcoord0;
in vec2 v_texcoord1;
in vec4 v_color;

uniform vec4 u_base_color;
uniform vec3 u_camera_world_position;
uniform vec3 u_light_direction;
uniform vec4 u_light_color;
uniform float u_ambient_intensity;
uniform float u_diffuse_intensity;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform vec3 u_emissive_factor;
uniform float u_occlusion_strength;
uniform float u_ior;
uniform float u_specular_factor;
uniform vec3 u_specular_color_factor;
uniform vec4 u_highlight_color;
uniform float u_highlight_strength;
uniform bool u_has_base_color_texture;
uniform bool u_has_metallic_roughness_texture;
uniform bool u_has_occlusion_texture;
uniform bool u_has_emissive_texture;
uniform sampler2D u_base_color_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;
uniform int u_texture_texcoord_sets[4];
uniform vec2 u_texture_offsets[4];
uniform vec2 u_texture_scales[4];
uniform float u_texture_rotations[4];
uniform int u_alpha_mode;
uniform float u_alpha_cutoff;
uniform bool u_unlit;
uniform bool u_clipping_section_plane_enabled;
uniform vec3 u_clipping_section_plane_normal;
uniform float u_clipping_section_plane_offset;
uniform bool u_clipping_retain_positive_half_space;
uniform int u_clipping_box_count;
uniform vec3 u_clipping_box_minimums[3];
uniform vec3 u_clipping_box_maximums[3];

layout(location = 0) out vec4 fragment_color;

vec3 safe_normalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    return length_squared > 0.00000001 ? value * inversesqrt(length_squared) : fallback;
}

bool clipping_contains_point(vec3 world_position)
{
    const float tolerance = 0.00001;
    if (u_clipping_section_plane_enabled) {
        float signed_distance = dot(u_clipping_section_plane_normal, world_position) +
                                u_clipping_section_plane_offset;
        if (!u_clipping_retain_positive_half_space) {
            signed_distance = -signed_distance;
        }
        if (signed_distance < -tolerance) {
            return false;
        }
    }

    if (u_clipping_box_count > 0) {
        bool inside_box = false;
        for (int index = 0; index < 3; ++index) {
            if (index >= u_clipping_box_count) {
                break;
            }
            vec3 minimums = u_clipping_box_minimums[index] - vec3(tolerance);
            vec3 maximums = u_clipping_box_maximums[index] + vec3(tolerance);
            if (all(greaterThanEqual(world_position, minimums)) &&
                all(lessThanEqual(world_position, maximums))) {
                inside_box = true;
            }
        }
        if (!inside_box) {
            return false;
        }
    }
    return true;
}

vec2 mapped_uv(int texture_slot)
{
    vec2 uv = u_texture_texcoord_sets[texture_slot] == 1 ? v_texcoord1 : v_texcoord0;
    uv *= u_texture_scales[texture_slot];
    float sine = sin(u_texture_rotations[texture_slot]);
    float cosine = cos(u_texture_rotations[texture_slot]);
    uv = mat2(cosine, sine, -sine, cosine) * uv;
    return u_texture_offsets[texture_slot] + uv;
}

void main()
{
    if (!clipping_contains_point(v_world_position)) {
        discard;
    }

    const float pi = 3.14159265359;
    vec4 base_sample = u_has_base_color_texture
        ? texture(u_base_color_texture, mapped_uv(0)) : vec4(1.0);
    vec4 base_color = u_base_color * base_sample * v_color;
    if (u_alpha_mode == 0) {
        base_color.a = 1.0;
    } else if (u_alpha_mode == 1) {
        if (base_color.a < u_alpha_cutoff) {
            discard;
        }
        base_color.a = 1.0;
    }
    vec4 metallic_roughness = u_has_metallic_roughness_texture
        ? texture(u_metallic_roughness_texture, mapped_uv(1)) : vec4(1.0);
    float metallic = clamp(u_metallic_factor * metallic_roughness.b, 0.0, 1.0);
    float roughness = clamp(u_roughness_factor * metallic_roughness.g, 0.045, 1.0);

    vec3 linear_color;
    if (u_unlit) {
        linear_color = max(base_color.rgb, vec3(0.0));
    } else {
        vec3 normal = safe_normalize(v_world_normal, vec3(0.0, 1.0, 0.0));
        if (!gl_FrontFacing) {
            normal = -normal;
        }
        vec3 view_direction = safe_normalize(u_camera_world_position - v_world_position, normal);
        vec3 light_direction = safe_normalize(-u_light_direction, vec3(0.0, 1.0, 0.0));
        vec3 half_vector = safe_normalize(view_direction + light_direction, normal);
        float n_dot_l = max(dot(normal, light_direction), 0.0);
        float n_dot_v = max(dot(normal, view_direction), 0.0001);
        float n_dot_h = max(dot(normal, half_vector), 0.0);
        float h_dot_v = max(dot(half_vector, view_direction), 0.0);

        float alpha = roughness * roughness;
        float alpha_squared = alpha * alpha;
        float denominator = n_dot_h * n_dot_h * (alpha_squared - 1.0) + 1.0;
        float distribution = alpha_squared / max(pi * denominator * denominator, 0.0001);
        float geometry_k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
        float geometry_view = n_dot_v / (n_dot_v * (1.0 - geometry_k) + geometry_k);
        float geometry_light = n_dot_l / (n_dot_l * (1.0 - geometry_k) + geometry_k);
        float dielectric_f0_scalar = pow((u_ior - 1.0) / (u_ior + 1.0), 2.0);
        vec3 dielectric_f0 = min(vec3(1.0),
                                 vec3(dielectric_f0_scalar * u_specular_factor) *
                                 u_specular_color_factor);
        vec3 f0 = mix(dielectric_f0, base_color.rgb, metallic);
        vec3 f90 = mix(vec3(u_specular_factor), vec3(1.0), metallic);
        vec3 fresnel = f0 + (f90 - f0) * pow(1.0 - h_dot_v, 5.0);
        vec3 specular = distribution * geometry_view * geometry_light * fresnel /
                        max(4.0 * n_dot_v * n_dot_l, 0.0001);
        vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base_color.rgb / pi;
        vec3 direct = (diffuse + specular) * u_light_color.rgb *
                      u_diffuse_intensity * n_dot_l;
        float occlusion = u_has_occlusion_texture
            ? mix(1.0, texture(u_occlusion_texture, mapped_uv(2)).r,
                  clamp(u_occlusion_strength, 0.0, 1.0))
            : 1.0;
        vec3 ambient = base_color.rgb * (1.0 - metallic) * u_ambient_intensity * occlusion;
        vec3 emissive_sample = u_has_emissive_texture
            ? texture(u_emissive_texture, mapped_uv(3)).rgb : vec3(1.0);
        vec3 emissive = u_emissive_factor * emissive_sample;
        linear_color = max(direct + ambient + emissive, vec3(0.0));
    }
    linear_color = mix(linear_color, u_highlight_color.rgb,
                       clamp(u_highlight_strength, 0.0, 1.0));

    fragment_color = vec4(linear_color, base_color.a);
}
)glsl";

} // namespace

void apply_clipping_description(const clipping::ClippingFilter& filter,
                                graphics::DrawIndexedDescription& draw) noexcept {
    draw.clipping_section_plane_enabled = filter.section_plane_enabled;
    draw.clipping_section_plane_normal = filter.section_plane_normal;
    draw.clipping_section_plane_offset = filter.section_plane_offset;
    draw.clipping_retain_positive_half_space = filter.retain_positive_half_space;
    draw.clipping_box_count = filter.enabled_box_count;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        draw.clipping_boxes[index] = filter.boxes[index];
    }
}

Result<std::unique_ptr<Renderer>> Renderer::create(std::unique_ptr<graphics::Device> device,
                                                   std::uint64_t engine_token) {
    if (!device || engine_token == 0) {
        return Error{ErrorCode::graphics_shutdown,
                     "Renderer creation requires an active graphics device and engine identity"};
    }

    const graphics::GraphicsPipelineDescription description{
        vertex_shader_source, fragment_shader_source,
        graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4};
    Result<std::unique_ptr<graphics::GraphicsPipeline>> pipeline_result =
        device->create_graphics_pipeline(description);
    if (!pipeline_result) {
        return pipeline_result.error();
    }
    Resources resources{std::move(device), engine_token, std::move(pipeline_result).value(),
                        std::make_unique<CacheState>()};
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(resources));
}

Renderer::Renderer(ConstructionKey, Resources resources) noexcept
    : device_(std::move(resources.device)), engine_token_(resources.engine_token),
      pipeline_(std::move(resources.pipeline)), cache_(std::move(resources.cache)) {}

Renderer::~Renderer() = default;

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return render(scene_storage, target, request, visibility.value());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request,
                                          const scene::VisibilityFilter& visibility) {
    return render(scene_storage, target, request, visibility, clipping::disabled_filter());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage,
                                          graphics::RenderTarget& target,
                                          const RenderRequest& request,
                                          const scene::VisibilityFilter& visibility,
                                          const clipping::ClippingFilter& clipping_filter) {
    if (!scene_storage.belongs_to_engine(engine_token_)) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_ || !pipeline_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }
    const double total_begin = device_->monotonic_time_milliseconds();

    const Result<void> clear_result = target.clear(request.clear_color);
    if (!clear_result) {
        return clear_result.error();
    }
    if (has_zero_component(target.extent())) {
        RenderStatistics statistics;
        statistics.unique_gpu_textures = cache_->texture_count;
        return statistics;
    }

    const double list_begin = device_->monotonic_time_milliseconds();
    Result<RenderList> list_result = build_render_list(
        scene_storage, request.camera, target.extent(), visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }

    const double list_end = device_->monotonic_time_milliseconds();
    RenderPass pass{request, std::move(list_result).value(), clipping_filter, {}};
    pass.statistics.cpu_render_list_milliseconds = list_end - list_begin;
    pass.statistics.candidate_primitives = pass.list.candidate_primitives;
    pass.statistics.visible_primitives = static_cast<std::uint64_t>(pass.list.items.size());
    pass.statistics.frustum_culled_primitives = pass.list.frustum_culled_primitives;
    pass.statistics.render_passes = 1;
    pass.statistics.clipping_bounds_tested = pass.list.clipping_bounds_tested;
    pass.statistics.clipping_bounds_rejected = pass.list.clipping_bounds_rejected;
    pass.statistics.clipping_bounds_intersecting = pass.list.clipping_bounds_intersecting;
    const double submission_begin = device_->monotonic_time_milliseconds();
    const Result<void> items_result = draw_render_items(scene_storage, target, pass);
    if (!items_result) {
        return items_result.error();
    }
    const Result<void> overlay_result = draw_render_overlay(target, pass);
    if (!overlay_result) {
        return overlay_result.error();
    }
    const double submission_end = device_->monotonic_time_milliseconds();
    pass.statistics.cpu_gl_submission_milliseconds = submission_end - submission_begin;
    pass.statistics.unique_gpu_textures = cache_->texture_count;
    pass.statistics.estimated_resident_geometry_bytes = cache_->resident_geometry_bytes;
    pass.statistics.estimated_resident_texture_bytes = cache_->resident_texture_bytes;
    pass.statistics.shader_switches = static_cast<std::uint64_t>(pass.statistics.draw_calls != 0);
    const graphics::GpuTimingSample main_timing =
        device_->delayed_gpu_timing(graphics::GpuTimingPass::main);
    pass.statistics.gpu_main_pass_milliseconds = main_timing.milliseconds;
    pass.statistics.gpu_main_pass_timing_available = main_timing.available;
    const graphics::GpuTimingSample resolve_timing =
        device_->delayed_gpu_timing(graphics::GpuTimingPass::resolve);
    pass.statistics.gpu_resolve_milliseconds = resolve_timing.milliseconds;
    pass.statistics.gpu_resolve_timing_available = resolve_timing.available;
    pass.statistics.cpu_total_milliseconds = submission_end - total_begin;
    return pass.statistics;
}

Result<void> Renderer::draw_render_items(const scene::Storage& scene,
                                         graphics::RenderTarget& target, RenderPass& pass) {
    synchronize_draw_packet_cache(scene);
    std::vector<PreparedDraw> prepared;
    prepared.reserve(pass.list.items.size());
    for (const RenderItem& item : pass.list.items) {
        const Result<void> prepare_result = prepare_render_item(scene, item, pass, prepared);
        if (!prepare_result) {
            return prepare_result.error();
        }
    }
    pass.statistics.material_switches = count_material_switches(prepared);

    std::vector<graphics::IndexedDrawBatchItem> batch;
    batch.reserve(prepared.size());
    for (PreparedDraw& draw : prepared) {
        draw.description.textures = draw.textures;
        batch.push_back(graphics::IndexedDrawBatchItem{draw.mesh, draw.description});
    }
    return device_->draw_indexed_batch(target, *pipeline_, batch);
}

std::uint64_t
Renderer::count_material_switches(const std::vector<PreparedDraw>& prepared) const noexcept {
    if (prepared.empty()) {
        return 0;
    }
    std::uint64_t switches = 1;
    for (std::size_t index = 1; index < prepared.size(); ++index) {
        switches += static_cast<std::uint64_t>(prepared[index - 1U].material_identity !=
                                               prepared[index].material_identity);
    }
    return switches;
}

Result<void> Renderer::prepare_render_item(const scene::Storage& scene, const RenderItem& item,
                                           RenderPass& pass, std::vector<PreparedDraw>& prepared) {
    const Result<const DrawPacket*> packet_result =
        cached_draw_packet(scene, item, pass.statistics);
    if (!packet_result) {
        return packet_result.error();
    }
    const DrawPacket& packet = *packet_result.value();
    prepared.emplace_back();
    PreparedDraw& prepared_draw = prepared.back();
    prepared_draw.mesh = packet.mesh;
    prepared_draw.textures = packet.textures;
    prepared_draw.material_identity = item.material_identity;
    for (const graphics::Texture2D* texture : packet.textures) {
        pass.statistics.texture_bindings += static_cast<std::uint64_t>(texture != nullptr);
    }
    graphics::DrawIndexedDescription& draw = prepared_draw.description;
    draw.model_matrix = item.model_matrix.elements;
    draw.view_matrix = pass.list.view_matrix.elements;
    draw.projection_matrix = pass.list.projection_matrix.elements;
    draw.normal_matrix = item.normal_matrix;
    draw.base_color = packet.material.base_color;
    draw.camera_world_position = pass.list.camera_world_position;
    draw.light_direction = pass.request.lighting.direction;
    draw.light_color = pass.request.lighting.color;
    draw.ambient_intensity = pass.request.lighting.ambient_intensity;
    draw.diffuse_intensity = pass.request.lighting.diffuse_intensity;
    draw.metallic_factor = packet.material.metallic_factor;
    draw.roughness_factor = packet.material.roughness_factor;
    draw.emissive_factor = packet.material.emissive_factor;
    draw.occlusion_strength = packet.material.occlusion_strength;
    draw.ior = packet.material.ior;
    draw.specular_factor = packet.material.specular_factor;
    draw.specular_color_factor = packet.material.specular_color_factor;
    const std::optional<EntityHighlight>& highlight = pass.request.options.highlight;
    if (highlight.has_value() && highlight->entity == item.entity) {
        draw.highlight_color = highlight->color;
        draw.highlight_strength = std::clamp(highlight->strength, 0.0F, 1.0F);
    }
    draw.texture_mappings = {packet.material.base_color_texture_mapping,
                             packet.material.metallic_roughness_texture_mapping,
                             packet.material.occlusion_texture_mapping,
                             packet.material.emissive_texture_mapping};
    draw.alpha_mode = packet.material.alpha_mode;
    draw.alpha_cutoff = packet.material.alpha_cutoff;
    draw.unlit =
        packet.material.unlit || pass.request.options.shading_mode == RenderShadingMode::unlit;
    draw.double_sided = packet.material.double_sided;
    draw.front_face_clockwise = item.orientation_reversed;
    apply_clipping_description(pass.clipping_filter, draw);

    ++pass.statistics.draw_calls;
    pass.statistics.vertices += packet.mesh->vertex_count();
    pass.statistics.indices += packet.mesh->index_count();
    pass.statistics.triangles += packet.mesh->index_count() / 3;
    return {};
}

void Renderer::synchronize_draw_packet_cache(const scene::Storage& scene) {
    CacheState::SceneEntry& scene_cache = cache_->scene(scene.id());
    const std::uint64_t revision = scene.render_content_revision();
    if (scene_cache.draw_packet_revision == revision) {
        return;
    }
    scene_cache.draw_packets.clear();
    scene_cache.draw_packet_revision = revision;
}

Result<const Renderer::DrawPacket*> Renderer::cached_draw_packet(const scene::Storage& scene,
                                                                 const RenderItem& item,
                                                                 RenderStatistics& statistics) {
    CacheState::SceneEntry& scene_cache = cache_->scene(scene.id());
    const std::size_t entity_index = static_cast<std::size_t>(item.entity.debug_value());
    if (entity_index >= scene_cache.draw_packets.size()) {
        scene_cache.draw_packets.resize(entity_index + 1U);
    }
    std::optional<CacheState::EntityPackets>& entity_packets =
        scene_cache.draw_packets[entity_index];
    if (!entity_packets.has_value()) {
        entity_packets.emplace();
    }
    const std::size_t primitive_index = static_cast<std::size_t>(item.primitive_index);
    if (primitive_index >= entity_packets->primitives.size()) {
        entity_packets->primitives.resize(primitive_index + 1U);
    }
    std::optional<DrawPacket>& cached = entity_packets->primitives[primitive_index];
    if (cached.has_value()) {
        return &*cached;
    }
    const Result<scene::RuntimePrimitiveView> primitive =
        scene.runtime_primitive(item.entity, item.primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    Result<graphics::StaticMesh*> mesh = cached_mesh(scene.id(), primitive.value(), statistics);
    if (!mesh) {
        return mesh.error();
    }
    DrawPacket packet;
    packet.mesh = mesh.value();
    packet.material = runtime_material_description(primitive.value().material_view);
    std::uint64_t ignored_texture_bindings = 0;
    const Result<void> textures =
        prepare_draw_textures(scene, primitive.value(), packet.textures,
                              statistics.gpu_texture_uploads, ignored_texture_bindings);
    if (!textures) {
        return textures.error();
    }
    cached = std::move(packet);
    ++statistics.draw_packet_rebuilds;
    return &*cached;
}

Result<void> Renderer::draw_render_overlay(graphics::RenderTarget& target, RenderPass& pass) {
    const ViewportRenderOptions& options = pass.request.options;
    if (options.overlay_lines.empty() && options.overlay_markers.empty()) {
        return {};
    }
    const graphics::DrawOverlayDescription overlay{pass.list.view_matrix.elements,
                                                   pass.list.projection_matrix.elements,
                                                   options.overlay_lines, options.overlay_markers};
    const Result<void> overlay_result = device_->draw_overlay(target, overlay);
    if (!overlay_result) {
        return overlay_result.error();
    }
    pass.statistics.overlay_lines = static_cast<std::uint64_t>(options.overlay_lines.size());
    pass.statistics.overlay_markers = static_cast<std::uint64_t>(options.overlay_markers.size());
    ++pass.statistics.render_passes;
    return {};
}

void Renderer::release_scene(SceneId scene_id) noexcept {
    const std::size_t index = static_cast<std::size_t>(scene_id.debug_value());
    if (index >= cache_->scenes.size() || !cache_->scenes[index].has_value()) {
        return;
    }
    const CacheState::SceneEntry& scene_cache = *cache_->scenes[index];
    cache_->resident_geometry_bytes -= scene_cache.resident_geometry_bytes;
    cache_->resident_texture_bytes -= scene_cache.resident_texture_bytes;
    cache_->texture_count -= scene_cache.texture_count;
    cache_->scenes[index].reset();
}

graphics::Device& Renderer::device() noexcept {
    return *device_;
}

const graphics::Device& Renderer::device() const noexcept {
    return *device_;
}

Result<void> Renderer::prepare_draw_textures(
    const scene::Storage& scene_storage, const scene::RuntimePrimitiveView& primitive,
    std::array<graphics::Texture2D*, graphics::material_texture_count>& textures,
    std::uint64_t& upload_count, std::uint64_t& texture_bindings) {
    constexpr std::array<scene::RuntimeMaterialTextureSlot, graphics::material_texture_count>
        texture_slots{scene::RuntimeMaterialTextureSlot::base_color,
                      scene::RuntimeMaterialTextureSlot::metallic_roughness,
                      scene::RuntimeMaterialTextureSlot::occlusion,
                      scene::RuntimeMaterialTextureSlot::emissive};
    constexpr std::array<TextureColorSpace, graphics::material_texture_count> texture_color_spaces{
        TextureColorSpace::srgb, TextureColorSpace::linear, TextureColorSpace::linear,
        TextureColorSpace::srgb};
    for (std::size_t index = 0; index < texture_slots.size(); ++index) {
        if (!primitive.material_view.has_texture(texture_slots[index])) {
            continue;
        }
        const Result<scene::RuntimeTextureView> texture =
            scene_storage.runtime_texture(primitive, texture_slots[index]);
        if (!texture) {
            return texture.error();
        }
        Result<graphics::Texture2D*> gpu_texture = cached_texture(
            scene_storage.id(), texture.value(), texture_color_spaces[index], upload_count);
        if (!gpu_texture) {
            return gpu_texture.error();
        }
        textures[index] = gpu_texture.value();
        ++texture_bindings;
    }
    return {};
}

Result<graphics::StaticMesh*> Renderer::cached_mesh(SceneId scene_id,
                                                    const scene::RuntimePrimitiveView& primitive,
                                                    RenderStatistics& statistics) {
    const bool document_primitive = primitive.document_primitive.is_valid();
    const std::uint64_t geometry = document_primitive ? primitive.document_primitive.debug_value()
                                                      : primitive.mesh.debug_value();
    if (geometry > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The mesh cache identity exceeds addressable storage"};
    }
    CacheState::SceneEntry& scene_cache = cache_->scene(scene_id);
    std::vector<std::optional<CacheState::MeshEntry>>& entries =
        document_primitive ? scene_cache.document_meshes : scene_cache.meshes;
    const std::size_t geometry_index = static_cast<std::size_t>(geometry);
    if (geometry_index >= entries.size()) {
        entries.resize(geometry_index + 1U);
    }
    std::optional<CacheState::MeshEntry>& slot = entries[geometry_index];
    if (slot.has_value()) {
        return slot->mesh.get();
    }
    if (primitive.vertex_count() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "The primitive vertex count exceeds the graphics abstraction limit"};
    }

    const RuntimeVertexBuffer vertices = runtime_vertex_buffer(primitive);
    const std::span<const float> vertex_span{vertices.values};
    const graphics::StaticMeshDescription description{
        std::as_bytes(vertex_span), vertices.vertex_count, primitive.indices(), vertices.layout};
    Result<std::unique_ptr<graphics::StaticMesh>> mesh_result =
        device_->create_static_mesh(description);
    if (!mesh_result) {
        return mesh_result.error();
    }

    const std::uint64_t resident_bytes =
        static_cast<std::uint64_t>(vertex_span.size_bytes()) +
        static_cast<std::uint64_t>(primitive.indices().size_bytes());
    slot.emplace(CacheState::MeshEntry{std::move(mesh_result).value(), resident_bytes});
    scene_cache.resident_geometry_bytes += resident_bytes;
    cache_->resident_geometry_bytes += resident_bytes;
    ++statistics.gpu_buffer_uploads;
    statistics.gpu_buffer_uploaded_bytes += resident_bytes;
    return slot->mesh.get();
}

Result<graphics::Texture2D*> Renderer::cached_texture(SceneId scene_id,
                                                      const scene::RuntimeTextureView& texture,
                                                      TextureColorSpace color_space,
                                                      std::uint64_t& upload_count) {
    if (texture.image_identity >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() - 1U)) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The texture cache identity exceeds addressable storage"};
    }
    CacheState::SceneEntry& scene_cache = cache_->scene(scene_id);
    std::vector<std::optional<CacheState::ImageEntry>>& images =
        texture.document_image ? scene_cache.document_images : scene_cache.images;
    const std::size_t image_index = static_cast<std::size_t>(texture.image_identity);
    if (image_index >= images.size()) {
        images.resize(image_index + 1U);
    }
    std::optional<CacheState::ImageEntry>& image = images[image_index];
    if (!image.has_value()) {
        image.emplace();
    }
    const auto existing = std::find_if(
        image->variants.begin(), image->variants.end(),
        [color_space, &texture](const CacheState::TextureVariant& variant) noexcept {
            return variant.color_space == color_space && variant.sampler == texture.sampler;
        });
    if (existing != image->variants.end()) {
        return existing->texture.get();
    }

    graphics::Texture2DDescription description;
    description.extent = Extent2D{texture.width, texture.height};
    description.format = color_space == TextureColorSpace::srgb
                             ? graphics::TextureFormat::rgba8_srgb
                             : graphics::TextureFormat::rgba8_unorm;
    description.pixels = texture.pixels;
    description.wrap_u = runtime_address_mode(texture.sampler.wrap_u);
    description.wrap_v = runtime_address_mode(texture.sampler.wrap_v);
    description.min_filter = runtime_filter_mode(texture.sampler.min_filter);
    description.mag_filter = runtime_filter_mode(texture.sampler.mag_filter);
    Result<std::unique_ptr<graphics::Texture2D>> gpu_result =
        device_->create_texture_2d(description);
    if (!gpu_result) {
        return gpu_result.error();
    }
    const std::uint64_t resident_bytes =
        static_cast<std::uint64_t>(texture.width) * static_cast<std::uint64_t>(texture.height) * 4;
    image->variants.push_back(CacheState::TextureVariant{
        color_space, texture.sampler, std::move(gpu_result).value(), resident_bytes});
    scene_cache.resident_texture_bytes += resident_bytes;
    cache_->resident_texture_bytes += resident_bytes;
    ++scene_cache.texture_count;
    ++cache_->texture_count;
    ++upload_count;
    return image->variants.back().texture.get();
}

} // namespace elf3d::renderer
