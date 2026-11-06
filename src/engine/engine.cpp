#include <core/exception_boundary.h>
#include <elf3d/elf3d.h>
#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <backend_opengl/backend_opengl.h>
#include <cstdint>
#include <graphics/graphics.h>
#include <memory>
#include <new>
#include <optional>
#include <picking/picking.h>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <viewport/viewport.h>

#include "engine_access.h"
#include "graphics_retirement.h"
#include "scene_load_report.h"
#include "studio_environment_resource.h"
#include "viewport_impl.h"

namespace elf3d {
namespace {

[[noreturn]] void fatal_allocation_failure() noexcept
{
    fatal_error("Elf3D memory allocation failed");
}

[[noreturn]] void fatal_unexpected_boundary_exception() noexcept
{
    fatal_error("Elf3D boundary encountered an unexpected exception");
}

[[nodiscard]] std::uint64_t allocate_engine_owner_token() noexcept
{
    static std::atomic<std::uint64_t> next_token{1};
    const std::uint64_t token = next_token.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        fatal_error("Elf3D exhausted engine owner identities");
    }
    return token;
}

[[nodiscard]] SceneLoadDiagnosticSeverity
scene_diagnostic_severity(ModelLoadDiagnosticSeverity severity) noexcept
{
    switch (severity) {
    case ModelLoadDiagnosticSeverity::information:
        return SceneLoadDiagnosticSeverity::information;
    case ModelLoadDiagnosticSeverity::warning:
        return SceneLoadDiagnosticSeverity::warning;
    }
    return SceneLoadDiagnosticSeverity::warning;
}

[[nodiscard]] SceneLoadDiagnosticCategory
scene_diagnostic_category(ModelLoadDiagnosticCategory category) noexcept
{
    switch (category) {
    case ModelLoadDiagnosticCategory::geometry:
        return SceneLoadDiagnosticCategory::geometry;
    case ModelLoadDiagnosticCategory::material:
        return SceneLoadDiagnosticCategory::material;
    case ModelLoadDiagnosticCategory::texture:
        return SceneLoadDiagnosticCategory::texture;
    case ModelLoadDiagnosticCategory::extension:
        return SceneLoadDiagnosticCategory::extension;
    case ModelLoadDiagnosticCategory::camera:
        return SceneLoadDiagnosticCategory::camera;
    case ModelLoadDiagnosticCategory::light:
        return SceneLoadDiagnosticCategory::light;
    case ModelLoadDiagnosticCategory::animation:
        return SceneLoadDiagnosticCategory::animation;
    case ModelLoadDiagnosticCategory::metadata:
        return SceneLoadDiagnosticCategory::metadata;
    case ModelLoadDiagnosticCategory::scene:
        return SceneLoadDiagnosticCategory::scene;
    }
    return SceneLoadDiagnosticCategory::scene;
}

[[nodiscard]] bool is_base_scene_diagnostic(ModelLoadDiagnosticCode code) noexcept
{
    constexpr std::array base_codes{
        ModelLoadDiagnosticCode::generated_normals,
        ModelLoadDiagnosticCode::degenerate_geometry,
        ModelLoadDiagnosticCode::missing_texture_coordinates,
        ModelLoadDiagnosticCode::unsupported_optional_extension,
        ModelLoadDiagnosticCode::material_fallback,
        ModelLoadDiagnosticCode::normal_map_fallback,
        ModelLoadDiagnosticCode::camera_fallback,
        ModelLoadDiagnosticCode::ignored_lights,
    };
    return std::find(base_codes.begin(), base_codes.end(), code) != base_codes.end();
}

[[nodiscard]] SceneLoadDiagnosticCode
base_scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept
{
    switch (code) {
    case ModelLoadDiagnosticCode::generated_normals:
        return SceneLoadDiagnosticCode::generated_normals;
    case ModelLoadDiagnosticCode::degenerate_geometry:
        return SceneLoadDiagnosticCode::degenerate_geometry;
    case ModelLoadDiagnosticCode::missing_texture_coordinates:
        return SceneLoadDiagnosticCode::missing_texture_coordinates;
    case ModelLoadDiagnosticCode::unsupported_optional_extension:
        return SceneLoadDiagnosticCode::unsupported_optional_extension;
    case ModelLoadDiagnosticCode::material_fallback:
        return SceneLoadDiagnosticCode::material_fallback;
    case ModelLoadDiagnosticCode::normal_map_fallback:
        return SceneLoadDiagnosticCode::normal_map_fallback;
    case ModelLoadDiagnosticCode::camera_fallback:
        return SceneLoadDiagnosticCode::camera_fallback;
    case ModelLoadDiagnosticCode::ignored_lights:
        return SceneLoadDiagnosticCode::ignored_lights;
    default:
        return SceneLoadDiagnosticCode::material_fallback;
    }
}

[[nodiscard]] SceneLoadDiagnosticCode
extended_scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept
{
    constexpr std::array codes{
        std::pair{ModelLoadDiagnosticCode::ignored_animation,
                  SceneLoadDiagnosticCode::ignored_animation},
        std::pair{ModelLoadDiagnosticCode::ignored_skin, SceneLoadDiagnosticCode::ignored_skin},
        std::pair{ModelLoadDiagnosticCode::ignored_morph_targets,
                  SceneLoadDiagnosticCode::ignored_morph_targets},
        std::pair{ModelLoadDiagnosticCode::ignored_instancing,
                  SceneLoadDiagnosticCode::ignored_instancing},
        std::pair{ModelLoadDiagnosticCode::skipped_invalid_transform,
                  SceneLoadDiagnosticCode::skipped_invalid_transform},
        std::pair{ModelLoadDiagnosticCode::texture_fallback,
                  SceneLoadDiagnosticCode::texture_fallback},
        std::pair{ModelLoadDiagnosticCode::skipped_unsupported_primitive,
                  SceneLoadDiagnosticCode::skipped_unsupported_primitive},
        std::pair{ModelLoadDiagnosticCode::metadata_not_preserved,
                  SceneLoadDiagnosticCode::metadata_not_preserved},
        std::pair{ModelLoadDiagnosticCode::repaired_signed_buffer_layout,
                  SceneLoadDiagnosticCode::repaired_signed_buffer_layout},
        std::pair{ModelLoadDiagnosticCode::ignored_vertex_attribute,
                  SceneLoadDiagnosticCode::ignored_vertex_attribute},
    };
    for (const auto& [model, scene] : codes) {
        if (code == model) {
            return scene;
        }
    }
    return SceneLoadDiagnosticCode::material_fallback;
}

[[nodiscard]] SceneLoadDiagnosticCode scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept
{
    return is_base_scene_diagnostic(code) ? base_scene_diagnostic_code(code)
                                          : extended_scene_diagnostic_code(code);
}

} // namespace

