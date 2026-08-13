#ifndef ELF3D_ENGINE_ACCESS_H
#define ELF3D_ENGINE_ACCESS_H

#include <elf3d/core/api.h>
#include <elf3d/core/result.h>
#include <elf3d/elf3d.h>

#include <cstdint>
#include <memory>

namespace elf3d::detail {

using GraphicsProcedure = void (*)();
using GraphicsProcedureLoader = GraphicsProcedure (*)(const char* name) noexcept;

struct EngineCreateOptions {
    GraphicsProcedureLoader load_opengl_procedure = nullptr;
};

enum class NativeGraphicsApi {
    none,
    opengl,
};

struct NativeTextureView {
    NativeGraphicsApi api = NativeGraphicsApi::none;
    std::uintptr_t value = 0;
    Extent2D extent;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return api != NativeGraphicsApi::none && value != 0 && extent.width != 0 &&
               extent.height != 0;
    }
};

// Private cross-target construction and presentation boundary. This header is
// not installed and is available only to the standard and embedding owners.
class ELF3D_API EngineAccess final {
  public:
    [[nodiscard]] static Result<std::unique_ptr<Engine>>
    create(const EngineCreateOptions& options) noexcept;

    [[nodiscard]] static Result<NativeTextureView>
    native_texture_view(const Engine& engine, TextureHandle texture) noexcept;
};

} // namespace elf3d::detail

#endif
