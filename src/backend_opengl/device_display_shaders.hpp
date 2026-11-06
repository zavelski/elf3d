#pragma once

namespace elf3d::backend::opengl::device_detail {

constexpr char display_resolve_vertex_shader_source[] = R"glsl(#version 410 core
out vec2 v_texcoord;

void main()
{
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );
    const vec2 texcoords[3] = vec2[3](
        vec2(0.0, 0.0),
        vec2(2.0, 0.0),
        vec2(0.0, 2.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_texcoord = texcoords[gl_VertexID];
}
)glsl";

constexpr char display_resolve_fragment_shader_source[] = R"glsl(#version 410 core
in vec2 v_texcoord;

uniform sampler2D u_linear_color_texture;
uniform float u_exposure_ev;
uniform int u_tone_mapping;

layout(location = 0) out vec4 fragment_color;

vec3 linear_to_srgb(vec3 linear_color)
{
    linear_color = max(linear_color, vec3(0.0));
    return mix(12.92 * linear_color,
               1.055 * pow(linear_color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), linear_color));
}

float sanitize_component(float value)
{
    return isnan(value) || isinf(value) || value < 0.0 ? 0.0 : value;
}

vec3 pbr_neutral_tone_mapping(vec3 color)
{
    const float start_compression = 0.76;
    const float desaturation = 0.15;
    float darkest = min(color.r, min(color.g, color.b));
    float offset = darkest < 0.08 ? darkest - 6.25 * darkest * darkest : 0.04;
    color -= vec3(offset);
    float peak = max(color.r, max(color.g, color.b));
    if (peak < start_compression) {
        return color;
    }
    float distance_to_white = 1.0 - start_compression;
    float compressed_peak =
        1.0 - distance_to_white * distance_to_white /
                  (peak + distance_to_white - start_compression);
    color *= compressed_peak / peak;
    float desaturation_weight =
        1.0 - 1.0 / (desaturation * (peak - compressed_peak) + 1.0);
    return mix(color, vec3(compressed_peak), desaturation_weight);
}

vec3 standard_tone_mapping(vec3 color)
{
    const float calibration = 1.590579;
    return vec3(1.0) - exp2(-calibration * color);
}

void main()
{
    vec4 linear_color = texture(u_linear_color_texture, v_texcoord);
    vec3 sanitized = vec3(sanitize_component(linear_color.r),
                          sanitize_component(linear_color.g),
                          sanitize_component(linear_color.b));
    vec3 exposed = sanitized * exp2(u_exposure_ev);
    vec3 display_linear = max(exposed, vec3(0.0));
    if (u_tone_mapping == 1) {
        display_linear = pbr_neutral_tone_mapping(exposed);
    } else if (u_tone_mapping == 2) {
        display_linear = standard_tone_mapping(exposed);
    }
    fragment_color = vec4(linear_to_srgb(display_linear), clamp(linear_color.a, 0.0, 1.0));
}
)glsl";

} // namespace elf3d::backend::opengl::device_detail