SceneLoadReport::SceneLoadReport() noexcept = default;

SceneLoadReport::SceneLoadReport(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
{
}

SceneLoadReport::~SceneLoadReport() noexcept = default;

SceneLoadReport::SceneLoadReport(SceneLoadReport&&) noexcept = default;

SceneLoadReport& SceneLoadReport::operator=(SceneLoadReport&&) noexcept = default;

std::size_t SceneLoadReport::diagnostic_count() const noexcept
{
    return impl_ != nullptr ? impl_->diagnostics.size() : 0;
}

Result<SceneLoadDiagnosticView> SceneLoadReport::diagnostic(std::size_t index) const noexcept
{
    if (impl_ == nullptr || index >= impl_->diagnostics.size()) {
        return Error{ErrorCode::invalid_argument,
                     "The scene-load diagnostic index is out of range"};
    }

    const ModelLoadDiagnostic& diagnostic = impl_->diagnostics[index];
    std::optional<std::string_view> source_context;
    if (diagnostic.source_context.has_value()) {
        source_context = std::string_view{*diagnostic.source_context};
    }
    return SceneLoadDiagnosticView{scene_diagnostic_severity(diagnostic.severity),
                                   scene_diagnostic_category(diagnostic.category),
                                   scene_diagnostic_code(diagnostic.code),
                                   std::string_view{diagnostic.message}, source_context};
}

bool SceneLoadReport::has_warnings() const noexcept
{
    if (impl_ == nullptr) {
        return false;
    }
    for (const ModelLoadDiagnostic& diagnostic : impl_->diagnostics) {
        if (diagnostic.severity == ModelLoadDiagnosticSeverity::warning) {
            return true;
        }
    }
    return false;
}

class Engine::Impl final {
  public:
    struct SceneReleaseTicket final {
        std::weak_ptr<renderer::Renderer> renderer;
        std::weak_ptr<picking::PickingService> picking;
    };

    Impl() noexcept : engine_token(allocate_engine_owner_token())
    {
    }

    [[nodiscard]] static Scene::ReleaseContext
    scene_release_context(SceneReleaseTicket& ticket) noexcept
    {
        return {reinterpret_cast<std::uintptr_t>(&ticket), &Impl::release_scene};
    }

    static void release_scene(std::uintptr_t context, SceneId scene) noexcept
    {
        std::unique_ptr<SceneReleaseTicket> ticket{reinterpret_cast<SceneReleaseTicket*>(context)};
        if (ticket == nullptr) {
            return;
        }
        if (const std::shared_ptr<renderer::Renderer> renderer = ticket->renderer.lock();
            renderer != nullptr) {
            renderer->release_scene(scene);
        }
        if (const std::shared_ptr<picking::PickingService> picking = ticket->picking.lock();
            picking != nullptr) {
            picking->release_scene(scene);
        }
    }

    std::uint64_t engine_token = 0;
    std::shared_ptr<renderer::Renderer> renderer;
    std::shared_ptr<picking::PickingService> picking;
    std::uint64_t next_scene_value = 1;
};

Engine::Engine(ConstructionKey, std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl))
{
}

