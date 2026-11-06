#include <elf3d/app/application.h>
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elf3d::renderer {
std::string_view main_fragment_shader_source() noexcept;
}
bool verify_material_shader_numerics();

namespace {
[[nodiscard]] bool load_probe_context()
{
    return gladLoadGL([](const char* name) { return glfwGetProcAddress(name); }) != 0;
}
constexpr std::string_view vertex_source = R"glsl(#version 410 core
out vec3 v_world_normal;
out vec3 v_world_position;
out vec2 v_texcoord0;
out vec2 v_texcoord1;
out vec4 v_color;
out vec4 v_world_tangent;
uniform bool probe_backside;
void main() {
    vec2 p = vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;
    gl_Position = vec4(p,0,1);
    v_world_position = vec3(p,0);
    v_world_normal = vec3(0,0,probe_backside ? -1.0 : 1.0);
    v_world_tangent = vec4(1,0,0,1);
    v_texcoord0 = vec2(0); v_texcoord1 = vec2(0);
    v_color = vec4(1);
}
)glsl";

struct Probe {
    std::array<GLuint, 4> programs{};
    std::array<GLuint, 5> textures{}; // target, normal, LUT, cube, dummy material
    GLuint framebuffer = 0, vao = 0, query = 0;
    GLint front_face = GL_CCW;
    ~Probe()
    {
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        for (auto program : programs)
            glDeleteProgram(program);
        glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteVertexArrays(1, &vao);
        glDeleteQueries(1, &query);
        glFrontFace(static_cast<GLenum>(front_face));
        glActiveTexture(GL_TEXTURE0);
    }
};

[[nodiscard]] bool replace(std::string& source, std::string_view before, std::string_view after)
{
    const auto at = source.find(before);
    if (at == source.npos)
        return false;
    source.replace(at, before.size(), after);
    return true;
}

[[nodiscard]] std::string trace_source(std::string source)
{
    source.insert(source.find('\n') + 1, R"glsl(
uniform bool probe_trace;
float coat_material_reads = 0.0;
float coat_normal_prepared = 0.0;
float coat_brdf_entered = 0.0;
float coat_ibl_reads = 0.0;
)glsl");
    const auto sample = source.find("vec4 material_sample(");
    if (sample == source.npos)
        return {};
    source.insert(source.find('{', sample) + 1, "\n if (slot >= 9) coat_material_reads += 1.0;\n");
    const bool normal = replace(source, "coat_normal = geometric_normal;",
                                "coat_normal = geometric_normal; coat_normal_prepared = 1.0;");
    const bool brdf = replace(
        source, "float coat_roughness =", "coat_brdf_entered = 1.0; float coat_roughness =");
    const bool ibl = replace(source, "vec3 reflected = textureLod(",
                             "coat_ibl_reads += 2.0; vec3 reflected = textureLod(");
    const bool output = replace(source, "fragment_color = vec4(linear_color, base_color.a);",
                                "fragment_color = probe_trace ? "
                                "vec4(coat_material_reads,coat_normal_prepared,coat_brdf_entered,"
                                "coat_ibl_reads) : vec4(linear_color,base_color.a);");
    return normal && brdf && ibl && output ? source : std::string{};
}

