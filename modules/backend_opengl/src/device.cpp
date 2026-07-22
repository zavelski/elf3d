module;

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_internal.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

module elf.backend.opengl;

import elf.graphics;

namespace elf3d::backend::opengl {
namespace {

using namespace device_detail;

class OpenGLDevice final : public graphics::Device {
  public:
    explicit OpenGLDevice(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state)) {}

    ~OpenGLDevice() override {
        if (state_->can_destroy_objects()) {
            release_picking_resources(picking_resources_);
            release_overlay_resources(overlay_resources_);
        }
        state_->shut_down();
    }

    [[nodiscard]] double monotonic_time_milliseconds() const noexcept override {
        const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    [[nodiscard]] graphics::GpuTimingSample
    delayed_gpu_timing(graphics::GpuTimingPass pass) noexcept override {
        GpuTimingKind kind = GpuTimingKind::main;
        switch (pass) {
        case graphics::GpuTimingPass::main:
            break;
        case graphics::GpuTimingPass::picking:
            kind = GpuTimingKind::picking;
            break;
        case graphics::GpuTimingPass::resolve:
            kind = GpuTimingKind::resolve;
            break;
        }
        const GpuTimingResult timing = state_->latest_gpu_timing(kind);
        return graphics::GpuTimingSample{timing.milliseconds, timing.available};
    }

    [[nodiscard]] GraphicsBackend backend() const noexcept override {
        return GraphicsBackend::opengl;
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
    create_render_target(Extent2D initial_extent) noexcept override {
        return device_detail::create_render_target(state_, initial_extent);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::PickingTarget>>
    create_picking_target(Extent2D initial_extent) noexcept override {
        return device_detail::create_picking_target(state_, initial_extent);
    }

    [[nodiscard]] Result<NativeTextureView>
    native_texture_view(TextureHandle texture) const noexcept override {
        return state_->native_texture_view(texture);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::StaticMesh>>
    create_static_mesh(const graphics::StaticMeshDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::create_static_mesh(state_, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>>
    create_texture_2d(const graphics::Texture2DDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::create_texture_2d(state_, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>> create_graphics_pipeline(
        const graphics::GraphicsPipelineDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::create_graphics_pipeline(state_, description);
    }

    [[nodiscard]] Result<void>
    draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                 graphics::StaticMesh& mesh,
                 const graphics::DrawIndexedDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::draw_indexed(target, pipeline, mesh, description);
    }

    [[nodiscard]] Result<void>
    draw_indexed_batch(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                       std::span<const graphics::IndexedDrawBatchItem> items) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        const GpuTimingScope timing{*state_, GpuTimingKind::main};
        return device_detail::draw_indexed_batch(target, pipeline, items);
    }

    [[nodiscard]] Result<void>
    draw_overlay(graphics::RenderTarget& target,
                 const graphics::DrawOverlayDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::draw_overlay(overlay_resources_, target, description);
    }

    [[nodiscard]] Result<void>
    draw_picking_indexed(graphics::PickingTarget& target, graphics::StaticMesh& mesh,
                         const graphics::PickingDrawDescription& description) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::draw_picking_indexed(picking_resources_, target, mesh, description);
    }

    [[nodiscard]] Result<void>
    draw_picking_batch(graphics::PickingTarget& target,
                       std::span<const graphics::PickingDrawBatchItem> items) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        const GpuTimingScope timing{*state_, GpuTimingKind::picking};
        return device_detail::draw_picking_batch(picking_resources_, target, items);
    }

    [[nodiscard]] Result<std::optional<graphics::PickingPixel>>
    read_picking_pixel(graphics::PickingTarget& target, Float2 position_pixels) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        Result<std::optional<PickingReadback>> readback =
            device_detail::read_picking_pixel(target, position_pixels);
        if (!readback) {
            return readback.error();
        }
        if (!readback.value().has_value()) {
            return std::optional<graphics::PickingPixel>{};
        }
        const PickingReadback& pixel = *readback.value();
        return std::optional<graphics::PickingPixel>{graphics::PickingPixel{
            pixel.object_id, pixel.primitive_index, pixel.instance_index, pixel.depth}};
    }

    [[nodiscard]] Result<std::vector<float>>
    read_picking_depths(graphics::PickingTarget& target) noexcept override {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::read_picking_depths(target);
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    PickingResources picking_resources_;
    OverlayResources overlay_resources_;
};

} // namespace

Result<std::unique_ptr<graphics::Device>>
create_device(const OpenGLConfiguration& configuration) noexcept {
    if (configuration.load_procedure == nullptr) {
        return Error{ErrorCode::missing_graphics_procedure_loader, missing_loader_message};
    }

    try {
        static_assert(std::is_same_v<GraphicsProcedure, GLADapiproc>);
        static_assert(std::is_convertible_v<GraphicsProcedureLoader, GLADloadfunc>);

        const int loaded_version = gladLoadGL(configuration.load_procedure);
        if (loaded_version == 0) {
            return Error{ErrorCode::graphics_initialization_failed, glad_failure_message};
        }
        if (GLAD_GL_VERSION_4_1 == 0) {
            return Error{ErrorCode::unsupported_graphics_version, version_failure_message};
        }
        if (glGetString(GL_VERSION) == nullptr) {
            return Error{ErrorCode::graphics_context_unavailable, context_failure_message};
        }

        GLint profile_mask = 0;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_mask);
        if ((profile_mask & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
            return Error{ErrorCode::unsupported_graphics_version,
                         "Elf3D requires an OpenGL core-profile context"};
        }

        GLint maximum_texture_size = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum_texture_size);
        if (maximum_texture_size <= 0) {
            return Error{ErrorCode::graphics_initialization_failed,
                         "OpenGL reported an invalid maximum texture size"};
        }

        auto state = std::make_shared<OpenGLDeviceState>(maximum_texture_size);
        return std::unique_ptr<graphics::Device>{std::make_unique<OpenGLDevice>(std::move(state))};
    } catch (const std::bad_alloc&) {
        fatal_opengl_allocation_failure();
    } catch (...) {
        fatal_unexpected_opengl_boundary_exception();
    }
}

} // namespace elf3d::backend::opengl
