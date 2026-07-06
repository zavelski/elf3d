module;

#include <elf3d/assets.h>
#include <elf3d/viewport.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

module elf.renderer;

import elf.assets;
import elf.clipping;
import elf.graphics;
import elf.math;
import elf.scene;

namespace elf3d::renderer {
namespace {

static_assert(sizeof(VertexPositionNormalTexCoord) == sizeof(float) * 14);
static_assert(offsetof(VertexPositionNormalTexCoord, normal) == sizeof(float) * 3);
static_assert(offsetof(VertexPositionNormalTexCoord, texcoord0) == sizeof(float) * 6);
static_assert(offsetof(VertexPositionNormalTexCoord, texcoord1) == sizeof(float) * 8);
static_assert(offsetof(VertexPositionNormalTexCoord, color) == sizeof(float) * 10);

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
    v_texcoord0 = a_texcoord0;
    v_texcoord1 = a_texcoord1;
    v_color = a_color;
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

graphics::TextureAddressMode address_mode(TextureWrap wrap) noexcept {
    switch (wrap) {
    case TextureWrap::repeat:
        return graphics::TextureAddressMode::repeat;
    case TextureWrap::mirrored_repeat:
        return graphics::TextureAddressMode::mirrored_repeat;
    case TextureWrap::clamp_to_edge:
        return graphics::TextureAddressMode::clamp_to_edge;
    }
    return graphics::TextureAddressMode::repeat;
}

graphics::TextureFilterMode filter_mode(TextureFilter filter) noexcept {
    switch (filter) {
    case TextureFilter::nearest:
        return graphics::TextureFilterMode::nearest;
    case TextureFilter::linear:
        return graphics::TextureFilterMode::linear;
    case TextureFilter::nearest_mipmap_nearest:
        return graphics::TextureFilterMode::nearest_mipmap_nearest;
    case TextureFilter::linear_mipmap_nearest:
        return graphics::TextureFilterMode::linear_mipmap_nearest;
    case TextureFilter::nearest_mipmap_linear:
        return graphics::TextureFilterMode::nearest_mipmap_linear;
    case TextureFilter::linear_mipmap_linear:
        return graphics::TextureFilterMode::linear_mipmap_linear;
    }
    return graphics::TextureFilterMode::linear;
}

[[nodiscard]] std::optional<GpuPickHit>
make_gpu_pick_hit(const RenderList& list, const RenderItem& item, graphics::PickingPixel pixel,
                  Extent2D extent, Float2 position_pixels) noexcept {
    if (extent.width == 0 || extent.height == 0 || !std::isfinite(pixel.depth) ||
        pixel.depth < 0.0F || pixel.depth >= 1.0F) {
        return std::nullopt;
    }
    const Result<Float3> world = math::unproject_viewport_point(
        list.view_matrix, list.projection_matrix, extent, position_pixels, pixel.depth);
    if (!world) {
        return std::nullopt;
    }
    const Float3 world_position = world.value();
    const float world_distance = math::distance(world_position, list.camera_world_position);
    if (!std::isfinite(world_distance) || world_distance < 0.0F) {
        return std::nullopt;
    }
    return GpuPickHit{item.entity,    item.mesh,   pixel.primitive_index, pixel.triangle_index,
                      world_position, pixel.depth, world_distance};
}

[[nodiscard]] double focus_depth_weight(Extent2D extent, std::uint32_t x,
                                        std::uint32_t y) noexcept {
    if (extent.width == 0U || extent.height == 0U) {
        return 0.0;
    }
    const double sample_x =
        (static_cast<double>(x) + 0.5) * 2.0 / static_cast<double>(extent.width) - 1.0;
    const double sample_y =
        (static_cast<double>(y) + 0.5) * 2.0 / static_cast<double>(extent.height) - 1.0;
    const double radius_squared = (sample_x * sample_x + sample_y * sample_y) * 0.5;
    if (!std::isfinite(radius_squared)) {
        return 0.0;
    }
    const double mass = 1.0 - std::min(radius_squared, 1.0);
    return mass * mass;
}

} // namespace

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return build_render_list(scene_storage, camera, extent, visibility.value());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility) {
    return build_render_list(scene_storage, camera, extent, visibility,
                             clipping::disabled_filter());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility,
                                     const clipping::ClippingFilter& clipping_filter) {
    const Result<const scene::EntityRecord*> camera_record = scene_storage.entity(camera);
    if (!camera_record) {
        return camera_record.error();
    }
    if (!camera_record.value()->camera.has_value()) {
        return Error{ErrorCode::entity_has_no_camera,
                     "Viewport rendering requires an entity with a perspective camera"};
    }
    if (!scene::valid_camera_description(camera_record.value()->camera.value())) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "The selected perspective camera configuration is invalid"};
    }
    if (extent.width == 0 || extent.height == 0) {
        return RenderList{};
    }

    const Result<Float4x4> camera_world = scene_storage.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<Float4x4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const PerspectiveCameraDescription& camera_description = camera_record.value()->camera.value();
    const Result<Float4x4> projection =
        math::perspective_matrix(camera_description.vertical_field_of_view_radians, aspect,
                                 camera_description.near_plane, camera_description.far_plane);
    if (!projection) {
        return projection.error();
    }

    RenderList list;
    list.view_matrix = view.value();
    list.projection_matrix = projection.value();
    list.camera_world_position =
        Float3{camera_world.value().elements[12], camera_world.value().elements[13],
               camera_world.value().elements[14]};
    for (const std::optional<scene::EntityRecord>& record : scene_storage.entities()) {
        if (!record.has_value() || !record->model.has_value() ||
            !scene::entity_visible_in_filter(scene_storage, visibility, record->id)) {
            continue;
        }
        const Result<Float4x4> model = scene_storage.world_matrix(record->id);
        if (!model) {
            return model.error();
        }
        const Result<math::Matrix3x3> normals = math::normal_matrix(model.value());
        if (!normals) {
            return normals.error();
        }
        const Result<bool> orientation = math::orientation_reversed(model.value());
        if (!orientation) {
            return orientation.error();
        }
        for (std::uint32_t primitive_index = 0; primitive_index < record->model->primitives.size();
             ++primitive_index) {
            const ModelPrimitiveBinding& primitive = record->model->primitives[primitive_index];
            const Result<const assets::MeshAsset*> mesh_result =
                scene_storage.assets().mesh(primitive.mesh);
            if (!mesh_result) {
                return mesh_result.error();
            }
            const Result<const assets::MaterialAsset*> material_result =
                scene_storage.assets().material(primitive.material);
            if (!material_result) {
                return material_result.error();
            }
            if (clipping_filter.has_clipping()) {
                ++list.clipping_bounds_tested;
                const Bounds3 world_bounds =
                    clipping::transform_bounds(mesh_result.value()->bounds, model.value());
                const clipping::BoundsClassification classification =
                    clipping::classify_bounds(clipping_filter, world_bounds);
                if (classification == clipping::BoundsClassification::outside) {
                    ++list.clipping_bounds_rejected;
                    continue;
                }
                if (classification == clipping::BoundsClassification::intersecting) {
                    ++list.clipping_bounds_intersecting;
                }
            }
            const Float3 model_origin{model.value().elements[12], model.value().elements[13],
                                      model.value().elements[14]};
            const float delta_x = model_origin.x - list.camera_world_position.x;
            const float delta_y = model_origin.y - list.camera_world_position.y;
            const float delta_z = model_origin.z - list.camera_world_position.z;
            const float distance_squared =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
            list.items.push_back(
                RenderItem{record->id, primitive.mesh, primitive.material, primitive_index,
                           model.value(), normals.value(), orientation.value(),
                           material_result.value()->description.alpha_mode, distance_squared});
        }
    }
    std::stable_sort(
        list.items.begin(), list.items.end(), [](const RenderItem& left, const RenderItem& right) {
            const bool left_blended = left.alpha_mode == AlphaMode::blend;
            const bool right_blended = right.alpha_mode == AlphaMode::blend;
            if (left_blended != right_blended) {
                return !left_blended;
            }
            return left_blended && left.camera_distance_squared > right.camera_distance_squared;
        });
    return list;
}

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

