#pragma once

#include "renderer_mesh_preparation.h"
#include "renderer_scene_preparation.h"
#include "renderer_texture_preparation.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <elf3d/assets.h>
#include <elf3d/core/assert.h>
#include <graphics/graphics.h>
#include <map>
#include <memory>
#include <optional>
#include <renderer/renderer.h>
#include <scene/scene.h>
#include <vector>

namespace elf3d::renderer {
struct Renderer::Resources final {
    std::unique_ptr<graphics::Device> device;
    std::uint64_t engine_token = 0;
    std::unique_ptr<StudioEnvironmentSource> environment_source;
    std::unique_ptr<CacheState> cache;
};

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
    GraphicsPreparationFailure preparation_attempt;
    GraphicsRetirementFailure retirement_attempt;
    [[nodiscard]] Result<GraphicsRetirementStep>
    retire_scene_unit(graphics::Device& device, const graphics::ResourceRetirementBudget& budget);
    struct MeshEntry {
        [[nodiscard]] GraphicsRetirementPhase retirement_phase() const noexcept;
        [[nodiscard]] Result<GraphicsRetirementStep>
        retire_unit(graphics::Device& device, const graphics::ResourceRetirementBudget& budget);
        std::unique_ptr<graphics::StaticMesh> mesh;
        std::uint64_t resident_bytes = 0;
        std::unique_ptr<MeshGeometryPreparation> preparation;
    };

    struct TextureVariant {
        [[nodiscard]] Result<TextureWorkStep>
        prepare_step(graphics::Device& device, const graphics::TextureStorageDescription& storage,
                     std::span<const std::byte> pixels, const TextureWorkBudget& budget);
        [[nodiscard]] Result<GraphicsRetirementStep>
        retire_unit(graphics::Device& device, const graphics::ResourceRetirementBudget& budget);
        TextureColorSpace color_space = TextureColorSpace::linear;
        scene::RuntimeSamplerDescription sampler;
        std::unique_ptr<graphics::Texture2D> texture;
        std::uint64_t resident_bytes = 0;
        std::uint64_t accounted_bytes = 0;
        Extent2D extent;
        std::unique_ptr<TextureImagePreparation> preparation;
        [[nodiscard]] bool matches(TextureColorSpace space,
                                   const scene::RuntimeTextureView& source) const noexcept
        {
            return color_space == space && sampler == source.sampler &&
                   extent == Extent2D{source.width, source.height};
        }
    };

    struct ImageEntry {
        std::deque<TextureVariant> variants;
    };

    struct EntityPackets {
        std::map<std::uint32_t, DrawPacket> primitives;
    };
    using MeshEntries = std::map<std::size_t, MeshEntry>;
    using ImageEntries = std::map<std::size_t, ImageEntry>;

    struct SceneEntry {
        [[nodiscard]] GraphicsRetirementPhase retirement_phase() const noexcept;
        [[nodiscard]] Result<GraphicsRetirementStep>
        retire_unit(graphics::Device& device, const graphics::ResourceRetirementBudget& budget);
        [[nodiscard]] GraphicsRetirementStatus retirement_status() const noexcept;
        [[nodiscard]] Result<GraphicsRetirementStep>
        retire_mesh_map(MeshEntries& entries, graphics::Device& device,
                        const graphics::ResourceRetirementBudget& budget);
        [[nodiscard]] Result<GraphicsRetirementStep>
        retire_image_map(ImageEntries& entries, graphics::Device& device,
                         const graphics::ResourceRetirementBudget& budget);
        std::optional<SceneResourcePreparation> preparation;
        // Sparse geometry IDs can follow many unused Document resources. One
        // insertion allocates one entry instead of resizing an ID-sized table.
        MeshEntries meshes;
        MeshEntries document_meshes;
        ImageEntries images;
        ImageEntries document_images;
        std::map<std::size_t, EntityPackets> draw_packets;
        std::uint64_t draw_packet_revision = 0;
        std::uint64_t resident_geometry_bytes = 0;
        std::uint64_t resident_texture_bytes = 0;
        std::uint64_t estimated_geometry_bytes = 0;
        std::uint64_t estimated_texture_bytes = 0;
        std::uint64_t staging_bytes = 0;
        std::uint64_t texture_entries = 0;
        std::uint64_t texture_count = 0;
    };

    [[nodiscard]] const SceneEntry& scene(SceneId id) const noexcept
    {
        const auto found = scenes.find(id);
        ELF3D_ASSERT(found != scenes.end() && found->first == id && found->second);
        return *found->second;
    }

    [[nodiscard]] SceneEntry& scene(SceneId id)
    {
        auto [found, inserted] = scenes.try_emplace(id);
        ELF3D_ASSERT(found->first == id);
        if (inserted)
            found->second = std::make_unique<SceneEntry>();
        return *found->second;
    }

    struct SceneLess {
        bool operator()(SceneId a, SceneId b) const noexcept
        {
            // One Renderer engine domain; complete typed equality is also
            // checked on use, including foreign release requests.
            return a.debug_value() < b.debug_value();
        }
    };
    std::map<SceneId, std::unique_ptr<SceneEntry>, SceneLess> scenes;
    std::map<SceneId, std::unique_ptr<SceneEntry>, SceneLess> retired_scenes;
    GraphicsRetirementStatus retired_status;
    std::uint64_t resident_geometry_bytes = 0;
    std::uint64_t resident_texture_bytes = 0;
    std::uint64_t texture_count = 0;
};

} // namespace elf3d::renderer
