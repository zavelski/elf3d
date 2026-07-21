#include <elf3d/elf3d.h>
#include <elf3d/model.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import elf.backend.opengl;
import elf.gltf;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

#include "viewport_impl.h"

namespace elf3d {
namespace {

[[noreturn]] void fatal_allocation_failure() noexcept {
    fatal_error("Elf3D memory allocation failed");
}

[[noreturn]] void fatal_unexpected_boundary_exception() noexcept {
    fatal_error("Elf3D boundary encountered an unexpected exception");
}

[[nodiscard]] std::uint64_t allocate_engine_owner_token() noexcept {
    static std::atomic<std::uint64_t> next_token{1};
    const std::uint64_t token = next_token.fetch_add(1, std::memory_order_relaxed);
    if (token == 0) {
        fatal_error("Elf3D exhausted engine owner identities");
    }
    return token;
}

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

[[nodiscard]] SceneLoadDiagnosticSeverity
scene_diagnostic_severity(ModelLoadDiagnosticSeverity severity) noexcept {
    switch (severity) {
    case ModelLoadDiagnosticSeverity::information:
        return SceneLoadDiagnosticSeverity::information;
    case ModelLoadDiagnosticSeverity::warning:
        return SceneLoadDiagnosticSeverity::warning;
    }
    return SceneLoadDiagnosticSeverity::warning;
}

[[nodiscard]] SceneLoadDiagnosticCategory
scene_diagnostic_category(ModelLoadDiagnosticCategory category) noexcept {
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

[[nodiscard]] bool is_base_scene_diagnostic(ModelLoadDiagnosticCode code) noexcept {
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
base_scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept {
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
extended_scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept {
    switch (code) {
    case ModelLoadDiagnosticCode::ignored_animation:
        return SceneLoadDiagnosticCode::ignored_animation;
    case ModelLoadDiagnosticCode::ignored_skin:
        return SceneLoadDiagnosticCode::ignored_skin;
    case ModelLoadDiagnosticCode::ignored_morph_targets:
        return SceneLoadDiagnosticCode::ignored_morph_targets;
    case ModelLoadDiagnosticCode::ignored_instancing:
        return SceneLoadDiagnosticCode::ignored_instancing;
    case ModelLoadDiagnosticCode::skipped_invalid_transform:
        return SceneLoadDiagnosticCode::skipped_invalid_transform;
    case ModelLoadDiagnosticCode::texture_fallback:
        return SceneLoadDiagnosticCode::texture_fallback;
    case ModelLoadDiagnosticCode::skipped_unsupported_primitive:
        return SceneLoadDiagnosticCode::skipped_unsupported_primitive;
    case ModelLoadDiagnosticCode::metadata_not_preserved:
        return SceneLoadDiagnosticCode::metadata_not_preserved;
    default:
        return SceneLoadDiagnosticCode::material_fallback;
    }
}

[[nodiscard]] SceneLoadDiagnosticCode scene_diagnostic_code(ModelLoadDiagnosticCode code) noexcept {
    return is_base_scene_diagnostic(code) ? base_scene_diagnostic_code(code)
                                          : extended_scene_diagnostic_code(code);
}

} // namespace

class SceneLoadReport::Impl final {
  public:
    std::vector<ModelLoadDiagnostic> diagnostics;
};

SceneLoadReport::SceneLoadReport() noexcept = default;

SceneLoadReport::SceneLoadReport(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

SceneLoadReport::~SceneLoadReport() noexcept = default;

SceneLoadReport::SceneLoadReport(SceneLoadReport&&) noexcept = default;

SceneLoadReport& SceneLoadReport::operator=(SceneLoadReport&&) noexcept = default;

std::size_t SceneLoadReport::diagnostic_count() const noexcept {
    return impl_ != nullptr ? impl_->diagnostics.size() : 0;
}

Result<SceneLoadDiagnosticView> SceneLoadReport::diagnostic(std::size_t index) const noexcept {
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

bool SceneLoadReport::has_warnings() const noexcept {
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

    Impl() noexcept : engine_token(allocate_engine_owner_token()) {}

    explicit Impl(GraphicsBackend backend) : Impl() {
        this->backend = backend;
    }

    static void release_scene(std::uintptr_t context, SceneId scene) noexcept {
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

    GraphicsBackend backend = GraphicsBackend::none;
    std::uint64_t engine_token = 0;
    std::shared_ptr<renderer::Renderer> renderer;
    std::shared_ptr<picking::PickingService> picking;
    std::uint64_t next_scene_value = 1;
};

Engine::Engine(ConstructionKey, std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Engine::~Engine() noexcept = default;

Result<std::unique_ptr<Engine>> Engine::create(const EngineConfiguration& configuration) noexcept {
    try {
        if (configuration.graphics_backend == GraphicsBackend::none) {
            auto impl = std::make_unique<Impl>(GraphicsBackend::none);
            return std::make_unique<Engine>(ConstructionKey{}, std::move(impl));
        }

        Result<std::unique_ptr<graphics::Device>> device_result =
            Error{ErrorCode::invalid_argument, "The requested graphics backend is unsupported"};

        switch (configuration.graphics_backend) {
        case GraphicsBackend::opengl:
            device_result = backend::opengl::create_device(configuration.opengl);
            break;
        default:
            return Error{ErrorCode::invalid_argument,
                         "The requested graphics backend is unsupported"};
        }

        if (!device_result) {
            return device_result.error();
        }

        auto impl = std::make_unique<Impl>(configuration.graphics_backend);
        Result<std::unique_ptr<renderer::Renderer>> renderer_result =
            renderer::Renderer::create(std::move(device_result).value(), impl->engine_token);
        if (!renderer_result) {
            return renderer_result.error();
        }
        impl->renderer = std::shared_ptr<renderer::Renderer>{std::move(renderer_result).value()};
        impl->picking = std::make_shared<picking::PickingService>();
        return std::make_unique<Engine>(ConstructionKey{}, std::move(impl));
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

GraphicsBackend Engine::graphics_backend() const noexcept {
    return impl_ != nullptr ? impl_->backend : GraphicsBackend::none;
}

bool Engine::graphics_initialized() const noexcept {
    return impl_ != nullptr && impl_->renderer != nullptr;
}

Result<std::unique_ptr<Viewport>> Engine::create_viewport(Extent2D initial_extent) noexcept {
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

Result<std::unique_ptr<Scene>> Engine::create_scene() noexcept {
    if (impl_ == nullptr || impl_->next_scene_value == 0) {
        return Error{ErrorCode::invalid_argument, "Scene creation requires a live Elf3D engine"};
    }

    try {
        const std::uint64_t scene_value = impl_->next_scene_value++;
        auto release_ticket = std::make_unique<Impl::SceneReleaseTicket>();
        release_ticket->renderer = impl_->renderer;
        release_ticket->picking = impl_->picking;
        Scene::ReleaseContext release_context{
            reinterpret_cast<std::uintptr_t>(release_ticket.get()), &Impl::release_scene};
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

Result<LoadedScene> Engine::load_scene(std::string_view path_utf8,
                                       const ModelLoadOptions& options) noexcept {
    try {
        Result<std::unique_ptr<Scene>> scene_result = create_scene();
        if (!scene_result) {
            return scene_result.error();
        }
        std::unique_ptr<Scene> scene = std::move(scene_result).value();
        scene::Storage* storage = scene::Access::storage(*scene);
        if (storage == nullptr) {
            return Error{ErrorCode::scene_import_failed,
                         "Scene loading could not access the new scene construction surface"};
        }

        Result<LoadedDocument> loaded_document =
            gltf::load_document(path_from_utf8(path_utf8), options);
        if (!loaded_document) {
            return loaded_document.error();
        }
        LoadedDocument loaded = std::move(loaded_document).value();
        const Result<void> populate_result = scene::populate_from_document(
            std::move(loaded.document), loaded.default_scene, *storage);
        if (!populate_result) {
            return populate_result.error();
        }
        auto report_impl = std::make_unique<SceneLoadReport::Impl>();
        report_impl->diagnostics = std::move(loaded.report.diagnostics);
        return LoadedScene{std::move(scene), SceneLoadReport{std::move(report_impl)}};
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

Result<NativeTextureView> Engine::native_texture_view(TextureHandle texture) const noexcept {
    if (impl_ == nullptr || impl_->renderer == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Native texture access requires an initialized graphics backend"};
    }

    try {
        return impl_->renderer->device().native_texture_view(texture);
    } catch (const std::bad_alloc&) {
        fatal_allocation_failure();
    } catch (...) {
        fatal_unexpected_boundary_exception();
    }
}

} // namespace elf3d
