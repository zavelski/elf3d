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

enum class OverlayDepthMode {
    depth_tested,
    always_visible,
};

struct OverlayLineSegment {
    Float3 start_world;
    Float3 end_world;
    Color4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float thickness_pixels = 1.0F;
    OverlayDepthMode depth_mode = OverlayDepthMode::always_visible;
};

struct OverlayPointMarker {
    Float3 position_world;
    Color4 color{1.0F, 1.0F, 1.0F, 1.0F};
    float radius_pixels = 4.0F;
    OverlayDepthMode depth_mode = OverlayDepthMode::always_visible;
};

class TextureHandle final {
  public:
    constexpr TextureHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != 0;
    }

    bool operator==(const TextureHandle&) const = default;

  private:
    friend class detail::TextureHandleAccess;

    explicit constexpr TextureHandle(std::uint64_t value) noexcept : value_(value) {}

    std::uint64_t value_ = 0;
};

} // namespace elf3d

#endif
