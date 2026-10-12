#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>

namespace elf3d::renderer {
std::string_view main_fragment_shader_source() noexcept;
}

namespace {
using Vector = std::array<double, 3>;
using Matrix = std::array<float, 9>;

struct ProbeResources {
    GLuint program = 0, framebuffer = 0, texture = 0, vertex_array = 0;
    ~ProbeResources()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);
        glBindVertexArray(0);
        glDeleteProgram(program);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        glDeleteVertexArrays(1, &vertex_array);
    }
};

GLuint compile(GLenum type, const std::string& source)
{
    const GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        std::array<char, 4096> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << log.data() << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool prepare(ProbeResources& probe)
{
    std::string fragment{elf3d::renderer::main_fragment_shader_source()};
    fragment.erase(fragment.find("void main()"));
    fragment += R"glsl(
uniform int probe_mode;
uniform vec4 probe_values;
uniform mat3 probe_matrix;
uniform vec3 probe_ray;
uniform vec3 probe_normal;
uniform sampler2D probe_texture;
void main() {
    if (probe_mode == 0) fragment_color = vec4(ggx_distribution(probe_values.x,probe_values.y),0,0,1);
    else if (probe_mode == 1) fragment_color = vec4(dielectric_f0(probe_values.x,probe_values.y,probe_normal),1);
    else if (probe_mode == 2) fragment_color = vec4(volume_path_from_local_metric(inverse(probe_matrix)*probe_ray,transpose(probe_matrix)*probe_normal,probe_values.x),0,0,1);
    else if (probe_mode == 3) fragment_color = vec4(volume_attenuation(probe_normal,probe_values.x,probe_values.y),1);
    else if (probe_mode == 6) fragment_color = vec4(mapped_normal(vec3(0,0,1),probe_texture,int(probe_values.x),probe_values.y),1);
    else if (probe_mode == 8) fragment_color = vec4(volume_path_from_local_metric(transpose(u_normal_matrix)*probe_ray,transpose(probe_matrix)*probe_normal,probe_values.x),0,0,1);
    else if (probe_mode == 7) fragment_color = vec4(derivative_normal(vec3(0,0,1),vec3(1,0,0),vec3(0,1,0),vec2(probe_values.x,0),vec2(0,probe_values.y)),1);
    else {
        vec4 sampled = material_sample(probe_texture,int(probe_values.x));
        fragment_color = vec4(probe_mode == 4 ? sampled.rgb : sampled.rga,1);
    }
}
)glsl";
    const GLuint vertex = compile(GL_VERTEX_SHADER, R"glsl(#version 410 core
out vec2 v_texcoord0;
out vec2 v_texcoord1;
out vec4 v_world_tangent;
void main() {
    v_world_tangent = vec4(1,0,0,1);
    v_texcoord0 = vec2(.125,.25); v_texcoord1 = vec2(.875,.75);
    vec2 p = vec2((gl_VertexID<<1)&2,gl_VertexID&2); gl_Position = vec4(p*2.0-1.0,0,1);
}
)glsl");
    const GLuint pixel = compile(GL_FRAGMENT_SHADER, fragment);
    if (!vertex || !pixel)
        return false;
    probe.program = glCreateProgram();
    glAttachShader(probe.program, vertex);
    glAttachShader(probe.program, pixel);
    glLinkProgram(probe.program);
    glDeleteShader(vertex);
    glDeleteShader(pixel);
    GLint linked = 0;
    glGetProgramiv(probe.program, GL_LINK_STATUS, &linked);
    if (!linked)
        return false;
    glGenTextures(1, &probe.texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, probe.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
    glGenFramebuffers(1, &probe.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, probe.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, probe.texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;
    glGenVertexArrays(1, &probe.vertex_array);
    glBindVertexArray(probe.vertex_array);
    glUseProgram(probe.program);
    glViewport(0, 0, 1, 1);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    return true;
}

struct ProbeInput {
    int mode = 0;
    float a = 0, b = 0;
    Vector normal{0, 0, 1};
    Vector ray{0, 0, -1};
    Matrix matrix{1, 0, 0, 0, 1, 0, 0, 0, 1};
};
Matrix reference_normal_matrix(Matrix matrix);

Vector run(const ProbeResources& probe, ProbeInput input)
{
    glUniform1i(glGetUniformLocation(probe.program, "probe_mode"), input.mode);
    glUniform4f(glGetUniformLocation(probe.program, "probe_values"), input.a, input.b, 0, 0);
    glUniform3f(glGetUniformLocation(probe.program, "probe_normal"),
                static_cast<float>(input.normal[0]), static_cast<float>(input.normal[1]),
                static_cast<float>(input.normal[2]));
    glUniform3f(glGetUniformLocation(probe.program, "probe_ray"), static_cast<float>(input.ray[0]),
                static_cast<float>(input.ray[1]), static_cast<float>(input.ray[2]));
    glUniformMatrix3fv(glGetUniformLocation(probe.program, "probe_matrix"), 1, GL_FALSE,
                       input.matrix.data());
    if (input.mode == 8) {
        const auto normal = reference_normal_matrix(input.matrix);
        glUniformMatrix3fv(glGetUniformLocation(probe.program, "u_normal_matrix"), 1, GL_FALSE,
                           normal.data());
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<float, 4> pixel{};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
    return {pixel[0], pixel[1], pixel[2]};
}

bool near(double actual, double reference)
{
    return std::isfinite(actual) &&
           std::abs(actual - reference) <= 1e-5 + 1e-3 * std::abs(reference);
}

// Independent double-precision Gaussian elimination; no shader matrix arithmetic reused.
Vector solve(Matrix matrix, Vector ray)
{
    std::array<std::array<double, 4>, 3> augmented{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t col = 0; col < 3; ++col)
            augmented[row][col] = matrix[col * 3 + row];
        augmented[row][3] = ray[row];
    }
    for (std::size_t pivot = 0; pivot < 3; ++pivot) {
        std::size_t best = pivot;
        for (std::size_t row = pivot + 1; row < 3; ++row)
            if (std::abs(augmented[row][pivot]) > std::abs(augmented[best][pivot]))
                best = row;
        std::swap(augmented[pivot], augmented[best]);
        const double diagonal = augmented[pivot][pivot];
        for (double& value : augmented[pivot])
            value /= diagonal;
        for (std::size_t row = 0; row < 3; ++row) {
            if (row == pivot)
                continue;
            const double weight = augmented[row][pivot];
            for (std::size_t col = 0; col < 4; ++col)
                augmented[row][col] -= weight * augmented[pivot][col];
        }
    }
    return {augmented[0][3], augmented[1][3], augmented[2][3]};
}

Matrix reference_normal_matrix(Matrix matrix)
{
    Matrix normal{};
    for (std::size_t column = 0; column < 3; ++column) {
        Vector axis{};
        axis[column] = 1;
        const auto inverse_column = solve(matrix, axis);
        for (std::size_t row = 0; row < 3; ++row)
            normal[row * 3 + column] = static_cast<float>(inverse_column[row]);
    }
    return normal;
}

double reference_path(Matrix matrix, Vector ray, Vector normal, float thickness)
{
    Vector local_normal{};
    for (std::size_t col = 0; col < 3; ++col)
        for (std::size_t row = 0; row < 3; ++row)
            local_normal[col] += matrix[col * 3 + row] * normal[row];
    double normal_length = 0;
    for (double v : local_normal)
        normal_length += v * v;
    normal_length = std::sqrt(normal_length);
    const Vector local_ray = solve(matrix, ray);
    double projection = 0;
    for (std::size_t i = 0; i < 3; ++i)
        projection += local_ray[i] * local_normal[i] / normal_length;
    return thickness / std::abs(projection);
}
} // namespace

namespace {
bool verify_ggx_probes(const ProbeResources& probe, std::size_t& cases)
{
    for (float roughness : {0.045F, 0.08F, 0.2F, 0.6F, 1.0F}) {
        for (float cosine : {0.0F, 0.25F, 0.9F, 0.9999F, 0.999999F, 1.0F}) {
            const double a = std::pow(static_cast<double>(roughness), 4);
            const double denominator = 1.0 + static_cast<double>(cosine) * cosine * (a - 1.0);
            const double reference = a / (std::numbers::pi * denominator * denominator);
            if (!near(run(probe, {0, cosine, roughness})[0], reference))
                return false;
            ++cases;
        }
    }
    return true;
}

bool verify_specular_probes(const ProbeResources& probe, std::size_t& cases)
{
    const Vector color{30.0F, 2.0F, 0.5F};
    for (float ior : {0.0F, 1.0F, 1.5F, 1.52F}) {
        for (float strength : {0.0F, 0.05F, 0.25F, 1.0F}) {
            const double reflectance =
                ior == 0 ? 1 : std::pow((static_cast<double>(ior) - 1) / (ior + 1), 2);
            const auto actual = run(probe, {1, ior, strength, color});
            for (std::size_t i = 0; i < 3; ++i)
                if (!near(actual[i], std::min(1.0, reflectance * color[i]) * strength))
                    return false;
            ++cases;
        }
    }
    return true;
}

bool verify_volume_probes(const ProbeResources& probe, std::size_t& cases)
{
    constexpr std::array<Matrix, 5> matrices{{{0.3048F, 0, 0, 0, 0.3048F, 0, 0, 0, 0.3048F},
                                              {2, 0, 0, 0, 0.5F, 0, 0, 0, 3},
                                              {-2, 0, 0, 0, 0.5F, 0, 0, 0, 3},
                                              {1, 0.25F, 0, 0.5F, 2, 0.2F, 0, 0.3F, 0.75F},
                                              {0, -2, 0, 1, 0, 0, 0, 0, 0.5F}}};
    for (auto matrix : matrices) {
        for (auto ray : {Vector{0, 0, -1}, Vector{0.6F, 0, -0.8F}}) {
            const Vector normal{0, 0, 1};
            const double reference = reference_path(matrix, ray, normal, 1);
            if (!near(run(probe, {2, 1, 0, normal, ray, matrix})[0], reference))
                return false;
            if (!near(run(probe, {8, 1, 0, normal, ray, matrix})[0], reference))
                return false;
            cases += 2;
        }
    }
    if (run(probe, {2, 0, 0})[0] != 0 ||
        run(probe, {2, 1, 0, {0, 0, 1}, {0, 0, -1}, {0, 0, 0, 0, 0, 0, 0, 0, 0}})[0] != -1)
        return false;
    return true;
}

bool verify_tabletop_probes(const ProbeResources& probe, std::size_t&)
{
    const float path = 0.3048F, distance = 0.3F, dark = 0.007843137718F;
    const auto attenuation = run(probe, {3, path, distance, {dark, dark, dark}});
    const double expected =
        std::pow(static_cast<double>(dark), static_cast<double>(path) / distance);
    if (!near(attenuation[0], expected) || expected < 0.0072 || expected > 0.0074)
        return false;
    std::cout << "Tabletop transmittance " << expected << "\n";
    return true;
}

bool verify_attenuation_probes(const ProbeResources& probe, std::size_t& cases)
{
    for (float length : {0.0F, 0.001F, 0.3F, 3.0F}) {
        for (float attenuation_distance : {0.0F, 0.3F, 2.0F}) {
            const Vector colors{0.0F, 1.0F, 0.25F};
            const auto actual = run(probe, {3, length, attenuation_distance, colors});
            for (std::size_t i = 0; i < 3; ++i) {
                const double reference =
                    length <= 0 || attenuation_distance <= 0
                        ? 1
                        : std::pow(colors[i], static_cast<double>(length) / attenuation_distance);
                if (!near(actual[i], reference))
                    return false;
            }
            ++cases;
        }
    }
    return true;
}
double decode_srgb(std::uint8_t value)
{
    const double encoded = static_cast<double>(value) / 255.0;
    return encoded <= .04045 ? encoded / 12.92 : std::pow((encoded + .055) / 1.055, 2.4);
}

struct ProbeTexture {
    GLuint texture = 0;
    ~ProbeTexture()
    {
        glDeleteTextures(1, &texture);
    }
};

bool verify_texture_pixel(const ProbeResources& probe, int slot,
                          std::array<std::uint8_t, 4> expected, bool srgb)
{
    const auto color = run(probe, {4, static_cast<float>(slot), 0});
    const auto channels = run(probe, {5, static_cast<float>(slot), 0});
    for (std::size_t i = 0; i < 3; ++i) {
        const double reference =
            srgb ? decode_srgb(expected[i]) : static_cast<double>(expected[i]) / 255;
        // The native sRGB sampler has implementation-defined conversion precision.
        // Arithmetic probes and linear/alpha channels retain the strict reference gate.
        const bool matches =
            srgb ? std::abs(color[i] - reference) <= 2e-3 : near(color[i], reference);
        if (!matches) {
            std::cerr << "Texture probe slot " << slot << " sRGB " << srgb << " channel " << i
                      << " expected " << reference << " actual " << color[i] << '\n';
            return false;
        }
    }
    return near(channels[2], static_cast<double>(expected[3]) / 255);
}

void set_probe_mapping(const ProbeResources& probe, int slot, int uv, bool mirrored)
{
    const std::string suffix = "[" + std::to_string(slot) + "]";
    glUniform1i(glGetUniformLocation(probe.program, ("u_texture_texcoord_sets" + suffix).c_str()),
                uv);
    glUniform2f(glGetUniformLocation(probe.program, ("u_texture_scales" + suffix).c_str()),
                mirrored ? -1.0F : 1.0F, 1);
    glUniform2f(glGetUniformLocation(probe.program, ("u_texture_offsets" + suffix).c_str()),
                mirrored ? 1.0F : 0.0F, 0);
}

Vector reference_normal(float scale, float y_sign)
{
    Vector value{(64.0 / 255 * 2 - 1) * scale, (128.0 / 255 * 2 - 1) * scale * y_sign,
                 192.0 / 255 * 2 - 1};
    const double length =
        std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    for (auto& component : value)
        component /= length;
    return value;
}

bool near_vector(Vector actual, Vector reference)
{
    for (std::size_t i = 0; i < 3; ++i)
        if (!near(actual[i], reference[i]))
            return false;
    return true;
}

bool verify_normal_probes(const ProbeResources& probe, std::size_t& cases)
{
    set_probe_mapping(probe, 11, 0, false);
    glUniform1i(glGetUniformLocation(probe.program, "u_clearcoat_normal_texture"), 1);
    for (float scale : {0.0F, .4F, 1.0F}) {
        if (!near_vector(run(probe, {6, 11, scale}), reference_normal(scale, 1)))
            return false;
        glUniform1f(glGetUniformLocation(probe.program, "u_clearcoat_normal_scale"), scale);
        if (!near_vector(run(probe, {7, 1, -1}), reference_normal(scale, -1)))
            return false;
        cases += 2;
    }
    if (!near_vector(run(probe, {7, 0, 1}), {0, 0, 1}))
        return false;
    ++cases;
    return true;
}

bool verify_texture_probes(const ProbeResources& probe, std::size_t& cases)
{
    constexpr std::array<std::uint8_t, 16> pixels{64, 128, 192, 32, 200, 10, 30, 240,
                                                  20, 220, 100, 80, 250, 90, 50, 160};
    ProbeTexture source;
    glGenTextures(1, &source.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, source.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(glGetUniformLocation(probe.program, "probe_texture"), 1);
    for (const bool srgb : {false, true}) {
        glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, 2, 2, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, pixels.data());
        for (int slot = 5; slot < 12; ++slot) {
            set_probe_mapping(probe, slot, 0, false);
            if (!verify_texture_pixel(probe, slot, {64, 128, 192, 32}, srgb))
                return false;
            set_probe_mapping(probe, slot, 1, false);
            if (!verify_texture_pixel(probe, slot, {250, 90, 50, 160}, srgb))
                return false;
            set_probe_mapping(probe, slot, 1, true);
            if (!verify_texture_pixel(probe, slot, {20, 220, 100, 80}, srgb))
                return false;
            cases += 3;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return verify_normal_probes(probe, cases);
}

} // namespace
bool verify_material_shader_numerics()
{
    ProbeResources probe;
    if (!prepare(probe))
        return false;
    std::size_t cases = 0;
    if (!verify_ggx_probes(probe, cases) || !verify_specular_probes(probe, cases) ||
        !verify_volume_probes(probe, cases) || !verify_tabletop_probes(probe, cases) ||
        !verify_attenuation_probes(probe, cases) || !verify_texture_probes(probe, cases))
        return false;
    std::cout << cases + 3 << " GPU float material probes passed\n";
    return glGetError() == GL_NO_ERROR;
}
