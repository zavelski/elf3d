#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>
#include <elf3d/selection.h>

#include <array>
#include <optional>
#include <utility>

import elf.assets;
import elf.picking;
import elf.scene;
import elf.tool.selection;

namespace {

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(31, value);
}

struct SelectionScene {
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
    elf3d::EntityId camera;
};

struct TwoModelScene {
    elf3d::scene::Storage scene;
    elf3d::EntityId left_model;
    elf3d::EntityId right_model;
    elf3d::EntityId camera;
};

struct DepthScene {
    elf3d::scene::Storage scene;
    elf3d::EntityId near_model;
    elf3d::EntityId far_model;
    elf3d::EntityId camera;
};

[[nodiscard]] SelectionScene make_scene() {
    elf3d::scene::Storage scene{scene_id(1)};
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-0.75F, -0.75F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.75F, -0.75F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 0.75F, -2.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::MeshHandle mesh = scene.create_mesh({vertices, indices}).value();
    const elf3d::MaterialHandle material = scene.create_material({}).value();
    const elf3d::EntityId model = scene.create_model(mesh, material).value();
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return SelectionScene{std::move(scene), model, camera};
}

[[nodiscard]] TwoModelScene make_two_model_scene() {
    elf3d::scene::Storage scene{scene_id(2)};
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-0.25F, -0.25F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.25F, -0.25F, -2.0F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 0.25F, -2.0F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::MeshHandle mesh = scene.create_mesh({vertices, indices}).value();
    const elf3d::MaterialHandle material = scene.create_material({}).value();
    const elf3d::EntityId left_model = scene.create_model(mesh, material).value();
    const elf3d::EntityId right_model = scene.create_model(mesh, material).value();
    elf3d::Transform left_transform;
    left_transform.translation = {-0.7F, 0.0F, 0.0F};
    elf3d::Transform right_transform;
    right_transform.translation = {0.7F, 0.0F, 0.0F};
    static_cast<void>(scene.set_local_transform(left_model, left_transform));
    static_cast<void>(scene.set_local_transform(right_model, right_transform));
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return TwoModelScene{std::move(scene), left_model, right_model, camera};
}

[[nodiscard]] DepthScene make_depth_scene() {
    elf3d::scene::Storage scene{scene_id(3)};
    const std::array<elf3d::VertexPositionNormal, 3> vertices{{
        {{-1.0F, -1.0F, 0.5F}, {0.0F, 0.0F, 1.0F}},
        {{1.0F, -1.0F, 0.5F}, {0.0F, 0.0F, 1.0F}},
        {{0.0F, 1.0F, -0.5F}, {0.0F, 0.0F, 1.0F}},
    }};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const elf3d::MeshHandle mesh = scene.create_mesh({vertices, indices}).value();
    elf3d::MaterialDescription material_description;
    material_description.double_sided = true;
    const elf3d::MaterialHandle material = scene.create_material(material_description).value();
    const elf3d::EntityId far_model = scene.create_model(mesh, material).value();
    elf3d::Transform far_transform;
    far_transform.translation = {0.0F, 0.0F, -4.0F};
    static_cast<void>(scene.set_local_transform(far_model, far_transform));
    const elf3d::EntityId near_model = scene.create_model(mesh, material).value();
    elf3d::Transform near_transform;
    near_transform.translation = {0.0F, 0.0F, -2.0F};
    static_cast<void>(scene.set_local_transform(near_model, near_transform));
    const elf3d::EntityId camera =
        scene.create_perspective_camera(elf3d::PerspectiveCameraDescription{}).value();
    return DepthScene{std::move(scene), near_model, far_model, camera};
}

[[nodiscard]] bool has_selected_model(const elf3d::Result<std::optional<elf3d::PickHit>>& selected,
                                      const elf3d::tools::selection::SelectionController& selection,
                                      elf3d::EntityId model) {
    return selected && selected.value().has_value() && selection.has_selection() &&
           selection.selected_entity() == model && selection.selection_hit()->entity == model;
}

