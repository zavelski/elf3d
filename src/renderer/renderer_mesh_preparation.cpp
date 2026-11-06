#include "renderer_mesh_preparation.h"

#include <algorithm>

namespace elf3d::renderer {
namespace {
using Phase = graphics::MeshPreparationPhase;
[[nodiscard]] bool same_source(const MeshGeometryPreparation& state,
                               const scene::RuntimePrimitiveView& source,
                               const RuntimeVertexDescription& description) noexcept
{
    const auto& stored = state.description.storage;
    const auto& current = description.storage;
    return state.source_mesh == source.mesh &&
           state.source_primitive == source.document_primitive &&
           state.source_geometry == source.geometry_identity &&
           stored.vertex_count == current.vertex_count &&
           stored.index_count == current.index_count &&
           stored.vertex_layout == current.vertex_layout;
}
[[nodiscard]] Result<MeshWorkStep> begin_mesh(graphics::Device& device,
                                              const scene::RuntimePrimitiveView& source,
                                              MeshGeometryPreparation& state,
                                              const MeshWorkBudget& budget)
{
    MeshWorkStep step{MeshWorkPhase::begin};
    if (budget.driver_calls == 0)
        return step;
    state.source_mesh = source.mesh;
    state.source_primitive = source.document_primitive;
    state.source_geometry = source.geometry_identity;
    state.initialized = true;
    state.attempt.driver_call_limit = 1;
    auto pending = device.begin_static_mesh(state.description.storage);
    if (!pending)
        return pending.error();
    state.pending = std::move(pending).value();
    const auto calls = state.pending->driver_calls();
    if (calls > 1)
        return Error{ErrorCode::invalid_argument,
                     "Mesh begin exceeded its context-validation call bound"};
    step.driver_calls = static_cast<std::uint32_t>(calls); // checked <= 1
    step.progressed = true;
    return step;
}
[[nodiscard]] MeshWorkStep allocate_staging(MeshGeometryPreparation& state,
                                            const graphics::MeshUploadRequest& request)
{
    state.attempt.phase = GraphicsPreparationPhase::vertex_packing;
    state.attempt.driver_call_limit = 0;
    const auto floats = request.bytes / sizeof(float);
    if (state.staging.capacity() < floats) {
        // All previous bytes were uploaded. Free before allocating a larger
        // chunk, without a transient second owned staging buffer.
        std::vector<float>{}.swap(state.staging);
        std::vector<float>(floats).swap(state.staging);
    } else {
        state.staging.resize(floats);
    }
    state.staging_offset = request.offset_bytes;
    state.filled_floats = 0;
    state.sent_floats = 0;
    MeshWorkStep step{MeshWorkPhase::staging, request.phase};
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<MeshWorkStep> pack_staging(const scene::RuntimePrimitiveView& source,
                                                MeshGeometryPreparation& state,
                                                const MeshWorkBudget& budget)
{
    state.attempt.phase = GraphicsPreparationPhase::vertex_packing;
    state.attempt.driver_call_limit = 0;
    const auto stride = state.description.stride_floats;
    const auto unit =
        std::min(budget.packing_bytes, maximum_vertex_pack_bytes) / sizeof(float) / stride * stride;
    if (unit == 0)
        return MeshWorkStep{MeshWorkPhase::packing, Phase::vertices};
    const auto count = std::min(unit, state.staging.size() - state.filled_floats);
    const auto first = state.staging_offset / sizeof(float) / stride + state.filled_floats / stride;
    const auto output = std::span{state.staging}.subspan(state.filled_floats, count);
    if (const auto packed = pack_runtime_vertices(source, first, output); !packed)
        return packed.error();
    state.filled_floats += count;
    MeshWorkStep step{MeshWorkPhase::packing, Phase::vertices};
    step.packed_bytes = output.size_bytes();
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<MeshWorkStep> native_step(MeshGeometryPreparation& state,
                                               const graphics::MeshUploadRequest& request,
                                               std::span<const std::byte> bytes)
{
    state.attempt.phase = request.phase == Phase::vertices ? GraphicsPreparationPhase::vertex_upload
                          : request.phase == Phase::indices ? GraphicsPreparationPhase::index_upload
                                                            : GraphicsPreparationPhase::mesh_setup;
    state.attempt.driver_call_limit = request.driver_call_limit;
    const auto result = state.pending->advance(bytes);
    if (!result)
        return result.error();
    const auto& native = result.value();
    if (native.driver_calls > request.driver_call_limit || native.uploaded_bytes != bytes.size())
        return Error{ErrorCode::invalid_argument, "Mesh backend violated its bounded step report"};
    // Counts are uint32 and the largest layout is 84 bytes: total < 2^39.
    const auto total =
        static_cast<std::uint64_t>(state.description.vertex_bytes) + state.description.index_bytes;
    if (native.allocated_bytes > total - state.allocated_bytes)
        return Error{ErrorCode::invalid_argument,
                     "Mesh allocation report exceeds its storage size"};
    state.allocated_bytes += native.allocated_bytes;
    MeshWorkStep step{MeshWorkPhase::gpu, request.phase};
    step.driver_calls = native.driver_calls;
    step.uploaded_bytes = native.uploaded_bytes;
    step.allocated_bytes = native.allocated_bytes;
    step.progressed = true;
    return step;
}
[[nodiscard]] Result<MeshWorkStep> upload_staging(MeshGeometryPreparation& state,
                                                  const graphics::MeshUploadRequest& request,
                                                  const MeshWorkBudget& budget)
{
    const auto stride_bytes = state.description.stride_floats * sizeof(float);
    if (request.offset_bytes != state.staging_offset + state.sent_floats * sizeof(float))
        return Error{ErrorCode::invalid_argument, "Mesh staging and native upload offsets differ"};
    const auto available = (state.staging.size() - state.sent_floats) * sizeof(float);
    const auto size = std::min({available, request.bytes, budget.upload_bytes});
    const auto bytes = size / stride_bytes * stride_bytes;
    if (bytes == 0)
        return MeshWorkStep{MeshWorkPhase::gpu, request.phase};
    const auto payload =
        std::as_bytes(std::span{state.staging}).subspan(state.sent_floats * sizeof(float), bytes);
    auto step = native_step(state, request, payload);
    if (!step)
        return step.error();
    state.sent_floats += bytes / sizeof(float);
    if (state.sent_floats == state.staging.size()) {
        state.staging.clear();
        state.filled_floats = 0;
        state.sent_floats = 0;
    }
    return std::move(step).value();
}
[[nodiscard]] Result<MeshWorkStep> adopt_mesh(MeshGeometryPreparation& state)
{
    state.attempt.phase = GraphicsPreparationPhase::mesh_setup;
    state.attempt.driver_call_limit = 0;
    auto mesh = state.pending->take_mesh();
    if (!mesh)
        return mesh.error();
    state.mesh = std::move(mesh).value();
    state.pending.reset();
    std::vector<float>{}.swap(state.staging);
    MeshWorkStep step{MeshWorkPhase::adopt, Phase::ready};
    step.progressed = true;
    step.ready = true;
    return step;
}
[[nodiscard]] Result<MeshWorkStep> advance_indices(const scene::RuntimePrimitiveView& source,
                                                   MeshGeometryPreparation& state,
                                                   const graphics::MeshUploadRequest& request,
                                                   const MeshWorkBudget& budget)
{
    const auto size = std::min(request.bytes, budget.upload_bytes) / sizeof(std::uint32_t) *
                      sizeof(std::uint32_t);
    if (size == 0)
        return MeshWorkStep{MeshWorkPhase::gpu, request.phase};
    const auto indices = std::as_bytes(source.indices()).subspan(request.offset_bytes, size);
    return native_step(state, request, indices);
}
[[nodiscard]] Result<graphics::MeshUploadRequest>
mesh_request(graphics::StaticMeshPreparation& pending, const MeshWorkBudget& budget)
{
    auto request = pending.request(budget.transfer_bytes);
    if (!request)
        return request.error();
    if (request.value().phase == Phase::vertices &&
        request.value().bytes > maximum_vertex_pack_bytes)
        return pending.request(maximum_vertex_pack_bytes);
    return request;
}
[[nodiscard]] Result<MeshWorkStep> advance_pending(const scene::RuntimePrimitiveView& source,
                                                   MeshGeometryPreparation& state,
                                                   const MeshWorkBudget& budget)
{
    const auto request = mesh_request(*state.pending, budget);
    if (!request)
        return request.error();
    const auto& next = request.value();
    if (next.phase == Phase::ready)
        return adopt_mesh(state);
    if (next.phase == Phase::vertices) {
        if (state.staging.empty())
            return allocate_staging(state, next);
        if (state.filled_floats < state.staging.size())
            return pack_staging(source, state, budget);
    }
    if (next.driver_call_limit > budget.driver_calls)
        return MeshWorkStep{MeshWorkPhase::gpu, next.phase};
    if (next.phase == Phase::vertices)
        return upload_staging(state, next, budget);
    if (next.phase == Phase::indices)
        return advance_indices(source, state, next, budget);
    return native_step(state, next, {});
}
[[nodiscard]] Result<MeshWorkStep>
advance_initialized_mesh(const scene::RuntimePrimitiveView& source, MeshGeometryPreparation& state,
                         const MeshWorkBudget& budget)
{
    state.attempt.driver_call_limit = budget.driver_calls;
    const auto grouped = state.pending->advance_setup(budget.driver_calls);
    if (!grouped)
        return grouped.error();
    if (grouped.value().driver_calls != 0) {
        MeshWorkStep step{MeshWorkPhase::gpu};
        step.driver_calls = grouped.value().driver_calls;
        step.progressed = true;
        return step;
    }
    return advance_pending(source, state, budget);
}

} // namespace
Result<MeshWorkStep> advance_mesh_geometry(graphics::Device& device,
                                           const scene::RuntimePrimitiveView& primitive,
                                           MeshGeometryPreparation& state,
                                           const MeshWorkBudget& budget)
{
    state.attempt = {};
    state.attempt.phase = GraphicsPreparationPhase::mesh_setup;
    const auto description = describe_runtime_vertices(primitive);
    if (!description)
        return description.error();
    if (state.initialized && !same_source(state, primitive, description.value()))
        return Error{ErrorCode::invalid_argument,
                     "Mesh preparation belongs to another runtime source"};
    if (state.failure)
        return *state.failure;
    if (state.mesh) {
        MeshWorkStep step{MeshWorkPhase::ready, Phase::ready};
        step.ready = true;
        return step;
    }
    state.description = description.value();
    auto step = state.initialized ? advance_initialized_mesh(primitive, state, budget)
                                  : begin_mesh(device, primitive, state, budget);
    if (!step)
        state.failure = step.error();
    return step;
}
} // namespace elf3d::renderer
