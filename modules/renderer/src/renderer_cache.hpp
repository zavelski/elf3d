#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <elf3d/assets.h>
#include <elf3d/internal/graphics.h>
#include <elf3d/internal/renderer.h>
#include <elf3d/internal/scene.h>
#include <memory>
#include <optional>
#include <vector>

namespace elf3d::renderer {

struct Renderer::DrawPacket {
    bool lit_textures_prepared = false;
    bool has_tangent_basis = false;
    std::size_t mesh_index = 0;
    bool document_mesh = false;
    std::array<std::size_t, graphics::material_texture_count> texture_indices{};
    std::array<std::size_t, graphics::material_texture_count> image_indices{};
    std::array<bool, graphics::material_texture_count> document_images{};
    std::array<bool, graphics::material_texture_count> has_textures{};
    MaterialDescription material;
};

struct Renderer::CacheState {
    struct MeshEntry {
        std::unique_ptr<graphics::StaticMesh> mesh;
        std::uint64_t resident_bytes = 0;
    };

    struct TextureVariant {
        TextureColorSpace color_space = TextureColorSpace::linear;
        scene::RuntimeSamplerDescription sampler;
        std::unique_ptr<graphics::Texture2D> texture;
        std::uint64_t resident_bytes = 0;
        [[nodiscard]] bool matches(TextureColorSpace space,
                                   const scene::RuntimeTextureView& source) const noexcept
        {
            return color_space == space && sampler == source.sampler;
        }
    };

    struct ImageEntry {
        std::vector<TextureVariant> variants;
    };

    struct EntityPackets {
        std::vector<std::optional<DrawPacket>> primitives;
    };

    struct SceneEntry {
        std::vector<std::optional<MeshEntry>> meshes;
        std::vector<std::optional<MeshEntry>> document_meshes;
        std::vector<std::optional<ImageEntry>> images;
        std::vector<std::optional<ImageEntry>> document_images;
        std::vector<std::optional<EntityPackets>> draw_packets;
        std::uint64_t draw_packet_revision = 0;
        std::uint64_t resident_geometry_bytes = 0;
        std::uint64_t resident_texture_bytes = 0;
        std::uint64_t texture_count = 0;
    };

    [[nodiscard]] const SceneEntry& scene(SceneId id) const noexcept
    {
        return *scenes[static_cast<std::size_t>(id.debug_value())];
    }

    [[nodiscard]] SceneEntry& scene(SceneId id)
    {
        const std::size_t index = static_cast<std::size_t>(id.debug_value());
        if (index >= scenes.size()) {
            scenes.resize(index + 1U);
        }
        if (!scenes[index].has_value()) {
            scenes[index].emplace();
        }
        return *scenes[index];
    }

    std::vector<std::optional<SceneEntry>> scenes;
    std::uint64_t resident_geometry_bytes = 0;
    std::uint64_t resident_texture_bytes = 0;
    std::uint64_t texture_count = 0;
};

} // namespace elf3d::renderer