[[nodiscard]] bool has_explicit_camera_selection(
    const elf3d::tools::selection::SelectionController& selection, elf3d::EntityId camera) {
    return selection.has_selection() && selection.selected_entity() == camera &&
           !selection.selection_hit().has_value() && selection.snapshot().entity.has_value() &&
           !selection.snapshot().pick_hit.has_value();
}

[[nodiscard]] bool has_reselected_model(const elf3d::Result<std::optional<elf3d::PickHit>>& selected,
                                        const elf3d::tools::selection::SelectionController& selection,
                                        elf3d::EntityId model) {
    return selected && selected.value().has_value() && selection.selected_entity() == model &&
           selection.selection_hit().has_value() && selection.snapshot().pick_hit.has_value();
}

[[nodiscard]] int verify_initial_selection(
    elf3d::picking::PickingService& picking, SelectionScene& fixture,
    elf3d::tools::selection::SelectionController& selection) {
    const elf3d::Result<std::optional<elf3d::PickHit>> selected =
        selection.select_at(picking, fixture.scene, {fixture.camera, {800, 600}, {399.5F, 299.5F}});
    if (!has_selected_model(selected, selection, fixture.model)) {
        return 1;
    }
    if (!selection.set_selected_entity(fixture.scene, fixture.camera) ||
        !has_explicit_camera_selection(selection, fixture.camera)) {
        return 11;
    }
    const elf3d::Result<std::optional<elf3d::PickHit>> picked_again =
        selection.select_at(picking, fixture.scene, {fixture.camera, {800, 600}, {399.5F, 299.5F}});
    if (!has_reselected_model(picked_again, selection, fixture.model)) {
        return 12;
    }
    return 0;
}

[[nodiscard]] int verify_cleared_and_disabled_selection(
    elf3d::picking::PickingService& picking, SelectionScene& fixture,
    elf3d::tools::selection::SelectionController& selection) {
    const elf3d::Result<std::optional<elf3d::PickHit>> cleared =
        selection.select_at(picking, fixture.scene, {fixture.camera, {800, 600}, {0.0F, 0.0F}});
    if (!cleared || cleared.value().has_value() || selection.has_selection()) {
        return 2;
    }

    selection.set_enabled(false);
    const elf3d::Result<std::optional<elf3d::PickHit>> disabled =
        selection.select_at(picking, fixture.scene, {fixture.camera, {800, 600}, {399.5F, 299.5F}});
    if (!disabled || disabled.value().has_value() || selection.has_selection()) {
        return 3;
    }
    selection.set_enabled(true);
    return 0;
}

[[nodiscard]] int verify_invalid_selection_settings(
    elf3d::tools::selection::SelectionController& selection) {
    elf3d::SelectionSettings invalid = selection.settings();
    invalid.click_drag_threshold_pixels = -1.0F;
    if (selection.set_settings(invalid).error().code() !=
        elf3d::ErrorCode::invalid_selection_settings) {
        return 4;
    }
    return 0;
}

[[nodiscard]] int verify_selection_validation(
    elf3d::picking::PickingService& picking, SelectionScene& fixture,
    elf3d::tools::selection::SelectionController& selection) {
    static_cast<void>(
        selection.select_at(picking, fixture.scene, {fixture.camera, {800, 600}, {399.5F, 299.5F}}));
    if (!selection.has_selection() || !fixture.scene.set_entity_visible(fixture.model, false)) {
        return 5;
    }
    selection.validate_against(fixture.scene);
    if (!selection.has_selection() || !fixture.scene.show_all_entities()) {
        return 51;
    }
    if (!fixture.scene.destroy_entity(fixture.model)) {
        return 52;
    }
    selection.validate_against(fixture.scene);
    if (selection.has_selection()) {
        return 6;
    }
    return 0;
}

[[nodiscard]] int verify_primary_selection(elf3d::picking::PickingService& picking) {
    SelectionScene fixture = make_scene();
    elf3d::tools::selection::SelectionController selection;
    const int initial = verify_initial_selection(picking, fixture, selection);
    if (initial != 0) {
        return initial;
    }
    const int cleared = verify_cleared_and_disabled_selection(picking, fixture, selection);
    if (cleared != 0) {
        return cleared;
    }
    const int settings = verify_invalid_selection_settings(selection);
    if (settings != 0) {
        return settings;
    }
    return verify_selection_validation(picking, fixture, selection);
}

