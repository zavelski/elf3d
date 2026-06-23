#ifndef ELF3D_ASSETS_STORAGE_H
#define ELF3D_ASSETS_STORAGE_H

#include <elf3d/assets.h>
#include <elf3d/core/result.h>

#include <cstdint>
#include <vector>

namespace elf3d::assets {

struct MeshAsset {
    std::vector<VertexPositionNormalTexCoord> vertices;
    std::vector<std::uint32_t> indices;
    Bounds3 bounds;
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
    [[nodiscard]] Result<MeshHandle> create_mesh(const MeshDataView &data);
    [[nodiscard]] Result<MeshHandle> create_mesh(const TexturedMeshDataView &data);
    [[nodiscard]] Result<ImageHandle> create_image(const ImageDescription &description);
    [[nodiscard]] Result<TextureAssetHandle>
    create_texture(const TextureDescription &description);
    [[nodiscard]] Result<MaterialHandle> create_material(const MaterialDescription &description);
    [[nodiscard]] Result<void> set_material(MaterialHandle material,
                                            const MaterialDescription &description);
    [[nodiscard]] Result<const MeshAsset *> mesh(MeshHandle mesh) const noexcept;
    [[nodiscard]] Result<const MaterialAsset *> material(MaterialHandle material) const noexcept;
    [[nodiscard]] Result<const ImageAsset *> image(ImageHandle image) const noexcept;
    [[nodiscard]] Result<const TextureAsset *>
    texture(TextureAssetHandle texture) const noexcept;
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

#endif
