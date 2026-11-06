#pragma once
#include <array>
#include <graphics/pipeline_preparation.h>
#include <utility>
namespace elf3d::graphics::tests {
class FakeAuxiliaryGraphics final {
  public:
    [[nodiscard]] AuxiliaryGraphicsStatus status() const noexcept
    {
        return {ready_[0], ready_[1], ready_[2], retiring_, pending_};
    }
    void fail(ShaderPipelineKind kind) noexcept
    {
        fail_kind_ = kind;
    }
    [[nodiscard]] Result<PipelinePreparationRequest> request(ShaderPipelineKind kind) const noexcept
    {
        if (kind < ShaderPipelineKind::picking || kind > ShaderPipelineKind::display_resolve)
            return Error{ErrorCode::invalid_argument, "Expected a fake auxiliary shader"};
        if (retiring_)
            return Error{ErrorCode::invalid_argument, "Fake auxiliary shader requires retirement"};
        return PipelinePreparationRequest{ready_[index(kind)]
                                              ? PipelinePreparationPhase::ready
                                              : PipelinePreparationPhase::vertex_compile,
                                          0};
    }
    [[nodiscard]] Result<PipelinePreparationStep> advance(ShaderPipelineKind kind) noexcept
    {
        const auto next = request(kind);
        if (!next)
            return next.error();
        if (fail_kind_ == kind) {
            fail_kind_.reset();
            retiring_ = true;
            pending_ = kind;
            return Error{ErrorCode::graphics_initialization_failed,
                         "Injected auxiliary shader failure"};
        }
        ready_[index(kind)] = true;
        return PipelinePreparationStep{0, true};
    }
    [[nodiscard]] Result<ResourceRetirementStep>
    retire(const ResourceRetirementBudget& budget) noexcept
    {
        if (!retiring_)
            return ResourceRetirementStep{0, 0, true, false};
        if (budget.objects == 0)
            return ResourceRetirementStep{};
        retiring_ = false;
        pending_.reset();
        return ResourceRetirementStep{1, 0, true, true};
    }

  private:
    [[nodiscard]] static std::size_t index(ShaderPipelineKind kind) noexcept
    {
        return static_cast<std::size_t>(kind) -
               static_cast<std::size_t>(ShaderPipelineKind::picking);
    }
    std::array<bool, 3> ready_{};
    std::optional<ShaderPipelineKind> fail_kind_;
    std::optional<ShaderPipelineKind> pending_;
    bool retiring_ = false;
};
class FakePipelinePreparation final : public GraphicsPipelinePreparation {
  public:
    explicit FakePipelinePreparation(std::unique_ptr<GraphicsPipeline> pipeline,
                                     bool fail = false) noexcept
        : pipeline_(std::move(pipeline)), fail_(fail)
    {
    }
    [[nodiscard]] Result<PipelinePreparationRequest> request() const noexcept override
    {
        if (!pipeline_)
            return Error{ErrorCode::invalid_argument, "Fake pipeline owner was consumed"};
        return PipelinePreparationRequest{
            ready_ ? PipelinePreparationPhase::ready : PipelinePreparationPhase::vertex_compile, 0};
    }
    [[nodiscard]] Result<PipelinePreparationStep> advance() noexcept override
    {
        const auto next = request();
        if (!next)
            return next.error();
        if (fail_)
            return Error{ErrorCode::graphics_initialization_failed,
                         "Injected shader preparation failure"};
        ready_ = true;
        return PipelinePreparationStep{0, true};
    }
    [[nodiscard]] std::uint64_t driver_calls() const noexcept override
    {
        return 0;
    }
    [[nodiscard]] Result<ResourceRetirementStep>
    advance_retirement(const ResourceRetirementBudget& budget) noexcept override
    {
        if (pipeline_ && budget.objects == 0)
            return ResourceRetirementStep{};
        const bool existed = bool(pipeline_);
        pipeline_.reset();
        return ResourceRetirementStep{existed ? 1U : 0U, 0, true, existed};
    }
    [[nodiscard]] Result<std::unique_ptr<GraphicsPipeline>> take_pipeline() noexcept override
    {
        if (!ready_ || !pipeline_)
            return Error{ErrorCode::invalid_argument, "Fake pipeline is not ready"};
        return std::move(pipeline_);
    }

  private:
    std::unique_ptr<GraphicsPipeline> pipeline_;
    bool ready_ = false;
    bool fail_ = false;
};
} // namespace elf3d::graphics::tests
