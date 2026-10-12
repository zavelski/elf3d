#include "renderer_detail.h"

#include <elf3d/internal/renderer.h>

#include <elf3d/rendering.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <elf3d/internal/clipping.h>
#include <elf3d/internal/math.h>
#include <elf3d/internal/scene.h>
#include <optional>

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
};

struct RenderListBuildContext final {
    scene::VisibilityFilter visibility;
    clipping::ClippingFilter clipping;
    Frustum frustum;
    Float3 camera_position;
    Float4x4 view;
    bool frustum_culling_required = true;
};

[[nodiscard]] Result<CameraFrame>
make_camera_frame(const scene::Storage& scene_storage, EntityId camera,
                  const PerspectiveCameraDescription& camera_description, Extent2D extent)
{
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
                                        const std::optional<scene::EntityRecord>& record)
{
    return record.has_value() && record->model.has_value() &&
           scene::entity_visible_in_filter(scene_storage, visibility, record->id);
}

[[nodiscard]] FrustumPlane combined_plane(const Float4x4& matrix, std::size_t row,
                                          float sign) noexcept
{
    return FrustumPlane{{matrix.elements[3] + sign * matrix.elements[row],
                         matrix.elements[7] + sign * matrix.elements[4U + row],
                         matrix.elements[11] + sign * matrix.elements[8U + row]},
                        matrix.elements[15] + sign * matrix.elements[12U + row]};
}

[[nodiscard]] Frustum make_frustum(const CameraFrame& camera) noexcept
{
    const Float4x4 view_projection = math::compose_world(camera.projection, camera.view);
    return {{combined_plane(view_projection, 0U, 1.0F), combined_plane(view_projection, 0U, -1.0F),
             combined_plane(view_projection, 1U, 1.0F), combined_plane(view_projection, 1U, -1.0F),
             combined_plane(view_projection, 2U, 1.0F),
             combined_plane(view_projection, 2U, -1.0F)}};
}

[[nodiscard]] bool bounds_outside_plane(Bounds3 bounds, const FrustumPlane& plane) noexcept
{
    const Float3 support{plane.normal.x >= 0.0F ? bounds.maximum.x : bounds.minimum.x,
                         plane.normal.y >= 0.0F ? bounds.maximum.y : bounds.minimum.y,
                         plane.normal.z >= 0.0F ? bounds.maximum.z : bounds.minimum.z};
    const float distance = math::dot(plane.normal, support) + plane.offset;
    constexpr float boundary_epsilon = 0.00001F;
    return distance < -boundary_epsilon;
}

[[nodiscard]] bool bounds_outside_frustum(Bounds3 bounds, const Frustum& frustum) noexcept
{
    return std::any_of(frustum.begin(), frustum.end(), [bounds](const FrustumPlane& plane) {
        return bounds_outside_plane(bounds, plane);
    });
}

[[nodiscard]] bool bounds_inside_plane(Bounds3 bounds, const FrustumPlane& plane) noexcept
{
    const Float3 support{plane.normal.x >= 0.0F ? bounds.minimum.x : bounds.maximum.x,
                         plane.normal.y >= 0.0F ? bounds.minimum.y : bounds.maximum.y,
                         plane.normal.z >= 0.0F ? bounds.minimum.z : bounds.maximum.z};
    constexpr float boundary_epsilon = 0.00001F;
    return math::dot(plane.normal, support) + plane.offset >= -boundary_epsilon;
}

[[nodiscard]] bool bounds_inside_frustum(Bounds3 bounds, const Frustum& frustum) noexcept
{
    return std::all_of(frustum.begin(), frustum.end(), [bounds](const FrustumPlane& plane) {
        return bounds_inside_plane(bounds, plane);
    });
}

[[nodiscard]] bool requires_primitive_frustum_culling(const scene::Storage& scene,
                                                      const Frustum& frustum)
{
    const std::optional<Bounds3> bounds = scene.world_bounds();
    return !bounds.has_value() || !bounds_inside_frustum(*bounds, frustum);
}

