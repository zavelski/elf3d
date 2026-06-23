#ifndef ELF3D_ASSETS_H
#define ELF3D_ASSETS_H

#include <elf3d/math/value_types.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace elf3d {

namespace detail {
class SceneHandleAccess;
}

class SceneId final {
  public:
    constexpr SceneId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return engine_token_ != 0 && value_ != 0;
    }

    // Stable only for diagnostics within the owning process.
    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const SceneId &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr SceneId(std::uintptr_t engine_token, std::uint64_t value) noexcept
        : engine_token_(engine_token), value_(value) {}

    std::uintptr_t engine_token_ = 0;
    std::uint64_t value_ = 0;
};

class EntityId final {
  public:
    constexpr EntityId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return scene_.is_valid() && value_ != 0;
    }

    // Stable only for diagnostics within the owning Scene lifetime.
    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const EntityId &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr EntityId(SceneId scene, std::uint64_t value) noexcept
        : scene_(scene), value_(value) {}

    SceneId scene_;
    std::uint64_t value_ = 0;
};

class MeshHandle final {
  public:
    constexpr MeshHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return scene_.is_valid() && value_ != 0;
    }

    // Stable only for diagnostics within the owning Scene lifetime.
    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const MeshHandle &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr MeshHandle(SceneId scene, std::uint64_t value) noexcept
        : scene_(scene), value_(value) {}

    SceneId scene_;
    std::uint64_t value_ = 0;
};

class MaterialHandle final {
  public:
    constexpr MaterialHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return scene_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const MaterialHandle &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr MaterialHandle(SceneId scene, std::uint64_t value) noexcept
        : scene_(scene), value_(value) {}

    SceneId scene_;
    std::uint64_t value_ = 0;
};

class ImageHandle final {
  public:
    constexpr ImageHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return scene_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const ImageHandle &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr ImageHandle(SceneId scene, std::uint64_t value) noexcept
        : scene_(scene), value_(value) {}

    SceneId scene_;
    std::uint64_t value_ = 0;
};

// Named separately from the graphics TextureHandle, which identifies a viewport output.
class TextureAssetHandle final {
  public:
    constexpr TextureAssetHandle() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return scene_.is_valid() && value_ != 0;
    }

    [[nodiscard]] constexpr std::uint64_t debug_value() const noexcept {
        return value_;
    }

    bool operator==(const TextureAssetHandle &) const = default;

  private:
    friend class detail::SceneHandleAccess;

    constexpr TextureAssetHandle(SceneId scene, std::uint64_t value) noexcept
        : scene_(scene), value_(value) {}

    SceneId scene_;
    std::uint64_t value_ = 0;
};

struct VertexPositionNormal {
    Float3 position;
    Float3 normal;

    bool operator==(const VertexPositionNormal &) const = default;
};

struct VertexPositionNormalTexCoord {
    Float3 position;
    Float3 normal;
    Float2 texcoord0;

    bool operator==(const VertexPositionNormalTexCoord &) const = default;
};

struct MeshDataView {
    // Scene::create_mesh copies both spans before returning and never retains
    // caller-owned storage.
    std::span<const VertexPositionNormal> vertices;
    std::span<const std::uint32_t> indices;
};

struct TexturedMeshDataView {
    // Scene::create_mesh copies both spans before returning and never retains
    // caller-owned storage.
    std::span<const VertexPositionNormalTexCoord> vertices;
    std::span<const std::uint32_t> indices;
};

enum class PixelFormat {
    rgba8_unorm,
};

struct ImageDescription {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    // Rows are tightly packed RGBA8 from the image's top row to bottom row.
    // Scene::create_image copies this span before returning.
    std::span<const std::byte> pixels;
};

enum class TextureWrap {
    repeat,
    mirrored_repeat,
    clamp_to_edge,
};

enum class TextureFilter {
    nearest,
    linear,
    nearest_mipmap_nearest,
    linear_mipmap_nearest,
    nearest_mipmap_linear,
    linear_mipmap_linear,
};

struct SamplerDescription {
    TextureWrap wrap_u = TextureWrap::repeat;
    TextureWrap wrap_v = TextureWrap::repeat;
    // glTF's default sampler uses LINEAR for both minification and magnification.
    TextureFilter min_filter = TextureFilter::linear;
    TextureFilter mag_filter = TextureFilter::linear;

    bool operator==(const SamplerDescription &) const = default;
};

struct TextureDescription {
    ImageHandle image;
    SamplerDescription sampler;

    bool operator==(const TextureDescription &) const = default;
};

struct MaterialDescription {
    Color4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
    bool double_sided = false;
    float metallic_factor = 1.0F;
    float roughness_factor = 1.0F;
    TextureAssetHandle base_color_texture;
    TextureAssetHandle metallic_roughness_texture;

    bool operator==(const MaterialDescription &) const = default;
};

struct ModelPrimitiveBinding {
    MeshHandle mesh;
    MaterialHandle material;

    bool operator==(const ModelPrimitiveBinding &) const = default;
};

} // namespace elf3d

#endif
