#pragma once
#include "../fixtures/pipeline_preparation_fake.h"

#include "../fixtures/cube_preparation_fake.h"
#include "../fixtures/mesh_preparation_fake.h"
#include "../fixtures/texture_preparation_fake.h"
#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace elf3d::viewport::tests {

constexpr std::uintptr_t fake_resource_token = 1;

class FakeRenderTarget final : public elf3d::graphics::RenderTarget {
  public:
    explicit FakeRenderTarget(elf3d::Extent2D extent) noexcept : extent_(extent)
    {
        update_handle();
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override
    {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override
    {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        update_handle();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4 color) noexcept override
    {
        last_clear_color = color;
        ++clear_count;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> read_color_pixels(std::span<std::uint8_t> pixels) override
    {
        std::fill(pixels.begin(), pixels.end(), std::uint8_t{0});
        return {};
    }
    void set_display_transform(const elf3d::DisplayTransform& transform) noexcept override
    {
        display_transform = transform;
    }

    [[nodiscard]] elf3d::Result<elf3d::graphics::Texture2D*>
    prepare_transmission_scene() noexcept override
    {
        return elf3d::Error{elf3d::ErrorCode::texture_unavailable, "Fake snapshot unavailable"};
    }
    [[nodiscard]] elf3d::Result<void> capture_transmission_scene() noexcept override
    {
        return {};
    }
    [[nodiscard]] std::uint64_t transmission_scene_bytes() const noexcept override
    {
        return 0;
    }
    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override
    {
        return texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override
    {
        return texture_handle_.is_valid();
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
    {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;
    elf3d::Color4 last_clear_color;
    elf3d::DisplayTransform display_transform;

  private:
    void update_handle() noexcept
    {
        texture_handle_ = extent_.width != 0 && extent_.height != 0
                              ? elf3d::detail::TextureHandleAccess::create(1)
                              : elf3d::TextureHandle{};
    }

    elf3d::Extent2D extent_;
    elf3d::TextureHandle texture_handle_;
};

class FakePickingTarget final : public elf3d::graphics::PickingTarget {
  public:
    explicit FakePickingTarget(elf3d::Extent2D extent) noexcept : extent_(extent)
    {
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override
    {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override
    {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear() noexcept override
    {
        ++clear_count;
        return {};
    }

    [[nodiscard]] bool is_valid() const noexcept override
    {
        return extent_.width != 0 && extent_.height != 0;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
    {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;

  private:
    elf3d::Extent2D extent_;
};

struct FakeDeviceState {
    elf3d::Extent2D latest_render_target_extent;
    std::span<FakeRenderTarget> render_target;
    elf3d::Extent2D last_picking_read_extent;
    std::vector<std::span<FakePickingTarget>> picking_targets;
    std::optional<elf3d::graphics::PickingPixel> picking_pixel;
    bool picking_read_failure = false;
    bool latest_picking_double_sided = false;
    std::vector<float> picking_depths;
    elf3d::Float2 last_picking_read_position;
    int picking_pixel_read_count = 0;
    int picking_depths_read_count = 0;
    int latest_overlay_lines = 0;
    int latest_overlay_markers = 0;
};

class FakeDevice final : public elf3d::graphics::Device {
  public:
    [[nodiscard]] double monotonic_time_milliseconds() const noexcept override
    {
        const double result = clock_milliseconds_;
        clock_milliseconds_ += 0.125;
        return result;
    }
    [[nodiscard]] elf3d::graphics::GpuTimingSample
    delayed_gpu_timing(elf3d::graphics::GpuTimingPass) noexcept override
    {
        return {};
    }

    [[nodiscard]] FakeDeviceState& state() noexcept
    {
        return state_;
    }

    [[nodiscard]] const FakeDeviceState& state() const noexcept
    {
        return state_;
    }

    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override
    {
        return elf3d::GraphicsBackend::none;
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D initial_extent) noexcept override
    {
        state_.latest_render_target_extent = initial_extent;
        auto target = std::make_unique<FakeRenderTarget>(initial_extent);
        state_.render_target = std::span{target.get(), 1U};
        return std::unique_ptr<elf3d::graphics::RenderTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) noexcept override
    {
        auto target = std::make_unique<FakePickingTarget>(initial_extent);
        state_.picking_targets.emplace_back(target.get(), 1U);
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<elf3d::graphics::NativeTextureView>
    native_texture_view(elf3d::TextureHandle texture) const noexcept override
    {
        if (!texture.is_valid()) {
            return elf3d::Error{elf3d::ErrorCode::texture_unavailable,
                                "Fake texture is unavailable"};
        }
        return elf3d::graphics::NativeTextureView{elf3d::graphics::NativeGraphicsApi::opengl, 1,
                                                  state_.latest_render_target_extent};
    }

    class FakeMesh final : public elf3d::graphics::StaticMesh {
      public:
        std::uint32_t retirement_objects = 3;
        [[nodiscard]] std::uint32_t vertex_count() const noexcept override
        {
            return 0;
        }
        [[nodiscard]] std::uint32_t index_count() const noexcept override
        {
            return 0;
        }
        [[nodiscard]] elf3d::graphics::VertexLayout vertex_layout() const noexcept override
        {
            return elf3d::graphics::VertexLayout::position_normal_float3;
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
        {
            return fake_resource_token;
        }
    };

    class FakePipeline final : public elf3d::graphics::GraphicsPipeline {
      public:
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
        {
            return fake_resource_token;
        }
    };
    class FakeTexture final : public elf3d::graphics::Texture2D {
      public:
        std::uint32_t retirement_objects = 1;
        [[nodiscard]] elf3d::Extent2D extent() const noexcept override
        {
            return {1, 1};
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
        {
            return fake_resource_token;
        }
    };
    class FakeTextureCube final : public elf3d::graphics::TextureCube {
      public:
        explicit FakeTextureCube(
            const elf3d::graphics::TextureCubeStorageDescription& description) noexcept
            : extent_(description.extent), mip_count_(description.mip_count)
        {
        }
        [[nodiscard]] std::uint32_t extent() const noexcept override
        {
            return extent_;
        }
        [[nodiscard]] std::uint32_t mip_count() const noexcept override
        {
            return mip_count_;
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override
        {
            return fake_resource_token;
        }

      private:
        std::uint32_t extent_ = 0;
        std::uint32_t mip_count_ = 0;
    };

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>>
    create_static_mesh(const elf3d::graphics::StaticMeshDescription& description) noexcept override
    {
        return elf3d::graphics::complete_static_mesh_upload(*this, description);
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMeshPreparation>>
    begin_static_mesh(const elf3d::graphics::MeshStorageDescription& description) noexcept override
    {
        return std::unique_ptr<elf3d::graphics::StaticMeshPreparation>{
            std::make_unique<elf3d::graphics::tests::FakeMeshPreparation>(
                description, std::make_unique<FakeMesh>())};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription& description) noexcept override
    {
        return elf3d::graphics::complete_texture_upload(*this, description);
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2DPreparation>>
    begin_texture_2d(
        const elf3d::graphics::TextureStorageDescription& description) noexcept override
    {
        return std::unique_ptr<elf3d::graphics::Texture2DPreparation>{
            std::make_unique<elf3d::graphics::tests::FakeTexturePreparation>(
                description, std::make_unique<FakeTexture>())};
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::ResourceRetirementStep>
    retire_static_mesh(elf3d::graphics::StaticMesh& mesh,
                       const elf3d::graphics::ResourceRetirementBudget& budget) noexcept override
    {
        return elf3d::graphics::tests::retire_fake_objects(
            static_cast<FakeMesh&>(mesh).retirement_objects, budget);
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::ResourceRetirementStep>
    retire_texture_2d(elf3d::graphics::Texture2D& texture,
                      const elf3d::graphics::ResourceRetirementBudget& budget) noexcept override
    {
        return elf3d::graphics::tests::retire_fake_objects(
            static_cast<FakeTexture&>(texture).retirement_objects, budget);
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::TextureCube>> create_texture_cube(
        const elf3d::graphics::TextureCubeDescription& description) noexcept override
    {
        return elf3d::graphics::complete_cube_upload(*this, description);
    }
    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::TextureCubePreparation>>
    begin_texture_cube(
        const elf3d::graphics::TextureCubeStorageDescription& description) noexcept override
    {
        if (const auto bytes = elf3d::graphics::cube_storage_bytes(description); !bytes)
            return bytes.error();
        return std::unique_ptr<elf3d::graphics::TextureCubePreparation>{
            std::make_unique<elf3d::graphics::tests::FakeCubePreparation>(
                description, std::make_unique<FakeTextureCube>(description))};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(const elf3d::graphics::GraphicsPipelineDescription&) noexcept override
    {
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipelinePreparation>>
    begin_graphics_pipeline(
        const elf3d::graphics::GraphicsPipelineDescription& description) noexcept override
    {
        auto pipeline = create_graphics_pipeline(description);
        if (!pipeline)
            return pipeline.error();
        return std::unique_ptr<elf3d::graphics::GraphicsPipelinePreparation>{
            std::make_unique<elf3d::graphics::tests::FakePipelinePreparation>(
                std::move(pipeline).value())};
    }
    [[nodiscard]] elf3d::graphics::AuxiliaryGraphicsStatus
    auxiliary_graphics_status() const noexcept override
    {
        return auxiliary_.status();
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::PipelinePreparationRequest>
    request_auxiliary_graphics(elf3d::ShaderPipelineKind kind) const noexcept override
    {
        return auxiliary_.request(kind);
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::PipelinePreparationStep>
    advance_auxiliary_graphics(elf3d::ShaderPipelineKind kind) noexcept override
    {
        return auxiliary_.advance(kind);
    }
    [[nodiscard]] elf3d::Result<elf3d::graphics::ResourceRetirementStep> retire_auxiliary_graphics(
        const elf3d::graphics::ResourceRetirementBudget& budget) noexcept override
    {
        return auxiliary_.retire(budget);
    }
    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget&, elf3d::graphics::GraphicsPipeline&,
                 elf3d::graphics::StaticMesh&,
                 const elf3d::graphics::DrawIndexedDescription&) noexcept override
    {
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_indexed_batch(
        elf3d::graphics::RenderTarget& target, elf3d::graphics::GraphicsPipeline& pipeline,
        std::span<elf3d::graphics::StaticMesh* const> meshes,
        std::span<const elf3d::graphics::DrawIndexedDescription> descriptions,
        std::optional<std::size_t> capture_scene_before = std::nullopt) noexcept override
    {
        if (meshes.size() != descriptions.size()) {
            return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                "Fake batch arrays must have equal counts"};
        }
        for (std::size_t index = 0; index < meshes.size(); ++index) {
            if (capture_scene_before && index == *capture_scene_before) {
                auto copied = target.capture_transmission_scene();
                if (!copied)
                    return copied.error();
            }
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
                 const elf3d::graphics::DrawOverlayDescription& description) noexcept override
    {
        state_.latest_overlay_lines = static_cast<int>(description.lines.size());
        state_.latest_overlay_markers = static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_picking_indexed(
        elf3d::graphics::PickingTarget&, elf3d::graphics::StaticMesh&,
        const elf3d::graphics::PickingDrawDescription& description) noexcept override
    {
        state_.latest_picking_double_sided = description.double_sided;
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_picking_batch(
        elf3d::graphics::PickingTarget& target,
        std::span<elf3d::graphics::StaticMesh* const> meshes,
        std::span<const elf3d::graphics::PickingDrawDescription> descriptions) noexcept override
    {
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
    read_picking_pixel(elf3d::graphics::PickingTarget&,
                       elf3d::Float2 position_pixels) noexcept override
    {
        state_.last_picking_read_position = position_pixels;
        ++state_.picking_pixel_read_count;
        if (state_.picking_read_failure)
            return elf3d::Error{elf3d::ErrorCode::graphics_context_unavailable,
                                "Synthetic pick read failure"};
        return state_.picking_pixel;
    }
    [[nodiscard]] elf3d::Result<std::vector<float>>
    read_picking_depths(elf3d::graphics::PickingTarget& target) noexcept override
    {
        ++state_.picking_depths_read_count;
        state_.last_picking_read_extent = target.extent();
        if (!state_.picking_depths.empty()) {
            return state_.picking_depths;
        }

        const elf3d::Extent2D target_extent = target.extent();
        return std::vector<float>(static_cast<std::size_t>(target_extent.width) *
                                      static_cast<std::size_t>(target_extent.height),
                                  state_.picking_pixel.has_value() ? state_.picking_pixel->depth
                                                                   : 1.0F);
    }

  private:
    elf3d::graphics::tests::FakeAuxiliaryGraphics auxiliary_;
    mutable double clock_milliseconds_ = 0.0;
    FakeDeviceState state_;
};

} // namespace elf3d::viewport::tests
