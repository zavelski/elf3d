module;

#include <elf3d/viewport.h>

#include <cmath>

module elf.viewport;

import elf.math;
import elf.scene;

namespace elf3d::viewport {
namespace {

[[nodiscard]] bool finite_float3(Float3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

Result<ProjectedViewportPoint>
OffscreenViewport::project_world_to_viewport(const scene::Storage& scene, EntityId camera,
                                             Float3 world_position) const {
    const Extent2D target_extent = extent();
    if (target_extent.width == 0 || target_extent.height == 0) {
        return Error{ErrorCode::invalid_viewport_dimensions,
                     "World-to-viewport projection requires a nonzero viewport extent"};
    }
    if (!finite_float3(world_position)) {
        return Error{ErrorCode::projection_failed,
                     "World-to-viewport projection requires a finite world position"};
    }

    const Result<PerspectiveCameraDescription> camera_description =
        scene.perspective_camera(camera);
    if (!camera_description) {
        return camera_description.error();
    }
    if (!scene::valid_camera_description(camera_description.value())) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "World-to-viewport projection requires a valid perspective camera"};
    }
    const Result<Float4x4> camera_world = scene.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<Float4x4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect =
        static_cast<float>(target_extent.width) / static_cast<float>(target_extent.height);
    const Result<Float4x4> projection = math::perspective_matrix(
        camera_description.value().vertical_field_of_view_radians, aspect,
        camera_description.value().near_plane, camera_description.value().far_plane);
    if (!projection) {
        return projection.error();
    }

    const Result<math::ViewportProjection> projected = math::project_world_to_viewport_point(
        view.value(), projection.value(), target_extent, world_position);
    if (!projected) {
        return projected.error();
    }

    return ProjectedViewportPoint{projected.value().position_pixels, projected.value().depth,
                                  projected.value().is_in_front,
                                  projected.value().is_inside_viewport};
}

} // namespace elf3d::viewport
