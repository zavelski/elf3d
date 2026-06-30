#include <elf3d/elf3d.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>

import elf.backend.opengl;
import elf.core;
import elf.gltf;
import elf.graphics;
import elf.picking;
import elf.renderer;
import elf.scene;
import elf.viewport;

namespace elf3d {

class Engine::Impl final {
  public:
    class SceneReleaseState final {
      public:
        std::weak_ptr<renderer::Renderer> renderer;
        std::weak_ptr<picking::PickingService> picking;
    };

    Impl()
        : scene_release_state(std::make_shared<SceneReleaseState>()),
          scene_release_context(std::make_shared<Scene::ReleaseContext>(
              std::weak_ptr<void>{scene_release_state}, &Impl::release_scene)) {}

    Impl(GraphicsBackend backend, std::shared_ptr<graphics::Device> device) : Impl() {
        this->backend = backend;
        this->device = std::move(device);
    }

    static void release_scene(const std::shared_ptr<void> &context, SceneId scene) noexcept {
        const std::shared_ptr<SceneReleaseState> state =
            std::static_pointer_cast<SceneReleaseState>(context);
        if (state == nullptr) {
            return;
        }
        const std::shared_ptr<renderer::Renderer> renderer = state->renderer.lock();
        if (renderer != nullptr) {
            renderer->release_scene(scene);
        }
        const std::shared_ptr<picking::PickingService> picking = state->picking.lock();
        if (picking != nullptr) {
            picking->release_scene(scene);
        }
    }

    GraphicsBackend backend = GraphicsBackend::opengl;
    std::shared_ptr<graphics::Device> device;
    std::shared_ptr<renderer::Renderer> renderer;
    std::shared_ptr<picking::PickingService> picking;
    std::shared_ptr<SceneReleaseState> scene_release_state;
    std::shared_ptr<Scene::ReleaseContext> scene_release_context;
    std::uint64_t next_scene_value = 1;
};

class Viewport::Impl final {
  public:
    explicit Impl(std::unique_ptr<viewport::OffscreenViewport> viewport) noexcept
        : viewport(std::move(viewport)) {}

