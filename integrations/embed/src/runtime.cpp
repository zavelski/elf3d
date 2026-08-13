#include <elf3d/core/assert.h>
#include <elf3d/embed/runtime.h>

#include <memory>
#include <new>
#include <utility>

#include "engine_access.h"

namespace elf3d {
namespace {

[[noreturn]] void fatal_embed_allocation_failure() noexcept {
    fatal_error("Elf3D embedding integration memory allocation failed");
}

[[noreturn]] void fatal_unexpected_embed_boundary_exception() noexcept {
    fatal_error("Elf3D embedding integration encountered an unexpected exception");
}

} // namespace

EmbeddedRuntime::EmbeddedRuntime(ConstructionKey, std::unique_ptr<Engine> engine) noexcept
    : engine_(std::move(engine)) {}

EmbeddedRuntime::~EmbeddedRuntime() noexcept = default;

Result<std::unique_ptr<EmbeddedRuntime>>
EmbeddedRuntime::create(const EmbeddedRuntimeOptions& options) noexcept {
    try {
        detail::EngineCreateOptions engine_options;
        engine_options.load_opengl_procedure = options.load_opengl_procedure;
        Result<std::unique_ptr<Engine>> engine = detail::EngineAccess::create(engine_options);
        if (!engine) {
            return engine.error();
        }
        return std::make_unique<EmbeddedRuntime>(ConstructionKey{}, std::move(engine).value());
    } catch (const std::bad_alloc&) {
        fatal_embed_allocation_failure();
    } catch (...) {
        fatal_unexpected_embed_boundary_exception();
    }
}

Engine& EmbeddedRuntime::engine() noexcept {
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

const Engine& EmbeddedRuntime::engine() const noexcept {
    ELF3D_ASSERT(engine_ != nullptr);
    return *engine_;
}

Result<NativeTextureView>
EmbeddedRuntime::native_texture_view(TextureHandle texture) const noexcept {
    if (engine_ == nullptr) {
        return Error{ErrorCode::graphics_shutdown,
                     "Native texture access requires a live embedded runtime"};
    }
    Result<detail::NativeTextureView> view =
        detail::EngineAccess::native_texture_view(*engine_, texture);
    if (!view) {
        return view.error();
    }
    const detail::NativeTextureView& value = view.value();
    const NativeGraphicsApi api = value.api == detail::NativeGraphicsApi::opengl
                                      ? NativeGraphicsApi::opengl
                                      : NativeGraphicsApi::none;
    return NativeTextureView{api, value.value, value.extent};
}

} // namespace elf3d
