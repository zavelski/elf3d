#include <elf3d/assets.h>
#include <elf3d/core/result.h>

#include <array>
#include <type_traits>
#include <vector>

import elf.assets;

int main() {
    static_assert(!std::is_convertible_v<elf3d::MeshHandle, elf3d::MaterialHandle>);
    static_assert(!std::is_convertible_v<elf3d::MaterialHandle, elf3d::MeshHandle>);

    const elf3d::SceneId scene_id = elf3d::detail::SceneHandleAccess::create_scene(1, 1);
    elf3d::assets::Storage storage{scene_id};
    const std::array<elf3d::VertexPositionNormal, 8> vertices{{
        {{-1.0F, -1.0F, -1.0F}, {-1.0F, -1.0F, -1.0F}},
        {{1.0F, -1.0F, -1.0F}, {1.0F, -1.0F, -1.0F}},
        {{1.0F, 1.0F, -1.0F}, {1.0F, 1.0F, -1.0F}},
        {{-1.0F, 1.0F, -1.0F}, {-1.0F, 1.0F, -1.0F}},
        {{-1.0F, -1.0F, 1.0F}, {-1.0F, -1.0F, 1.0F}},
        {{1.0F, -1.0F, 1.0F}, {1.0F, -1.0F, 1.0F}},
        {{1.0F, 1.0F, 1.0F}, {1.0F, 1.0F, 1.0F}},
        {{-1.0F, 1.0F, 1.0F}, {-1.0F, 1.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 36> indices{{
        4, 5, 6, 4, 6, 7, 1, 0, 3, 1, 3, 2, 5, 1, 2, 5, 2, 6,
        0, 4, 7, 0, 7, 3, 7, 6, 2, 7, 2, 3, 0, 1, 5, 0, 5, 4,
    }};

    const elf3d::Result<elf3d::MeshHandle> mesh =
        storage.create_mesh(elf3d::MeshDataView{vertices, indices});
    if (!mesh) {
        return 1;
    }
    const elf3d::Result<const elf3d::assets::MeshAsset *> asset = storage.mesh(mesh.value());
    if (!asset || asset.value()->bounds.minimum != elf3d::Float3{-1.0F, -1.0F, -1.0F} ||
        asset.value()->bounds.maximum != elf3d::Float3{1.0F, 1.0F, 1.0F}) {
        return 2;
    }

    if (storage.create_mesh(elf3d::MeshDataView{{}, indices}).error().code() !=
        elf3d::ErrorCode::invalid_mesh_data) {
        return 3;
    }
    const std::array<std::uint32_t, 2> incomplete{{0, 1}};
    if (storage.create_mesh(elf3d::MeshDataView{vertices, incomplete}).error().code() !=
        elf3d::ErrorCode::invalid_mesh_data) {
        return 4;
    }
    const std::array<std::uint32_t, 3> out_of_range{{0, 1, 8}};
    if (storage.create_mesh(elf3d::MeshDataView{vertices, out_of_range}).error().code() !=
        elf3d::ErrorCode::mesh_index_out_of_range) {
        return 5;
    }

    const elf3d::MaterialDescription description{elf3d::Color4{0.2F, 0.4F, 0.6F, 1.0F}};
    const elf3d::Result<elf3d::MaterialHandle> material = storage.create_material(description);
    if (!material || !storage.material(material.value()) ||
        storage.material(material.value()).value()->description != description) {
        return 6;
    }

    const elf3d::SceneId other_scene = elf3d::detail::SceneHandleAccess::create_scene(1, 2);
    const elf3d::MeshHandle foreign_mesh =
        elf3d::detail::SceneHandleAccess::create_mesh(other_scene, 1);
    if (storage.mesh(foreign_mesh).error().code() != elf3d::ErrorCode::invalid_mesh_handle) {
        return 7;
    }

    const std::array<std::byte, 8> caller_pixels{{std::byte{1}, std::byte{2}, std::byte{3},
                                                  std::byte{4}, std::byte{5}, std::byte{6},
                                                  std::byte{7}, std::byte{8}}};
    if (storage.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, caller_pixels}).error().code() !=
        elf3d::ErrorCode::invalid_argument) {
        return 8;
    }
    const std::array<std::byte, 4> pixel{{std::byte{1}, std::byte{2}, std::byte{3},
                                         std::byte{4}}};
    const auto image =
        storage.create_image({1, 1, elf3d::PixelFormat::rgba8_unorm, pixel});
    const elf3d::SamplerDescription sampler{elf3d::TextureWrap::clamp_to_edge,
                                             elf3d::TextureWrap::mirrored_repeat,
                                             elf3d::TextureFilter::linear_mipmap_linear,
                                             elf3d::TextureFilter::nearest};
    const auto texture = storage.create_texture({image.value(), sampler});
    if (!image || !texture || storage.images().size() != 1 || storage.textures().size() != 1 ||
        storage.image(image.value()).value()->pixels != std::vector<std::byte>(pixel.begin(), pixel.end()) ||
        storage.texture(texture.value()).value()->description.sampler != sampler) {
        return 9;
    }
    elf3d::SamplerDescription invalid_sampler;
    invalid_sampler.mag_filter = elf3d::TextureFilter::linear_mipmap_linear;
    if (storage.create_texture({image.value(), invalid_sampler}).error().code() !=
        elf3d::ErrorCode::invalid_sampler_description) {
        return 10;
    }
    return 0;
}
