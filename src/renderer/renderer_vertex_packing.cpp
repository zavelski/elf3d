#include "renderer_vertex_packing.h"

#include <limits>

namespace elf3d::renderer {
namespace {
[[nodiscard]] Result<graphics::VertexLayout>
vertex_layout(scene::RuntimeVertexLayout layout) noexcept
{
    switch (layout) {
    case scene::RuntimeVertexLayout::position_normal:
        return graphics::VertexLayout::position_normal_float3;
    case scene::RuntimeVertexLayout::textured:
        return graphics::VertexLayout::position_normal_float3_texcoord_float2;
    case scene::RuntimeVertexLayout::full:
        return graphics::VertexLayout::position_normal_float3_texcoord2_float2_color_float4;
    case scene::RuntimeVertexLayout::tangent:
        return graphics::VertexLayout::
            position_normal_float3_texcoord2_float2_color_float4_tangent_float4;
    }
    return Error{ErrorCode::unsupported_vertex_layout, "Unsupported runtime vertex layout"};
}
void write_vertex(const scene::RuntimePrimitiveView& primitive, std::size_t index,
                  std::span<float> output) noexcept
{
    const auto position = primitive.position(index);
    const auto normal = primitive.normal(index);
    output[0] = position.x;
    output[1] = position.y;
    output[2] = position.z;
    output[3] = normal.x;
    output[4] = normal.y;
    output[5] = normal.z;
    if (output.size() >= 8) {
        const auto texcoord = primitive.texcoord0(index);
        output[6] = texcoord.x;
        output[7] = texcoord.y;
    }
    if (output.size() >= 14) {
        const auto texcoord = primitive.texcoord1(index);
        const auto color = primitive.color(index);
        output[8] = texcoord.x;
        output[9] = texcoord.y;
        output[10] = color.red;
        output[11] = color.green;
        output[12] = color.blue;
        output[13] = color.alpha;
    }
    if (output.size() >= 18) {
        const auto tangent = primitive.tangent(index);
        output[14] = tangent.x;
        output[15] = tangent.y;
        output[16] = tangent.z;
        output[17] = tangent.w;
    }
}
} // namespace
Result<RuntimeVertexDescription>
describe_runtime_vertices(const scene::RuntimePrimitiveView& primitive) noexcept
{
    const auto layout = vertex_layout(primitive.vertex_layout());
    if (!layout)
        return layout.error();
    const auto stride = graphics::mesh_vertex_stride(layout.value());
    if (!stride)
        return stride.error();
    const auto vertices = primitive.vertex_count();
    const auto indices = primitive.indices().size();
    if (vertices == 0 || indices == 0 || vertices > std::numeric_limits<std::uint32_t>::max() ||
        indices > std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::gpu_buffer_creation_failed,
                     "Runtime mesh counts exceed the graphics contract"};
    if (vertices > std::numeric_limits<std::size_t>::max() / stride.value() ||
        indices > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t))
        return Error{ErrorCode::size_overflow, "Runtime mesh storage size overflows"};
    // Both counts were checked against the destination representation above.
    const graphics::MeshStorageDescription storage{
        static_cast<std::uint32_t>(vertices), static_cast<std::uint32_t>(indices), layout.value()};
    return RuntimeVertexDescription{storage, stride.value() / sizeof(float),
                                    vertices * stride.value(), indices * sizeof(std::uint32_t)};
}
Result<void> pack_runtime_vertices(const scene::RuntimePrimitiveView& primitive,
                                   std::size_t first_vertex, std::span<float> output) noexcept
{
    const auto description = describe_runtime_vertices(primitive);
    if (!description)
        return description.error();
    const auto stride = description.value().stride_floats;
    if (output.size() > maximum_vertex_pack_bytes / sizeof(float) || output.size() % stride != 0)
        return Error{ErrorCode::invalid_argument,
                     "Vertex packing requires a bounded complete-record output"};
    const auto count = output.size() / stride;
    const auto available = primitive.vertex_count();
    if (first_vertex > available || count > available - first_vertex)
        return Error{ErrorCode::invalid_argument,
                     "Vertex packing range is outside the runtime mesh"};
    for (std::size_t i = 0; i < count; ++i)
        write_vertex(primitive, first_vertex + i, output.subspan(i * stride, stride));
    return {};
}
} // namespace elf3d::renderer