Engine::~Engine() noexcept = default;

Result<std::unique_ptr<Engine>>
detail::EngineAccess::create(const detail::EngineCreateOptions& options) noexcept
{
    try {
        Result<std::unique_ptr<graphics::Device>> device_result = backend::opengl::create_device(
            backend::opengl::DeviceOptions{options.load_opengl_procedure});

        if (!device_result) {
            return device_result.error();
        }

        auto impl = std::make_unique<Engine::Impl>();
        Result<std::unique_ptr<renderer::Renderer>> renderer_result =
            renderer::Renderer::create(std::move(device_result).value(), impl->engine_token,
                                       detail::create_studio_environment_source());
        if (!renderer_result) {
            return renderer_result.error();
        }
        impl->renderer = std::shared_ptr<renderer::Renderer>{std::move(renderer_result).value()};
        impl->picking = std::make_shared<picking::PickingService>();
        return std::make_unique<Engine>(Engine::ConstructionKey{}, std::move(impl));
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

GraphicsBackend Engine::graphics_backend() const noexcept
{
    return impl_ != nullptr ? GraphicsBackend::opengl : GraphicsBackend::none;
}

Result<std::unique_ptr<Viewport>> Engine::create_viewport(Extent2D initial_extent) noexcept
{
    if (impl_ == nullptr || impl_->renderer == nullptr || impl_->picking == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport creation requires an initialized graphics backend"};
    }

    try {
        Result<std::unique_ptr<viewport::OffscreenViewport>> viewport_result =
            viewport::OffscreenViewport::create(impl_->renderer->device(), initial_extent);
        if (!viewport_result) {
            return viewport_result.error();
        }

        auto viewport_impl = std::make_unique<Viewport::Impl>(std::move(viewport_result).value(),
                                                              impl_->renderer, impl_->picking);
        return std::make_unique<Viewport>(Viewport::ConstructionKey{}, std::move(viewport_impl));
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<std::unique_ptr<Scene>> Engine::create_scene() noexcept
{
    if (impl_ == nullptr || impl_->next_scene_value == 0) {
        return Error{ErrorCode::invalid_argument, "Scene creation requires a live Elf3D engine"};
    }

    try {
        const std::uint64_t scene_value = impl_->next_scene_value++;
        auto release_ticket = std::make_unique<Impl::SceneReleaseTicket>();
        release_ticket->renderer = impl_->renderer;
        release_ticket->picking = impl_->picking;
        Scene::ReleaseContext release_context = Impl::scene_release_context(*release_ticket);
        Result<std::unique_ptr<Scene>> scene =
            Scene::create(impl_->engine_token, scene_value, std::move(release_context));
        if (!scene) {
            return scene.error();
        }

        release_ticket.release();
        return scene;
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<PreparedSceneDocument> Engine::create_scene_preparation() noexcept
{
    if (impl_ == nullptr || impl_->next_scene_value == 0) {
        return Error{ErrorCode::invalid_argument, "Scene preparation requires a live Elf3D engine"};
    }
    return PreparedSceneDocument::reserve(impl_->engine_token, impl_->next_scene_value++);
}

Result<std::unique_ptr<Scene>>
Engine::install_prepared_document(PreparedSceneDocument&& prepared) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<std::unique_ptr<Scene>> {
        if (impl_ == nullptr || !prepared.ready() ||
            !prepared.belongs_to_engine(impl_->engine_token)) {
            return Error{ErrorCode::invalid_argument,
                         "Scene installation requires preparation reserved by this live Engine"};
        }
        auto release_ticket = std::make_unique<Impl::SceneReleaseTicket>();
        release_ticket->renderer = impl_->renderer;
        release_ticket->picking = impl_->picking;
        auto scene =
            Scene::create(std::move(prepared), Impl::scene_release_context(*release_ticket));
        if (!scene) {
            return scene.error();
        }
        release_ticket.release();
        return scene;
    });
}

Result<std::unique_ptr<Scene>>
Engine::create_scene_from_document(Document&& document, DocumentSceneId selected_scene) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<std::unique_ptr<Scene>> {
        auto prepared = create_scene_preparation();
        if (!prepared) {
            return prepared.error();
        }
        const auto populated = prepared.value().prepare(std::move(document), selected_scene);
        if (!populated) {
            return populated.error();
        }
        auto scene = install_prepared_document(std::move(prepared).value());
        if (!scene && prepared.value().ready()) {
            // Preserve the synchronous wrapper's exact-owner recovery contract.
            document = std::move(prepared.value().take_document()).value();
        }
        return scene;
    });
}

Result<SceneGraphicsPreparationReport>
Engine::advance_scene_graphics(const Scene& scene, const GraphicsPreparationBudget& budget) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<SceneGraphicsPreparationReport> {
        const auto* storage = scene::Access::storage(scene);
        if (!impl_ || !impl_->renderer || !impl_->picking || !storage)
            return Error{ErrorCode::graphics_shutdown,
                         "Scene graphics preparation requires live Engine and Scene owners"};
        if (!storage->belongs_to_engine(impl_->engine_token))
            return Error{ErrorCode::foreign_engine_object,
                         "Graphics preparation requires this Engine's Scene"};
        return detail::advance_scene_graphics(*impl_->renderer, *impl_->picking, *storage, budget);
    });
}
Result<GraphicsRetirementReport>
Engine::advance_graphics_retirement(const GraphicsRetirementBudget& budget) noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<GraphicsRetirementReport> {
        if (!impl_ || !impl_->renderer || !impl_->picking)
            return Error{ErrorCode::graphics_shutdown, "Retirement requires a live Engine"};
        return detail::advance_graphics_retirement(*impl_->renderer, *impl_->picking, budget);
    });
}
Result<GraphicsRetirementStatus> Engine::graphics_retirement_status() const noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<GraphicsRetirementStatus> {
        if (!impl_ || !impl_->renderer || !impl_->picking)
            return Error{ErrorCode::graphics_shutdown, "Retirement status requires a live Engine"};
        return detail::graphics_retirement_status(*impl_->renderer, *impl_->picking);
    });
}
Result<SceneGraphicsPreparationStatus>
Engine::scene_graphics_status(const Scene& scene) const noexcept
{
    return detail::fatal_exception_boundary([&]() -> Result<SceneGraphicsPreparationStatus> {
        const auto* storage = scene::Access::storage(scene);
        if (!impl_ || !impl_->renderer || !storage)
            return Error{ErrorCode::graphics_shutdown,
                         "Graphics status requires live Engine and Scene owners"};
        if (!storage->belongs_to_engine(impl_->engine_token))
            return Error{ErrorCode::foreign_engine_object,
                         "Graphics status requires this Engine's Scene"};
        return impl_->renderer->scene_resource_status(*storage);
    });
}

