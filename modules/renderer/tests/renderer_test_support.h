#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "studio_environment_test_source.h"

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
    void set_display_transform(const elf3d::DisplayTransform& transform) noexcept override {
        display_transform = transform;
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
    elf3d::DisplayTransform display_transform;
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
    FakeMesh(std::uint32_t vertices, std::uint32_t indices,
             elf3d::graphics::VertexLayout layout) noexcept
        : vertices_(vertices), indices_(indices), layout_(layout) {}
    [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
        return vertices_;
    }
    [[nodiscard]] std::uint32_t index_count() const noexcept override {
        return indices_;
    }
    [[nodiscard]] elf3d::graphics::VertexLayout vertex_layout() const noexcept override {
        return layout_;
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

  private:
    std::uint32_t vertices_ = 0;
    std::uint32_t indices_ = 0;
    elf3d::graphics::VertexLayout layout_ = elf3d::graphics::VertexLayout::position_normal_float3;
};

class FakePipeline final : public elf3d::graphics::GraphicsPipeline {
  public:
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }
};
class FakeTexture final : public elf3d::graphics::Texture2D {
  public:
    explicit FakeTexture(int& live_count) noexcept : live_count_(&live_count, 1U) {
        ++live_count_.front();
    }
    ~FakeTexture() override {
        --live_count_.front();
    }
    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return {1, 1};
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

  private:
    std::span<int> live_count_;
};
class FakeTextureCube final : public elf3d::graphics::TextureCube {
  public:
    FakeTextureCube(std::uint32_t extent, std::uint32_t mip_count, int& live_count) noexcept
        : extent_(extent), mip_count_(mip_count), live_count_(&live_count, 1U) {
        ++live_count_.front();
    }
    ~FakeTextureCube() override {
        --live_count_.front();
    }
    [[nodiscard]] std::uint32_t extent() const noexcept override {
        return extent_;
    }
    [[nodiscard]] std::uint32_t mip_count() const noexcept override {
        return mip_count_;
    }
    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

  private:
    std::uint32_t extent_ = 0;
    std::uint32_t mip_count_ = 0;
    std::span<int> live_count_;
};

