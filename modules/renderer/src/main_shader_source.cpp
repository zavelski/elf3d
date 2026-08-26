module;

#include <string_view>

module elf.renderer;

namespace elf3d::renderer {
namespace {

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
uniform float u_environment_intensity;
uniform float u_environment_rotation;
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
uniform samplerCube u_diffuse_environment;
uniform samplerCube u_specular_environment;
uniform sampler2D u_environment_brdf_lut;
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

vec3 rotate_environment_direction(vec3 direction)
{
    float sine = sin(u_environment_rotation);
    float cosine = cos(u_environment_rotation);
    return vec3(cosine * direction.x - sine * direction.z,
                direction.y,
                sine * direction.x + cosine * direction.z);
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
        vec3 indirect_fresnel =
            f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - n_dot_v, 5.0);
        vec3 indirect_diffuse_weight = (vec3(1.0) - indirect_fresnel) * (1.0 - metallic);
        vec3 irradiance = texture(u_diffuse_environment,
                                  rotate_environment_direction(normal)).rgb;
        vec3 environment_diffuse = irradiance * base_color.rgb / pi;
        vec3 reflection = reflect(-view_direction, normal);
        vec3 prefiltered = textureLod(u_specular_environment,
                                      rotate_environment_direction(reflection),
                                      roughness * 7.0).rgb;
        vec2 environment_brdf = texture(u_environment_brdf_lut,
                                        vec2(n_dot_v, roughness)).rg;
        vec3 environment_specular =
            prefiltered * (f0 * environment_brdf.x + environment_brdf.y);
        vec3 indirect = (indirect_diffuse_weight * environment_diffuse + environment_specular) *
                        u_environment_intensity * occlusion;
        vec3 ambient = base_color.rgb * (1.0 - metallic) * u_ambient_intensity * occlusion;
        vec3 emissive_sample = u_has_emissive_texture
            ? texture(u_emissive_texture, mapped_uv(3)).rgb : vec3(1.0);
        vec3 emissive = u_emissive_factor * emissive_sample;
        linear_color = max(direct + indirect + ambient + emissive, vec3(0.0));
    }
    linear_color = mix(linear_color, u_highlight_color.rgb,
                       clamp(u_highlight_strength, 0.0, 1.0));

    fragment_color = vec4(linear_color, base_color.a);
}
)glsl";

} // namespace

std::string_view main_vertex_shader_source() noexcept {
    return vertex_shader_source;
}

std::string_view main_fragment_shader_source() noexcept {
    return fragment_shader_source;
}

} // namespace elf3d::renderer