[[nodiscard]] int verify_viewport_independence(elf3d::picking::PickingService& picking) {
    SelectionScene fixture = make_scene();
    elf3d::tools::selection::SelectionController first_viewport;
    elf3d::tools::selection::SelectionController second_viewport;
    static_cast<void>(first_viewport.select_at(
        picking, fixture.scene, {fixture.camera, {800, 600}, {399.5F, 299.5F}}));
    static_cast<void>(second_viewport.select_at(
        picking, fixture.scene, {fixture.camera, {800, 600}, {0.0F, 0.0F}}));
    if (!first_viewport.has_selection() || second_viewport.has_selection()) {
        return 7;
    }
    return 0;
}

[[nodiscard]] int verify_multiple_model_selection(elf3d::picking::PickingService& picking) {
    TwoModelScene fixture = make_two_model_scene();
    elf3d::tools::selection::SelectionController left_viewport;
    elf3d::tools::selection::SelectionController right_viewport;
    static_cast<void>(left_viewport.select_at(
        picking, fixture.scene, {fixture.camera, {800, 600}, {217.5F, 299.5F}}));
    static_cast<void>(right_viewport.select_at(
        picking, fixture.scene, {fixture.camera, {800, 600}, {581.5F, 299.5F}}));
    if (!left_viewport.has_selection() || !right_viewport.has_selection() ||
        left_viewport.selected_entity() != fixture.left_model ||
        right_viewport.selected_entity() != fixture.right_model) {
        return 8;
    }
    left_viewport.clear_scene(fixture.scene.id());
    if (left_viewport.has_selection() || !right_viewport.has_selection()) {
        return 9;
    }
    return 0;
}

[[nodiscard]] int verify_clipped_selection(elf3d::picking::PickingService& picking) {
    DepthScene fixture = make_depth_scene();
    elf3d::tools::selection::SelectionController clipped_viewport;
    const elf3d::scene::VisibilityFilter visibility =
        elf3d::scene::make_visibility_filter(fixture.scene, std::nullopt).value();
    elf3d::SectionPlane plane;
    plane.enabled = true;
    plane.point = {0.0F, 0.0F, -2.25F};
    plane.normal = {0.0F, 0.0F, -1.0F};
    const elf3d::clipping::ClippingFilter filter =
        elf3d::clipping::make_filter(plane, {}, 1).value();
    const elf3d::Result<std::optional<elf3d::PickHit>> picked =
        clipped_viewport.select_at(picking, fixture.scene,
                                   {fixture.camera, {800, 600}, {399.5F, 299.5F}}, visibility, filter);
    if (!picked || !picked.value().has_value() || picked.value()->entity != fixture.far_model ||
        !clipped_viewport.has_selection() || clipped_viewport.selected_entity() != fixture.far_model) {
        return 10;
    }
    plane.point = {0.0F, 0.0F, 1.0F};
    plane.normal = {0.0F, 0.0F, 1.0F};
    const elf3d::clipping::ClippingFilter reject_all =
        elf3d::clipping::make_filter(plane, {}, 2).value();
    const elf3d::Result<std::optional<elf3d::PickHit>> empty =
        clipped_viewport.select_at(picking, fixture.scene,
                                   {fixture.camera, {800, 600}, {399.5F, 299.5F}}, visibility, reject_all);
    if (!empty || empty.value().has_value() || clipped_viewport.has_selection()) {
        return 11;
    }
    return 0;
}

} // namespace

int elf3d_selection_test() {
    elf3d::picking::PickingService picking;
    const int primary = verify_primary_selection(picking);
    if (primary != 0) {
        return primary;
    }
    const int independent = verify_viewport_independence(picking);
    if (independent != 0) {
        return independent;
    }
    const int multiple = verify_multiple_model_selection(picking);
    if (multiple != 0) {
        return multiple;
    }
    return verify_clipped_selection(picking);
}
