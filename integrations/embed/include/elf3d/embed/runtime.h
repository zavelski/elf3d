#ifndef ELF3D_EMBED_RUNTIME_H
#define ELF3D_EMBED_RUNTIME_H

#include <elf3d/core/result.h>
#include <elf3d/elf3d.h>

#include <cstdint>
#include <memory>

namespace elf3d {

using EmbeddedGraphicsProcedure = void (*)();
using EmbeddedGraphicsProcedureLoader = EmbeddedGraphicsProcedure (*)(const char* name) noexcept;

struct EmbeddedRuntimeOptions {
    // The host must keep a compatible OpenGL 4.1 core context current for
    // creation, rendering, native-texture access, and destruction.
    EmbeddedGraphicsProcedureLoader load_opengl_procedure = nullptr;
};

enum class NativeGraphicsApi {
    none,
    opengl,
};

// This view is non-owning. It remains valid only until the source viewport is
// resized or destroyed, and the host must never delete the native texture.
struct NativeTextureView {
    NativeGraphicsApi api = NativeGraphicsApi::none;
    std::uintptr_t value = 0;
    Extent2D extent;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return api != NativeGraphicsApi::none && value != 0 && extent.width != 0 &&
               extent.height != 0;
    }
};

class EmbeddedRuntime final {
  private:
    struct ConstructionKey final {};

  public:
    ~EmbeddedRuntime() noexcept;

    EmbeddedRuntime(const EmbeddedRuntime&) = delete;
    EmbeddedRuntime& operator=(const EmbeddedRuntime&) = delete;
    EmbeddedRuntime(EmbeddedRuntime&&) = delete;
    EmbeddedRuntime& operator=(EmbeddedRuntime&&) = delete;

    // The host owns its window, current graphics context, event loop, input,
    // presentation, and context teardown. The returned runtime and all objects
    // created by it must be destroyed before that external context.
    [[nodiscard]] static Result<std::unique_ptr<EmbeddedRuntime>>
    create(const EmbeddedRuntimeOptions& options) noexcept;

    [[nodiscard]] Engine& engine() noexcept;
    [[nodiscard]] const Engine& engine() const noexcept;

    // The returned view is operation-scoped and is invalidated when the source
    // viewport is resized or destroyed. The host must never delete it.
    [[nodiscard]] Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const noexcept;

    explicit EmbeddedRuntime(ConstructionKey, std::unique_ptr<Engine> engine) noexcept;

  private:
    std::unique_ptr<Engine> engine_;
};

} // namespace elf3d

#endif
