#include <elf3d/elf3d.h>
#include <elf3d/model.h>

#include <memory>
#include <utility>

namespace elf3d_examples {

// Start on the Scene-owning thread. The owner contains metadata only and keeps
// no Scene borrow; retain it in application state between frames.
[[nodiscard]] elf3d::Result<elf3d::SceneHierarchySnapshot>
begin_runtime_browser(const elf3d::Scene& scene) noexcept
{
    return elf3d::SceneHierarchySnapshot::create(scene);
}

// Drive a bounded slice on that same thread, before submitting the Scene for
// rendering. A hierarchy/visibility change requires starting a new capture.
[[nodiscard]] elf3d::Result<bool> advance_runtime_browser(elf3d::SceneHierarchySnapshot& snapshot,
                                                          const elf3d::Scene& scene) noexcept
{
    return snapshot.advance(scene, 256);
}

// Reserve with Engine::create_scene_preparation() on the framework thread, then
// move that opaque owner to the application's exclusive CPU lane. This function
// creates no threads and uses no Engine/Scene/context observer.
[[nodiscard]] elf3d::Result<void> prepare_reserved_preview(elf3d::PreparedSceneDocument& prepared,
                                                           elf3d::Document& document,
                                                           elf3d::DocumentSceneId selected) noexcept
{
    return prepared.prepare(std::move(document), selected, {.additional_runtime_entities = 1});
}

// Back on the framework thread, bind the prepared storage without traversal.
[[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::Scene>>
install_reserved_preview(elf3d::Engine& engine, elf3d::PreparedSceneDocument& prepared) noexcept
{
    return engine.install_prepared_document(std::move(prepared));
}

// Call once during each framework update and submit this Scene only after ready.
// Geometry/material pixels are borrowed inside this call, without a retained
// source view or copy. Shared renderer pipelines/lighting have their own lifetime.
[[nodiscard]] elf3d::Result<elf3d::SceneGraphicsPreparationReport>
advance_preview_graphics(elf3d::Engine& engine, const elf3d::Scene& preview) noexcept
{
    // Ready includes the shared environment for nonempty Scenes. Its allocation
    // estimate is reported separately from model storage; failed shared owners
    // also drain through the normal context-thread retirement service.
    return engine.advance_scene_graphics(preview);
}

// Call after the framework has consumed this frame's Scene observers. Graphics
// and picking bindings detach here, on the context thread. Move the result to
// the application's CPU lane to release canonical/derived data without holding
// a live Scene or Engine reference there. Elf3D creates no background thread.
[[nodiscard]] elf3d::Result<elf3d::SceneCpuRetirement>
retire_preview_cpu_state(elf3d::Scene& preview) noexcept
{
    return preview.retire_cpu_state();
}

// Continue on each framework update, including while no model is accepted.
// Complete this service before final Engine/context destruction. Source CPU
// data can already have been released by the application's exclusive worker.
[[nodiscard]] elf3d::Result<elf3d::GraphicsRetirementReport>
advance_preview_retirement(elf3d::Engine& engine) noexcept
{
    return engine.advance_graphics_retirement();
}

// Call on the framework thread. An error preserves candidate, including its
// owner-scoped IDs. Success transfers all canonical storage without a clone.
// The application must also prepare/render the new preview before committing
// GUI history; GPU uploads occur lazily after this CPU construction step.
[[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::Scene>>
prepare_document_preview(elf3d::Engine& engine, elf3d::Document& candidate,
                         elf3d::DocumentSceneId selected_scene) noexcept
{
    return engine.create_scene_from_document(std::move(candidate), selected_scene);
}

// Keep history as CPU data when no comparison preview needs it. Taking the
// Document retires this Scene and invalidates its runtime handles; release or
// rebind dependent views before continuing. The returned owner survives Engine
// shutdown, but no borrowed Scene::document() view may be retained across take.
[[nodiscard]] elf3d::Result<elf3d::Document> detach_preview(elf3d::Scene& preview) noexcept
{
    return preview.take_document();
}

} // namespace elf3d_examples
