#include <renderer/renderer.h>
#include <renderer/renderer_mesh_preparation.h>
#include <scene/scene.h>

#include "renderer_test_support.h"

#include <array>
#include <limits>

namespace {
using namespace elf3d;
using namespace elf3d::renderer;
using namespace elf3d::renderer::tests;
constexpr std::size_t vertices_count = 35000;
constexpr std::size_t indices_count = 120000;
[[nodiscard]] Error failure(const char* message)
{
    return {ErrorCode::invalid_argument, message};
}
[[nodiscard]] std::array<float, 18> expected_vertex(std::size_t index)
{
    // Independent fixture format: position, normal, UV0, UV1, RGBA and tangent.
    return {static_cast<float>(index),
            static_cast<float>(index % 5),
            static_cast<float>(index % 7),
            0,
            0,
            1,
            static_cast<float>(index % 2),
            .25F,
            .5F,
            static_cast<float>(index % 3),
            .1F,
            .2F,
            .3F,
            1,
            1,
            0,
            0,
            index % 2 == 0 ? 1.0F : -1.0F};
}
[[nodiscard]] PrimitiveData geometry()
{
    PrimitiveData data;
    data.positions.reserve(vertices_count);
    data.normals.reserve(vertices_count);
    data.texcoord0.reserve(vertices_count);
    data.texcoord1.reserve(vertices_count);
    data.colors.reserve(vertices_count);
    data.tangents.reserve(vertices_count);
    for (std::size_t i = 0; i < vertices_count; ++i) {
        const auto v = expected_vertex(i);
        data.positions.push_back({v[0], v[1], v[2]});
        data.normals.push_back({0, 0, 1});
        data.texcoord0.push_back({v[6], v[7]});
        data.texcoord1.push_back({v[8], v[9]});
        data.colors.push_back({.1F, .2F, .3F, 1});
        data.tangents.push_back({1, 0, 0, v[17]});
    }
    data.indices.reserve(indices_count);
    for (std::size_t i = 0; i < indices_count; ++i)
        data.indices.push_back(static_cast<std::uint32_t>(i % vertices_count)); // < 35000
    return data;
}
[[nodiscard]] Result<EntityId> prepare_scene(scene::Storage& storage)
{
    Document document;
    const auto material = document.create_material({}).value();
    const auto mesh = document.create_mesh("Large packed mesh").value();
    const auto primitive = document.create_primitive(mesh, material, geometry());
    if (!primitive)
        return primitive.error();
    if (const auto adopted = storage.set_document(std::move(document)); !adopted)
        return adopted.error();
    const auto entity = storage.create_entity();
    if (!entity)
        return entity.error();
    const std::array primitives{primitive.value()};
    if (const auto bound = storage.set_model_document_primitives(entity.value(), primitives);
        !bound)
        return bound.error();
    return entity.value();
}
void hash_bytes(std::uint64_t& digest, std::span<const std::byte> bytes)
{
    for (const auto byte : bytes) {
        digest ^= std::to_integer<std::uint8_t>(byte);
        digest *= 1099511628211ULL;
    }
}
struct Digests {
    std::uint64_t vertices = 0;
    std::uint64_t indices = 0;
    bool captured = false;
};
[[nodiscard]] Digests expected_digests()
{
    Digests result{14695981039346656037ULL, 14695981039346656037ULL, true};
    for (std::size_t i = 0; i < vertices_count; ++i) {
        const auto vertex = expected_vertex(i);
        hash_bytes(result.vertices, std::as_bytes(std::span{vertex}));
    }
    for (std::size_t i = 0; i < indices_count; ++i) {
        const auto index = static_cast<std::uint32_t>(i % vertices_count);
        hash_bytes(result.indices, std::as_bytes(std::span{&index, 1}));
    }
    return result;
}
void capture_digest(const MeshGeometryPreparation& state, Digests& result)
{
    if (!state.pending)
        return;
    const auto next = state.pending->request();
    if (!next || next.value().phase != graphics::MeshPreparationPhase::ready)
        return;
    // The fixture's FakeDevice constructs exactly this concrete preparation.
    const auto& fake = static_cast<const graphics::tests::FakeMeshPreparation&>(*state.pending);
    result = {fake.vertex_digest(), fake.index_digest(), true};
}
[[nodiscard]] Result<void> reject_foreign_source(FakeDevice& device,
                                                 const scene::RuntimePrimitiveView& primitive,
                                                 MeshGeometryPreparation& state)
{
    auto foreign = primitive; // Synchronous views remain within their live Storage owner.
    foreign.geometry_identity ^= 1;
    const auto calls = state.pending->driver_calls();
    const auto filled = state.filled_floats;
    if (advance_mesh_geometry(device, foreign, state) || state.failure ||
        state.pending->driver_calls() != calls || state.filled_floats != filled)
        return failure("A foreign mesh source changed the accepted preparation");
    return {};
}
[[nodiscard]] Result<void> check_bounds(const MeshGeometryPreparation& state,
                                        const MeshWorkStep& step, const MeshWorkBudget& budget)
{
    if (state.staging.size() * sizeof(float) > graphics::maximum_mesh_upload_chunk ||
        state.filled_floats > state.staging.size() || state.sent_floats > state.filled_floats)
        return failure("Renderer staging ownership exceeded its bounds");
    if (step.packed_bytes > maximum_vertex_pack_bytes ||
        step.uploaded_bytes > budget.upload_bytes || step.driver_calls > budget.driver_calls)
        return failure("Renderer mesh preparation exceeded its work allowance");
    return {};
}
[[nodiscard]] Result<Digests> complete(FakeDevice& device,
                                       const scene::RuntimePrimitiveView& primitive,
                                       MeshGeometryPreparation& state, const MeshWorkBudget& budget)
{
    Digests observed;
    // Forces partial consumption when a small budget is selected.
    for (std::size_t i = 0; i < 2048; ++i) {
        capture_digest(state, observed);
        const auto step = advance_mesh_geometry(device, primitive, state, budget);
        if (!step)
            return step.error();
        if (const auto bounds = check_bounds(state, step.value(), budget); !bounds)
            return bounds.error();
        if (step.value().ready)
            return observed;
        if (!step.value().progressed)
            return failure("Renderer mesh preparation stalled with valid allowances");
    }
    return failure("Renderer mesh preparation did not finish its bounded units");
}
[[nodiscard]] Result<void> reject_bad_pack(const scene::RuntimePrimitiveView& primitive)
{
    std::array<float, 18> output;
    output.fill(42);
    if (pack_runtime_vertices(primitive, vertices_count, output) || output.front() != 42 ||
        pack_runtime_vertices(primitive, 0, std::span{output}.first(17)))
        return failure("Invalid vertex packing range/alignment changed output");
    return {};
}
[[nodiscard]] Result<void> begin_checked(FakeDevice& device,
                                         const scene::RuntimePrimitiveView& primitive,
                                         MeshGeometryPreparation& state)
{
    const auto idle = advance_mesh_geometry(device, primitive, state, {0, 0});
    if (!idle || idle.value().progressed || state.initialized || device.state().upload_count != 0)
        return failure("Zero allowance admitted native mesh work");
    if (!advance_mesh_geometry(device, primitive, state))
        return failure("Renderer mesh preparation did not begin");
    return reject_foreign_source(device, primitive, state);
}
[[nodiscard]] Result<void> check_ready(FakeDevice& device,
                                       const scene::RuntimePrimitiveView& primitive,
                                       MeshGeometryPreparation& state)
{
    if (!state.mesh || state.pending || state.staging.capacity() != 0)
        return failure("Ready mesh retained a source preparation/staging allocation");
    const auto repeated = advance_mesh_geometry(device, primitive, state);
    if (!repeated || !repeated.value().ready || repeated.value().progressed ||
        device.state().upload_count != 1)
        return failure("A ready mesh was uploaded more than once");
    return reject_bad_pack(primitive);
}
[[nodiscard]] Result<void> verify_bulk_digest(const scene::RuntimePrimitiveView& primitive,
                                              const Digests& expected)
{
    FakeDevice bulk_device;
    MeshGeometryPreparation bulk;
    const auto large = complete(bulk_device, primitive, bulk, {});
    if (!large || !large.value().captured || large.value().vertices != expected.vertices ||
        large.value().indices != expected.indices)
        return failure("Enlarged default portions changed geometry payloads");
    return check_ready(bulk_device, primitive, bulk);
}
[[nodiscard]] Result<void> verify(const scene::RuntimePrimitiveView& primitive)
{
    FakeDevice device;
    MeshGeometryPreparation state;
    if (const auto begun = begin_checked(device, primitive, state); !begun)
        return begun.error();
    const auto observed =
        complete(device, primitive, state, {4096, 8, 64U * 1024U, 2U * 1024U * 1024U});
    if (!observed)
        return observed.error();
    const auto expected = expected_digests();
    if (!observed.value().captured || observed.value().vertices != expected.vertices ||
        observed.value().indices != expected.indices)
        return failure("Bounded packing/upload changed attribute order or payload bytes");
    if (const auto ready = check_ready(device, primitive, state); !ready)
        return ready.error();
    return verify_bulk_digest(primitive, expected);
}
} // namespace
int elf3d_renderer_mesh_preparation_test()
{
    const auto id = elf3d::detail::SceneHandleAccess::create_scene(11, 8);
    elf3d::scene::Storage storage{id};
    const auto entity = prepare_scene(storage);
    if (!entity)
        return 21;
    const auto primitive = storage.runtime_primitive(entity.value(), 0);
    if (!primitive)
        return 22;
    return verify(primitive.value()) ? 0 : 23;
}
