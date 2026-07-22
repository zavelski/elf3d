module;

#include <elf3d/rendering.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

module elf.renderer;

import elf.clipping;
import elf.math;
import elf.scene;

namespace elf3d::renderer {
namespace {

struct CameraFrame final {
    Float4x4 view;
    Float4x4 projection;
    Float3 position;
};

struct FrustumPlane final {
    Float3 normal;
    float offset = 0.0F;
};

using Frustum = std::array<FrustumPlane, 6>;

struct RenderEntityContext final {
    EntityId entity;
    Float4x4 model;
    math::Matrix3x3 normals;
    bool orientation_reversed = false;
    Float3 camera_position;
};

struct RenderListBuildContext final {
    const scene::Storage& scene;
    const Frustum& frustum;
    const clipping::ClippingFilter& clipping;
    Float3 camera_position;
    bool frustum_culling_required = true;
};

[[nodiscard]] Result<CameraFrame>
make_camera_frame(const scene::Storage& scene_storage, EntityId camera,
                  const PerspectiveCameraDescription& camera_description, Extent2D extent) {
    const Result<Float4x4> camera_world = scene_storage.world_matrix(camera);
    if (!camera_world) {
        return camera_world.error();
    }
    const Result<Float4x4> view = math::camera_view_matrix(camera_world.value());
    if (!view) {
        return view.error();
    }
    const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const Result<Float4x4> projection =
        math::perspective_matrix(camera_description.vertical_field_of_view_radians, aspect,
                                 camera_description.near_plane, camera_description.far_plane);
    if (!projection) {
        return projection.error();
    }
    return CameraFrame{view.value(), projection.value(),
                       Float3{camera_world.value().elements[12], camera_world.value().elements[13],
                              camera_world.value().elements[14]}};
}

[[nodiscard]] bool record_is_renderable(const scene::Storage& scene_storage,
                                        const scene::VisibilityFilter& visibility,
                                        const std::optional<scene::EntityRecord>& record) {
    return record.has_value() && record->model.has_value() &&
           scene::entity_visible_in_filter(scene_storage, visibility, record->id);
}

[[nodiscard]] FrustumPlane combined_plane(const Float4x4& matrix, std::size_t row,
                                          float sign) noexcept {
    return FrustumPlane{{matrix.elements[3] + sign * matrix.elements[row],
                         matrix.elements[7] + sign * matrix.elements[4U + row],
                         matrix.elements[11] + sign * matrix.elements[8U + row]},
                        matrix.elements[15] + sign * matrix.elements[12U + row]};
}

[[nodiscard]] Frustum make_frustum(const CameraFrame& camera) noexcept {
    const Float4x4 view_projection = math::compose_world(camera.projection, camera.view);
    return {{combined_plane(view_projection, 0U, 1.0F), combined_plane(view_projection, 0U, -1.0F),
             combined_plane(view_projection, 1U, 1.0F), combined_plane(view_projection, 1U, -1.0F),
             combined_plane(view_projection, 2U, 1.0F),
             combined_plane(view_projection, 2U, -1.0F)}};
}

[[nodiscard]] bool bounds_outside_plane(Bounds3 bounds, const FrustumPlane& plane) noexcept {
    const Float3 support{plane.normal.x >= 0.0F ? bounds.maximum.x : bounds.minimum.x,
                         plane.normal.y >= 0.0F ? bounds.maximum.y : bounds.minimum.y,
                         plane.normal.z >= 0.0F ? bounds.maximum.z : bounds.minimum.z};
    const float distance = math::dot(plane.normal, support) + plane.offset;
    constexpr float boundary_epsilon = 0.00001F;
    return distance < -boundary_epsilon;
}

[[nodiscard]] bool bounds_outside_frustum(Bounds3 bounds, const Frustum& frustum) noexcept {
    return std::any_of(frustum.begin(), frustum.end(), [bounds](const FrustumPlane& plane) {
        return bounds_outside_plane(bounds, plane);
    });
}

[[nodiscard]] bool bounds_inside_plane(Bounds3 bounds, const FrustumPlane& plane) noexcept {
    const Float3 support{plane.normal.x >= 0.0F ? bounds.minimum.x : bounds.maximum.x,
                         plane.normal.y >= 0.0F ? bounds.minimum.y : bounds.maximum.y,
                         plane.normal.z >= 0.0F ? bounds.minimum.z : bounds.maximum.z};
    constexpr float boundary_epsilon = 0.00001F;
    return math::dot(plane.normal, support) + plane.offset >= -boundary_epsilon;
}

[[nodiscard]] bool bounds_inside_frustum(Bounds3 bounds, const Frustum& frustum) noexcept {
    return std::all_of(frustum.begin(), frustum.end(), [bounds](const FrustumPlane& plane) {
        return bounds_inside_plane(bounds, plane);
    });
}

[[nodiscard]] bool requires_primitive_frustum_culling(const scene::Storage& scene,
                                                      const Frustum& frustum) noexcept {
    const std::optional<Bounds3> bounds = scene.world_bounds();
    return !bounds.has_value() || !bounds_inside_frustum(*bounds, frustum);
}

[[nodiscard]] bool primitive_is_clipped(Bounds3 world_bounds,
                                        const clipping::ClippingFilter& filter,
                                        RenderList& list) noexcept {
    if (!filter.has_clipping()) {
        return false;
    }
    ++list.clipping_bounds_tested;
    const clipping::BoundsClassification classification =
        clipping::classify_bounds(filter, world_bounds);
    if (classification == clipping::BoundsClassification::outside) {
        ++list.clipping_bounds_rejected;
        return true;
    }
    if (classification == clipping::BoundsClassification::intersecting) {
        ++list.clipping_bounds_intersecting;
    }
    return false;
}

[[nodiscard]] float camera_distance_squared(const RenderEntityContext& context) noexcept {
    const Float3 model_origin{context.model.elements[12], context.model.elements[13],
                              context.model.elements[14]};
    const float delta_x = model_origin.x - context.camera_position.x;
    const float delta_y = model_origin.y - context.camera_position.y;
    const float delta_z = model_origin.z - context.camera_position.z;
    return delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
}

[[nodiscard]] Result<void> append_primitive(const RenderListBuildContext& build,
                                            const RenderEntityContext& context,
                                            std::uint32_t primitive_index, RenderList& list) {
    ++list.candidate_primitives;
    const Result<scene::RuntimePrimitiveView> primitive =
        build.scene.runtime_primitive(context.entity, primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    if (build.frustum_culling_required || build.clipping.has_clipping()) {
        const Result<Bounds3> world_bounds =
            build.scene.primitive_world_bounds(context.entity, primitive_index);
        if (!world_bounds) {
            return world_bounds.error();
        }
        if (build.frustum_culling_required &&
            bounds_outside_frustum(world_bounds.value(), build.frustum)) {
            ++list.frustum_culled_primitives;
            return {};
        }
        if (primitive_is_clipped(world_bounds.value(), build.clipping, list)) {
            return {};
        }
    }
    list.items.push_back(RenderItem{
        context.entity, primitive.value().mesh, primitive_index, context.model, context.normals,
        context.orientation_reversed, primitive.value().material_identity,
        runtime_material_description(primitive.value().material_view).alpha_mode,
        camera_distance_squared(context)});
    return {};
}

[[nodiscard]] Result<void> append_entity(const RenderListBuildContext& build,
                                         const scene::EntityRecord& record, RenderList& list) {
    const Result<Float4x4> model = build.scene.world_matrix(record.id);
    if (!model) {
        return model.error();
    }
    const Result<math::Matrix3x3> normals = build.scene.world_normal_matrix(record.id);
    if (!normals) {
        return normals.error();
    }
    const Result<bool> orientation = build.scene.world_orientation_reversed(record.id);
    if (!orientation) {
        return orientation.error();
    }
    const RenderEntityContext context{record.id, model.value(), normals.value(),
                                      orientation.value(), build.camera_position};
    for (std::uint32_t primitive_index = 0; primitive_index < record.model->primitives.size();
         ++primitive_index) {
        Result<void> appended = append_primitive(build, context, primitive_index, list);
        if (!appended) {
            return appended.error();
        }
    }
    return {};
}

[[nodiscard]] bool is_opaque_item(const RenderItem& item) noexcept {
    return item.alpha_mode != AlphaMode::blend;
}

[[nodiscard]] bool blended_item_precedes(const RenderItem& left, const RenderItem& right) noexcept {
    return left.camera_distance_squared > right.camera_distance_squared;
}

void order_render_items(RenderList& list) {
    const auto first_blended =
        std::find_if_not(list.items.begin(), list.items.end(), is_opaque_item);
    if (first_blended == list.items.end()) {
        return;
    }
    const auto blended_begin =
        std::stable_partition(list.items.begin(), list.items.end(), is_opaque_item);
    std::stable_sort(blended_begin, list.items.end(), blended_item_precedes);
}

} // namespace

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent) {
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return build_render_list(scene_storage, camera, extent, visibility.value());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility) {
    return build_render_list(scene_storage, camera, extent, visibility,
                             clipping::disabled_filter());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility,
                                     const clipping::ClippingFilter& clipping_filter) {
    const Result<const scene::EntityRecord*> camera_record = scene_storage.entity(camera);
    if (!camera_record) {
        return camera_record.error();
    }
    const std::optional<PerspectiveCameraDescription>& camera_component =
        camera_record.value()->camera;
    if (!camera_component.has_value()) {
        return Error{ErrorCode::entity_has_no_camera,
                     "Viewport rendering requires an entity with a perspective camera"};
    }
    const PerspectiveCameraDescription& camera_description = *camera_component;
    if (!scene::valid_camera_description(camera_description)) {
        return Error{ErrorCode::invalid_camera_configuration,
                     "The selected perspective camera configuration is invalid"};
    }
    if (extent.width == 0 || extent.height == 0) {
        return RenderList{};
    }

    const Result<CameraFrame> camera_frame =
        make_camera_frame(scene_storage, camera, camera_description, extent);
    if (!camera_frame) {
        return camera_frame.error();
    }

    RenderList list;
    list.view_matrix = camera_frame.value().view;
    list.projection_matrix = camera_frame.value().projection;
    list.camera_world_position = camera_frame.value().position;
    const Frustum frustum = make_frustum(camera_frame.value());
    const bool frustum_culling_required =
        requires_primitive_frustum_culling(scene_storage, frustum);
    const RenderListBuildContext build{scene_storage, frustum, clipping_filter,
                                       list.camera_world_position, frustum_culling_required};
    for (const std::optional<scene::EntityRecord>& record : scene_storage.entities()) {
        if (!record_is_renderable(scene_storage, visibility, record)) {
            continue;
        }
        Result<void> appended = append_entity(build, *record, list);
        if (!appended) {
            return appended.error();
        }
    }
    order_render_items(list);
    return list;
}

} // namespace elf3d::renderer