    std::unique_ptr<viewport::OffscreenViewport> viewport;
};

Version version() noexcept {
    const core::VersionData current = core::version_data();
    return Version{current.major, current.minor, current.patch};
}

const char *version_string() noexcept {
    return core::version_string();
}

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::Engine(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Engine::~Engine() = default;

Engine::Engine(Engine &&) noexcept = default;

Engine &Engine::operator=(Engine &&) noexcept = default;

Result<std::unique_ptr<Engine>> Engine::create(const EngineConfiguration &configuration) noexcept {
    try {
        Result<std::shared_ptr<graphics::Device>> device_result =
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

        auto impl = std::make_unique<Impl>(configuration.graphics_backend,
                                           std::move(device_result).value());
        Result<std::shared_ptr<renderer::Renderer>> renderer_result =
            renderer::Renderer::create(impl->device, reinterpret_cast<std::uintptr_t>(impl.get()));
        if (!renderer_result) {
            return renderer_result.error();
        }
        impl->renderer = std::move(renderer_result).value();
        impl->picking = std::make_shared<picking::PickingService>();
        impl->scene_release_state->renderer = impl->renderer;
        impl->scene_release_state->picking = impl->picking;
        return std::unique_ptr<Engine>{new Engine{std::move(impl)}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Elf3D engine creation threw an exception"};
    }
}

GraphicsBackend Engine::graphics_backend() const noexcept {
    return impl_ != nullptr ? impl_->backend : GraphicsBackend::opengl;
}

bool Engine::graphics_initialized() const noexcept {
    return impl_ != nullptr && impl_->device != nullptr;
}

Result<std::unique_ptr<Viewport>> Engine::create_viewport(Extent2D initial_extent) {
    if (impl_ == nullptr || impl_->device == nullptr || impl_->renderer == nullptr ||
        impl_->picking == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Viewport creation requires an initialized graphics backend"};
    }

    try {
        Result<std::unique_ptr<viewport::OffscreenViewport>> viewport_result =
            viewport::OffscreenViewport::create(impl_->device, impl_->renderer, impl_->picking,
                                                initial_extent);
        if (!viewport_result) {
            return viewport_result.error();
        }

        auto viewport_impl = std::make_unique<Viewport::Impl>(std::move(viewport_result).value());
        return std::unique_ptr<Viewport>{new Viewport{std::move(viewport_impl)}};
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Elf3D viewport facade creation threw an exception"};
    }
}

Result<std::unique_ptr<Scene>> Engine::create_scene() {
    if (impl_ == nullptr || impl_->next_scene_value == 0) {
        return Error{ErrorCode::invalid_argument, "Scene creation requires a live Elf3D engine"};
    }

    const std::uint64_t scene_value = impl_->next_scene_value++;
    return Scene::create(reinterpret_cast<std::uintptr_t>(impl_.get()), scene_value,
                         impl_->scene_release_context);
}

Result<std::unique_ptr<Scene>> Engine::load_scene(const std::filesystem::path &path,
                                                  const SceneLoadOptions &options) {
    Result<LoadedScene> loaded_result = load_scene_with_report(path, options);
    if (!loaded_result) {
        return loaded_result.error();
    }
    LoadedScene loaded = std::move(loaded_result).value();
    for (const SceneLoadDiagnostic &diagnostic : loaded.report.diagnostics) {
        if (diagnostic.severity != SceneLoadDiagnosticSeverity::warning) {
            continue;
        }
        std::clog << "Elf3D scene import warning: " << diagnostic.message;
        if (!diagnostic.source_context.empty()) {
            std::clog << " [" << diagnostic.source_context << ']';
        }
        std::clog << '\n';
    }
    return std::move(loaded.scene);
}

Result<LoadedScene> Engine::load_scene_with_report(const std::filesystem::path &path,
                                                   const SceneLoadOptions &options) {
    Result<std::unique_ptr<Scene>> scene_result = create_scene();
    if (!scene_result) {
        return scene_result.error();
    }
    std::unique_ptr<Scene> scene = std::move(scene_result).value();
    scene::Storage *storage = scene::Access::storage(*scene);
    if (storage == nullptr) {
        return Error{ErrorCode::scene_import_failed,
                     "Scene loading could not access the new scene construction surface"};
    }

    scene::ImportBuilder builder{*storage};
    const Result<gltf::ImportReport> import_result = gltf::import_scene(path, options, builder);
    if (!import_result) {
        return import_result.error();
    }
    return LoadedScene{std::move(scene), SceneLoadReport{import_result.value().diagnostics}};
}

Result<NativeTextureView> Engine::native_texture_view(TextureHandle texture) const {
    if (impl_ == nullptr || impl_->device == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Native texture access requires an initialized graphics backend"};
    }

    try {
        return impl_->device->native_texture_view(texture);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Native texture access threw an exception"};
    }
}

Viewport::Viewport(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Viewport::~Viewport() = default;

Viewport::Viewport(Viewport &&) noexcept = default;

Viewport &Viewport::operator=(Viewport &&) noexcept = default;

Extent2D Viewport::extent() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->extent() : Extent2D{};
}

Result<void> Viewport::resize(Extent2D extent) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->resize(extent);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport resize threw an exception"};
    }
}

void Viewport::set_clear_color(Color4 color) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_clear_color(color);
    }
}

Color4 Viewport::clear_color() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clear_color()
                                                          : Color4{};
}

void Viewport::set_basic_lighting(const BasicLighting &lighting) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_basic_lighting(lighting);
    }
}

BasicLighting Viewport::basic_lighting() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->basic_lighting()
                                                          : BasicLighting{};
}

Result<void> Viewport::update_navigation(Scene &scene, EntityId camera,
                                         const ViewportInput &input) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport navigation requires a live scene"};
        }
        return impl_->viewport->update_navigation(*storage, camera, input);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport navigation update threw an exception"};
    }
}

Result<void> Viewport::set_examine_pivot(Scene &scene, EntityId camera, Float3 world_position) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport pivot update requires a live scene"};
        }
        return impl_->viewport->set_examine_pivot(*storage, camera, world_position);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport pivot update threw an exception"};
    }
}

Result<void> Viewport::fit_to_scene(Scene &scene, EntityId camera) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport fitting requires a live scene"};
        }
        return impl_->viewport->fit_to_scene(*storage, camera);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport fitting threw an exception"};
    }
}

Result<void> Viewport::reset_view(Scene &scene, EntityId camera) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport reset requires a live scene"};
        }
        return impl_->viewport->reset_view(*storage, camera);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport reset threw an exception"};
    }
}

Result<void> Viewport::synchronize_navigation(const Scene &scene, EntityId camera) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport navigation synchronization requires a live scene"};
        }
        return impl_->viewport->synchronize_navigation(*storage, camera);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport navigation synchronization threw an exception"};
    }
}

void Viewport::cancel_interaction() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->cancel_interaction();
    }
}

void Viewport::set_navigation_enabled(bool enabled) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_navigation_enabled(enabled);
    }
}