struct FakeDeviceState {
    int upload_count = 0;
    int draw_count = 0;
    int indexed_batch_count = 0;
    int overlay_draw_count = 0;
    int picking_draw_count = 0;
    int picking_batch_count = 0;
    int overlay_line_count = 0;
    int overlay_marker_count = 0;
    int texture_upload_count = 0;
    int cubemap_upload_count = 0;
    std::vector<std::uint32_t> cubemap_extents;
    std::vector<std::uint32_t> cubemap_mip_counts;
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
    std::vector<bool> draw_environment_presence;
    std::vector<elf3d::graphics::PickingDrawDescription> picking_draws;
    std::vector<elf3d::graphics::VertexLayout> mesh_layouts;
    std::vector<std::size_t> mesh_uploaded_bytes;
    std::string vertex_shader_source;
    std::string fragment_shader_source;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] double monotonic_time_milliseconds() const noexcept override {
        const double result = clock_milliseconds_;
        clock_milliseconds_ += 0.125;
        return result;
    }
    [[nodiscard]] elf3d::graphics::GpuTimingSample
    delayed_gpu_timing(elf3d::graphics::GpuTimingPass) noexcept override {
        return {};
    }

    [[nodiscard]] FakeDeviceState& state() noexcept {
        return state_;
    }

    [[nodiscard]] const FakeDeviceState& state() const noexcept {
        return state_;
    }
    void fail_cubemap_upload_at(int attempt) noexcept {
        failed_cubemap_upload_ = attempt;
    }
    void fail_texture_upload_at(int attempt) noexcept {
        failed_texture_upload_ = attempt;
    }
    [[nodiscard]] int live_cubemap_count() const noexcept {
        return live_cubemap_count_;
    }
    [[nodiscard]] int live_texture_count() const noexcept {
        return live_texture_count_;
    }

    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::none;
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
    [[nodiscard]] elf3d::Result<elf3d::graphics::NativeTextureView>
    native_texture_view(elf3d::TextureHandle) const noexcept override {
        return elf3d::Error{elf3d::ErrorCode::invalid_argument, "Not used"};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>> create_static_mesh(
        const elf3d::graphics::StaticMeshDescription& description) noexcept override {
        ++state_.upload_count;
        state_.mesh_layouts.push_back(description.vertex_layout);
        state_.mesh_uploaded_bytes.push_back(description.vertex_bytes.size());
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>(
            description.vertex_count, static_cast<std::uint32_t>(description.indices.size()),
            description.vertex_layout)};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription& description) noexcept override {
        ++state_.texture_upload_count;
        state_.texture_descriptions.push_back(FakeDeviceState::TextureDescriptionSnapshot{
            description.format, description.wrap_u, description.wrap_v, description.min_filter,
            description.mag_filter});
        if (state_.texture_upload_count == failed_texture_upload_) {
            return elf3d::Error{elf3d::ErrorCode::graphics_initialization_failed,
                                "Injected texture creation failure"};
        }
        return std::unique_ptr<elf3d::graphics::Texture2D>{
            std::make_unique<FakeTexture>(live_texture_count_)};
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::TextureCube>> create_texture_cube(
        const elf3d::graphics::TextureCubeDescription& description) noexcept override {
        ++state_.cubemap_upload_count;
        state_.cubemap_extents.push_back(
            description.mips.empty() ? 0U : description.mips.front().extent);
        state_.cubemap_mip_counts.push_back(static_cast<std::uint32_t>(description.mips.size()));
        if (state_.cubemap_upload_count == failed_cubemap_upload_) {
            return elf3d::Error{elf3d::ErrorCode::graphics_initialization_failed,
                                "Injected cubemap creation failure"};
        }
        return std::unique_ptr<elf3d::graphics::TextureCube>{std::make_unique<FakeTextureCube>(
            state_.cubemap_extents.back(), state_.cubemap_mip_counts.back(), live_cubemap_count_)};
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
        state_.draw_environment_presence.push_back(description.environment_cubemaps.size() == 2U &&
                                                   description.environment_luts.size() == 1U &&
                                                   description.environment_cubemaps[0] != nullptr &&
                                                   description.environment_cubemaps[1] != nullptr &&
                                                   description.environment_luts[0] != nullptr);
        elf3d::graphics::DrawIndexedDescription stored_description = description;
        stored_description.textures = {};
        stored_description.environment_cubemaps = {};
        stored_description.environment_luts = {};
        state_.draws.push_back(stored_description);
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_indexed_batch(
        elf3d::graphics::RenderTarget& target, elf3d::graphics::GraphicsPipeline& pipeline,
        std::span<elf3d::graphics::StaticMesh* const> meshes,
        std::span<const elf3d::graphics::DrawIndexedDescription> descriptions) noexcept override {
        ++state_.indexed_batch_count;
        if (meshes.size() != descriptions.size()) {
            return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                "Fake batch arrays must have equal counts"};
        }
        for (std::size_t index = 0; index < meshes.size(); ++index) {
            if (meshes[index] == nullptr) {
                return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                    "Fake batch item requires a mesh"};
            }
            const elf3d::Result<void> result =
                draw_indexed(target, pipeline, *meshes[index], descriptions[index]);
            if (!result) {
                return result.error();
            }
        }
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
    [[nodiscard]] elf3d::Result<void> draw_picking_batch(
        elf3d::graphics::PickingTarget& target,
        std::span<elf3d::graphics::StaticMesh* const> meshes,
        std::span<const elf3d::graphics::PickingDrawDescription> descriptions) noexcept override {
        ++state_.picking_batch_count;
        if (meshes.size() != descriptions.size()) {
            return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                "Fake picking batch arrays must have equal counts"};
        }
        for (std::size_t index = 0; index < meshes.size(); ++index) {
            if (meshes[index] == nullptr) {
                return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                    "Fake picking batch item requires a mesh"};
            }
            const elf3d::Result<void> result =
                draw_picking_indexed(target, *meshes[index], descriptions[index]);
            if (!result) {
                return result.error();
            }
        }
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
    mutable double clock_milliseconds_ = 0.0;
    FakeDeviceState state_;
    int failed_cubemap_upload_ = -1;
    int failed_texture_upload_ = -1;
    int live_cubemap_count_ = 0;
    int live_texture_count_ = 0;
};

} // namespace elf3d::renderer::tests
