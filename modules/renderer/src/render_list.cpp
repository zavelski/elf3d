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

    RenderList list;
    list.view_matrix = view.value();
    list.projection_matrix = projection.value();
    list.camera_world_position =
        Float3{camera_world.value().elements[12], camera_world.value().elements[13],
               camera_world.value().elements[14]};
    for (const std::optional<scene::EntityRecord>& record : scene_storage.entities()) {
        if (!record.has_value() || !record->model.has_value() ||
            !scene::entity_visible_in_filter(scene_storage, visibility, record->id)) {
            continue;
        }
        const Result<Float4x4> model = scene_storage.world_matrix(record->id);
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
        for (std::uint32_t primitive_index = 0; primitive_index < record->model->primitives.size();
             ++primitive_index) {
            const Result<scene::RuntimePrimitiveView> primitive =
                scene_storage.runtime_primitive(record->id, primitive_index);
            if (!primitive) {
                return primitive.error();
            }
            if (clipping_filter.has_clipping()) {
                ++list.clipping_bounds_tested;
                const Bounds3 world_bounds =
                    clipping::transform_bounds(primitive.value().bounds, model.value());
                const clipping::BoundsClassification classification =
                    clipping::classify_bounds(clipping_filter, world_bounds);
                if (classification == clipping::BoundsClassification::outside) {
                    ++list.clipping_bounds_rejected;
                    continue;
                }
                if (classification == clipping::BoundsClassification::intersecting) {
                    ++list.clipping_bounds_intersecting;
                }
            }
            const Float3 model_origin{model.value().elements[12], model.value().elements[13],
                                      model.value().elements[14]};
            const float delta_x = model_origin.x - list.camera_world_position.x;
            const float delta_y = model_origin.y - list.camera_world_position.y;
            const float delta_z = model_origin.z - list.camera_world_position.z;
            const float distance_squared =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;
            list.items.push_back(
                RenderItem{record->id, primitive.value().mesh, primitive_index, model.value(),
                           normals.value(), orientation.value(),
                           runtime_material_description(primitive.value().material_view).alpha_mode,
                           distance_squared});
        }
    }
    std::stable_sort(
        list.items.begin(), list.items.end(), [](const RenderItem& left, const RenderItem& right) {
            const bool left_blended = left.alpha_mode == AlphaMode::blend;
            const bool right_blended = right.alpha_mode == AlphaMode::blend;
            if (left_blended != right_blended) {
                return !left_blended;
            }
            return left_blended && left.camera_distance_squared > right.camera_distance_squared;
        });
    return list;
}

} // namespace elf3d::renderer
