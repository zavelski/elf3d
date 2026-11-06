#include "picking_cache.h"

#include <elf3d/core/assert.h>
#include <limits>
#include <tuple>

namespace elf3d::picking {
namespace {
using acceleration_detail::MeshAcceleration;
void add_bytes(std::uint64_t& total, std::uint64_t additional) noexcept
{
    if (additional > std::numeric_limits<std::uint64_t>::max() - total)
        fatal_error("Picking cache byte count exhausted");
    total += additional;
}
[[nodiscard]] std::uint64_t acceleration_bytes(const MeshAcceleration& acceleration) noexcept
{
    // Each capacity allocation is representable in size_t by the vector's own
    // max_size contract. The aggregate is checked separately below.
    std::uint64_t bytes =
        acceleration.triangles.capacity() * sizeof(acceleration_detail::TriangleReference);
    add_bytes(bytes, acceleration.triangle_order.capacity() * sizeof(std::uint32_t));
    add_bytes(bytes, acceleration.nodes.capacity() * sizeof(acceleration_detail::BvhNode));
    return bytes;
}
[[nodiscard]] PickingRetirementStep release_buffer(MeshAcceleration& acceleration)
{
    PickingRetirementStep step;
    step.progressed = true;
    if (acceleration.triangles.capacity() != 0) {
        step.phase = PickingRetirementPhase::triangles;
        step.freed_bytes =
            acceleration.triangles.capacity() * sizeof(acceleration_detail::TriangleReference);
        std::vector<acceleration_detail::TriangleReference>{}.swap(acceleration.triangles);
    } else if (acceleration.triangle_order.capacity() != 0) {
        step.phase = PickingRetirementPhase::order;
        step.freed_bytes = acceleration.triangle_order.capacity() * sizeof(std::uint32_t);
        std::vector<std::uint32_t>{}.swap(acceleration.triangle_order);
    } else if (acceleration.nodes.capacity() != 0) {
        step.phase = PickingRetirementPhase::nodes;
        step.freed_bytes = acceleration.nodes.capacity() * sizeof(acceleration_detail::BvhNode);
        std::vector<acceleration_detail::BvhNode>{}.swap(acceleration.nodes);
    }
    return step;
}
} // namespace
bool MeshCache::SceneLess::operator()(const SceneKey& a, const SceneKey& b) const noexcept
{
    const auto first = std::tuple{detail::SceneHandleAccess::engine_token(a.scene),
                                  a.scene.debug_value(), a.generation};
    const auto second = std::tuple{detail::SceneHandleAccess::engine_token(b.scene),
                                   b.scene.debug_value(), b.generation};
    return first < second;
}
std::uint64_t MeshCache::size() const noexcept
{
    return mesh_count_;
}
Result<const MeshAcceleration*> MeshCache::acquire(SceneId scene_id,
                                                   const scene::RuntimePrimitiveView& primitive,
                                                   PickingStatistics& statistics)
{
    const bool document = primitive.document_primitive.is_valid();
    const auto geometry =
        document ? primitive.document_primitive.debug_value() : primitive.mesh.debug_value();
    const std::pair key{document, geometry};
    auto& group = active_[{scene_id, 0}];
    const auto existing = group.meshes.find(key);
    if (existing != group.meshes.end())
        return &existing->second;
    auto built = acceleration_detail::build_acceleration(primitive);
    if (!built)
        return built.error();
    const auto bytes = acceleration_bytes(built.value());
    const auto inserted = group.meshes.emplace(key, std::move(built).value());
    ELF3D_ASSERT(inserted.second);
    add_bytes(group.bytes, bytes);
    ++mesh_count_;
    ++statistics.latest_bvh_builds;
    ++statistics.lifetime_bvh_builds;
    statistics.cached_mesh_bvhs = mesh_count_;
    return &inserted.first->second;
}
void MeshCache::release(SceneId scene) noexcept
{
    const auto found = active_.find({scene, 0});
    if (found == active_.end())
        return;
    const auto count = found->second.meshes.size();
    if (count == 0) {
        active_.erase(found);
        return;
    }
    if (retirement_sequence_ == std::numeric_limits<std::uint64_t>::max())
        fatal_error("Picking cache retirement sequence exhausted");
    mesh_count_ -= count;
    retired_status_.meshes += count;
    add_bytes(retired_status_.bytes, found->second.bytes);
    auto node = active_.extract(found);
    node.key().generation = ++retirement_sequence_;
    const auto inserted = retired_.insert(std::move(node));
    ELF3D_ASSERT(inserted.inserted);
    retired_status_.scenes = retired_.size();
}
PickingRetirementStatus MeshCache::retirement_status() const noexcept
{
    return retired_status_;
}
PickingRetirementStep MeshCache::advance_retirement()
{
    if (retired_.empty())
        return {};
    auto group = retired_.begin();
    auto mesh = group->second.meshes.begin();
    auto step = release_buffer(mesh->second);
    step.scene = group->first.scene;
    retired_status_.bytes -= step.freed_bytes;
    group->second.bytes -= step.freed_bytes;
    if (step.freed_bytes == 0) {
        group->second.meshes.erase(mesh); // all three owned arrays are empty
        --retired_status_.meshes;
        if (group->second.meshes.empty())
            retired_.erase(group);
    }
    retired_status_.scenes = retired_.size();
    return step;
}
} // namespace elf3d::picking