[[nodiscard]] GLuint compile(GLenum type, const std::string& source)
{
    const auto shader = glCreateShader(type);
    const auto* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        std::array<char, 4096> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Clearcoat shader: %s\n", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint program(const std::string& fragment)
{
    const auto vertex = compile(GL_VERTEX_SHADER, std::string{vertex_source});
    const auto pixel = compile(GL_FRAGMENT_SHADER, fragment);
    if (!vertex || !pixel) {
        glDeleteShader(vertex);
        glDeleteShader(pixel);
        return 0;
    }
    const auto result = glCreateProgram();
    glAttachShader(result, vertex);
    glAttachShader(result, pixel);
    glLinkProgram(result);
    glDeleteShader(vertex);
    glDeleteShader(pixel);
    GLint linked = 0;
    glGetProgramiv(result, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::array<char, 4096> log{};
        glGetProgramInfoLog(result, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Clearcoat link: %s\n", log.data());
        glDeleteProgram(result);
        return 0;
    }
    return result;
}

void integer(GLuint shader, const char* name, int value)
{
    glUniform1i(glGetUniformLocation(shader, name), value);
}
void scalar(GLuint shader, const char* name, float value)
{
    glUniform1f(glGetUniformLocation(shader, name), value);
}
void vector3(GLuint shader, const char* name, float x, float y, float z)
{
    glUniform3f(glGetUniformLocation(shader, name), x, y, z);
}

struct ImageUpload {
    GLint format;
    int size;
    GLenum channels;
};

void texture2d(GLuint texture, int unit, ImageUpload image, const float* pixels)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, image.format, image.size, image.size, 0, image.channels,
                 GL_FLOAT, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void environment(Probe& probe)
{
    std::vector<float> lut(256 * 256 * 2);
    for (std::size_t y = 0; y < 256; ++y) {
        for (std::size_t x = 0; x < 256; ++x) {
            lut[(y * 256 + x) * 2] = .2F + .7F * static_cast<float>(x) / 255;
            lut[(y * 256 + x) * 2 + 1] = .01F + .03F * static_cast<float>(y) / 255;
        }
    }
    texture2d(probe.textures[2], 2, {GL_RG16F, 256, GL_RG}, lut.data());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, probe.textures[3]);
    for (int level = 0, size = 128; level < 8; ++level, size = std::max(1, size / 2)) {
        for (int face = 0; face < 6; ++face) {
            const std::array<float, 4> color{.2F + .1F * face, .6F - .07F * face, .3F, 1};
            std::vector<float> pixels(static_cast<std::size_t>(size * size * 4));
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                std::copy(color.begin(), color.end(), pixels.begin() + i);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, GL_RGBA16F, size, size, 0,
                         GL_RGBA, GL_FLOAT, pixels.data());
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 7);
}

void uniforms(GLuint shader)
{
    glUseProgram(shader);
    vector3(shader, "u_camera_world_position", .3F, .2F, 3);
    vector3(shader, "u_light_direction", -.2F, -.1F, -1);
    glUniform4f(glGetUniformLocation(shader, "u_light_color"), 1, 1, 1, 1);
    glUniform4f(glGetUniformLocation(shader, "u_base_color"), .32F, .24F, .08F, 1);
    vector3(shader, "u_specular_color_factor", 1, 1, 1);
    scalar(shader, "u_metallic_factor", .2F);
    scalar(shader, "u_roughness_factor", .3F);
    scalar(shader, "u_ior", 1.5F);
    scalar(shader, "u_specular_factor", 1);
    scalar(shader, "u_normal_scale", 1);
    scalar(shader, "u_clearcoat_normal_scale", 1);
    scalar(shader, "u_diffuse_intensity", 1);
    scalar(shader, "u_environment_intensity", 1);
    scalar(shader, "u_ambient_intensity", .1F);
    integer(shader, "u_normal_texture", 1);
    integer(shader, "u_clearcoat_normal_texture", 1);
    integer(shader, "u_clearcoat_uses_tangent_basis", 1);
    integer(shader, "u_diffuse_environment", 3);
    integer(shader, "u_specular_environment", 3);
    integer(shader, "u_environment_brdf_lut", 2);
    std::array<float, 24> scales;
    scales.fill(1);
    glUniform2fv(glGetUniformLocation(shader, "u_texture_scales[0]"), 12, scales.data());
}

[[nodiscard]] bool resize(Probe& probe, int width, int height, bool half)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, probe.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, half ? GL_RGBA16F : GL_RGBA32F, width, height, 0, GL_RGBA,
                 GL_FLOAT, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, probe.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, probe.textures[0],
                           0);
    glViewport(0, 0, width, height);
    glBindTexture(GL_TEXTURE_2D, probe.textures[4]);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