[[nodiscard]] bool primitive_is_clipped(Bounds3 world_bounds,
                                        const clipping::ClippingFilter& filter,
                                        RenderList& list) noexcept
{
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

struct PrimitiveVisibility {
    bool visible = true;
    float camera_depth = 0.0F;
};
[[nodiscard]] PrimitiveVisibility evaluate_primitive_bounds(const RenderListBuildContext& build,
                                                            Bounds3 bounds, bool optical,
                                                            RenderList& list)
{
    float camera_depth = 0.0F;
    if (optical) {
        const auto& b = bounds;
        const Float3 center{(b.minimum.x + b.maximum.x) * 0.5F, (b.minimum.y + b.maximum.y) * 0.5F,
                            (b.minimum.z + b.maximum.z) * 0.5F};
        const auto& v = build.view.elements;
        camera_depth = -(v[2] * center.x + v[6] * center.y + v[10] * center.z + v[14]);
    }
    if (build.frustum_culling_required && bounds_outside_frustum(bounds, build.frustum)) {
        ++list.frustum_culled_primitives;
        return PrimitiveVisibility{false, camera_depth};
    }
    if (primitive_is_clipped(bounds, build.clipping, list)) {
        return PrimitiveVisibility{false, camera_depth};
    }
    return {true, camera_depth};
}

[[nodiscard]] bool has_optical_transmission(const scene::RuntimeMaterialView& material) noexcept
{
    return !material.unlit && material.transmission_factor > 0 &&
           (material.metallic_factor < 1 || material.has_metallic_roughness_texture);
}

[[nodiscard]] Result<void> append_primitive(const RenderListBuildContext& build,
                                            const scene::Storage& scene,
                                            const RenderEntityContext& context,
                                            std::uint32_t primitive_index, RenderList& list)
{
    ++list.candidate_primitives;
    const Result<scene::RuntimePrimitiveView> primitive =
        scene.runtime_primitive(context.entity, primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    const auto& material = primitive.value().material_view;
    const bool transmission = has_optical_transmission(material);
    const bool optical = material.alpha_mode == scene::RuntimeAlphaMode::blend || transmission;
    float camera_depth = 0.0F;
    if (build.frustum_culling_required || build.clipping.has_clipping() || optical) {
        const Result<Bounds3> world_bounds =
            scene.primitive_world_bounds(context.entity, primitive_index);
        if (!world_bounds) {
            return world_bounds.error();
        }
        const auto visibility =
            evaluate_primitive_bounds(build, world_bounds.value(), optical, list);
        if (!visibility.visible)
            return {};
        camera_depth = visibility.camera_depth;
    }

    list.items.push_back(RenderItem{context.entity, primitive.value().mesh, primitive_index,
                                    context.model, context.normals, context.orientation_reversed,
                                    primitive.value().material_identity,
                                    runtime_alpha_mode(primitive.value().material_view.alpha_mode),
                                    camera_depth, transmission});
    return {};
}

[[nodiscard]] Result<void> append_entity(const RenderListBuildContext& build,
                                         const scene::Storage& scene,
                                         const scene::EntityRecord& record, RenderList& list)
{
    const Result<Float4x4> model = scene.world_matrix(record.id);
    if (!model) {
        return model.error();
    }

    const Result<math::Matrix3x3> normals = scene.world_normal_matrix(record.id);
    if (!normals) {
        return normals.error();
    }

    const Result<bool> orientation = scene.world_orientation_reversed(record.id);
    if (!orientation) {
        return orientation.error();
    }

    const RenderEntityContext context{record.id, model.value(), normals.value(),
                                      orientation.value()};
    for (std::uint32_t primitive_index = 0; primitive_index < record.model->primitives.size();
         ++primitive_index) {
        Result<void> appended = append_primitive(build, scene, context, primitive_index, list);
        if (!appended) {
            return appended.error();
        }
    }
    return {};
}

[[nodiscard]] bool is_opaque_item(const RenderItem& item) noexcept
{
    return item.alpha_mode != AlphaMode::blend && !item.transmission_active;
}

[[nodiscard]] bool blended_item_precedes(const RenderItem& left, const RenderItem& right) noexcept
{
    if (left.camera_depth != right.camera_depth)
        return left.camera_depth > right.camera_depth;
    if (left.entity != right.entity)
        return left.entity.debug_value() < right.entity.debug_value();
    return left.primitive_index < right.primitive_index;
}

void order_render_items(RenderList& list)
{
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
                                     Extent2D extent)
{
    const Result<scene::VisibilityFilter> visibility =
        scene::make_visibility_filter(scene_storage, std::nullopt);
    if (!visibility) {
        return visibility.error();
    }
    return build_render_list(scene_storage, camera, extent, visibility.value());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility)
{
    return build_render_list(scene_storage, camera, extent, visibility,
                             clipping::disabled_filter());
}

Result<RenderList> build_render_list(const scene::Storage& scene_storage, EntityId camera,
                                     Extent2D extent, const scene::VisibilityFilter& visibility,
                                     const clipping::ClippingFilter& clipping_filter)
{
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
    const RenderListBuildContext build{visibility,       clipping_filter,
                                       frustum,          list.camera_world_position,
                                       list.view_matrix, frustum_culling_required};
    for (const std::optional<scene::EntityRecord>& record : scene_storage.entities()) {
        if (!record_is_renderable(scene_storage, visibility, record)) {
            continue;
        }
        Result<void> appended = append_entity(build, scene_storage, *record, list);
        if (!appended) {
            return appended.error();
        }
    }

    order_render_items(list);
    return list;
}

} // namespace elf3d::renderer
