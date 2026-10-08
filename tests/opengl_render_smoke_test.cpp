#include "engine_access.h"
#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
int run_scene_allocation_failure(elf3d::Engine& engine, elf3d::Scene& scene, elf3d::EntityId entity,
                                 std::string_view scenario);
#endif

namespace {

constexpr int skipped = 77;

elf3d::detail::GraphicsProcedure load_opengl_procedure(const char* name) noexcept
{
    return glfwGetProcAddress(name);
}

[[nodiscard]] bool in_range(std::uint8_t value, std::uint8_t minimum, std::uint8_t maximum) noexcept
{
    return value >= minimum && value <= maximum;
}

[[nodiscard]] int fail(int code, const char* message)
{
    std::cerr << message << '\n';
    return code;
}

struct SmokeFixture {
    std::unique_ptr<elf3d::Scene> scene;
    std::unique_ptr<elf3d::Viewport> viewport;
    elf3d::EntityId non_camera_entity;
    elf3d::EntityId camera;
};

struct SmokeAssets {
    elf3d::MeshHandle mesh;
    elf3d::MaterialHandle red;
    elf3d::MaterialHandle green;
};

struct ForeignGlState {
    GLint draw_framebuffer = 0;
    GLint read_framebuffer = 0;
    std::array<GLint, 4> viewport{};
    GLint program = 0;
    GLint vertex_array = 0;
    GLint active_texture = 0;
    std::array<GLint, 8> textures_2d{};
    std::array<GLint, 8> textures_cube{};
    GLboolean blend = GL_FALSE;
    GLboolean depth_test = GL_FALSE;
    GLboolean cull_face = GL_FALSE;
    std::array<GLboolean, 4> color_mask{};
    GLboolean depth_mask = GL_FALSE;

    bool operator==(const ForeignGlState&) const = default;
};

class ForeignGlObjects final {
  public:
    ~ForeignGlObjects()
    {
        glUseProgram(0);
        glBindVertexArray(0);
        for (std::size_t index = 0; index < 8; ++index) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &texture_2d);
        glDeleteTextures(1, &texture_cube);
        glDeleteVertexArrays(1, &vertex_array);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteProgram(program);
    }

    ForeignGlObjects(const ForeignGlObjects&) = delete;
    ForeignGlObjects& operator=(const ForeignGlObjects&) = delete;

    GLuint framebuffer = 0;
    GLuint vertex_array = 0;
    GLuint texture_2d = 0;
    GLuint texture_cube = 0;
    GLuint program = 0;

    ForeignGlObjects() = default;
};

[[nodiscard]] GLuint compile_foreign_shader(GLenum type, const char* source) noexcept
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint create_foreign_program() noexcept
{
    constexpr const char* vertex_source =
        "#version 410 core\nvoid main(){gl_Position=vec4(0.0,0.0,0.0,1.0);}";
    constexpr const char* fragment_source =
        "#version 410 core\nout vec4 color;void main(){color=vec4(1.0);}";
    const GLuint vertex = compile_foreign_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = compile_foreign_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (vertex == 0 || fragment == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

[[nodiscard]] ForeignGlState capture_foreign_state() noexcept
{
    ForeignGlState state;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state.draw_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state.read_framebuffer);
    glGetIntegerv(GL_VIEWPORT, state.viewport.data());
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertex_array);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.active_texture);
    for (std::size_t index = 0; index < state.textures_2d.size(); ++index) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.textures_2d[index]);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &state.textures_cube[index]);
    }
    glActiveTexture(static_cast<GLenum>(state.active_texture));
    state.blend = glIsEnabled(GL_BLEND);
    state.depth_test = glIsEnabled(GL_DEPTH_TEST);
    state.cull_face = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_COLOR_WRITEMASK, state.color_mask.data());
    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depth_mask);
    return state;
}

