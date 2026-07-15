module;

#include <elf3d/core/result.h>
#include <elf3d/picking.h>

#include <memory>
#include <optional>

export module elf.picking;

import elf.clipping;
import elf.core;
import elf.scene;

export namespace elf3d::picking {

struct RayBoundsHit {
    float entry_distance = 0.0F;
    float exit_distance = 0.0F;
};

struct TriangleHit {
    float distance = 0.0F;
    std::uint32_t triangle_index = 0;
    Float3 barycentric_coordinates;
    Float3 geometric_normal;
};

struct PickCandidate {
    EntityId entity;
    MeshHandle mesh;
    std::uint32_t primitive_index = 0;
    std::uint32_t triangle_index = 0;
};

struct PickRequest {
    EntityId camera;
    Extent2D extent;
    Float2 position_pixels;
    PickOptions options;
};

[[nodiscard]] bool is_valid_ray(const Ray3& ray) noexcept;
[[nodiscard]] bool intersect_ray_bounds(const Ray3& ray, Bounds3 bounds,
                                        RayBoundsHit& hit) noexcept;
[[nodiscard]] std::optional<TriangleHit>
intersect_ray_triangle(const Ray3& ray, Float3 a, Float3 b, Float3 c, bool cull_back_face) noexcept;

class PickingService final {
  public:
    PickingService();
    ~PickingService();

    PickingService(const PickingService&) = delete;
    PickingService& operator=(const PickingService&) = delete;
    PickingService(PickingService&&) noexcept;
    PickingService& operator=(PickingService&&) noexcept;

    [[nodiscard]] Result<Ray3> make_picking_ray(const scene::Storage& scene, EntityId camera,
                                                Extent2D extent, Float2 position_pixels) const;
    [[nodiscard]] Result<std::optional<PickHit>> pick(const scene::Storage& scene,
                                                      const PickRequest& request);
    [[nodiscard]] Result<std::optional<PickHit>> pick(const scene::Storage& scene,
                                                      const PickRequest& request,
                                                      const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<std::optional<PickHit>>
    pick(const scene::Storage& scene, const PickRequest& request,
         const scene::VisibilityFilter& visibility,
         const clipping::ClippingFilter& clipping_filter);
    [[nodiscard]] Result<std::optional<PickHit>>
    refine_candidate(const scene::Storage& scene, const PickRequest& request,
                     const scene::VisibilityFilter& visibility,
                     const clipping::ClippingFilter& clipping_filter,
                     const PickCandidate& candidate);
    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options = {});
    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options,
             const scene::VisibilityFilter& visibility);
    [[nodiscard]] Result<std::optional<PickHit>>
    pick_ray(const scene::Storage& scene, const Ray3& ray, const PickOptions& options,
             const scene::VisibilityFilter& visibility,
             const clipping::ClippingFilter& clipping_filter);

    void release_scene(SceneId scene) noexcept;
    [[nodiscard]] PickingStatistics statistics() const noexcept;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace elf3d::picking
