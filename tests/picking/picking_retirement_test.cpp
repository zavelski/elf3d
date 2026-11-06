#include <picking/picking.h>

#include <array>

int elf3d_picking_behavior_test();
namespace {
using namespace elf3d;
[[nodiscard]] Result<void> queue_cache(picking::PickingService& service)
{
    const auto id = detail::SceneHandleAccess::create_scene(11, 7);
    scene::Storage scene{id};
    const std::array vertices{VertexPositionNormal{{-1, -1, -2}, {0, 0, 1}},
                              VertexPositionNormal{{1, -1, -2}, {0, 0, 1}},
                              VertexPositionNormal{{0, 1, -2}, {0, 0, 1}}};
    const std::array<std::uint32_t, 3> indices{0, 1, 2};
    const auto mesh = scene.create_mesh(MeshDataView{vertices, indices});
    const auto material = scene.create_material({});
    if (!mesh || !material || !scene.create_model(mesh.value(), material.value()))
        return Error{ErrorCode::invalid_argument, "Cannot create picking retirement fixture"};
    const auto hit = service.pick_ray(scene, {{0, 0, 0}, {0, 0, -1}});
    if (!hit || !hit.value() || service.statistics().cached_mesh_bvhs != 1)
        return Error{ErrorCode::invalid_argument, "Picking retirement fixture did not build a BVH"};
    service.release_scene(detail::SceneHandleAccess::create_scene(12, 7));
    if (service.statistics().cached_mesh_bvhs != 1)
        return Error{ErrorCode::invalid_argument, "Foreign release evicted another owner's BVH"};
    service.release_scene(id);
    if (service.statistics().cached_mesh_bvhs != 0)
        return Error{ErrorCode::invalid_argument, "Detached BVH remained in the active cache"};
    return {};
} // Source geometry is destroyed before any retired-array advancement.
[[nodiscard]] bool check_final_state(picking::PickingService& service, std::uint64_t released,
                                     std::uint32_t buffers, std::uint64_t expected)
{
    const auto final = service.retirement_status();
    return final.scenes == 0 && final.meshes == 0 && final.bytes == 0 && buffers == 6 &&
           released == expected && !service.advance_retirement().progressed;
}
[[nodiscard]] bool check_retirement()
{
    picking::PickingService service;
    if (!queue_cache(service) || !queue_cache(service))
        return false;
    const auto initial = service.retirement_status();
    if (initial.scenes != 2 || initial.meshes != 2 || initial.bytes == 0)
        return false;
    std::uint64_t released = 0;
    std::uint32_t buffers = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        const auto step = service.advance_retirement();
        if (!step.progressed)
            return false;
        released += step.freed_bytes;
        if (step.freed_bytes != 0)
            ++buffers;
    }
    return check_final_state(service, released, buffers, initial.bytes);
}
} // namespace
int elf3d_picking_test()
{
    const auto previous = elf3d_picking_behavior_test();
    return previous == 0 ? (check_retirement() ? 0 : 32) : previous;
}