[[nodiscard]] bool configure_foreign_state(ForeignGlObjects& objects) noexcept
{
    glGenFramebuffers(1, &objects.framebuffer);
    glGenVertexArrays(1, &objects.vertex_array);
    glGenTextures(1, &objects.texture_2d);
    glGenTextures(1, &objects.texture_cube);
    objects.program = create_foreign_program();
    if (objects.framebuffer == 0 || objects.vertex_array == 0 || objects.texture_2d == 0 ||
        objects.texture_cube == 0 || objects.program == 0) {
        return false;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, objects.framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, objects.framebuffer);
    glViewport(3, 4, 17, 19);
    glUseProgram(objects.program);
    glBindVertexArray(objects.vertex_array);
    for (std::size_t index = 0; index < 8; ++index) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
        glBindTexture(GL_TEXTURE_2D, objects.texture_2d);
        glBindTexture(GL_TEXTURE_CUBE_MAP, objects.texture_cube);
    }
    glActiveTexture(GL_TEXTURE2);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glDepthMask(GL_FALSE);
    return glGetError() == GL_NO_ERROR;
}

[[nodiscard]] int create_public_objects(elf3d::Engine& engine, SmokeFixture& fixture)
{
    elf3d::Result<std::unique_ptr<elf3d::Scene>> scene_result = engine.create_scene();
    elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport_result =
        engine.create_viewport({64, 64});
    if (!scene_result || !viewport_result) {
        return fail(3, "OpenGL smoke test failed to create public scene or viewport");
    }
    fixture.scene = std::move(scene_result).value();
    fixture.viewport = std::move(viewport_result).value();
    return 0;
}