Result<LoadedScene> Engine::load_scene(std::string_view path_utf8,
                                       const ModelLoadOptions& options) noexcept
{
    try {
        auto prepared = create_scene_preparation();
        if (!prepared) {
            return prepared.error();
        }
        auto report = prepared.value().load(path_utf8, options);
        if (!report) {
            return report.error();
        }
        auto scene = install_prepared_document(std::move(prepared).value());
        if (!scene) {
            return scene.error();
        }
        return LoadedScene{std::move(scene).value(), std::move(report).value()};
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<detail::NativeTextureView>
detail::EngineAccess::native_texture_view(const Engine& engine, TextureHandle texture) noexcept
{
    if (engine.impl_ == nullptr || engine.impl_->renderer == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Native texture access requires an initialized graphics backend"};
    }

    try {
        Result<graphics::NativeTextureView> view =
            engine.impl_->renderer->device().native_texture_view(texture);
        if (!view) {
            return view.error();
        }

        const graphics::NativeTextureView& value = view.value();
        const detail::NativeGraphicsApi api = value.api == graphics::NativeGraphicsApi::opengl
                                                  ? detail::NativeGraphicsApi::opengl
                                                  : detail::NativeGraphicsApi::none;
        return detail::NativeTextureView{api, value.value, value.extent};
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

} // namespace elf3d
