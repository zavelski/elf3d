#ifndef ELF3D_ELF3D_H
#define ELF3D_ELF3D_H

#include <elf3d/clipping.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/graphics_preparation.h>
#include <elf3d/model_write.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/projection.h>
#include <elf3d/rendering.h>
#include <elf3d/scene.h>
#include <elf3d/scene_load.h>
#include <elf3d/selection.h>
#include <elf3d/surface_anchor.h>
#include <elf3d/viewport.h>

#include <memory>
#include <string_view>

namespace elf3d {

namespace detail {
class EngineAccess;
}

struct LoadedScene {
    std::unique_ptr<Scene> scene;
    SceneLoadReport report;
};

#if defined(_MSC_VER)
#pragma warning(push)
// The exported special members keep unique_ptr operations inside the DLL.
#pragma warning(disable : 4251)
#endif
class ELF3D_API Engine {
  private:
    friend class detail::EngineAccess;

    class Impl;
    struct ConstructionKey final {};

  public:
    ~Engine() noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    explicit Engine(ConstructionKey, std::unique_ptr<Impl> impl) noexcept;

    [[nodiscard]] GraphicsBackend graphics_backend() const noexcept;

    [[nodiscard]] Result<std::unique_ptr<Viewport>>
    create_viewport(Extent2D initial_extent) noexcept;
    // The Engine must outlive every Scene and Viewport created from it.
    [[nodiscard]] Result<std::unique_ptr<Scene>> create_scene() noexcept;
    // Reserves only a future identity on the Engine/context thread. The returned
    // CPU-only owner retains no Engine reference and may be prepared/destroyed
    // on another exclusive CPU thread while the producing libraries are loaded.
    [[nodiscard]] Result<PreparedSceneDocument> create_scene_preparation() noexcept;
    // Context-thread, constant-size ownership binding of an already prepared
    // Document/runtime payload. Performs no geometry/name/entity traversal and
    // no graphics upload. Only the reserving Engine accepts it. Success consumes
    // the preparation; an error preserves it. All borrowed views expire here.
    [[nodiscard]] Result<std::unique_ptr<Scene>>
    install_prepared_document(PreparedSceneDocument&& prepared) noexcept;
    // Explicit context-thread preparation of this Scene's geometry and material
    // textures. Borrows expire per call. Keep its render content unchanged until
    // completion. After later edits, call again to refresh readiness. An admitted
    // incomplete/stale Scene cannot render or pick. Shared programs and built-in
    // lighting participate in readiness; failures retain value-only diagnostic context.
    [[nodiscard]] Result<SceneGraphicsPreparationReport>
    advance_scene_graphics(const Scene& scene,
                           const GraphicsPreparationBudget& budget = {}) noexcept;
    // Constant-size query without source traversal, allocation or native work.
    [[nodiscard]] Result<SceneGraphicsPreparationStatus>
    scene_graphics_status(const Scene& scene) const noexcept;
    // Context-thread incremental release of detached graphics and picking caches.
    // Canonical CPU data belongs to SceneCpuRetirement, separately. No Scene
    // observer is retained. Insufficient budgets preserve pending owners.
    [[nodiscard]] Result<GraphicsRetirementReport>
    advance_graphics_retirement(const GraphicsRetirementBudget& budget = {}) noexcept;
    [[nodiscard]] Result<GraphicsRetirementStatus> graphics_retirement_status() const noexcept;
    // Synchronous ownership transfer on the framework thread; no canonical copy.
    // Selects one runtime scene while retaining the complete Document and its
    // unchanged IDs/default-scene state. Success moves from document; every
    // returned error leaves it unchanged. Old borrowed views expire on success.
    // GPU preparation remains lazy. Link the matching-source Model SDK when using
    // Model operations; both products require the same toolchain/shared CRT.
    [[nodiscard]] Result<std::unique_ptr<Scene>>
    create_scene_from_document(Document&& document, DocumentSceneId scene) noexcept;
    // Loading is synchronous. The existing scene, if any, is not modified.
    [[nodiscard]] Result<LoadedScene> load_scene(std::string_view path_utf8,
                                                 const ModelLoadOptions& options = {}) noexcept;

  private:
    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
