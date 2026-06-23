#include <elf3d/renderer/renderer.h>

#include <elf3d/assets/handle_access.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace elf3d::renderer {
namespace {

static_assert(sizeof(VertexPositionNormalTexCoord) == sizeof(float) * 8);
static_assert(offsetof(VertexPositionNormalTexCoord, normal) == sizeof(float) * 3);
static_assert(offsetof(VertexPositionNormalTexCoord, texcoord0) == sizeof(float) * 6);

constexpr char vertex_shader_source[] = R"glsl(#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normal_matrix;

out vec3 v_world_normal;
out vec3 v_world_position;
out vec2 v_texcoord0;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_normal = normalize(u_normal_matrix * a_normal);
    v_world_position = world_position.xyz;
    v_texcoord0 = a_texcoord0;
    gl_Position = u_projection * u_view * world_position;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(#version 410 core
in vec3 v_world_normal;
in vec3 v_world_position;
in vec2 v_texcoord0;

uniform vec4 u_base_color;
uniform vec3 u_camera_world_position;
uniform vec3 u_light_direction;
uniform vec4 u_light_color;
uniform float u_ambient_intensity;
uniform float u_diffuse_intensity;
uniform float u_metallic_factor;
uniform float u_roughness_factor;
uniform vec4 u_highlight_color;
uniform float u_highlight_strength;
uniform bool u_has_base_color_texture;
uniform bool u_has_metallic_roughness_texture;
uniform sampler2D u_base_color_texture;
uniform sampler2D u_metallic_roughness_texture;
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

void main()
{
    if (!clipping_contains_point(v_world_position)) {
        discard;
    }

    const float pi = 3.14159265359;
    vec4 base_sample = u_has_base_color_texture
        ? texture(u_base_color_texture, v_texcoord0) : vec4(1.0);
    vec4 base_color = u_base_color * base_sample;
    vec4 metallic_roughness = u_has_metallic_roughness_texture
        ? texture(u_metallic_roughness_texture, v_texcoord0) : vec4(1.0);
    float metallic = clamp(u_metallic_factor * metallic_roughness.b, 0.0, 1.0);
    float roughness = clamp(u_roughness_factor * metallic_roughness.g, 0.045, 1.0);

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
    vec3 f0 = mix(vec3(0.04), base_color.rgb, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - h_dot_v, 5.0);
    vec3 specular = distribution * geometry_view * geometry_light * fresnel /
                    max(4.0 * n_dot_v * n_dot_l, 0.0001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * base_color.rgb / pi;
    vec3 direct = (diffuse + specular) * u_light_color.rgb *
                  u_diffuse_intensity * n_dot_l;
    vec3 ambient = base_color.rgb * (1.0 - metallic) * u_ambient_intensity;
    vec3 linear_color = max(direct + ambient, vec3(0.0));
    linear_color = mix(linear_color, u_highlight_color.rgb,
                       clamp(u_highlight_strength, 0.0, 1.0));

    // The viewport target is RGBA8 (not sRGB), so encode exactly once here.
    vec3 srgb_color = mix(12.92 * linear_color,
                          1.055 * pow(linear_color, vec3(1.0 / 2.4)) - 0.055,
                          step(vec3(0.0031308), linear_color));
    fragment_color = vec4(srgb_color, 1.0);
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

template <std::size_t Size> std::array<float, Size> matrix_values(const float *values) noexcept {
    std::array<float, Size> result{};
    std::copy_n(values, Size, result.begin());
    return result;
}

} // namespace

Result<RenderList> build_render_list(const scene::Storage &scene_storage, EntityId camera,
                                     Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return build_render_list(scene_storage, camera, extent, visibility.value());
}

Result<RenderList> build_render_list(const scene::Storage &scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter &visibility) {
    return build_render_list(scene_storage, camera, extent, visibility,
                             clipping::disabled_filter());
}

Result<RenderList>
build_render_list(const scene::Storage &scene_storage, EntityId camera, Extent2D extent,
                  const scene::VisibilityFilter &visibility,
                  const clipping::ClippingFilter &clipping_filter) {
    const Result<const scene::EntityRecord *> camera_record = scene_storage.entity(camera);
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

    const Result<math::Matrix4> camera_world = scene_storage.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<math::Matrix4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const PerspectiveCameraDescription &camera_description = camera_record.value()->camera.value();
    const Result<math::Matrix4> projection =
        math::perspective_matrix(camera_description.vertical_field_of_view_radians, aspect,
                                 camera_description.near_plane, camera_description.far_plane);
    if (!projection) {
        return projection.error();
    }

    try {
        RenderList list;
        list.view_matrix = view.value();
        list.projection_matrix = projection.value();
        list.camera_world_position =
            Float3{camera_world.value()[3].x, camera_world.value()[3].y, camera_world.value()[3].z};
        for (const std::optional<scene::EntityRecord> &record : scene_storage.entities()) {
            if (!record.has_value() || !record->model.has_value() ||
                !scene::entity_visible_in_filter(scene_storage, visibility, record->id)) {
                continue;
            }
            const Result<math::Matrix4> model = scene_storage.world_matrix(record->id);
            if (!model) {
                return model.error();
            }
            const Result<math::Matrix3> normals = math::normal_matrix(model.value());
            if (!normals) {
                return normals.error();
            }
            const float determinant = glm::determinant(math::Matrix3{model.value()});
            const bool orientation_reversed = determinant < 0.0F;
            for (const ModelPrimitiveBinding &primitive : record->model->primitives) {
                const Result<const assets::MeshAsset *> mesh_result =
                    scene_storage.assets().mesh(primitive.mesh);
                if (!mesh_result) {
                    return mesh_result.error();
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
                list.items.push_back(RenderItem{record->id, primitive.mesh, primitive.material,
                                                model.value(), normals.value(),
                                                orientation_reversed});
            }
        }
        return list;
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Render-list construction failed while allocating storage"};
    }
}

void apply_clipping_description(const clipping::ClippingFilter &filter,
                                graphics::DrawIndexedDescription &draw) noexcept {
    draw.clipping_section_plane_enabled = filter.section_plane_enabled;
    draw.clipping_section_plane_normal = filter.section_plane_normal;
    draw.clipping_section_plane_offset = filter.section_plane_offset;
    draw.clipping_retain_positive_half_space = filter.retain_positive_half_space;
    draw.clipping_box_count = filter.enabled_box_count;
    for (std::uint32_t index = 0; index < filter.enabled_box_count; ++index) {
        draw.clipping_boxes[index] = filter.boxes[index];
    }
}

Result<std::shared_ptr<Renderer>> Renderer::create(std::shared_ptr<graphics::Device> device,
                                                   std::uintptr_t engine_token) noexcept {
    if (!device || engine_token == 0) {
        return Error{ErrorCode::graphics_shutdown,
                     "Renderer creation requires an active graphics device and engine identity"};
    }

    try {
        const graphics::GraphicsPipelineDescription description{
            vertex_shader_source, fragment_shader_source,
            graphics::VertexLayout::position_normal_float3_texcoord_float2};
        Result<std::unique_ptr<graphics::GraphicsPipeline>> pipeline_result =
            device->create_graphics_pipeline(description);
        if (!pipeline_result) {
            return pipeline_result.error();
        }
        return std::shared_ptr<Renderer>{
            new Renderer{std::move(device), engine_token, std::move(pipeline_result).value()}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Renderer creation threw an exception"};
    }
}

Renderer::Renderer(std::shared_ptr<graphics::Device> device, std::uintptr_t engine_token,
                   std::unique_ptr<graphics::GraphicsPipeline> pipeline) noexcept
    : device_(std::move(device)), engine_token_(engine_token), pipeline_(std::move(pipeline)) {}

Result<RenderStatistics> Renderer::render(const scene::Storage &scene_storage, EntityId camera,
                                          graphics::RenderTarget &target, Color4 clear_color,
                                          const BasicLighting &lighting,
                                          const ViewportRenderOptions &options) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return render(scene_storage, camera, target, clear_color, lighting, options,
                  visibility.value());
}

Result<RenderStatistics> Renderer::render(const scene::Storage &scene_storage, EntityId camera,
                                          graphics::RenderTarget &target, Color4 clear_color,
                                          const BasicLighting &lighting,
                                          const ViewportRenderOptions &options,
                                          const scene::VisibilityFilter &visibility) {
    return render(scene_storage, camera, target, clear_color, lighting, options, visibility,
                  clipping::disabled_filter());
}

Result<RenderStatistics>
Renderer::render(const scene::Storage &scene_storage, EntityId camera,
                 graphics::RenderTarget &target, Color4 clear_color,
                 const BasicLighting &lighting, const ViewportRenderOptions &options,
                 const scene::VisibilityFilter &visibility,
                 const clipping::ClippingFilter &clipping_filter) {
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
    const RenderList &list = list_result.value();
    statistics.clipping_bounds_tested = list.clipping_bounds_tested;
    statistics.clipping_bounds_rejected = list.clipping_bounds_rejected;
    statistics.clipping_bounds_intersecting = list.clipping_bounds_intersecting;
    for (const RenderItem &item : list.items) {
        const Result<const assets::MeshAsset *> mesh_result =
            scene_storage.assets().mesh(item.mesh);
        if (!mesh_result) {
            return mesh_result.error();
        }
        const Result<const assets::MaterialAsset *> material_result =
            scene_storage.assets().material(item.material);
        if (!material_result) {
            return material_result.error();
        }
        Result<graphics::StaticMesh *> gpu_mesh_result =
            cached_mesh(scene_storage.id(), item.mesh, *mesh_result.value());
        if (!gpu_mesh_result) {
            return gpu_mesh_result.error();
        }

        graphics::Texture2D *base_color_texture = nullptr;
        graphics::Texture2D *metallic_roughness_texture = nullptr;
        const MaterialDescription &material = material_result.value()->description;
        if (material.base_color_texture.is_valid()) {
            Result<graphics::Texture2D *> texture_result = cached_texture(
                scene_storage.id(), material.base_color_texture, TextureColorSpace::srgb,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            base_color_texture = texture_result.value();
            ++statistics.texture_bindings;
        }
        if (material.metallic_roughness_texture.is_valid()) {
            Result<graphics::Texture2D *> texture_result = cached_texture(
                scene_storage.id(), material.metallic_roughness_texture, TextureColorSpace::linear,
                scene_storage.assets(), statistics.gpu_texture_uploads);
            if (!texture_result) {
                return texture_result.error();
            }
            metallic_roughness_texture = texture_result.value();
            ++statistics.texture_bindings;
        }

        graphics::DrawIndexedDescription draw;
        draw.model_matrix = matrix_values<16>(glm::value_ptr(item.model_matrix));
        draw.view_matrix = matrix_values<16>(glm::value_ptr(list.view_matrix));
        draw.projection_matrix = matrix_values<16>(glm::value_ptr(list.projection_matrix));
        draw.normal_matrix = matrix_values<9>(glm::value_ptr(item.normal_matrix));
        draw.base_color = material.base_color;
        draw.camera_world_position = list.camera_world_position;
        draw.light_direction = lighting.direction;
        draw.light_color = lighting.color;
        draw.ambient_intensity = lighting.ambient_intensity;
        draw.diffuse_intensity = lighting.diffuse_intensity;
        draw.metallic_factor = material.metallic_factor;
        draw.roughness_factor = material.roughness_factor;
        if (options.highlight.has_value() && options.highlight->entity == item.entity) {
            draw.highlight_color = options.highlight->color;
            draw.highlight_strength = std::clamp(options.highlight->strength, 0.0F, 1.0F);
        }
        draw.base_color_texture = base_color_texture;
        draw.metallic_roughness_texture = metallic_roughness_texture;
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
            matrix_values<16>(glm::value_ptr(list.view_matrix)),
            matrix_values<16>(glm::value_ptr(list.projection_matrix)),
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

std::size_t Renderer::TextureCacheHash::operator()(const TextureCacheKey &key) const noexcept {
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

std::size_t Renderer::MeshCacheHash::operator()(const MeshCacheKey &key) const noexcept {
    const std::size_t first = std::hash<std::uint64_t>{}(key.scene);
    const std::size_t second = std::hash<std::uint64_t>{}(key.mesh);
    return first ^ (second + static_cast<std::size_t>(0x9e3779b9U) + (first << 6U) + (first >> 2U));
}

Result<graphics::StaticMesh *> Renderer::cached_mesh(SceneId scene_id, MeshHandle handle,
                                                     const assets::MeshAsset &mesh) {
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
        graphics::VertexLayout::position_normal_float3_texcoord_float2};
    Result<std::unique_ptr<graphics::StaticMesh>> mesh_result =
        device_->create_static_mesh(description);
    if (!mesh_result) {
        return mesh_result.error();
    }

    try {
        auto [iterator, inserted] = mesh_cache_.emplace(key, std::move(mesh_result).value());
        if (!inserted) {
            return Error{ErrorCode::gpu_buffer_creation_failed,
                         "The static mesh cache rejected a unique mesh identity"};
        }
        return iterator->second.get();
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Static mesh caching failed while allocating storage"};
    }
}

Result<graphics::Texture2D *> Renderer::cached_texture(SceneId scene_id, TextureAssetHandle handle,
                                                       TextureColorSpace color_space,
                                                       const assets::Storage &assets_storage,
                                                       std::uint64_t &upload_count) {
    const Result<const assets::TextureAsset *> texture_result = assets_storage.texture(handle);
    if (!texture_result) {
        return texture_result.error();
    }
    const TextureDescription &texture = texture_result.value()->description;
    const TextureCacheKey key{detail::SceneHandleAccess::value(scene_id),
                              detail::SceneHandleAccess::value(texture.image), color_space,
                              texture.sampler};
    const auto existing = texture_cache_.find(key);
    if (existing != texture_cache_.end()) {
        return existing->second.get();
    }

    const Result<const assets::ImageAsset *> image_result = assets_storage.image(texture.image);
    if (!image_result) {
        return image_result.error();
    }
    const assets::ImageAsset &image = *image_result.value();
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
    try {
        auto [iterator, inserted] = texture_cache_.emplace(key, std::move(gpu_result).value());
        if (!inserted) {
            return Error{ErrorCode::gpu_texture_creation_failed,
                         "The GPU texture cache rejected a unique texture identity"};
        }
        ++upload_count;
        return iterator->second.get();
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "GPU texture caching failed while allocating storage"};
    }
}

} // namespace elf3d::renderer