bool Viewport::navigation_enabled() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->navigation_enabled();
}

Result<void> Viewport::set_navigation_settings(const OrbitNavigationSettings &settings) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_navigation_settings(settings);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport navigation settings update threw an exception"};
    }
}

OrbitNavigationSettings Viewport::navigation_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->navigation_settings()
                                                          : OrbitNavigationSettings{};
}

NavigationSnapshot Viewport::navigation_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->navigation_snapshot()
                                                          : NavigationSnapshot{};
}

void Viewport::set_active_tool(ViewportTool tool) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_active_tool(tool);
    }
}

ViewportTool Viewport::active_tool() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->active_tool()
                                                          : ViewportTool::selection;
}

Result<Ray3> Viewport::make_picking_ray(const Scene &scene, EntityId camera,
                                        Float2 position_pixels) const {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport picking requires a live scene"};
        }
        return impl_->viewport->make_picking_ray(*storage, camera, position_pixels);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport picking ray construction threw an exception"};
    }
}

Result<std::optional<PickHit>> Viewport::pick(const Scene &scene, EntityId camera,
                                              Float2 position_pixels,
                                              const PickOptions &options) const {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport picking requires a live scene"};
        }
        return impl_->viewport->pick(*storage, camera, position_pixels, options);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport picking threw an exception"};
    }
}

Result<std::optional<PickHit>> Viewport::select_at(const Scene &scene, EntityId camera,
                                                   Float2 position_pixels) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport selection requires a live scene"};
        }
        return impl_->viewport->select_at(*storage, camera, position_pixels);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport selection threw an exception"};
    }
}

Result<void> Viewport::set_selected_entity(const Scene &scene, EntityId entity) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport selection requires a live scene"};
        }
        return impl_->viewport->set_selected_entity(*storage, entity);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport hierarchy selection threw an exception"};
    }
}

void Viewport::clear_selection() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_selection();
    }
}

bool Viewport::has_selection() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->has_selection();
}

std::optional<EntityId> Viewport::selected_entity() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selected_entity()
                                                          : std::nullopt;
}

std::optional<PickHit> Viewport::selection_hit() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_hit()
                                                          : std::nullopt;
}

SelectionSnapshot Viewport::selection_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_snapshot()
                                                          : SelectionSnapshot{};
}

void Viewport::set_selection_enabled(bool enabled) noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->set_selection_enabled(enabled);
    }
}

bool Viewport::selection_enabled() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->selection_enabled();
}

Result<void> Viewport::set_selection_settings(const SelectionSettings &settings) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_selection_settings(settings);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport selection settings update threw an exception"};
    }
}

SelectionSettings Viewport::selection_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->selection_settings()
                                                          : SelectionSettings{};
}

PickingStatistics Viewport::picking_statistics() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->picking_statistics()
                                                          : PickingStatistics{};
}

Result<void> Viewport::begin_distance_measurement() {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->begin_distance_measurement();
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport measurement activation threw an exception"};
    }
}

void Viewport::cancel_distance_measurement() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->cancel_distance_measurement();
    }
}

void Viewport::clear_distance_measurement() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_distance_measurement();
    }
}

DistanceMeasurementSnapshot Viewport::distance_measurement_snapshot(const Scene &scene) const {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        DistanceMeasurementSnapshot result;
        result.diagnostic =
            Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
        return result;
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            DistanceMeasurementSnapshot result;
            result.diagnostic =
                Error{ErrorCode::invalid_argument, "Viewport measurement requires a live scene"};
            return result;
        }
        return impl_->viewport->distance_measurement_snapshot(*storage);
    } catch (...) {
        DistanceMeasurementSnapshot result;
        result.diagnostic = Error{ErrorCode::unexpected_exception,
                                  "Viewport measurement snapshot threw an exception"};
        return result;
    }
}

Result<void> Viewport::set_measurement_settings(const DistanceMeasurementSettings &settings) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_measurement_settings(settings);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport measurement settings update threw an exception"};
    }
}

DistanceMeasurementSettings Viewport::measurement_settings() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->measurement_settings()
                                                          : DistanceMeasurementSettings{};
}

MeasurementStatistics Viewport::measurement_statistics() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr
               ? impl_->viewport->measurement_statistics()
               : MeasurementStatistics{};
}

Result<ProjectedViewportPoint> Viewport::project_world_to_viewport(const Scene &scene,
                                                                   EntityId camera,
                                                                   Float3 world_position) const {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport projection requires a live scene"};
        }
        return impl_->viewport->project_world_to_viewport(*storage, camera, world_position);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport projection threw an exception"};
    }
}