[[nodiscard]] int create_scene_assets(elf3d::Scene& scene, SmokeAssets& assets)
{
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-2.0F, -2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{2.0F, -2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 2.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const auto mesh = scene.create_mesh({vertices, indices});

    elf3d::MaterialDescription red_material;
    red_material.base_color = {1.0F, 0.0F, 0.0F, 0.5F};
    red_material.alpha_mode = elf3d::AlphaMode::blend;
    red_material.unlit = true;
    const auto red = scene.create_material(red_material);

    elf3d::MaterialDescription green_material;
    green_material.base_color = {0.0F, 1.0F, 0.0F, 0.5F};
    green_material.alpha_mode = elf3d::AlphaMode::blend;
    green_material.unlit = true;
    const auto green = scene.create_material(green_material);
    if (!mesh || !red || !green) {
        return fail(4, "OpenGL smoke test failed to create scene assets");
    }
    assets = SmokeAssets{mesh.value(), red.value(), green.value()};
    return 0;
}

[[nodiscard]] int create_scene_entities(SmokeFixture& fixture, const SmokeAssets& assets)
{
    const auto far_model = fixture.scene->create_model_entity(assets.mesh, assets.red);
    const auto near_model = fixture.scene->create_model_entity(assets.mesh, assets.green);
    const auto camera = fixture.scene->create_perspective_camera_entity({});
    if (!far_model || !near_model || !camera) {
        return fail(4, "OpenGL smoke test failed to create scene entities");
    }
    fixture.non_camera_entity = far_model.value();
    fixture.camera = camera.value();
    elf3d::Transform far_transform;
    far_transform.translation = {0.0F, 0.0F, -3.0F};
    elf3d::Transform near_transform;
    near_transform.translation = {0.0F, 0.0F, -2.0F};
    if (!fixture.scene->set_local_transform(far_model.value(), far_transform) ||
        !fixture.scene->set_local_transform(near_model.value(), near_transform)) {
        return fail(5, "OpenGL smoke test failed to position transparent models");
    }
    return 0;
}

[[nodiscard]] int verify_camera_role_errors(SmokeFixture& fixture)
{
    const elf3d::Result<void> navigation = fixture.viewport->update_navigation(
        *fixture.scene, fixture.non_camera_entity, elf3d::NavigationInput{});
    if (navigation || navigation.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(20, "Viewport navigation accepted a non-camera entity");
    }

    const elf3d::Result<elf3d::Ray3> picking_ray = fixture.viewport->make_picking_ray(
        *fixture.scene, fixture.non_camera_entity, {32.0F, 32.0F});
    if (picking_ray || picking_ray.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(21, "Viewport picking accepted a non-camera entity");
    }

    const elf3d::Result<elf3d::ProjectedViewportPoint> projection =
        fixture.viewport->project_world_to_viewport(*fixture.scene, fixture.non_camera_entity, {});
    if (projection || projection.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(22, "Viewport projection accepted a non-camera entity");
    }

    const elf3d::Result<void> rendered =
        fixture.viewport->render(*fixture.scene, fixture.non_camera_entity);
    if (rendered || rendered.error().code() != elf3d::ErrorCode::entity_has_no_camera) {
        return fail(23, "Viewport rendering accepted a non-camera entity");
    }
    return 0;
}

[[nodiscard]] int verify_foreign_state_preserved(elf3d::Engine& engine, SmokeFixture& fixture)
{
    ForeignGlObjects objects;
    if (!configure_foreign_state(objects)) {
        return fail(24, "OpenGL smoke test failed to configure foreign host state");
    }

    const ForeignGlState expected = capture_foreign_state();

    const elf3d::Result<void> render = fixture.viewport->render(*fixture.scene, fixture.camera);
    if (!render || capture_foreign_state() != expected) {
        return fail(25, "Viewport rendering did not preserve foreign OpenGL state");
    }

    const elf3d::Result<std::optional<elf3d::PickHit>> pick =
        fixture.viewport->pick(*fixture.scene, fixture.camera, {32.0F, 32.0F});
    if (!pick || capture_foreign_state() != expected) {
        return fail(26, "Viewport picking did not preserve foreign OpenGL state");
    }

    const elf3d::Result<elf3d::detail::NativeTextureView> texture =
        elf3d::detail::EngineAccess::native_texture_view(engine, fixture.viewport->color_texture());
    if (!texture || capture_foreign_state() != expected) {
        return fail(27, "Viewport display resolve did not preserve foreign OpenGL state");
    }
    return 0;
}

[[nodiscard]] int verify_rendered_pixel(elf3d::Engine&, const SmokeFixture& fixture)
{
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    if (!fixture.viewport->read_color_pixels(pixels)) {
        return fail(9, "OpenGL smoke test could not read viewport pixels");
    }
    const std::size_t center = ((32U * 64U) + 32U) * 4U;
    const std::uint8_t red_channel = pixels[center];
    const std::uint8_t green_channel = pixels[center + 1U];
    const std::uint8_t blue_channel = pixels[center + 2U];
    const std::uint8_t alpha_channel = pixels[center + 3U];
    if (!in_range(red_channel, 130U, 145U) || !in_range(green_channel, 162U, 184U) ||
        blue_channel > 8U || alpha_channel < 250U) {
        std::cerr << "Unexpected linear-blend sRGB pixel: R=" << static_cast<unsigned>(red_channel)
                  << " G=" << static_cast<unsigned>(green_channel)
                  << " B=" << static_cast<unsigned>(blue_channel)
                  << " A=" << static_cast<unsigned>(alpha_channel) << '\n';
        return 10;
    }
    return 0;
}

[[nodiscard]] std::optional<std::array<std::uint8_t, 4>>
read_center_pixel(elf3d::Engine&, const SmokeFixture& fixture)
{
    std::vector<std::uint8_t> pixels(64U * 64U * 4U);
    if (!fixture.viewport->read_color_pixels(pixels)) {
        return std::nullopt;
    }
    constexpr std::size_t center = ((32U * 64U) + 32U) * 4U;
    return std::array<std::uint8_t, 4>{pixels[center], pixels[center + 1U], pixels[center + 2U],
                                       pixels[center + 3U]};
}

struct NormalMapPixelSample final {
    std::array<std::uint8_t, 4> pixel{};
    elf3d::RenderStatistics statistics;
};

[[nodiscard]] std::optional<NormalMapPixelSample>
render_normal_map_fixture(elf3d::Engine& engine, const std::filesystem::path& path)
{
    auto loaded = engine.load_scene(path.string());
    auto viewport = engine.create_viewport({64U, 64U});
    if (!loaded || !viewport) {
        return std::nullopt;
    }

    std::unique_ptr<elf3d::Scene> scene = std::move(loaded).value().scene;
    const auto camera = scene->create_perspective_camera_entity({});
    if (!camera) {
        return std::nullopt;
    }
    elf3d::Transform camera_transform;
    camera_transform.translation.z = 3.0F;
    if (!scene->set_local_transform(camera.value(), camera_transform)) {
        return std::nullopt;
    }

    std::unique_ptr<elf3d::Viewport> fixture_viewport = std::move(viewport).value();
    fixture_viewport->set_clear_color({0.0F, 0.0F, 0.0F, 1.0F});
    elf3d::EnvironmentLighting environment;
    environment.intensity = 0.0F;
    fixture_viewport->set_environment_lighting(environment);
    if (!fixture_viewport->render(*scene, camera.value())) {
        return std::nullopt;
    }
    SmokeFixture fixture{std::move(scene), std::move(fixture_viewport), {}, camera.value()};
    const auto pixel = read_center_pixel(engine, fixture);
    if (!pixel) {
        return std::nullopt;
    }
    return NormalMapPixelSample{*pixel, fixture.viewport->render_statistics()};
}

[[nodiscard]] unsigned pixel_rgb_difference(const std::array<std::uint8_t, 4>& first,
                                            const std::array<std::uint8_t, 4>& second) noexcept
{
    unsigned difference = 0;
    for (std::size_t index = 0; index < 3U; ++index) {
        const int delta = static_cast<int>(first[index]) - static_cast<int>(second[index]);
        difference += static_cast<unsigned>(delta < 0 ? -delta : delta);
    }
    return difference;
}

[[nodiscard]] int verify_tangent_space_normal_mapping(elf3d::Engine& engine)
{
    const std::filesystem::path fixtures =
        std::filesystem::path{ELF3D_TEST_SOURCE_DIR} / "tests" / "fixtures" / "normal_mapping";
    const auto geometric = render_normal_map_fixture(engine, fixtures / "geometric.gltf");
    const auto flat = render_normal_map_fixture(engine, fixtures / "flat.gltf");
    const auto tilted = render_normal_map_fixture(engine, fixtures / "tilted.gltf");
    const auto scale_zero = render_normal_map_fixture(engine, fixtures / "scale_zero.gltf");
    if (!geometric || !flat || !tilted || !scale_zero) {
        return fail(38, "Normal-map pixel fixtures could not be rendered");
    }
    if (pixel_rgb_difference(geometric->pixel, flat->pixel) > 6U ||
        pixel_rgb_difference(geometric->pixel, scale_zero->pixel) > 6U) {
        return fail(39, "Flat or zero-scale normal map did not match geometric-normal output");
    }
    if (pixel_rgb_difference(geometric->pixel, tilted->pixel) < 8U ||
        flat->statistics.gpu_texture_uploads != 1U ||
        geometric->statistics.gpu_texture_uploads != 0U) {
        std::cerr << "Normal-map pixels geometric=" << static_cast<unsigned>(geometric->pixel[0])
                  << ',' << static_cast<unsigned>(geometric->pixel[1]) << ','
                  << static_cast<unsigned>(geometric->pixel[2])
                  << " flat=" << static_cast<unsigned>(flat->pixel[0]) << ','
                  << static_cast<unsigned>(flat->pixel[1]) << ','
                  << static_cast<unsigned>(flat->pixel[2])
                  << " tilted=" << static_cast<unsigned>(tilted->pixel[0]) << ','
                  << static_cast<unsigned>(tilted->pixel[1]) << ','
                  << static_cast<unsigned>(tilted->pixel[2])
                  << " uploads=" << geometric->statistics.gpu_texture_uploads << ','
                  << flat->statistics.gpu_texture_uploads << '\n';
        return fail(40, "Tilted normal map did not perturb lighting or upload lazily");
    }
    return 0;
}

[[nodiscard]] int verify_lazy_display_invalidation(elf3d::Engine& engine, SmokeFixture& fixture)
{
    const auto before = read_center_pixel(engine, fixture);
    const std::uint64_t revision = fixture.viewport->render_revision();
    elf3d::DisplayTransform display;
    display.exposure_ev = 1.0F;
    fixture.viewport->set_display_transform(display);
    const auto after = read_center_pixel(engine, fixture);
    if (!before || !after || fixture.viewport->render_revision() != revision + 1U ||
        (*after)[0] <= (*before)[0] || (*after)[1] <= (*before)[1]) {
        return fail(37,
                    "Display-transform change did not lazily re-resolve the retained HDR frame");
    }
    return 0;
}

[[nodiscard]] bool has_render_gpu_timings(const elf3d::RenderStatistics& statistics) noexcept
{
    return statistics.gpu_main_pass_timing_available && statistics.gpu_resolve_timing_available &&
           statistics.gpu_main_pass_milliseconds >= 0.0 &&
           statistics.gpu_resolve_milliseconds >= 0.0;
}

[[nodiscard]] int verify_foreign_timer_query_preserved(SmokeFixture& fixture)
{
    GLuint query = 0;
    glGenQueries(1, &query);
    glBeginQuery(GL_TIME_ELAPSED, query);
    const elf3d::Result<void> render = fixture.viewport->render(*fixture.scene, fixture.camera);
    GLint current_query = 0;
    glGetQueryiv(GL_TIME_ELAPSED, GL_CURRENT_QUERY, &current_query);
    glEndQuery(GL_TIME_ELAPSED);
    glDeleteQueries(1, &query);
    return render && current_query == static_cast<GLint>(query)
               ? 0
               : fail(32, "Viewport rendering disturbed a foreign timer query");
}

[[nodiscard]] int prepare_smoke_fixture(elf3d::Engine& engine, SmokeFixture& fixture)
{
    const int objects = create_public_objects(engine, fixture);
    if (objects != 0) {
        return objects;
    }
    SmokeAssets assets;
    const int asset_status = create_scene_assets(*fixture.scene, assets);
    if (asset_status != 0) {
        return asset_status;
    }

    const int entities = create_scene_entities(fixture, assets);
    if (entities != 0) {
        return entities;
    }
    return verify_camera_role_errors(fixture);
}

[[nodiscard]] int verify_late_dependent_destruction()
{
    const elf3d::detail::EngineCreateOptions options{load_opengl_procedure};
    auto created = elf3d::detail::EngineAccess::create(options);
    if (!created) {
        return fail(33, "Lifetime probe could not create Engine");
    }
    auto engine = std::move(created).value();
    auto scene_result = engine->create_scene();
    auto viewport_result = engine->create_viewport({16, 16});
    if (!scene_result || !viewport_result) {
        return fail(33, "Lifetime probe could not create dependents");
    }
    auto scene = std::move(scene_result).value();
    auto viewport = std::move(viewport_result).value();
    const auto camera = scene->create_perspective_camera_entity({});
    if (!camera) {
        return fail(34, "Lifetime probe could not create camera");
    }
    engine.reset();
    const auto rendered = viewport->render(*scene, camera.value());
    if (rendered || rendered.error().code() != elf3d::ErrorCode::graphics_shutdown) {
        return fail(35, "Late operation did not report Engine shutdown");
    }
    std::array<std::uint8_t, 16 * 16 * 4> pixels{};
    const auto readback = viewport->read_color_pixels(pixels);
    if (readback || readback.error().code() != elf3d::ErrorCode::graphics_shutdown) {
        return fail(41, "Late readback did not report Engine shutdown");
    }
    viewport.reset();
    scene.reset();
    return 0;
}

[[nodiscard]] bool has_top_down_pattern(std::span<const std::uint8_t> pixels) noexcept
{
    return pixels[0] == 0 && pixels[1] == 255 && pixels[8] == 255 && pixels[9] == 0;
}

[[nodiscard]] bool verify_readback_pack_state(elf3d::Viewport& viewport, GLuint texture)
{
    std::array<std::uint8_t, 16> pixels{};
    // OpenGL writes bottom row first; public readback must reverse these rows.
    constexpr std::array<std::uint8_t, 16> pattern{255, 0,   0, 255, 255, 0,   0, 255,
                                                   0,   255, 0, 255, 0,   255, 0, 255};
    GLint previous_texture = 0;
    GLint previous_pack_buffer = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previous_pack_buffer);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, pattern.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    constexpr std::array<GLenum, 6> names{GL_PACK_ALIGNMENT,    GL_PACK_ROW_LENGTH,
                                          GL_PACK_SKIP_ROWS,    GL_PACK_SKIP_PIXELS,
                                          GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_IMAGES};
    constexpr std::array<GLint, 6> unusual{8, 9, 3, 2, 10, 1};
    std::array<GLint, 6> original{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        glGetIntegerv(names[index], &original[index]);
        glPixelStorei(names[index], unusual[index]);
    }
    GLuint pack_buffer = 0;
    glGenBuffers(1, &pack_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
    glBufferData(GL_PIXEL_PACK_BUFFER, 1024, nullptr, GL_STREAM_READ);
    const auto readback = viewport.read_color_pixels(pixels);
    bool restored = true;
    for (std::size_t index = 0; index < names.size(); ++index) {
        GLint actual = 0;
        glGetIntegerv(names[index], &actual);
        restored = restored && actual == unusual[index];
        glPixelStorei(names[index], original[index]);
    }
    GLint actual_buffer = 0;
    GLint actual_texture = 0;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &actual_buffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &actual_texture);
    restored = restored && actual_buffer == static_cast<GLint>(pack_buffer) &&
               actual_texture == previous_texture;
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previous_pack_buffer));
    glDeleteBuffers(1, &pack_buffer);
    return readback && restored && has_top_down_pattern(pixels);
}