[[nodiscard]] bool prepare(Probe& probe)
{
    const std::string source{elf3d::renderer::main_fragment_shader_source()};
    const auto traced = trace_source(source);
    if (traced.empty())
        return false;
    std::string legacy = source;
    legacy.insert(legacy.find('\n') + 1, "#define ELF3D_EXTENDED_MATERIALS 0\n");
    probe.programs[0] = program(traced);
    probe.programs[1] = program(legacy);
    if (!probe.programs[0] || !probe.programs[1])
        return false;
    glGenTextures(static_cast<GLsizei>(probe.textures.size()), probe.textures.data());
    constexpr std::array<float, 4> normal{.8F, .5F, 1, 1}, white{1, 1, 1, 1};
    texture2d(probe.textures[1], 1, {GL_RGBA32F, 1, GL_RGBA}, normal.data());
    texture2d(probe.textures[4], 0, {GL_RGBA32F, 1, GL_RGBA}, white.data());
    environment(probe);
    glGenFramebuffers(1, &probe.framebuffer);
    glGenVertexArrays(1, &probe.vao);
    glBindVertexArray(probe.vao);
    glGetIntegerv(GL_FRONT_FACE, &probe.front_face);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for (GLuint unit = 0; unit < 4; ++unit)
        glBindSampler(unit, 0);
    for (auto shader : probe.programs) {
        if (shader)
            uniforms(shader);
    }
    return resize(probe, 1, 1, false);
}

struct PixelCase {
    float factor = 0, roughness = 0;
    bool trace = false, base_normal = false, coat_textures = false, backside = false;
};

[[nodiscard]] std::array<float, 4> pixel(GLuint shader, PixelCase input)
{
    glUseProgram(shader);
    scalar(shader, "u_clearcoat_factor", input.factor);
    scalar(shader, "u_clearcoat_roughness_factor", input.roughness);
    integer(shader, "probe_trace", input.trace);
    integer(shader, "u_has_normal_texture", input.base_normal);
    integer(shader, "probe_backside", input.backside);
    glFrontFace(input.backside ? GL_CW : GL_CCW);
    const std::array<GLint, 7> slots{
        0, 0, 0, 0, input.coat_textures, input.coat_textures, input.coat_textures};
    glUniform1iv(glGetUniformLocation(shader, "u_has_extension_textures[0]"), 7, slots.data());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::array<float, 4> result{};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, result.data());
    return result;
}

[[nodiscard]] bool near(const std::array<float, 4>& a, const std::array<float, 4>& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (!std::isfinite(a[i]) || std::abs(a[i] - b[i]) > 2e-6F * (1 + std::abs(b[i])))
            return false;
    }
    return true;
}

[[nodiscard]] bool zero_layer(Probe& probe)
{
    // Captured in Debug/Release before changing the production coat branch.
    constexpr std::array<std::array<float, 4>, 2> previous_pbr{
        {{.251775146F, .158980235F, .0720171854F, 1}, {.161391422F, .143023938F, .0483857766F, 1}}};
    if (near(pixel(probe.programs[1], {}), pixel(probe.programs[1], {.base_normal = true})))
        return false; // The base normal map must actually influence this control.
    for (bool base_normal : {false, true}) {
        const auto old = pixel(probe.programs[1], {.base_normal = base_normal});
        if (!near(old, previous_pbr[base_normal ? 1 : 0]))
            return false;
        for (float roughness : {0.0F, 1.0F}) {
            PixelCase input{
                .roughness = roughness, .base_normal = base_normal, .coat_textures = true};
            const auto zero = pixel(probe.programs[0], input);
            input.trace = true;
            const auto trace = pixel(probe.programs[0], input);
            if (!near(old, zero) || !near(trace, {0, 0, 0, 0}))
                return false;
        }
    }
    return true;
}

