#pragma once

#include "renderer_scene_preparation.h"
#include <array>
#include <graphics/cube_preparation.h>

namespace elf3d::renderer {
struct EnvironmentResources final {
    std::unique_ptr<graphics::TextureCube> diffuse;
    std::unique_ptr<graphics::TextureCube> specular;
    std::unique_ptr<graphics::Texture2D> brdf_lut;
    std::uint64_t resident_bytes = 0;
};
// One fixed built-in environment transaction. Pixel views expire per advance;
// ready preparations keep their owners until the whole set can be adopted.
struct EnvironmentPreparation {
    enum class Phase { header, checksum, diffuse, specular, lut, ready };
    Phase phase = Phase::header;
    bool failed = false;
    GraphicsPreparationFailure attempt;
    std::array<std::unique_ptr<graphics::TextureCubePreparation>, 2> cubes;
    std::unique_ptr<graphics::Texture2DPreparation> lut;
    std::array<std::uint64_t, 3> allocated{};
    [[nodiscard]] Result<SceneResourceWork> advance(graphics::Device& device,
                                                    std::span<const std::byte> resource,
                                                    const GraphicsPreparationBudget& budget);
    [[nodiscard]] bool needs_retirement() const noexcept;
    [[nodiscard]] std::uint64_t allocated_bytes() const noexcept;
    [[nodiscard]] Result<GraphicsRetirementStep>
    retire_unit(const graphics::ResourceRetirementBudget& budget);
    [[nodiscard]] std::unique_ptr<EnvironmentResources> take_resources();

  private:
    [[nodiscard]] Result<SceneResourceWork>
    advance_payload(graphics::Device& device, std::span<const std::byte> resource,
                    const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<SceneResourceWork> checksum(std::span<const std::byte> payload);
    [[nodiscard]] Result<SceneResourceWork> advance_cube(graphics::Device& device,
                                                         std::span<const std::byte> payload,
                                                         const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<SceneResourceWork> begin_cube(graphics::Device& device, std::size_t index,
                                                       const GraphicsPreparationBudget& budget);
    [[nodiscard]] Result<SceneResourceWork> advance_lut(graphics::Device& device,
                                                        std::span<const std::byte> payload,
                                                        const GraphicsPreparationBudget& budget);
    std::size_t checksum_cursor_ = 0;
    std::uint64_t checksum_ = 14'695'981'039'346'656'037ULL;
    std::uint64_t expected_checksum_ = 0;
};
} // namespace elf3d::renderer
