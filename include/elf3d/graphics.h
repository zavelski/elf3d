#ifndef ELF3D_GRAPHICS_H
#define ELF3D_GRAPHICS_H

#include <elf3d/math/value_types.h>

#include <cstdint>

namespace elf3d {

namespace detail {
class TextureHandleAccess;
}

enum class GraphicsBackend {
    none,
    opengl,
};

using GraphicsProcedure = void (*)();
using GraphicsProcedureLoader = GraphicsProcedure (*)(const char *name) noexcept;

struct OpenGLConfiguration {
    // The host must make its OpenGL context current before Engine::create.
    GraphicsProcedureLoader load_procedure = nullptr;
};

struct EngineConfiguration {
    GraphicsBackend graphics_backend = GraphicsBackend::opengl;
    OpenGLConfiguration opengl;
};

class TextureHandle final {
  public:
    constexpr TextureHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != 0;
    }

    bool operator==(const TextureHandle &) const = default;

  private:
    friend class detail::TextureHandleAccess;

    explicit constexpr TextureHandle(std::uint64_t value) noexcept : value_(value) {}

    std::uint64_t value_ = 0;
};

enum class NativeGraphicsApi {
    none,
    opengl,
};

// This view is non-owning. The native value remains valid only until the source
// viewport is resized or destroyed. The host must never delete the texture.
struct NativeTextureView {
    NativeGraphicsApi api = NativeGraphicsApi::none;
    std::uintptr_t value = 0;
    Extent2D extent;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return api != NativeGraphicsApi::none && value != 0 && extent.width != 0 &&
               extent.height != 0;
    }
};

} // namespace elf3d

#endif