Result<void> Viewport::isolate_entity(const Scene &scene, EntityId entity) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport isolation requires a live scene"};
        }
        return impl_->viewport->isolate_entity(*storage, entity);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport isolation threw an exception"};
    }
}

void Viewport::clear_isolation() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_isolation();
    }
}

bool Viewport::is_isolating() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->is_isolating();
}

std::optional<EntityId> Viewport::isolated_entity() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->isolated_entity()
                                                          : std::nullopt;
}

Result<void> Viewport::hide_selected(Scene &scene) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport hide-selected requires a live scene"};
        }
        return impl_->viewport->hide_selected(*storage);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport hide-selected command threw an exception"};
    }
}

Result<void> Viewport::show_selected(Scene &scene) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport show-selected requires a live scene"};
        }
        return impl_->viewport->show_selected(*storage);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport show-selected command threw an exception"};
    }
}

Result<void> Viewport::isolate_selected(const Scene &scene) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport isolate-selected requires a live scene"};
        }
        return impl_->viewport->isolate_selected(*storage);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport isolate-selected command threw an exception"};
    }
}

Result<Bounds3> Viewport::visible_bounds(const Scene &scene) const {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport visible-bounds query requires a live scene"};
        }
        return impl_->viewport->visible_bounds(*storage);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport visible-bounds query threw an exception"};
    }
}

Result<void> Viewport::set_section_plane(const SectionPlane &plane) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_section_plane(plane);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport section-plane update threw an exception"};
    }
}

void Viewport::clear_section_plane() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_section_plane();
    }
}

Result<std::uint32_t> Viewport::add_clipping_box(const ClippingBox &box) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->add_clipping_box(box);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport clipping-box creation threw an exception"};
    }
}

Result<void> Viewport::set_clipping_box(std::uint32_t index, const ClippingBox &box) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->set_clipping_box(index, box);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport clipping-box update threw an exception"};
    }
}

Result<void> Viewport::remove_clipping_box(std::uint32_t index) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        return impl_->viewport->remove_clipping_box(index);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport clipping-box removal threw an exception"};
    }
}

void Viewport::clear_clipping_boxes() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_clipping_boxes();
    }
}

void Viewport::clear_clipping() noexcept {
    if (impl_ != nullptr && impl_->viewport != nullptr) {
        impl_->viewport->clear_clipping();
    }
}

Result<void> Viewport::set_clipping_helpers_visible(bool visible) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    return impl_->viewport->set_clipping_helpers_visible(visible);
}

Result<void>
Viewport::set_clipping_helper_settings(const ClippingHelperSettings &settings) noexcept {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }
    return impl_->viewport->set_clipping_helper_settings(settings);
}

Result<void> Viewport::reset_clipping_box_to_visible_bounds(const Scene &scene,
                                                            std::uint32_t index) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport clipping box reset requires a live scene"};
        }
        return impl_->viewport->reset_clipping_box_to_visible_bounds(*storage, index);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport clipping box reset threw an exception"};
    }
}

Result<std::uint32_t> Viewport::add_clipping_box_from_visible_bounds(const Scene &scene) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument,
                         "Viewport clipping box creation requires a live scene"};
        }
        return impl_->viewport->add_clipping_box_from_visible_bounds(*storage);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception,
                     "Viewport clipping box creation threw an exception"};
    }
}

ClippingSnapshot Viewport::clipping_snapshot() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->clipping_snapshot()
                                                          : ClippingSnapshot{};
}

Result<void> Viewport::render(const Scene &scene, EntityId camera) {
    if (impl_ == nullptr || impl_->viewport == nullptr) {
        return Error{ErrorCode::graphics_shutdown, "The viewport has no graphics resources"};
    }

    try {
        const scene::Storage *storage = scene::Access::storage(scene);
        if (storage == nullptr) {
            return Error{ErrorCode::invalid_argument, "Viewport rendering requires a live scene"};
        }
        return impl_->viewport->render(*storage, camera);
    } catch (...) {
        return Error{ErrorCode::unexpected_exception, "Viewport rendering threw an exception"};
    }
}

RenderStatistics Viewport::statistics() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->statistics()
                                                          : RenderStatistics{};
}

TextureHandle Viewport::color_texture() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr ? impl_->viewport->color_texture()
                                                          : TextureHandle{};
}

bool Viewport::framebuffer_valid() const noexcept {
    return impl_ != nullptr && impl_->viewport != nullptr && impl_->viewport->framebuffer_valid();
}

} // namespace elf3d
