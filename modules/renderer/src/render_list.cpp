module;

#include <elf3d/viewport.h>

#include <algorithm>
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

struct RenderEntityContext final {
    EntityId entity;
    Float4x4 model;
    math::Matrix3x3 normals;
    bool orientation_reversed = false;
    Float3 camera_position;
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

[[nodiscard]] bool primitive_is_clipped(const scene::RuntimePrimitiveView& primitive,
                                        const RenderEntityContext& context,
                                        const clipping::ClippingFilter& filter,
                                        RenderList& list) noexcept {
    if (!filter.has_clipping()) {
        return false;
    }
    ++list.clipping_bounds_tested;
    const Bounds3 world_bounds = clipping::transform_bounds(primitive.bounds, context.model);
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

[[nodiscard]] Result<void> append_primitive(const scene::Storage& scene_storage,
                                            const RenderEntityContext& context,
                                            std::uint32_t primitive_index,
                                            const clipping::ClippingFilter& clipping_filter,
                                            RenderList& list) {
    const Result<scene::RuntimePrimitiveView> primitive =
        scene_storage.runtime_primitive(context.entity, primitive_index);
    if (!primitive) {
        return primitive.error();
    }
    if (primitive_is_clipped(primitive.value(), context, clipping_filter, list)) {
        return {};
    }
    list.items.push_back(
        RenderItem{context.entity, primitive.value().mesh, primitive_index, context.model,
                   context.normals, context.orientation_reversed,
                   runtime_material_description(primitive.value().material_view).alpha_mode,
                   camera_distance_squared(context)});
    return {};
}

[[nodiscard]] Result<void> append_entity(const scene::Storage& scene_storage,
                                         const scene::EntityRecord& record,
                                         const clipping::ClippingFilter& clipping_filter,
                                         Float3 camera_position, RenderList& list) {
    const Result<Float4x4> model = scene_storage.world_matrix(record.id);
    if (!model) {
        return model.error();
    }
    const Result<math::Matrix3x3> normals = math::normal_matrix(model.value());
    if (!normals) {
        return normals.error();
    }
    const Result<bool> orientation = math::orientation_reversed(model.value());
    if (!orientation) {
        return orientation.error();
    }
    const RenderEntityContext context{record.id, model.value(), normals.value(),
                                      orientation.value(), camera_position};
    for (std::uint32_t primitive_index = 0; primitive_index < record.model->primitives.size();
         ++primitive_index) {
        Result<void> appended =
            append_primitive(scene_storage, context, primitive_index, clipping_filter, list);
        if (!appended) {
            return appended.error();
        }
    }
    return {};
}

[[nodiscard]] bool render_item_precedes(const RenderItem& left, const RenderItem& right) noexcept {
    const bool left_blended = left.alpha_mode == AlphaMode::blend;
    const bool right_blended = right.alpha_mode == AlphaMode::blend;
    if (left_blended != right_blended) {
        return !left_blended;
    }
    return left_blended && left.camera_distance_squared > right.camera_distance_squared;
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
    for (const std::optional<scene::EntityRecord>& record : scene_storage.entities()) {
        if (!record_is_renderable(scene_storage, visibility, record)) {
            continue;
        }
        Result<void> appended = append_entity(scene_storage, *record, clipping_filter,
                                              list.camera_world_position, list);
        if (!appended) {
            return appended.error();
        }
    }
    std::stable_sort(list.items.begin(), list.items.end(), render_item_precedes);
    return list;
}

} // namespace elf3d::renderer
