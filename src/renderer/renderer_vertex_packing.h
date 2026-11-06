#pragma once

#include <graphics/mesh_preparation.h>
#include <scene/scene.h>

namespace elf3d::renderer {
inline constexpr std::size_t maximum_vertex_pack_bytes = 128U * 1024U * 1024U;
struct RuntimeVertexDescription {
    graphics::MeshStorageDescription storage;
    std::size_t stride_floats = 0;
    std::size_t vertex_bytes = 0;
    std::size_t index_bytes = 0;
};
[[nodiscard]] Result<RuntimeVertexDescription>
describe_runtime_vertices(const scene::RuntimePrimitiveView& primitive) noexcept;
// Synchronous source/output borrows. Writes complete records only, in a checked
// source range, and rejects an output unit larger than maximum_vertex_pack_bytes.
[[nodiscard]] Result<void> pack_runtime_vertices(const scene::RuntimePrimitiveView& primitive,
                                                 std::size_t first_vertex,
                                                 std::span<float> output) noexcept;
} // namespace elf3d::renderer