[[nodiscard]] int verify_readback_resize(elf3d::Viewport& viewport, elf3d::Scene& scene,
                                         elf3d::EntityId camera)
{
    std::array<std::uint8_t, 16> pixels{};
    if (!viewport.resize({2, 2}) || !viewport.read_color_pixels(pixels) ||
        !viewport.resize({3, 2})) {
        return fail(48, "Readback fixture resize failed");
    }
    const auto resized = viewport.read_color_pixels(pixels);
    if (resized || resized.error().code() != elf3d::ErrorCode::texture_unavailable) {
        return fail(49, "Recreated target retained a readable image");
    }
    std::array<std::uint8_t, 24> new_pixels{};
    if (!viewport.render(scene, camera) || !viewport.read_color_pixels(new_pixels)) {
        return fail(50, "Resized target did not produce a new readable image");
    }
    return 0;
}

[[nodiscard]] int verify_readback_before_render(elf3d::Viewport& viewport)
{
    std::array<std::uint8_t, 16> pixels{};
    const auto absent = viewport.read_color_pixels(pixels);
    if (absent || absent.error().code() != elf3d::ErrorCode::texture_unavailable) {
        return fail(43, "Unrendered viewport unexpectedly provided pixels");
    }
    return 0;
}

[[nodiscard]] int verify_readback_contract(elf3d::Engine& engine)
{
    auto created_scene = engine.create_scene();
    auto created_viewport = engine.create_viewport({2, 2});
    if (!created_scene || !created_viewport) {
        return fail(42, "Readback fixture creation failed");
    }
    auto scene = std::move(created_scene).value();
    auto viewport = std::move(created_viewport).value();
    const auto camera = scene->create_perspective_camera_entity({});
    if (!camera || verify_readback_before_render(*viewport) != 0) {
        return 43;
    }
    if (!viewport->render(*scene, camera.value())) {
        return fail(44, "Readback fixture render failed");
    }
    std::array<std::uint8_t, 15> wrong_size{};
    const auto invalid = viewport->read_color_pixels(wrong_size);
    if (invalid || invalid.error().code() != elf3d::ErrorCode::invalid_argument) {
        return fail(45, "Readback accepted incorrect storage size");
    }
    const auto native =
        elf3d::detail::EngineAccess::native_texture_view(engine, viewport->color_texture());
    if (!native) {
        return fail(46, "Readback test could not resolve image");
    }
    if (!verify_readback_pack_state(*viewport, static_cast<GLuint>(native.value().value))) {
        return fail(47, "Readback orientation or OpenGL pack state contract failed");
    }
    return verify_readback_resize(*viewport, *scene, camera.value());
}

