#ifndef ELF3D_ASSETS_HANDLE_ACCESS_H
#define ELF3D_ASSETS_HANDLE_ACCESS_H

#include <elf3d/assets.h>

namespace elf3d::detail {

class SceneHandleAccess final {
  public:
    [[nodiscard]] static constexpr SceneId create_scene(std::uintptr_t engine_token,
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

    [[nodiscard]] static constexpr TextureAssetHandle
    create_texture(SceneId scene, std::uint64_t value) noexcept {
        return TextureAssetHandle{scene, value};
    }

    [[nodiscard]] static constexpr std::uintptr_t engine_token(SceneId scene) noexcept {
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

#endif