void apply_clipping_description(const clipping::ClippingFilter& filter,
                                graphics::PickingDrawDescription& draw) noexcept {
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
                                                   std::uintptr_t engine_token) {
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
    return std::make_unique<Renderer>(ConstructionKey{}, std::move(device), engine_token,
                                      std::move(pipeline_result).value());
}

Renderer::Renderer(ConstructionKey, std::unique_ptr<graphics::Device> device,
                   std::uintptr_t engine_token,
                   std::unique_ptr<graphics::GraphicsPipeline> pipeline) noexcept
    : device_(std::move(device)), engine_token_(engine_token), pipeline_(std::move(pipeline)) {}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage, EntityId camera,
                                          graphics::RenderTarget& target, Color4 clear_color,
                                          const BasicLighting& lighting,
                                          const ViewportRenderOptions& options) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return render(scene_storage, camera, target, clear_color, lighting, options,
                  visibility.value());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage, EntityId camera,
                                          graphics::RenderTarget& target, Color4 clear_color,
                                          const BasicLighting& lighting,
                                          const ViewportRenderOptions& options,
                                          const scene::VisibilityFilter& visibility) {
    return render(scene_storage, camera, target, clear_color, lighting, options, visibility,
                  clipping::disabled_filter());
}

Result<RenderStatistics> Renderer::render(const scene::Storage& scene_storage, EntityId camera,
                                          graphics::RenderTarget& target, Color4 clear_color,
                                          const BasicLighting& lighting,
                                          const ViewportRenderOptions& options,
                                          const scene::VisibilityFilter& visibility,
                                          const clipping::ClippingFilter& clipping_filter) {
    if (detail::SceneHandleAccess::engine_token(scene_storage.id()) != engine_token_) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_ || !pipeline_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    const Result<void> clear_result = target.clear(clear_color);
    if (!clear_result) {
        return clear_result.error();
    }
    if (target.extent().width == 0 || target.extent().height == 0) {
        RenderStatistics statistics;
        statistics.unique_gpu_textures = static_cast<std::uint64_t>(texture_cache_.size());
        return statistics;
    }

    Result<RenderList> list_result =
        build_render_list(scene_storage, camera, target.extent(), visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }

    RenderStatistics statistics;
    const RenderList& list = list_result.value();
    statistics.clipping_bounds_tested = list.clipping_bounds_tested;
    statistics.clipping_bounds_rejected = list.clipping_bounds_rejected;
    statistics.clipping_bounds_intersecting = list.clipping_bounds_intersecting;
    for (const RenderItem& item : list.items) {
        const Result<const assets::MeshAsset*> mesh_result = scene_storage.assets().mesh(item.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset*> material_result =
            scene_storage.assets().material(item.material);
        if (!material_result) {
            return material_result.error();
        }
        Result<graphics::StaticMesh*> gpu_mesh_result =
            cached_mesh(scene_storage.id(), item.mesh, *mesh_result.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }

        std::array<graphics::Texture2D*, graphics::material_texture_count> textures{};
        const MaterialDescription& material = material_result.value()->description;
        if (material.base_color_texture.is_valid()) {
            Result<graphics::Texture2D*> texture_result = cached_texture(
                scene_storage.id(), material.base_color_texture, TextureColorSpace::srgb,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            textures[0] = texture_result.value();
            ++statistics.texture_bindings;
        }
        if (material.metallic_roughness_texture.is_valid()) {
            Result<graphics::Texture2D*> texture_result = cached_texture(
                scene_storage.id(), material.metallic_roughness_texture, TextureColorSpace::linear,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            textures[1] = texture_result.value();
            ++statistics.texture_bindings;
        }
        if (material.occlusion_texture.is_valid()) {
            Result<graphics::Texture2D*> texture_result = cached_texture(
                scene_storage.id(), material.occlusion_texture, TextureColorSpace::linear,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            textures[2] = texture_result.value();
            ++statistics.texture_bindings;
        }
        if (material.emissive_texture.is_valid()) {
            Result<graphics::Texture2D*> texture_result = cached_texture(
                scene_storage.id(), material.emissive_texture, TextureColorSpace::srgb,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            textures[3] = texture_result.value();
            ++statistics.texture_bindings;
        }

        graphics::DrawIndexedDescription draw;
        draw.model_matrix = item.model_matrix.elements;
        draw.view_matrix = list.view_matrix.elements;
        draw.projection_matrix = list.projection_matrix.elements;
        draw.normal_matrix = item.normal_matrix;
        draw.base_color = material.base_color;
        draw.camera_world_position = list.camera_world_position;
        draw.light_direction = lighting.direction;
        draw.light_color = lighting.color;
        draw.ambient_intensity = lighting.ambient_intensity;
        draw.diffuse_intensity = lighting.diffuse_intensity;
        draw.metallic_factor = material.metallic_factor;
        draw.roughness_factor = material.roughness_factor;
        draw.emissive_factor = material.emissive_factor;
        draw.occlusion_strength = material.occlusion_strength;
        draw.ior = material.ior;
        draw.specular_factor = material.specular_factor;
        draw.specular_color_factor = material.specular_color_factor;
        if (options.highlight.has_value() && options.highlight->entity == item.entity) {
            draw.highlight_color = options.highlight->color;
            draw.highlight_strength = std::clamp(options.highlight->strength, 0.0F, 1.0F);
        }
        draw.textures = textures;
        draw.texture_mappings = {
            material.base_color_texture_mapping, material.metallic_roughness_texture_mapping,
            material.occlusion_texture_mapping, material.emissive_texture_mapping};
        draw.alpha_mode = material.alpha_mode;
        draw.alpha_cutoff = material.alpha_cutoff;
        draw.unlit = material.unlit;
        draw.double_sided = material.double_sided;
        draw.front_face_clockwise = item.orientation_reversed;
        apply_clipping_description(clipping_filter, draw);
        const Result<void> draw_result =
            device_->draw_indexed(target, *pipeline_, *gpu_mesh_result.value(), draw);
        if (!draw_result) {
            return draw_result.error();
        }

        ++statistics.draw_calls;
        statistics.vertices += gpu_mesh_result.value()->vertex_count();
        statistics.indices += gpu_mesh_result.value()->index_count();
        statistics.triangles += gpu_mesh_result.value()->index_count() / 3;
    }

    if (!options.overlay_lines.empty() || !options.overlay_markers.empty()) {
        const graphics::DrawOverlayDescription overlay{
            list.view_matrix.elements,
            list.projection_matrix.elements,
            options.overlay_lines,
            options.overlay_markers,
        };
        const Result<void> overlay_result = device_->draw_overlay(target, overlay);
        if (!overlay_result) {
            return overlay_result.error();
        }
        statistics.overlay_lines = static_cast<std::uint64_t>(options.overlay_lines.size());
        statistics.overlay_markers = static_cast<std::uint64_t>(options.overlay_markers.size());
    }
    statistics.unique_gpu_textures = static_cast<std::uint64_t>(texture_cache_.size());
    return statistics;
}

Result<GpuPickResult> Renderer::gpu_pick(const scene::Storage& scene_storage, EntityId camera,
                                         graphics::PickingTarget& target,
                                         Float2 target_position_pixels, Extent2D viewport_extent,
                                         Float2 viewport_position_pixels,
                                         const scene::VisibilityFilter& visibility,
                                         const clipping::ClippingFilter& clipping_filter) {
    if (detail::SceneHandleAccess::engine_token(scene_storage.id()) != engine_token_) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    const Extent2D target_extent = target.extent();
    if (target_extent.width == 0 || target_extent.height == 0) {
        return GpuPickResult{};
    }
    if (!std::isfinite(target_position_pixels.x) || !std::isfinite(target_position_pixels.y) ||
        target_position_pixels.x < 0.0F || target_position_pixels.y < 0.0F ||
        target_position_pixels.x >= static_cast<float>(target_extent.width) ||
        target_position_pixels.y >= static_cast<float>(target_extent.height) ||
        viewport_extent.width == 0U || viewport_extent.height == 0U ||
        !std::isfinite(viewport_position_pixels.x) || !std::isfinite(viewport_position_pixels.y) ||
        viewport_position_pixels.x < 0.0F || viewport_position_pixels.y < 0.0F ||
        viewport_position_pixels.x >= static_cast<float>(viewport_extent.width) ||
        viewport_position_pixels.y >= static_cast<float>(viewport_extent.height)) {
        return Error{ErrorCode::invalid_viewport_position,
                     "Picking coordinates are outside the viewport extent"};
    }

    Result<RenderList> list_result =
        build_render_list(scene_storage, camera, viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuPickResult{};
    }
    if (list.items.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1U) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "GPU picking cannot encode the number of visible render items"};
    }

    GpuPickResult result;
    const Result<void> clear_result = target.clear();
    if (!clear_result) {
        return clear_result.error();
    }

    std::vector<std::size_t> item_indices;
    item_indices.reserve(list.items.size() + 1U);
    item_indices.push_back(0U);

    const auto pass_start = std::chrono::steady_clock::now();
    for (std::size_t item_index = 0; item_index < list.items.size(); ++item_index) {
        const RenderItem& item = list.items[item_index];
        const Result<const assets::MeshAsset*> mesh_result = scene_storage.assets().mesh(item.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset*> material_result =
            scene_storage.assets().material(item.material);
        if (!material_result) {
            return material_result.error();
        }
        Result<graphics::StaticMesh*> gpu_mesh_result =
            cached_mesh(scene_storage.id(), item.mesh, *mesh_result.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }

        item_indices.push_back(item_index);
        graphics::PickingDrawDescription draw;
        draw.model_matrix = item.model_matrix.elements;
        draw.view_matrix = list.view_matrix.elements;
        draw.projection_matrix = list.projection_matrix.elements;
        draw.object_id = static_cast<std::uint32_t>(item_indices.size() - 1U);
        draw.primitive_index = item.primitive_index;
        draw.double_sided = material_result.value()->description.double_sided;
        draw.front_face_clockwise = item.orientation_reversed;
        apply_clipping_description(clipping_filter, draw);
        const Result<void> draw_result =
            device_->draw_picking_indexed(target, *gpu_mesh_result.value(), draw);
        if (!draw_result) {
            return draw_result.error();
        }
        ++result.draw_calls;
    }
    const auto pass_end = std::chrono::steady_clock::now();
    result.picking_pass_time_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(pass_end - pass_start).count());

    const auto read_start = std::chrono::steady_clock::now();
    Result<std::optional<graphics::PickingPixel>> pixel_result =
        device_->read_picking_pixel(target, target_position_pixels);
    const auto read_end = std::chrono::steady_clock::now();
    result.readback_time_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start).count());
    if (!pixel_result) {
        return pixel_result.error();
    }
    result.pixels_read = 1;

    if (!pixel_result.value().has_value()) {
        return result;
    }
    const graphics::PickingPixel pixel = pixel_result.value().value();
    if (pixel.object_id == 0U || pixel.object_id >= item_indices.size()) {
        return result;
    }

    const std::size_t item_index = item_indices[pixel.object_id];
    result.hit = make_gpu_pick_hit(list, list.items[item_index], pixel, viewport_extent,
                                   viewport_position_pixels);
    return result;
}

Result<GpuFocusDepthAnchorResult>
Renderer::gpu_focus_depth_anchor(const scene::Storage& scene_storage, EntityId camera,
                                 graphics::PickingTarget& target, Extent2D viewport_extent,
                                 const scene::VisibilityFilter& visibility,
                                 const clipping::ClippingFilter& clipping_filter) {
    if (detail::SceneHandleAccess::engine_token(scene_storage.id()) != engine_token_) {
        return Error{ErrorCode::foreign_engine_object,
                     "The scene was created by a different Elf3D engine instance"};
    }
    if (!device_) {
        return Error{ErrorCode::graphics_shutdown, "Renderer graphics resources are unavailable"};
    }

    const Extent2D target_extent = target.extent();
    if (target_extent.width == 0 || target_extent.height == 0 || viewport_extent.width == 0U ||
        viewport_extent.height == 0U) {
        return GpuFocusDepthAnchorResult{};
    }

    Result<RenderList> list_result =
        build_render_list(scene_storage, camera, viewport_extent, visibility, clipping_filter);
    if (!list_result) {
        return list_result.error();
    }
    RenderList& list = list_result.value();
    if (list.items.empty()) {
        return GpuFocusDepthAnchorResult{};
    }
    if (list.items.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - 1U) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "GPU picking cannot encode the number of visible render items"};
    }

    GpuFocusDepthAnchorResult result;
    const Result<void> clear_result = target.clear();
    if (!clear_result) {
        return clear_result.error();
    }

    const auto pass_start = std::chrono::steady_clock::now();
    for (std::size_t item_index = 0; item_index < list.items.size(); ++item_index) {
        const RenderItem& item = list.items[item_index];
        const Result<const assets::MeshAsset*> mesh_result = scene_storage.assets().mesh(item.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset*> material_result =
            scene_storage.assets().material(item.material);
        if (!material_result) {
            return material_result.error();
        }
        Result<graphics::StaticMesh*> gpu_mesh_result =
            cached_mesh(scene_storage.id(), item.mesh, *mesh_result.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }

        graphics::PickingDrawDescription draw;
        draw.model_matrix = item.model_matrix.elements;
        draw.view_matrix = list.view_matrix.elements;
        draw.projection_matrix = list.projection_matrix.elements;
        draw.object_id = static_cast<std::uint32_t>(item_index + 1U);
        draw.primitive_index = item.primitive_index;
        draw.double_sided = material_result.value()->description.double_sided;
        draw.front_face_clockwise = item.orientation_reversed;
        apply_clipping_description(clipping_filter, draw);
        const Result<void> draw_result =
            device_->draw_picking_indexed(target, *gpu_mesh_result.value(), draw);
        if (!draw_result) {
            return draw_result.error();
        }
        ++result.draw_calls;
    }
    const auto pass_end = std::chrono::steady_clock::now();
    result.picking_pass_time_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(pass_end - pass_start).count());

    const auto read_start = std::chrono::steady_clock::now();
    Result<std::vector<float>> depths_result = device_->read_picking_depths(target);
    const auto read_end = std::chrono::steady_clock::now();
    result.readback_time_microseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start).count());
    if (!depths_result) {
        return depths_result.error();
    }

    const std::vector<float>& depths = depths_result.value();
    result.pixels_read = static_cast<std::uint64_t>(depths.size());
    const std::size_t expected_pixels = static_cast<std::size_t>(target_extent.width) *
                                        static_cast<std::size_t>(target_extent.height);
    if (depths.size() < expected_pixels) {
        return Error{ErrorCode::draw_submission_failed,
                     "Picking depth readback returned fewer pixels than the picking target extent"};
    }

    double weighted_depth = 0.0;
    double total_weight = 0.0;
    for (std::uint32_t y = 0; y < target_extent.height; ++y) {
        for (std::uint32_t x = 0; x < target_extent.width; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(target_extent.width) +
                static_cast<std::size_t>(x);
            const float depth = depths[index];
            if (!std::isfinite(depth) || depth <= 0.0F || depth >= 1.0F) {
                continue;
            }
            const double weight = focus_depth_weight(target_extent, x, y);
            if (weight <= 0.0) {
                continue;
            }
            weighted_depth += static_cast<double>(depth) * weight;
            total_weight += weight;
        }
    }

    if (total_weight > 0.0 && std::isfinite(total_weight)) {
        const Float2 anchor_position{static_cast<float>(viewport_extent.width) * 0.5F,
                                     static_cast<float>(viewport_extent.height) * 0.5F};
        const float anchor_depth = static_cast<float>(weighted_depth / total_weight);
        const Result<Float3> anchor =
            math::unproject_viewport_point(list.view_matrix, list.projection_matrix,
                                           viewport_extent, anchor_position, anchor_depth);
        if (anchor && math::is_finite(anchor.value())) {
            result.world_position = anchor.value();
        }
    }
    return result;
}