class SmokeApplication final : public elf3d::Application {
  public:
    explicit SmokeApplication(std::string_view failure_scenario)
        : failure_scenario_(failure_scenario)
    {
    }
    elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override
    {
        // Native backend probes borrow the framework context and never own its lifecycle.
        if (gladLoadGL(load_opengl_procedure) == 0) {
            return error("GLAD test table failed");
        }
        if (prepare_smoke_fixture(context.engine(), fixture_) != 0) {
            return error("Fixture preparation failed");
        }
#if defined(_MSC_VER) && defined(_DEBUG)
        if (!failure_scenario_.empty()) {
            run_scene_allocation_failure(context.engine(), *fixture_.scene, fixture_.camera,
                                         failure_scenario_);
            return error("Allocation failure probe returned unexpectedly");
        }
#endif
        fixture_.viewport->set_clear_color({0, 0, 0, 1});
        return {};
    }
    elf3d::Result<void> update(elf3d::ApplicationUpdateContext& context) noexcept override
    {
        if (!context.previous_frame_statistics()) {
            return {};
        }
        auto& engine = context.engine();
        if (attempt_ == 0) {
            if (!verify_initial_frame(engine)) {
                return error("Native graphics or pixel contract failed");
            }
        }
        const auto picked = fixture_.viewport->pick(*fixture_.scene, fixture_.camera, {32, 32});
        if (!picked) {
            return picked.error();
        }
        const auto picking = fixture_.viewport->picking_statistics();
        if (has_render_gpu_timings(fixture_.viewport->render_statistics()) && picking &&
            picking.value().latest_gpu_timing_available &&
            picking.value().latest_gpu_milliseconds >= 0.0) {
            passed_ = true;
            context.request_exit();
        } else if (++attempt_ == 16) {
            return error("Delayed GPU timing samples did not mature");
        }
        return {};
    }
    elf3d::Result<void> build_ui(elf3d::ApplicationUiContext& context) noexcept override
    {
        return context.queue_viewport_render(*fixture_.viewport, *fixture_.scene, fixture_.camera);
    }
    void stop(elf3d::ApplicationContext&) noexcept override
    {
        fixture_.viewport.reset();
        fixture_.scene.reset();
    }
    bool passed() const noexcept
    {
        return passed_;
    }