[[nodiscard]] bool positive_case(GLuint shader, float roughness)
{
    std::array<float, 4> partial{};
    for (float factor : {.045F, 1.0F}) {
        PixelCase input{.factor = factor, .roughness = roughness};
        auto plain = pixel(shader, input);
        input.base_normal = true;
        const auto mapped = pixel(shader, input);
        input.trace = true;
        const auto trace = pixel(shader, input);
        input.trace = false;
        input.backside = true;
        const auto backside = pixel(shader, input);
        if (!near(plain, mapped) || !near(plain, backside) || plain[0] <= 0 ||
            !near(trace, {0, 1, 1, 2}))
            return false;
        if (factor == .045F)
            partial = plain;
        else {
            for (std::size_t i = 0; i < 3; ++i)
                plain[i] *= .045F;
            if (!near(partial, plain))
                return false;
        }
    }
    return true;
}

[[nodiscard]] bool active_layer(Probe& probe)
{
    const auto shader = probe.programs[0];
    glUseProgram(shader);
    glUniform4f(glGetUniformLocation(shader, "u_base_color"), 0, 0, 0, 1);
    scalar(shader, "u_specular_factor", 0);
    scalar(shader, "u_metallic_factor", 0);
    for (bool direct : {false, true}) {
        scalar(shader, "u_diffuse_intensity", direct ? 1.0F : 0.0F);
        scalar(shader, "u_environment_intensity", direct ? 0.0F : 1.0F);
        if (!positive_case(shader, 0) || !positive_case(shader, 1))
            return false;
        if (near(pixel(shader, {.factor = 1, .roughness = 0}),
                 pixel(shader, {.factor = 1, .roughness = 1})))
            return false;
    }
    return true;
}

[[nodiscard]] bool benchmark_programs(Probe& probe)
{
    std::string source{elf3d::renderer::main_fragment_shader_source()};
    probe.programs[2] = program(source);
    const auto first = source.find("vec3 reflected = textureLod(");
    const auto last = source.find("linear_color = (linear_color-direct)", first);
    if (first == source.npos || last == source.npos)
        return false;
    source.replace(first, last - first, "vec3 coat_environment = vec3(0.0);\n");
    probe.programs[3] = program(source);
    if (!probe.programs[2] || !probe.programs[3])
        return false;
    uniforms(probe.programs[2]);
    uniforms(probe.programs[3]);
    glGenQueries(1, &probe.query);
    return resize(probe, 1280, 720, true);
}

[[nodiscard]] double measured_draw(GLuint query)
{
    glBeginQuery(GL_TIME_ELAPSED, query);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEndQuery(GL_TIME_ELAPSED);
    GLuint64 nanoseconds = 0;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
    return static_cast<double>(nanoseconds) / 1000000.0;
}

constexpr std::size_t warmup = 60, samples = 240;
using Timings = std::array<std::vector<double>, 2>;

[[nodiscard]] Timings measure_pair(Probe& probe, float factor, float roughness)
{
    Timings times;
    for (std::size_t frame = 0; frame < warmup + samples; ++frame) {
        for (std::size_t mode = 0; mode < 2; ++mode) {
            const bool ibl = (mode + frame) % 2 == 0;
            const auto shader = probe.programs[ibl ? 2 : 3];
            glUseProgram(shader);
            scalar(shader, "u_clearcoat_factor", factor);
            scalar(shader, "u_clearcoat_roughness_factor", roughness);
            const auto ms = measured_draw(probe.query);
            if (frame >= warmup)
                times[ibl ? 1 : 0].push_back(ms);
        }
    }
    return times;
}

void write_timings(std::ostream& report, Timings& times, float factor, float roughness)
{
    for (std::size_t mode = 0; mode < 2; ++mode) {
        auto& values = times[mode];
        std::sort(values.begin(), values.end());
        const auto median = (values[(samples - 1) / 2] + values[samples / 2]) * .5;
        report << factor << ',' << roughness << ',' << mode << ",1280,720," << warmup << ','
               << samples << ',' << median << ',' << values[(samples * 95 + 99) / 100 - 1] << ",\""
               << glGetString(GL_VENDOR) << "\",\"" << glGetString(GL_RENDERER) << "\",\""
               << glGetString(GL_VERSION) << "\"\n";
    }
}

