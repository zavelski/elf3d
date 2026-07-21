#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace elf3d::renderer::tests {

constexpr std::uintptr_t fake_resource_token = 1;

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_value;
    }
    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        extent_value = extent;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4) noexcept override {
        ++clear_count;
        return {};
    }
    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override {
        return {};
    }
    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_value.width != 0 && extent_value.height != 0;
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    elf3d::Extent2D extent_value{640, 360};
    int clear_count = 0;
};

class FakePickingTarget final : public elf3d::graphics::PickingTarget {
  public:
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_value;
    }
    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        extent_value = extent;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> clear() noexcept override {
        ++clear_count;
        return {};
    }
    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_value.width != 0 && extent_value.height != 0;
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    elf3d::Extent2D extent_value{640, 360};
    int clear_count = 0;
};

class FakeMesh final : public elf3d::graphics::StaticMesh {
  public:
    FakeMesh(std::uint32_t vertices, std::uint32_t indices) noexcept
        : vertices_(vertices), indices_(indices) {}
    [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
        return vertices_;
    }
    [[nodiscard]] std::uint32_t index_count() const noexcept override {
        return indices_;
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

  private:
    std::uint32_t vertices_ = 0;
    std::uint32_t indices_ = 0;
};

class FakePipeline final : public elf3d::graphics::GraphicsPipeline {
  public:
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }
};
class FakeTexture final : public elf3d::graphics::Texture2D {
  public:
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return {1, 1};
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }
};

struct FakeDeviceState {
    int upload_count = 0;
    int draw_count = 0;
    int overlay_draw_count = 0;
    int picking_draw_count = 0;
    int overlay_line_count = 0;
    int overlay_marker_count = 0;
    int texture_upload_count = 0;
    struct TextureDescriptionSnapshot {
        elf3d::graphics::TextureFormat format;
        elf3d::graphics::TextureAddressMode wrap_u;
        elf3d::graphics::TextureAddressMode wrap_v;
        elf3d::graphics::TextureFilterMode min_filter;
        elf3d::graphics::TextureFilterMode mag_filter;
    };
    std::optional<elf3d::graphics::PickingPixel> picking_pixel;
    std::vector<float> picking_depths;
    std::vector<TextureDescriptionSnapshot> texture_descriptions;
    std::vector<elf3d::graphics::DrawIndexedDescription> draws;
    std::vector<std::array<bool, elf3d::graphics::material_texture_count>> draw_texture_presence;
    std::vector<elf3d::graphics::PickingDrawDescription> picking_draws;
    std::string vertex_shader_source;
    std::string fragment_shader_source;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] FakeDeviceState& state() noexcept {
        return state_;
    }

    [[nodiscard]] const FakeDeviceState& state() const noexcept {
        return state_;
    }

    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::opengl;
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D) noexcept override {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Not used"};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) noexcept override {
        auto target = std::make_unique<FakePickingTarget>();
        target->extent_value = initial_extent;
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }
    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle) const noexcept override {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Not used"};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>> create_static_mesh(
        const elf3d::graphics::StaticMeshDescription& description) noexcept override {
        ++state_.upload_count;
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>(
            description.vertex_count, static_cast<std::uint32_t>(description.indices.size()))};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription& description) noexcept override {
        ++state_.texture_upload_count;
        state_.texture_descriptions.push_back(FakeDeviceState::TextureDescriptionSnapshot{
            description.format, description.wrap_u, description.wrap_v, description.min_filter,
            description.mag_filter});
        return std::unique_ptr<elf3d::graphics::Texture2D>{std::make_unique<FakeTexture>()};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(
        const elf3d::graphics::GraphicsPipelineDescription& description) noexcept override {
        state_.vertex_shader_source = description.vertex_shader_source;
        state_.fragment_shader_source = description.fragment_shader_source;
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget&, elf3d::graphics::GraphicsPipeline&,
                 elf3d::graphics::StaticMesh&,
                 const elf3d::graphics::DrawIndexedDescription& description) noexcept override {
        ++state_.draw_count;
        std::array<bool, elf3d::graphics::material_texture_count> texture_presence{};
        for (std::size_t index = 0; index < texture_presence.size(); ++index) {
            texture_presence[index] =
                description.textures.size() > index && description.textures[index] != nullptr;
        }
        state_.draw_texture_presence.push_back(texture_presence);
        elf3d::graphics::DrawIndexedDescription stored_description = description;
        stored_description.textures = {};
        state_.draws.push_back(stored_description);
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_overlay(elf3d::graphics::RenderTarget&,
                 const elf3d::graphics::DrawOverlayDescription& description) noexcept override {
        ++state_.overlay_draw_count;
        state_.overlay_line_count += static_cast<int>(description.lines.size());
        state_.overlay_marker_count += static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_picking_indexed(
        elf3d::graphics::PickingTarget&, elf3d::graphics::StaticMesh&,
        const elf3d::graphics::PickingDrawDescription& description) noexcept override {
        ++state_.picking_draw_count;
        state_.picking_draws.push_back(description);
        return {};
    }
    [[nodiscard]] elf3d::Result<std::optional<elf3d::graphics::PickingPixel>>
    read_picking_pixel(elf3d::graphics::PickingTarget&, elf3d::Float2) noexcept override {
        return state_.picking_pixel;
    }
    [[nodiscard]] elf3d::Result<std::vector<float>>
    read_picking_depths(elf3d::graphics::PickingTarget& target) noexcept override {
        if (!state_.picking_depths.empty()) {
            return state_.picking_depths;
        }
        const elf3d::Extent2D extent = target.extent();
        return std::vector<float>(
            static_cast<std::size_t>(extent.width) * static_cast<std::size_t>(extent.height),
            state_.picking_pixel.has_value() ? state_.picking_pixel->depth : 1.0F);
    }

  private:
    FakeDeviceState state_;
};

} // namespace elf3d::renderer::tests