  private:
    bool verify_initial_frame(elf3d::Engine& engine) noexcept
    {
        return verify_rendered_pixel(engine, fixture_) == 0 &&
               verify_foreign_state_preserved(engine, fixture_) == 0 &&
               verify_foreign_timer_query_preserved(fixture_) == 0 &&
               verify_tangent_space_normal_mapping(engine) == 0 &&
               verify_lazy_display_invalidation(engine, fixture_) == 0 &&
               verify_readback_contract(engine) == 0 && verify_late_dependent_destruction() == 0;
    }
    static elf3d::Error error(std::string_view message) noexcept
    {
        return {elf3d::ErrorCode::draw_submission_failed, message};
    }
    // argv storage remains valid throughout this synchronous application run.
    std::string_view failure_scenario_;
    SmokeFixture fixture_;
    unsigned attempt_ = 0;
    bool passed_ = false;
};

} // namespace

int main(int argument_count, char** arguments)
{
    const std::string_view failure_scenario =
        argument_count == 3 && std::string_view{arguments[1]} == "--allocation-failure"
            ? std::string_view{arguments[2]}
            : std::string_view{};
    SmokeApplication application{failure_scenario};
    elf3d::ApplicationOptions options;
    options.initial_window_extent = {64, 64};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    options.presentation_mode = elf3d::PresentationMode::immediate;
    const auto result = elf3d::run_application(options, application);
    if (!result) {
        const auto code = result.error().code();
        if (code == elf3d::ErrorCode::graphics_initialization_failed ||
            code == elf3d::ErrorCode::graphics_context_unavailable ||
            code == elf3d::ErrorCode::unsupported_graphics_version) {
            std::cout << "SKIP: " << result.error().message() << '\n';
            return 77;
        }
        std::cerr << result.error().message() << '\n';
        return 1;
    }
    return application.passed() ? 0 : 1;
}
