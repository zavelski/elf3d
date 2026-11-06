#include <backend_opengl/backend_opengl.h>

#include <elf3d/graphics.h>

#include <glad/gl.h>

#include "device_cube.h"
#include "device_internal.hpp"
#include "device_mesh.h"
#include "device_shared_preparation.h"
#include "device_texture.h"
#include <graphics/mesh_preparation.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <graphics/graphics.h>
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

// Use the imported graphics types; global-fragment forward declarations create different types.
namespace elf3d::backend::opengl::device_detail {
[[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
create_render_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::PickingTarget>>
create_picking_target(std::shared_ptr<OpenGLDeviceState> state, Extent2D initial_extent) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::TextureCube>>
create_texture_cube(std::shared_ptr<OpenGLDeviceState> state,
                    const graphics::TextureCubeDescription& description) noexcept;
[[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>>
create_graphics_pipeline(std::shared_ptr<OpenGLDeviceState> state,
                         const graphics::GraphicsPipelineDescription& description) noexcept;
[[nodiscard]] Result<void>
draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
             graphics::StaticMesh& mesh,
             const graphics::DrawIndexedDescription& description) noexcept;
[[nodiscard]] Result<void>
draw_indexed_batch(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                   std::span<graphics::StaticMesh* const> meshes,
                   std::span<const graphics::DrawIndexedDescription> descriptions,
                   std::optional<std::size_t> capture_scene_before) noexcept;
[[nodiscard]] Result<void>
draw_overlay(OverlayResources& resources, graphics::RenderTarget& target,
             const graphics::DrawOverlayDescription& description) noexcept;
[[nodiscard]] Result<void>
draw_picking_indexed(PickingResources& resources, graphics::PickingTarget& target,
                     graphics::StaticMesh& mesh,
                     const graphics::PickingDrawDescription& description) noexcept;
[[nodiscard]] Result<void>
draw_picking_batch(PickingResources& resources, graphics::PickingTarget& target,
                   std::span<graphics::StaticMesh* const> meshes,
                   std::span<const graphics::PickingDrawDescription> descriptions) noexcept;
[[nodiscard]] Result<std::optional<PickingReadback>>
read_picking_pixel(graphics::PickingTarget& target, Float2 position_pixels) noexcept;
[[nodiscard]] Result<std::vector<float>>
read_picking_depths(graphics::PickingTarget& target) noexcept;
} // namespace elf3d::backend::opengl::device_detail

namespace elf3d::backend::opengl {
namespace {

using namespace device_detail;

class OpenGLDevice final : public graphics::Device {
  public:
    explicit OpenGLDevice(std::shared_ptr<OpenGLDeviceState> state) noexcept
        : state_(std::move(state))
    {
    }

    ~OpenGLDevice() override
    {
        shared_preparation_.reset();
        state_->shut_down();
    }

    [[nodiscard]] double monotonic_time_milliseconds() const noexcept override
    {
        const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    [[nodiscard]] graphics::GpuTimingSample
    delayed_gpu_timing(graphics::GpuTimingPass pass) noexcept override
    {
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

    [[nodiscard]] GraphicsBackend backend() const noexcept override
    {
        return GraphicsBackend::opengl;
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::RenderTarget>>
    create_render_target(Extent2D initial_extent) noexcept override
    {
        return device_detail::create_render_target(state_, initial_extent);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::PickingTarget>>
    create_picking_target(Extent2D initial_extent) noexcept override
    {
        return device_detail::create_picking_target(state_, initial_extent);
    }

    [[nodiscard]] Result<graphics::NativeTextureView>
    native_texture_view(TextureHandle texture) const noexcept override
    {
        Result<OpenGLTextureView> view = state_->native_texture_view(texture);
        if (!view) {
            return view.error();
        }
        return graphics::NativeTextureView{graphics::NativeGraphicsApi::opengl, view.value().value,
                                           view.value().extent};
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::StaticMesh>>
    create_static_mesh(const graphics::StaticMeshDescription& description) noexcept override
    {
        return graphics::complete_static_mesh_upload(*this, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::StaticMeshPreparation>>
    begin_static_mesh(const graphics::MeshStorageDescription& description) noexcept override
    {
        return device_detail::begin_static_mesh(state_, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::Texture2D>>
    create_texture_2d(const graphics::Texture2DDescription& description) noexcept override
    {
        return graphics::complete_texture_upload(*this, description);
    }
    [[nodiscard]] Result<std::unique_ptr<graphics::Texture2DPreparation>>
    begin_texture_2d(const graphics::TextureStorageDescription& description) noexcept override
    {
        return device_detail::begin_texture_2d(state_, description);
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    retire_static_mesh(graphics::StaticMesh& mesh,
                       const graphics::ResourceRetirementBudget& budget) noexcept override
    {
        if (mesh.backend_resource_token() != opengl_resource_token())
            return Error{ErrorCode::backend_mismatch, "Mesh retirement requires an OpenGL owner"};
        return static_cast<OpenGLStaticMesh&>(mesh).advance_retirement(budget);
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    retire_texture_2d(graphics::Texture2D& texture,
                      const graphics::ResourceRetirementBudget& budget) noexcept override
    {
        if (texture.backend_resource_token() != opengl_resource_token())
            return Error{ErrorCode::backend_mismatch,
                         "Texture retirement requires an OpenGL owner"};
        return static_cast<OpenGLTexture2D&>(texture).advance_retirement(budget);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::TextureCube>>
    create_texture_cube(const graphics::TextureCubeDescription& description) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::create_texture_cube(state_, description);
    }
    [[nodiscard]] Result<std::unique_ptr<graphics::TextureCubePreparation>>
    begin_texture_cube(const graphics::TextureCubeStorageDescription& description) noexcept override
    {
        return device_detail::begin_texture_cube(state_, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipeline>> create_graphics_pipeline(
        const graphics::GraphicsPipelineDescription& description) noexcept override
    {
        return device_detail::create_graphics_pipeline(state_, description);
    }

    [[nodiscard]] Result<std::unique_ptr<graphics::GraphicsPipelinePreparation>>
    begin_graphics_pipeline(
        const graphics::GraphicsPipelineDescription& description) noexcept override
    {
        return device_detail::begin_graphics_pipeline(state_, description);
    }

    [[nodiscard]] graphics::AuxiliaryGraphicsStatus
    auxiliary_graphics_status() const noexcept override
    {
        const auto& ready = state_->shared_resources();
        graphics::AuxiliaryGraphicsStatus status;
        status.picking_ready = ready.ready(ShaderPipelineKind::picking);
        status.overlay_ready = ready.ready(ShaderPipelineKind::overlay);
        status.display_resolve_ready = ready.ready(ShaderPipelineKind::display_resolve);
        status.retirement_pending = shared_preparation_ && shared_preparation_->failed();
        if (shared_preparation_)
            status.preparing = shared_preparation_->kind();
        return status;
    }
    [[nodiscard]] Result<graphics::PipelinePreparationRequest>
    request_auxiliary_graphics(ShaderPipelineKind kind) const noexcept override
    {
        if (const auto valid = state_->validate_thread(); !valid)
            return valid.error();
        if (shared_preparation_) {
            if (shared_preparation_->kind() != kind)
                return Error{ErrorCode::invalid_argument,
                             "Another auxiliary program is being prepared"};
            return shared_preparation_->request();
        }
        if (state_->shared_resources().ready(kind))
            return graphics::PipelinePreparationRequest{ShaderPreparationPhase::ready, 0};
        if (kind < ShaderPipelineKind::picking || kind > ShaderPipelineKind::display_resolve)
            return Error{ErrorCode::invalid_argument, "Expected an auxiliary graphics program"};
        return graphics::PipelinePreparationRequest{ShaderPreparationPhase::factory, 1};
    }
    [[nodiscard]] Result<graphics::PipelinePreparationStep>
    advance_auxiliary_graphics(ShaderPipelineKind kind) noexcept override
    {
        const auto next = request_auxiliary_graphics(kind);
        if (!next)
            return next.error();
        if (next.value().phase == ShaderPreparationPhase::ready)
            return graphics::PipelinePreparationStep{0, true};
        if (!shared_preparation_) {
            auto created = begin_shared_graphics(state_, kind);
            if (!created)
                return created.error();
            shared_preparation_ = std::move(created).value();
        }
        auto step = shared_preparation_->advance();
        if (step && step.value().ready)
            shared_preparation_.reset();
        return step;
    }
    [[nodiscard]] Result<graphics::ResourceRetirementStep>
    retire_auxiliary_graphics(const graphics::ResourceRetirementBudget& budget) noexcept override
    {
        if (!shared_preparation_ || !shared_preparation_->failed())
            return graphics::ResourceRetirementStep{0, 0, true, false};
        auto step = shared_preparation_->advance_retirement(budget);
        if (step && step.value().complete)
            shared_preparation_.reset();
        return step;
    }
    [[nodiscard]] Result<void>
    draw_indexed(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                 graphics::StaticMesh& mesh,
                 const graphics::DrawIndexedDescription& description) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::draw_indexed(target, pipeline, mesh, description);
    }

    [[nodiscard]] Result<void>
    draw_indexed_batch(graphics::RenderTarget& target, graphics::GraphicsPipeline& pipeline,
                       std::span<graphics::StaticMesh* const> meshes,
                       std::span<const graphics::DrawIndexedDescription> descriptions,
                       std::optional<std::size_t> capture_scene_before) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        const GpuTimingScope timing{*state_, GpuTimingKind::main};
        return device_detail::draw_indexed_batch(target, pipeline, meshes, descriptions,
                                                 capture_scene_before);
    }

    [[nodiscard]] Result<void>
    draw_overlay(graphics::RenderTarget& target,
                 const graphics::DrawOverlayDescription& description) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (const auto prepared = ensure_auxiliary_graphics(state_, ShaderPipelineKind::overlay);
            !prepared)
            return prepared.error();
        return device_detail::draw_overlay(state_->shared_resources().overlay, target, description);
    }

    [[nodiscard]] Result<void>
    draw_picking_indexed(graphics::PickingTarget& target, graphics::StaticMesh& mesh,
                         const graphics::PickingDrawDescription& description) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        if (const auto prepared = ensure_auxiliary_graphics(state_, ShaderPipelineKind::picking);
            !prepared)
            return prepared.error();
        return device_detail::draw_picking_indexed(state_->shared_resources().picking, target, mesh,
                                                   description);
    }

    [[nodiscard]] Result<void> draw_picking_batch(
        graphics::PickingTarget& target, std::span<graphics::StaticMesh* const> meshes,
        std::span<const graphics::PickingDrawDescription> descriptions) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }

        if (const auto prepared = ensure_auxiliary_graphics(state_, ShaderPipelineKind::picking);
            !prepared)
            return prepared.error();
        const GpuTimingScope timing{*state_, GpuTimingKind::picking};
        return device_detail::draw_picking_batch(state_->shared_resources().picking, target, meshes,
                                                 descriptions);
    }

    [[nodiscard]] Result<std::optional<graphics::PickingPixel>>
    read_picking_pixel(graphics::PickingTarget& target, Float2 position_pixels) noexcept override
    {
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
    read_picking_depths(graphics::PickingTarget& target) noexcept override
    {
        const Result<void> validation = state_->validate_operation();
        if (!validation) {
            return validation.error();
        }
        return device_detail::read_picking_depths(target);
    }

  private:
    std::shared_ptr<OpenGLDeviceState> state_;
    std::unique_ptr<SharedGraphicsPreparation> shared_preparation_;
};

} // namespace

Result<std::unique_ptr<graphics::Device>> create_device(const DeviceOptions& options) noexcept
{
    if (options.load_procedure == nullptr) {
        return Error{ErrorCode::missing_graphics_procedure_loader, missing_loader_message};
    }

    try {
        static_assert(std::is_same_v<GraphicsProcedure, GLADapiproc>);
        static_assert(std::is_convertible_v<GraphicsProcedureLoader, GLADloadfunc>);

        const int loaded_version = gladLoadGL(options.load_procedure);
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

        GLint fragment_texture_units = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &fragment_texture_units);
        if (fragment_texture_units < 16)
            return Error{ErrorCode::graphics_initialization_failed,
                         "Standard rendering requires sixteen fragment texture units"};
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
