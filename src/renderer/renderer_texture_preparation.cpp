#include "renderer_texture_preparation.h"

#include <algorithm>

namespace elf3d::renderer {
namespace {
using Phase = graphics::TexturePreparationPhase;
[[nodiscard]] Result<TextureWorkStep> begin_texture(graphics::Device& device,
                                                    TextureImagePreparation& state,
                                                    const TextureWorkBudget& budget)
{
    TextureWorkStep step;
    if (budget.driver_calls == 0)
        return step;
    state.attempt.driver_call_limit = 1;
    auto pending = device.begin_texture_2d(state.storage);
    if (!pending)
        return pending.error();
    state.pending = std::move(pending).value();
    state.initialized = true;
    if (state.pending->driver_calls() > 1)
        return Error{ErrorCode::invalid_argument, "Texture begin exceeded its native call bound"};
    step.driver_calls = static_cast<std::uint32_t>(state.pending->driver_calls()); // <= 1
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<TextureWorkStep> adopt_texture(TextureImagePreparation& state)
{
    state.attempt.phase = GraphicsPreparationPhase::texture_setup;
    state.attempt.driver_call_limit = 0;
    auto texture = state.pending->take_texture();
    if (!texture)
        return texture.error();
    state.texture = std::move(texture).value();
    state.pending.reset();
    TextureWorkStep step;
    step.phase = Phase::ready;
    step.ready = true;
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<TextureWorkStep> execute_packet(TextureImagePreparation& state,
                                                     const graphics::TextureUploadRequest& request,
                                                     std::span<const std::byte> pixels)
{
    TextureWorkStep step;
    step.phase = request.phase;
    const auto payload = pixels.subspan(request.offset_bytes, request.bytes);
    state.attempt.phase = request.phase == Phase::pixels ? GraphicsPreparationPhase::texture_upload
                          : request.phase == Phase::mipmaps
                              ? GraphicsPreparationPhase::texture_mipmaps
                              : GraphicsPreparationPhase::texture_setup;
    state.attempt.driver_call_limit = request.driver_call_limit;
    const auto advanced = state.pending->advance(payload);
    if (!advanced)
        return advanced.error();
    const auto& native = advanced.value();
    if (native.uploaded_bytes != payload.size() || native.driver_calls > request.driver_call_limit)
        return Error{ErrorCode::invalid_argument,
                     "Texture backend violated its bounded step report"};
    const auto total = graphics::texture_storage_bytes(state.storage).value();
    if (native.allocated_bytes > total - state.allocated_bytes)
        return Error{ErrorCode::invalid_argument, "Texture allocation report exceeds storage size"};
    state.allocated_bytes += native.allocated_bytes;
    step.uploaded_bytes = native.uploaded_bytes;
    step.allocated_bytes = native.allocated_bytes;
    step.driver_calls = native.driver_calls;
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<TextureWorkStep> advance_pending(TextureImagePreparation& state,
                                                      std::span<const std::byte> pixels,
                                                      const TextureWorkBudget& budget)
{
    state.attempt.driver_call_limit = budget.driver_calls;
    const auto grouped = state.pending->advance_setup(budget.driver_calls);
    if (!grouped)
        return grouped.error();
    if (grouped.value().driver_calls != 0) {
        TextureWorkStep step;
        step.driver_calls = grouped.value().driver_calls;
        step.progressed = true;
        return step;
    }
    auto next = state.pending->request();
    if (!next)
        return next.error();
    if (next.value().phase == Phase::ready)
        return adopt_texture(state);
    TextureWorkStep step;
    step.phase = next.value().phase;
    if (next.value().driver_call_limit > budget.driver_calls)
        return step;
    if (next.value().phase == Phase::pixels) {
        if (budget.upload_bytes < graphics::texture_pixel_bytes(state.storage.format).value())
            return step;
        next = state.pending->request(std::min(
            {budget.upload_bytes, budget.transfer_bytes, graphics::maximum_texture_upload_chunk}));
        if (!next)
            return next.error();
    }
    return execute_packet(state, next.value(), pixels);
}
} // namespace
Result<TextureWorkStep> advance_texture_image(graphics::Device& device,
                                              const graphics::TextureStorageDescription& storage,
                                              std::span<const std::byte> pixels,
                                              TextureImagePreparation& state,
                                              const TextureWorkBudget& budget)
{
    state.attempt = {};
    state.attempt.phase = GraphicsPreparationPhase::texture_setup;
    const auto bytes = graphics::texture_storage_bytes(storage);
    if (!bytes)
        return bytes.error();
    const auto expected =
        storage.initialization == graphics::TextureInitialization::pixels ? bytes.value() : 0;
    if (pixels.size() != expected || (state.initialized && state.storage != storage))
        return Error{ErrorCode::invalid_argument, "Texture preparation source storage changed"};
    if (state.failure)
        return *state.failure;
    if (state.texture) {
        TextureWorkStep ready;
        ready.phase = Phase::ready;
        ready.ready = true;
        return ready;
    }
    state.storage = storage;
    auto step = state.initialized ? advance_pending(state, pixels, budget)
                                  : begin_texture(device, state, budget);
    if (!step)
        state.failure = step.error();
    return step;
}
} // namespace elf3d::renderer
