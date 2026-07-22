module;

#include <elf3d/assets.h>
#include <elf3d/core/result.h>

#include <cstdint>
#include <vector>

export module elf.assets;

import elf.core;
import elf.math;

export namespace elf3d::assets {

struct MeshAsset {
    std::vector<VertexPositionNormalTexCoord> vertices;
    std::vector<std::uint32_t> indices;
    Bounds3 bounds;
    bool has_full_vertex_attributes = false;
};

struct ImageAsset {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelFormat format = PixelFormat::rgba8_unorm;
    std::vector<std::byte> pixels;
};

struct TextureAsset {
    TextureDescription description;
};

struct MaterialAsset {
    MaterialDescription description;
};

class Storage final {
  public:
    explicit Storage(SceneId scene) noexcept;

    [[nodiscard]] SceneId scene_id() const noexcept;
    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView& data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView& data);
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription& description);
    [[nodiscard]] Result<TextureAssetHandle> create_texture(const TextureDescription& description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription& description);
    [[nodiscard]] Result<void> set_material(MaterialHandle material,
                                            const MaterialDescription& description);
    [[nodiscard]] Result<const MeshAsset*> mesh(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<const MaterialAsset*> material(MaterialHandle material) const noexcept;
    [[nodiscard]] Result<const ImageAsset*> image(ImageHandle image) const noexcept;
    [[nodiscard]] Result<const TextureAsset*> texture(TextureAssetHandle texture) const noexcept;
    [[nodiscard]] std::span<const MeshAsset> meshes() const noexcept;
    [[nodiscard]] std::span<const MaterialAsset> materials() const noexcept;
    [[nodiscard]] std::span<const ImageAsset> images() const noexcept;
    [[nodiscard]] std::span<const TextureAsset> textures() const noexcept;

  private:
    [[nodiscard]] bool owns(MeshHandle mesh) const noexcept;
    [[nodiscard]] bool owns(MaterialHandle material) const noexcept;
    [[nodiscard]] bool owns(ImageHandle image) const noexcept;
    [[nodiscard]] bool owns(TextureAssetHandle texture) const noexcept;

    SceneId scene_;
    std::vector<MeshAsset> meshes_;
    std::vector<MaterialAsset> materials_;
    std::vector<ImageAsset> images_;
    std::vector<TextureAsset> textures_;
};

} // namespace elf3d::assets

export namespace elf3d::detail {

class SceneHandleAccess final {
  public:
    [[nodiscard]] static constexpr SceneId create_scene(std::uint64_t engine_token,
                                                        std::uint64_t value) noexcept {
        return SceneId{engine_token, value};
    }

    [[nodiscard]] static constexpr EntityId create_entity(SceneId scene,
                                                          std::uint64_t value) noexcept {
        return EntityId{scene, value};
    }

    [[nodiscard]] static constexpr MeshHandle create_mesh(SceneId scene,
                                                          std::uint64_t value) noexcept {
        return MeshHandle{scene, value};
    }

    [[nodiscard]] static constexpr MaterialHandle create_material(SceneId scene,
                                                                  std::uint64_t value) noexcept {
        return MaterialHandle{scene, value};
    }

    [[nodiscard]] static constexpr ImageHandle create_image(SceneId scene,
                                                            std::uint64_t value) noexcept {
        return ImageHandle{scene, value};
    }

    [[nodiscard]] static constexpr TextureAssetHandle create_texture(SceneId scene,
                                                                     std::uint64_t value) noexcept {
        return TextureAssetHandle{scene, value};
    }

    [[nodiscard]] static constexpr std::uint64_t engine_token(SceneId scene) noexcept {
        return scene.engine_token_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(SceneId scene) noexcept {
        return scene.value_;
    }

    [[nodiscard]] static constexpr SceneId scene(EntityId entity) noexcept {
        return entity.scene_;
    }

    [[nodiscard]] static constexpr SceneId scene(MeshHandle mesh) noexcept {
        return mesh.scene_;
    }

    [[nodiscard]] static constexpr SceneId scene(MaterialHandle material) noexcept {
        return material.scene_;
    }

    [[nodiscard]] static constexpr SceneId scene(ImageHandle image) noexcept {
        return image.scene_;
    }

    [[nodiscard]] static constexpr SceneId scene(TextureAssetHandle texture) noexcept {
        return texture.scene_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(EntityId entity) noexcept {
        return entity.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MeshHandle mesh) noexcept {
        return mesh.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(MaterialHandle material) noexcept {
        return material.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(ImageHandle image) noexcept {
        return image.value_;
    }

    [[nodiscard]] static constexpr std::uint64_t value(TextureAssetHandle texture) noexcept {
        return texture.value_;
    }
};

} // namespace elf3d::detail