void Renderer::release_scene(SceneId scene_id) noexcept {
    const std::uint64_t scene_value = detail::SceneHandleAccess::value(scene_id);
    for (auto iterator = mesh_cache_.begin(); iterator != mesh_cache_.end();) {
        if (iterator->first.scene == scene_value) {
            iterator = mesh_cache_.erase(iterator);
        } else {
            ++iterator;
        }
    }
    for (auto iterator = texture_cache_.begin(); iterator != texture_cache_.end();) {
        if (iterator->first.scene == scene_value) {
            iterator = texture_cache_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

graphics::Device& Renderer::device() noexcept {
    return *device_;
}

const graphics::Device& Renderer::device() const noexcept {
    return *device_;
}

std::size_t Renderer::TextureCacheHash::operator()(const TextureCacheKey& key) const noexcept {
    std::size_t result = std::hash<std::uint64_t>{}(key.scene);
    const auto combine = [&result](std::size_t value) {
        result ^= value + static_cast<std::size_t>(0x9e3779b9U) + (result << 6U) + (result >> 2U);
    };
    combine(std::hash<std::uint64_t>{}(key.image));
    combine(static_cast<std::size_t>(key.color_space));
    combine(static_cast<std::size_t>(key.sampler.wrap_u));
    combine(static_cast<std::size_t>(key.sampler.wrap_v));
    combine(static_cast<std::size_t>(key.sampler.min_filter));
    combine(static_cast<std::size_t>(key.sampler.mag_filter));
    return result;
}

std::size_t Renderer::MeshCacheHash::operator()(const MeshCacheKey& key) const noexcept {
    const std::size_t first = std::hash<std::uint64_t>{}(key.scene);
    const std::size_t second = std::hash<std::uint64_t>{}(key.mesh);
    return first ^ (second + static_cast<std::size_t>(0x9e3779b9U) + (first << 6U) + (first >> 2U));
}

Result<graphics::StaticMesh*> Renderer::cached_mesh(SceneId scene_id, MeshHandle handle,
                                                    const assets::MeshAsset& mesh) {
    const MeshCacheKey key{detail::SceneHandleAccess::value(scene_id),
                           detail::SceneHandleAccess::value(handle)};
    const auto existing = mesh_cache_.find(key);
    if (existing != mesh_cache_.end()) {
        return existing->second.get();
    }
    if (mesh.vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "The mesh vertex count exceeds the graphics abstraction limit"};
    }

    const std::span<const VertexPositionNormalTexCoord> vertices{mesh.vertices};
    const graphics::StaticMeshDescription description{
        std::as_bytes(vertices), static_cast<std::uint32_t>(vertices.size()), mesh.indices,
        graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4};
    Result<std::unique_ptr<graphics::StaticMesh>> mesh_result =
        device_->create_static_mesh(description);
    if (!mesh_result) {
        return mesh_result.error();
    }

    auto [iterator, inserted] = mesh_cache_.emplace(key, std::move(mesh_result).value());
    if (!inserted) {
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "The static mesh cache rejected a unique mesh identity"};
    }
    return iterator->second.get();
}

Result<graphics::Texture2D*> Renderer::cached_texture(SceneId scene_id, TextureAssetHandle handle,
                                                      TextureColorSpace color_space,
                                                      const assets::Storage& assets_storage,
                                                      std::uint64_t& upload_count) {
    const Result<const assets::TextureAsset*> texture_result = assets_storage.texture(handle);
    if (!texture_result) {
        return texture_result.error();
    }
    const TextureDescription& texture = texture_result.value()->description;
    const TextureCacheKey key{detail::SceneHandleAccess::value(scene_id),
                              detail::SceneHandleAccess::value(texture.image), color_space,
                              texture.sampler};
    const auto existing = texture_cache_.find(key);
    if (existing != texture_cache_.end()) {
        return existing->second.get();
    }

    const Result<const assets::ImageAsset*> image_result = assets_storage.image(texture.image);
    if (!image_result) {
        return image_result.error();
    }
    const assets::ImageAsset& image = *image_result.value();
    graphics::Texture2DDescription description;
    description.extent = Extent2D{image.width, image.height};
    description.format = color_space == TextureColorSpace::srgb
                             ? graphics::TextureFormat::rgba8_srgb
                             : graphics::TextureFormat::rgba8_unorm;
    description.pixels = image.pixels;
    description.wrap_u = address_mode(texture.sampler.wrap_u);
    description.wrap_v = address_mode(texture.sampler.wrap_v);
    description.min_filter = filter_mode(texture.sampler.min_filter);
    description.mag_filter = filter_mode(texture.sampler.mag_filter);
    Result<std::unique_ptr<graphics::Texture2D>> gpu_result =
        device_->create_texture_2d(description);
    if (!gpu_result) {
        return gpu_result.error();
    }
    auto [iterator, inserted] = texture_cache_.emplace(key, std::move(gpu_result).value());
    if (!inserted) {
        return Error{ErrorCode::gpu_texture_creation_failed,
                     "The GPU texture cache rejected a unique texture identity"};
    }
    ++upload_count;
    return iterator->second.get();
}

} // namespace elf3d::renderer
