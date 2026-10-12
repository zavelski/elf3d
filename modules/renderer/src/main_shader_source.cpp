#include "renderer_detail.h"

#include <elf3d/internal/renderer.h>

#include <string_view>
namespace elf3d::renderer {
namespace {

constexpr char vertex_shader_source[] = R"glsl(#version 410 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec4 a_color;
layout(location = 5) in vec4 a_tangent;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat3 u_normal_matrix;
uniform int u_vertex_layout;
uniform float u_orientation_sign;

out vec3 v_world_normal;
out vec3 v_world_position;
out vec2 v_texcoord0;
out vec2 v_texcoord1;
out vec4 v_color;
out vec4 v_world_tangent;

void main()
{
    vec4 world_position = u_model * vec4(a_position, 1.0);
    v_world_normal = normalize(u_normal_matrix * a_normal);
    v_world_position = world_position.xyz;
    v_texcoord0 = u_vertex_layout >= 1 ? a_texcoord0 : vec2(0.0);
    v_texcoord1 = u_vertex_layout >= 2 ? a_texcoord1 : vec2(0.0);
    v_color = u_vertex_layout >= 2 ? a_color : vec4(1.0);
    vec3 tangent = mat3(u_model) * a_tangent.xyz;
    tangent -= v_world_normal * dot(v_world_normal, tangent);
    float tangent_length_squared = dot(tangent, tangent);
    v_world_tangent = u_vertex_layout >= 3 && tangent_length_squared > 0.00000001
        ? vec4(tangent * inversesqrt(tangent_length_squared),
               a_tangent.w * u_orientation_sign)
        : vec4(1.0, 0.0, 0.0, 1.0);
    gl_Position = u_projection * u_view * world_position;
}
)glsl";

constexpr char fragment_shader_source[] = R"glsl(#version 410 core
#ifndef ELF3D_EXTENDED_MATERIALS
#define ELF3D_EXTENDED_MATERIALS 1
#endif

in vec3 v_world_normal;
in vec3 v_world_position;
in vec2 v_texcoord0;
in vec2 v_texcoord1;
in vec4 v_color;
in vec4 v_world_tangent;

uniform mat4 u_model;
uniform mat3 u_normal_matrix;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform float u_transmission_factor;
uniform float u_thickness_factor;
uniform float u_attenuation_distance;
uniform vec3 u_attenuation_color;
uniform float u_clearcoat_factor;
uniform float u_clearcoat_roughness_factor;
uniform float u_clearcoat_normal_scale;
uniform bool u_clearcoat_uses_tangent_basis;
uniform bool u_has_extension_textures[7];
uniform sampler2D u_specular_texture;
uniform sampler2D u_specular_color_texture;
uniform sampler2D u_transmission_texture;
uniform sampler2D u_thickness_texture;
uniform sampler2D u_clearcoat_texture;
uniform sampler2D u_clearcoat_roughness_texture;
uniform sampler2D u_clearcoat_normal_texture;
uniform sampler2D u_transmission_scene;
uniform bool u_has_transmission_scene;
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
uniform float u_normal_scale;
uniform float u_ior;
uniform float u_specular_factor;
uniform vec3 u_specular_color_factor;
uniform vec4 u_highlight_color;
uniform float u_highlight_strength;
uniform bool u_has_base_color_texture;
uniform bool u_has_metallic_roughness_texture;
uniform bool u_has_normal_texture;
uniform bool u_has_occlusion_texture;
uniform bool u_has_emissive_texture;
uniform sampler2D u_base_color_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_occlusion_texture;
uniform sampler2D u_emissive_texture;
uniform samplerCube u_diffuse_environment;
uniform samplerCube u_specular_environment;
uniform sampler2D u_environment_brdf_lut;
uniform int u_texture_texcoord_sets[12];
uniform vec2 u_texture_offsets[12];
uniform vec2 u_texture_scales[12];
uniform float u_texture_rotations[12];
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



vec4 material_sample(sampler2D image, int slot)
{
    return texture(image, mapped_uv(slot));
}

vec3 mapped_normal(vec3 geometric_normal, sampler2D image, int slot, float scale)
{
    vec2 texture_scale = u_texture_scales[slot];
    float determinant = texture_scale.x * texture_scale.y;
    if (abs(determinant) <= 0.00000001) {
        return geometric_normal;
    }

    vec3 tangent = safe_normalize(
        v_world_tangent.xyz - geometric_normal * dot(geometric_normal, v_world_tangent.xyz),
        vec3(1.0, 0.0, 0.0));
    float handedness = v_world_tangent.w < 0.0 ? -1.0 : 1.0;
    vec3 bitangent = safe_normalize(cross(geometric_normal, tangent) * handedness,
                                    vec3(0.0, 1.0, 0.0));

    float sine = sin(u_texture_rotations[slot]);
    float cosine = cos(u_texture_rotations[slot]);
    mat2 texture_linear = mat2(cosine, sine, -sine, cosine) *
                          mat2(texture_scale.x, 0.0, 0.0, texture_scale.y);
    mat2 inverse_linear = inverse(texture_linear);
    vec3 corrected_tangent = safe_normalize(
        tangent * inverse_linear[0][0] + bitangent * inverse_linear[0][1], tangent);
    float corrected_handedness = handedness * (determinant < 0.0 ? -1.0 : 1.0);
    vec3 corrected_bitangent = safe_normalize(
        cross(geometric_normal, corrected_tangent) * corrected_handedness, bitangent);

    vec3 tangent_normal = material_sample(image, slot).rgb * 2.0 - vec3(1.0);
    tangent_normal.xy *= scale;
    tangent_normal = safe_normalize(tangent_normal, vec3(0.0, 0.0, 1.0));
    return safe_normalize(corrected_tangent * tangent_normal.x +
                          corrected_bitangent * tangent_normal.y +
                          geometric_normal * tangent_normal.z,
                          geometric_normal);
}

// Stable GGX: cancellation-free at aligned half vectors; roughness is >= 0.045.
float ggx_distribution(float cosine, float roughness)
{
    float c = clamp(cosine, 0.0, 1.0);
    float a2 = roughness * roughness * roughness * roughness;
    float q = (1.0-c)*(1.0+c) + a2*c*c;
    return a2 / (3.14159265359*q*q);
}

vec3 dielectric_f0(float ior, float strength, vec3 color)
{
    float valid_ior = max(ior,1.0);
    float ratio = ior == 0.0 ? 1.0 : (valid_ior-1.0)/(valid_ior+1.0);
    return min(vec3(1.0), ratio*ratio*max(color,vec3(0.0))) * strength;
}

bool finite_vector(vec3 value)
{
    return !any(isnan(value)) && !any(isinf(value));
}

// Parallel slab path in world units. A includes inherited shear and signed scale.
// Negative result denotes an unusable metric: bounded thin-surface fallback.
float volume_path_from_local_metric(vec3 local_ray, vec3 local_normal, float thickness)
{
    if (thickness <= 0.0) return 0.0;
    float normal_length = length(local_normal);
    if (!finite_vector(local_ray) || !finite_vector(local_normal) || normal_length <= 0.0) return -1.0;
    float projection = abs(dot(local_ray, local_normal / normal_length));
    float path = thickness / projection;
    return projection > 0.0 && !isnan(path) && !isinf(path) && path <= 1e8 ? path : -1.0;
}

vec3 volume_attenuation(vec3 color, float path, float distance)
{
    if (path <= 0.0 || distance <= 0.0) return vec3(1.0);
    // Exact zero is absorbing; log(0) never enters GPU arithmetic.
    vec3 attenuation = exp(log(max(color, vec3(1e-30))) * (path/distance));
    return mix(attenuation, vec3(0.0), equal(color,vec3(0.0)));
}

vec3 derivative_normal(vec3 normal, vec3 dpdx, vec3 dpdy, vec2 duvdx, vec2 duvdy)
{
    float determinant = duvdx.x*duvdy.y-duvdx.y*duvdy.x;
    if (abs(determinant) <= 1e-12) return normal;
    vec3 tangent = (dpdx*duvdy.y-dpdy*duvdx.y)/determinant;
    vec3 bitangent = (-dpdx*duvdy.x+dpdy*duvdx.x)/determinant;
    tangent -= normal*dot(normal,tangent);
    bitangent -= normal*dot(normal,bitangent);
    float scale = max(dot(tangent,tangent),dot(bitangent,bitangent));
    if (!finite_vector(tangent) || !finite_vector(bitangent) || scale <= 1e-12) return normal;
    vec3 sampled = material_sample(u_clearcoat_normal_texture,11).xyz*2.0-1.0;
    sampled.xy *= u_clearcoat_normal_scale;
    return safe_normalize((tangent*sampled.x+bitangent*sampled.y)*inversesqrt(scale)+normal*sampled.z,normal);
}

vec3 ibl_specular_weight(float n_dot_v, float roughness, vec3 f0, vec3 f90,
                         vec2 environment_brdf)
{
    vec3 grazing_response = max(f90 - vec3(roughness), f0) - f0;
    vec3 fresnel = f0 + grazing_response * pow(1.0 - n_dot_v, 5.0);
    vec3 single_scattering = fresnel * environment_brdf.x + f90 * environment_brdf.y;
    float missing_energy = 1.0 - environment_brdf.x - environment_brdf.y;
    vec3 average_fresnel = f0 + (f90 - f0) / 21.0;
    vec3 multiple_scattering =
        missing_energy * single_scattering * average_fresnel /
        max(vec3(1.0) - average_fresnel * missing_energy, vec3(0.0001));
    return single_scattering + multiple_scattering;
}

void main()
{
    vec3 geometric_normal = safe_normalize(v_world_normal, vec3(0.0,1.0,0.0));
#if ELF3D_EXTENDED_MATERIALS
    vec3 coat_normal = geometric_normal;
    if (!u_unlit && u_clearcoat_factor > 0.0 && u_has_extension_textures[6] && !u_clearcoat_uses_tangent_basis) {
        // Uniform slot branch, before discards or fragment-varying control flow.
        vec2 coat_uv = mapped_uv(11);
        coat_normal = derivative_normal(geometric_normal, dFdx(v_world_position), dFdy(v_world_position), dFdx(coat_uv), dFdy(coat_uv));
    }
#endif
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
#if ELF3D_EXTENDED_MATERIALS
        float transmission = clamp(u_transmission_factor * (u_has_extension_textures[2] ? material_sample(u_transmission_texture,7).r : 1.0),0.0,1.0) * (1.0-metallic);
        float thickness = u_thickness_factor * (u_has_extension_textures[3] ? material_sample(u_thickness_texture,8).g : 1.0);
        if (transmission > 0.0 && thickness > 0.0 && !gl_FrontFacing) discard;
#else
        const float transmission = 0.0;
#endif
        vec3 normal = u_has_normal_texture ? mapped_normal(geometric_normal,u_normal_texture,2,u_normal_scale) : geometric_normal;
#if ELF3D_EXTENDED_MATERIALS
        if (u_has_extension_textures[6] && u_clearcoat_uses_tangent_basis)
            coat_normal = mapped_normal(geometric_normal,u_clearcoat_normal_texture,11,u_clearcoat_normal_scale);
#endif
        if (!gl_FrontFacing) {
            normal = -normal;
#if ELF3D_EXTENDED_MATERIALS
            coat_normal = -coat_normal;
#endif
        }
        vec3 view_direction = safe_normalize(u_camera_world_position - v_world_position, normal);
        vec3 light_direction = safe_normalize(-u_light_direction, vec3(0.0, 1.0, 0.0));
        vec3 half_vector = safe_normalize(view_direction + light_direction, normal);
        float n_dot_l = max(dot(normal, light_direction), 0.0);
        float n_dot_v = max(dot(normal, view_direction), 0.0001);
        float n_dot_h = max(dot(normal, half_vector), 0.0);
        float h_dot_v = max(dot(half_vector, view_direction), 0.0);

        float distribution = ggx_distribution(n_dot_h,roughness);
        float geometry_k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
        float geometry_view = n_dot_v / (n_dot_v * (1.0 - geometry_k) + geometry_k);
        float geometry_light = n_dot_l / (n_dot_l * (1.0 - geometry_k) + geometry_k);
#if ELF3D_EXTENDED_MATERIALS
        float specular_strength = clamp(u_specular_factor * (u_has_extension_textures[0] ? material_sample(u_specular_texture,5).a : 1.0),0.0,1.0);
        vec3 specular_color = u_specular_color_factor * (u_has_extension_textures[1] ? material_sample(u_specular_color_texture,6).rgb : vec3(1.0));
#else
        float specular_strength = clamp(u_specular_factor,0.0,1.0);
        vec3 specular_color = u_specular_color_factor;
#endif
        vec3 dielectric = dielectric_f0(u_ior,specular_strength,specular_color);
        vec3 f0 = mix(dielectric,base_color.rgb,metallic);
        vec3 f90 = mix(u_ior == 0.0 ? dielectric : vec3(specular_strength),vec3(1.0),metallic);
        vec3 fresnel = f0 + (f90 - f0) * pow(1.0 - h_dot_v, 5.0);
        vec3 specular = distribution * geometry_view * geometry_light * fresnel /
                        max(4.0 * n_dot_v * n_dot_l, 0.0001);
        vec3 diffuse = (1.0 - max(max(fresnel.r,fresnel.g),fresnel.b)) * (1.0 - metallic) * (1.0-transmission) * base_color.rgb / pi;
        vec3 direct = (diffuse + specular) * u_light_color.rgb *
                      u_diffuse_intensity * n_dot_l;
        float occlusion = u_has_occlusion_texture
            ? mix(1.0, texture(u_occlusion_texture, mapped_uv(3)).r,
                  clamp(u_occlusion_strength, 0.0, 1.0))
            : 1.0;
        vec3 irradiance = texture(u_diffuse_environment,
                                  rotate_environment_direction(normal)).rgb;
        vec3 environment_diffuse = irradiance * base_color.rgb / pi;
        vec3 reflection = reflect(-view_direction, normal);
        vec3 prefiltered = textureLod(u_specular_environment,
                                      rotate_environment_direction(reflection),
                                      roughness * 7.0).rgb;
        vec2 environment_brdf = texture(u_environment_brdf_lut,
                                        vec2(n_dot_v, roughness)).rg;
        vec3 environment_fresnel =
            ibl_specular_weight(n_dot_v, roughness, f0, f90, environment_brdf);
        vec3 indirect_diffuse_weight =
            vec3(1.0-max(max(environment_fresnel.r,environment_fresnel.g),environment_fresnel.b)) * (1.0-metallic) * (1.0-transmission);
        vec3 environment_specular = prefiltered * environment_fresnel;
        vec3 indirect = (indirect_diffuse_weight * environment_diffuse + environment_specular) *
                        u_environment_intensity * occlusion;
        vec3 ambient = base_color.rgb * (1.0 - metallic) * (1.0-transmission) * u_ambient_intensity * occlusion;
        vec3 emissive_sample = u_has_emissive_texture
            ? texture(u_emissive_texture, mapped_uv(4)).rgb : vec3(1.0);
        vec3 emissive = u_emissive_factor * emissive_sample;
        linear_color = max(direct + indirect + ambient + emissive, vec3(0.0));
#if ELF3D_EXTENDED_MATERIALS
        if (transmission > 0.0 && u_has_transmission_scene) {
            vec2 scene_size = vec2(textureSize(u_transmission_scene,0));
            vec2 screen_uv = gl_FragCoord.xy / scene_size;
            vec3 ray = refract(-view_direction,normal,u_ior == 0.0 ? 0.0 : 1.0/max(u_ior,1.0));
            float path = volume_path_from_local_metric(transpose(u_normal_matrix)*ray,transpose(mat3(u_model))*normal,thickness);
            vec4 exit_clip = u_projection*u_view*vec4(v_world_position+ray*max(path,0.0),1.0);
            vec2 exit_uv = exit_clip.xy/exit_clip.w*0.5+0.5;
            if (path >= 0.0 && exit_clip.w > 0.0 && all(greaterThanEqual(exit_uv,vec2(0.0))) && all(lessThanEqual(exit_uv,vec2(1.0)))) screen_uv = exit_uv;
            float lod = roughness*roughness*min(6.0,log2(max(scene_size.x,scene_size.y)));
            vec3 background = textureLod(u_transmission_scene,screen_uv,lod).rgb;
            vec3 view_fresnel = f0+(f90-f0)*pow(1.0-n_dot_v,5.0);
            linear_color += background*base_color.rgb*volume_attenuation(u_attenuation_color,path,u_attenuation_distance)*(vec3(1.0)-view_fresnel)*transmission;
        }
        float coat = clamp(u_clearcoat_factor * (u_has_extension_textures[4] ? material_sample(u_clearcoat_texture,9).r : 1.0),0.0,1.0);
        if (coat > 0.0) {
            float coat_roughness = clamp(u_clearcoat_roughness_factor * (u_has_extension_textures[5] ? material_sample(u_clearcoat_roughness_texture,10).g : 1.0),0.045,1.0);
            float cv = max(dot(coat_normal,view_direction),0.0001);
            float cl = max(dot(coat_normal,light_direction),0.0);
            float ch = max(dot(coat_normal,half_vector),0.0);
            float fv = 0.04+0.96*pow(1.0-cv,5.0);
            float fl = 0.04+0.96*pow(1.0-cl,5.0);
            float fh = 0.04+0.96*pow(1.0-h_dot_v,5.0);
            float k = (coat_roughness+1.0)*(coat_roughness+1.0)/8.0;
            float gv = cv/(cv*(1.0-k)+k), gl = cl/(cl*(1.0-k)+k);
            vec3 coat_direct = ggx_distribution(ch,coat_roughness)*gv*gl*fh/max(4.0*cv*cl,0.0001)*u_light_color.rgb*u_diffuse_intensity*cl;
            vec3 reflected = textureLod(u_specular_environment,rotate_environment_direction(reflect(-view_direction,coat_normal)),coat_roughness*7.0).rgb;
            vec2 brdf = texture(u_environment_brdf_lut,vec2(cv,coat_roughness)).rg;
            vec3 coat_environment = reflected*ibl_specular_weight(cv,coat_roughness,vec3(0.04),vec3(1.0),brdf)*u_environment_intensity*occlusion;
            linear_color = (linear_color-direct)*(1.0-coat*fv) + direct*(1.0-coat*fv)*(1.0-coat*fl) + coat*(coat_direct+coat_environment);
        }
#endif
    }
    linear_color = mix(linear_color, u_highlight_color.rgb,
                       clamp(u_highlight_strength, 0.0, 1.0));

    fragment_color = vec4(linear_color, base_color.a);
}
)glsl";

} // namespace

std::string_view main_vertex_shader_source() noexcept
{
    return vertex_shader_source;
}

std::string_view main_fragment_shader_source() noexcept
{
    return fragment_shader_source;
}

} // namespace elf3d::renderer
