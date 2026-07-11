#ifndef ELF3D_ELF3D_H
#define ELF3D_ELF3D_H

#include <elf3d/clipping.h>
#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>
#include <elf3d/measurement.h>
#include <elf3d/navigation.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/scene_load.h>
#include <elf3d/selection.h>
#include <elf3d/viewport.h>

#include <memory>
#include <string_view>

namespace elf3d {

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
    class Impl;
    struct ConstructionKey final {};

  public:
    ~Engine() noexcept;

    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;

    Engine(Engine &&) = delete;
    Engine &operator=(Engine &&) = delete;

    [[nodiscard]] static Result<std::unique_ptr<Engine>>
    create(const EngineConfiguration &configuration) noexcept;

    [[nodiscard]] GraphicsBackend graphics_backend() const noexcept;
    [[nodiscard]] bool graphics_initialized() const noexcept;

    [[nodiscard]] Result<std::unique_ptr<Viewport>>
    create_viewport(Extent2D initial_extent) noexcept;
    // The Engine must outlive every Scene and Viewport created from it.
    [[nodiscard]] Result<std::unique_ptr<Scene>> create_scene() noexcept;
    // Loading is synchronous. The existing scene, if any, is not modified.
    [[nodiscard]] Result<std::unique_ptr<Scene>>
    load_scene(std::string_view path_utf8, const SceneLoadOptions &options = {}) noexcept;
    [[nodiscard]] Result<LoadedScene>
    load_scene_with_report(std::string_view path_utf8,
                           const SceneLoadOptions &options = {}) noexcept;

    // Native texture access is non-owning and requires the owning graphics
    // thread with a compatible host OpenGL context current.
    [[nodiscard]] Result<NativeTextureView> native_texture_view(TextureHandle texture) const
        noexcept;

    explicit Engine(ConstructionKey, std::unique_ptr<Impl> impl) noexcept;

  private:
    std::unique_ptr<Impl> impl_;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace elf3d

#endif
