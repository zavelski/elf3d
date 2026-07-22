#pragma once

#include <elf3d/core/result.h>
#include <elf3d/graphics.h>

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
    explicit FakeRenderTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {
        update_handle();
    }

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        update_handle();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear(elf3d::Color4 color) noexcept override {
        last_clear_color = color;
        ++clear_count;
        return {};
    }

    [[nodiscard]] elf3d::TextureHandle color_texture() const noexcept override {
        return texture_handle_;
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return texture_handle_.is_valid();
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;
    elf3d::Color4 last_clear_color;

  private:
    void update_handle() noexcept {
        texture_handle_ = extent_.width != 0 && extent_.height != 0
                              ? elf3d::detail::TextureHandleAccess::create(1)
                              : elf3d::TextureHandle{};
    }

    elf3d::Extent2D extent_;
    elf3d::TextureHandle texture_handle_;
};

class FakePickingTarget final : public elf3d::graphics::PickingTarget {
  public:
    explicit FakePickingTarget(elf3d::Extent2D extent) noexcept : extent_(extent) {}

    [[nodiscard]] elf3d::Extent2D extent() const noexcept override {
        return extent_;
    }

    [[nodiscard]] elf3d::Result<void> resize(elf3d::Extent2D extent) noexcept override {
        if (extent == extent_) {
            return {};
        }
        extent_ = extent;
        ++resize_count;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> clear() noexcept override {
        ++clear_count;
        return {};
    }

    [[nodiscard]] bool is_valid() const noexcept override {
        return extent_.width != 0 && extent_.height != 0;
    }

    [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
        return fake_resource_token;
    }

    int resize_count = 0;
    int clear_count = 0;

  private:
    elf3d::Extent2D extent_;
};

struct FakeDeviceState {
    elf3d::Extent2D latest_render_target_extent;
    elf3d::Extent2D last_picking_read_extent;
    std::vector<FakePickingTarget*> picking_targets;
    std::optional<elf3d::graphics::PickingPixel> picking_pixel;
    std::vector<float> picking_depths;
    elf3d::Float2 last_picking_read_position;
    int picking_pixel_read_count = 0;
    int picking_depths_read_count = 0;
    int latest_overlay_lines = 0;
    int latest_overlay_markers = 0;
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

    [[nodiscard]] elf3d::GraphicsBackend backend() const noexcept override {
        return elf3d::GraphicsBackend::opengl;
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::RenderTarget>>
    create_render_target(elf3d::Extent2D initial_extent) noexcept override {
        state_.latest_render_target_extent = initial_extent;
        auto target = std::make_unique<FakeRenderTarget>(initial_extent);
        return std::unique_ptr<elf3d::graphics::RenderTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::PickingTarget>>
    create_picking_target(elf3d::Extent2D initial_extent) noexcept override {
        auto target = std::make_unique<FakePickingTarget>(initial_extent);
        state_.picking_targets.push_back(target.get());
        return std::unique_ptr<elf3d::graphics::PickingTarget>{std::move(target)};
    }

    [[nodiscard]] elf3d::Result<elf3d::NativeTextureView>
    native_texture_view(elf3d::TextureHandle texture) const noexcept override {
        if (!texture.is_valid()) {
            return elf3d::Error{elf3d::ErrorCode::texture_unavailable,
                                "Fake texture is unavailable"};
        }
        return elf3d::NativeTextureView{elf3d::NativeGraphicsApi::opengl, 1,
                                        state_.latest_render_target_extent};
    }

    class FakeMesh final : public elf3d::graphics::StaticMesh {
      public:
        [[nodiscard]] std::uint32_t vertex_count() const noexcept override {
            return 0;
        }
        [[nodiscard]] std::uint32_t index_count() const noexcept override {
            return 0;
        }
        [[nodiscard]] elf3d::graphics::VertexLayout vertex_layout() const noexcept override {
            return elf3d::graphics::VertexLayout::position_normal_float3;
        }
        [[nodiscard]] std::uintptr_t backend_resource_token() const noexcept override {
            return fake_resource_token;
        }
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

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::StaticMesh>>
    create_static_mesh(const elf3d::graphics::StaticMeshDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::StaticMesh>{std::make_unique<FakeMesh>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::Texture2D>>
    create_texture_2d(const elf3d::graphics::Texture2DDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::Texture2D>{std::make_unique<FakeTexture>()};
    }

    [[nodiscard]] elf3d::Result<std::unique_ptr<elf3d::graphics::GraphicsPipeline>>
    create_graphics_pipeline(
        const elf3d::graphics::GraphicsPipelineDescription&) noexcept override {
        return std::unique_ptr<elf3d::graphics::GraphicsPipeline>{std::make_unique<FakePipeline>()};
    }

    [[nodiscard]] elf3d::Result<void>
    draw_indexed(elf3d::graphics::RenderTarget&, elf3d::graphics::GraphicsPipeline&,
                 elf3d::graphics::StaticMesh&,
                 const elf3d::graphics::DrawIndexedDescription&) noexcept override {
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_indexed_batch(
        elf3d::graphics::RenderTarget& target, elf3d::graphics::GraphicsPipeline& pipeline,
        std::span<const elf3d::graphics::IndexedDrawBatchItem> items) noexcept override {
        for (const elf3d::graphics::IndexedDrawBatchItem& item : items) {
            if (item.mesh == nullptr) {
                return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                    "Fake batch item requires a mesh"};
            }
            const elf3d::Result<void> result =
                draw_indexed(target, pipeline, *item.mesh, item.description);
            if (!result) {
                return result.error();
            }
        }
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_overlay(elf3d::graphics::RenderTarget&,
                 const elf3d::graphics::DrawOverlayDescription& description) noexcept override {
        state_.latest_overlay_lines = static_cast<int>(description.lines.size());
        state_.latest_overlay_markers = static_cast<int>(description.markers.size());
        return {};
    }
    [[nodiscard]] elf3d::Result<void>
    draw_picking_indexed(elf3d::graphics::PickingTarget&, elf3d::graphics::StaticMesh&,
                         const elf3d::graphics::PickingDrawDescription&) noexcept override {
        return {};
    }
    [[nodiscard]] elf3d::Result<void> draw_picking_batch(
        elf3d::graphics::PickingTarget& target,
        std::span<const elf3d::graphics::PickingDrawBatchItem> items) noexcept override {
        for (const elf3d::graphics::PickingDrawBatchItem& item : items) {
            if (item.mesh == nullptr) {
                return elf3d::Error{elf3d::ErrorCode::invalid_argument,
                                    "Fake picking batch item requires a mesh"};
            }
            const elf3d::Result<void> result =
                draw_picking_indexed(target, *item.mesh, item.description);
            if (!result) {
                return result.error();
            }
        }
        return {};
    }
    [[nodiscard]] elf3d::Result<std::optional<elf3d::graphics::PickingPixel>>
    read_picking_pixel(elf3d::graphics::PickingTarget&,
                       elf3d::Float2 position_pixels) noexcept override {
        state_.last_picking_read_position = position_pixels;
        ++state_.picking_pixel_read_count;
        return state_.picking_pixel;
    }
    [[nodiscard]] elf3d::Result<std::vector<float>>
    read_picking_depths(elf3d::graphics::PickingTarget& target) noexcept override {
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
    mutable double clock_milliseconds_ = 0.0;
    FakeDeviceState state_;
};

} // namespace elf3d::viewport::tests
