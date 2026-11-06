#include <graphics/pipeline_preparation.h>

namespace elf3d::graphics {
namespace {
[[nodiscard]] Result<void> retire_pipeline(GraphicsPipelinePreparation& preparation) noexcept
{
    for (;;) {
        const auto step = preparation.advance_retirement({});
        if (!step)
            return step.error();
        if (step.value().complete)
            return {};
        if (!step.value().progressed)
            return Error{ErrorCode::invalid_argument,
                         "Synchronous pipeline retirement did not advance"};
    }
}
} // namespace
Result<std::unique_ptr<GraphicsPipeline>>
complete_pipeline_preparation(GraphicsPipelinePreparation& preparation) noexcept
{
    for (;;) {
        auto step = preparation.advance();
        if (!step) {
            const auto error = step.error();
            const auto retired = retire_pipeline(preparation);
            return retired ? Result<std::unique_ptr<GraphicsPipeline>>{error}
                           : Result<std::unique_ptr<GraphicsPipeline>>{retired.error()};
        }
        if (step.value().ready)
            return preparation.take_pipeline();
    }
}
} // namespace elf3d::graphics
