#include <graphics/mesh_preparation.h>

#include <limits>

namespace elf3d::graphics {
Result<std::size_t> mesh_vertex_stride(VertexLayout layout) noexcept
{
    switch (layout) {
    case VertexLayout::position_normal_float3:
        return 6 * sizeof(float);
    case VertexLayout::position_normal_float3_texcoord_float2:
        return 8 * sizeof(float);
    case VertexLayout::position_normal_float3_texcoord2_float2_color_float4:
        return 14 * sizeof(float);
    case VertexLayout::position_normal_float3_texcoord2_float2_color_float4_tangent_float4:
        return 18 * sizeof(float);
    }
    return Error{ErrorCode::unsupported_vertex_layout, "Unsupported mesh vertex layout"};
}
namespace {
[[nodiscard]] Result<MeshStorageDescription>
storage_description(const StaticMeshDescription& source) noexcept
{
    const auto stride = mesh_vertex_stride(source.vertex_layout);
    if (!stride)
        return stride.error();
    if (source.vertex_count == 0 || source.indices.empty() ||
        source.indices.size() > std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::gpu_buffer_creation_failed, "Invalid static mesh element counts"};
    if (source.vertex_count > std::numeric_limits<std::size_t>::max() / stride.value() ||
        source.vertex_bytes.size() != source.vertex_count * stride.value())
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "Invalid static mesh vertex byte count"};
    // The size check above proves representability in this backend-neutral value.
    return MeshStorageDescription{source.vertex_count,
                                  static_cast<std::uint32_t>(source.indices.size()),
                                  source.vertex_layout};
}
} // namespace
Result<std::unique_ptr<StaticMesh>>
complete_static_mesh_upload(Device& device, const StaticMeshDescription& description) noexcept
{
    const auto storage = storage_description(description);
    if (!storage)
        return storage.error();
    auto prepared = device.begin_static_mesh(storage.value());
    if (!prepared)
        return prepared.error();
    const auto indices = std::as_bytes(description.indices);
    for (;;) {
        if (const auto setup =
                prepared.value()->advance_setup(GraphicsPreparationBudget{}.driver_calls);
            !setup)
            return setup.error();
        const auto request = prepared.value()->request();
        if (!request)
            return request.error();
        if (request.value().phase == MeshPreparationPhase::ready)
            return prepared.value()->take_mesh();
        std::span<const std::byte> payload;
        if (request.value().phase == MeshPreparationPhase::vertices)
            payload = description.vertex_bytes.subspan(request.value().offset_bytes,
                                                       request.value().bytes);
        else if (request.value().phase == MeshPreparationPhase::indices)
            payload = indices.subspan(request.value().offset_bytes, request.value().bytes);
        if (const auto advanced = prepared.value()->advance(payload); !advanced)
            return advanced.error();
    }
}
} // namespace elf3d::graphics