[[nodiscard]] bool benchmark(Probe& probe, std::string_view path)
{
    if (!benchmark_programs(probe))
        return false;
    std::ofstream report{std::string{path}};
    if (!report)
        return false;
    report << "factor,roughness,coat_ibl,width,height,warmup,samples,median_ms,p95_ms,vendor,"
              "renderer,version\n";
    for (float factor : {0.0F, .045F, 1.0F}) {
        for (float roughness : {0.0F, 1.0F}) {
            auto times = measure_pair(probe, factor, roughness);
            write_timings(report, times, factor, roughness);
        }
    }
    return report.good();
}
} // namespace

// Capture before production GLSL changes; values are the independent zero-layer oracle.
bool capture_clearcoat_legacy_reference()
{
    Probe probe;
    if (!prepare(probe))
        return false;
    for (bool mapped : {false, true}) {
        const auto rgba = pixel(probe.programs[1], {.base_normal = mapped});
        std::printf("Legacy PBR normal=%d rgba=%.9g,%.9g,%.9g,%.9g\n", mapped, rgba[0], rgba[1],
                    rgba[2], rgba[3]);
    }
    return glGetError() == GL_NO_ERROR;
}

// Production GLSL, framework-owned hidden context, test-only trace and IBL A/B.
// No material UV/tangent vertex attributes or production feature switch is added.
bool verify_clearcoat_gpu_contract(std::string_view benchmark_report)
{
    Probe probe;
    if (!prepare(probe) || !zero_layer(probe) || !active_layer(probe)) {
        std::fprintf(stderr, "FAIL scalar clearcoat GPU contract\n");
        return false;
    }
    return (benchmark_report.empty() || benchmark(probe, benchmark_report)) &&
           glGetError() == GL_NO_ERROR;
}

namespace {
class ClearcoatProbeApplication final : public elf3d::Application {
  public:
    ClearcoatProbeApplication(std::string_view report, bool reference)
        : report_(report), reference_(reference)
    {
    }
    elf3d::Result<void> start(elf3d::ApplicationContext&) noexcept override
    {
        if (!load_probe_context())
            return elf3d::Error{elf3d::ErrorCode::graphics_initialization_failed,
                                "GLAD probe load failed"};
        const bool passed = reference_ ? capture_clearcoat_legacy_reference()
                                       : verify_clearcoat_gpu_contract(report_);
        return passed ? elf3d::Result<void>{}
                      : elf3d::Error{elf3d::ErrorCode::draw_submission_failed,
                                     "Clearcoat probe failed"};
    }
    elf3d::Result<void> update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        context.request_exit();
        return {};
    }
    elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override
    {
        return {};
    }
    void stop(elf3d::ApplicationContext&) noexcept override
    {
    }

  private:
    // Borrowed argv storage outlives this synchronous framework run.
    std::string_view report_;
    bool reference_;
};

[[nodiscard]] int run_probe(std::string_view report, bool reference)
{
    ClearcoatProbeApplication application{report, reference};
    elf3d::ApplicationOptions options;
    options.initial_window_extent = {64, 64};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    options.presentation_mode = elf3d::PresentationMode::immediate;
    const auto result = elf3d::run_application(options, application);
    if (result)
        return 0;
    std::fprintf(stderr, "%s\n", result.error().message());
    const auto code = result.error().code();
    return code == elf3d::ErrorCode::graphics_initialization_failed ||
                   code == elf3d::ErrorCode::graphics_context_unavailable ||
                   code == elf3d::ErrorCode::unsupported_graphics_version
               ? 77
               : 1;
}
} // namespace

std::optional<int> run_clearcoat_probe_command(int count, char** arguments)
{
    if (count == 2 && std::string_view{arguments[1]} == "--clearcoat-reference")
        return run_probe({}, true);
    if (count == 3 && std::string_view{arguments[1]} == "--clearcoat-benchmark")
        return run_probe(arguments[2], false);
    return {};
}

bool verify_material_gpu_probes()
{
    return load_probe_context() && verify_material_shader_numerics() &&
           verify_clearcoat_gpu_contract({});
}
