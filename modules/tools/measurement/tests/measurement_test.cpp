#include <elf3d/assets.h>
#include <elf3d/clipping.h>
#include <elf3d/core/result.h>
#include <elf3d/measurement.h>
#include <elf3d/model.h>
#include <elf3d/picking.h>
#include <elf3d/scene.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

import elf.assets;
import elf.model;
import elf.scene;
import elf.tool.measurement;

namespace {

[[nodiscard]] bool nearly_equal(double left, double right, double tolerance = 0.0001) noexcept {
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool nearly_equal(elf3d::Float3 left, elf3d::Float3 right,
                                float tolerance = 0.0001F) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z) <= tolerance;
}

[[nodiscard]] elf3d::SceneId scene_id(std::uint64_t value) noexcept {
    return elf3d::detail::SceneHandleAccess::create_scene(47, value);
}

struct MeasurementFixture {
    elf3d::scene::Storage scene;
    elf3d::EntityId model;
    elf3d::MeshHandle mesh;
};

[[nodiscard]] MeasurementFixture make_fixture() {
    elf3d::scene::Storage scene{scene_id(1)};
    const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
    const std::array<elf3d::Float3, 3> positions{{
        {0.0F, 0.0F, 0.0F},
        {1.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F},
    }};
    elf3d::Document document;
    const elf3d::MaterialId document_material = document.create_material({}).value();
    const elf3d::MeshId document_mesh = document.create_mesh().value();
    const elf3d::PrimitiveId document_primitive =
        document
            .create_primitive(document_mesh, document_material,
                              {positions, {}, {}, {}, {}, indices})
            .value();
    static_cast<void>(scene.set_document(std::move(document)));
    const elf3d::EntityId model = scene.create_entity().value();
    const std::array<elf3d::PrimitiveId, 1> document_primitives{{document_primitive}};
    static_cast<void>(scene.set_model_document_primitives(model, document_primitives));
    const elf3d::MeshHandle mesh = scene.runtime_primitive(model, 0).value().mesh;
    return MeasurementFixture{std::move(scene), model, mesh};
}

[[nodiscard]] elf3d::PickHit make_hit(const MeasurementFixture& fixture,
                                      elf3d::Float3 barycentric) noexcept {
    return elf3d::PickHit{
        fixture.model,      fixture.mesh, 0,   0, {barycentric.y, barycentric.z, 0.0F},
        {0.0F, 0.0F, 1.0F}, barycentric,  1.0F};
}

[[nodiscard]] elf3d::scene::VisibilityFilter
visibility_filter(const elf3d::scene::Storage& scene,
                  std::optional<elf3d::EntityId> isolated = std::nullopt) {
    return elf3d::scene::make_visibility_filter(scene, isolated).value();
}

[[nodiscard]] elf3d::Quaternion axis_angle_z(float radians) noexcept {
    const float half_angle = radians * 0.5F;
    return elf3d::Quaternion{0.0F, 0.0F, std::sin(half_angle), std::cos(half_angle)};
}

} // namespace

int elf3d_tool_measurement_test() {
    using elf3d::LengthDisplayUnit;
    using elf3d::tools::measurement::display_length;
    using elf3d::tools::measurement::DistanceMeasurementController;

    if (!nearly_equal(display_length(2.0, LengthDisplayUnit::meters).value, 2.0) ||
        !nearly_equal(display_length(2.0, LengthDisplayUnit::centimeters).value, 200.0) ||
        !nearly_equal(display_length(2.0, LengthDisplayUnit::millimeters).value, 2000.0) ||
        !nearly_equal(display_length(1.0, LengthDisplayUnit::feet).value, 3.280839895) ||
        !nearly_equal(display_length(1.0, LengthDisplayUnit::inches).value, 39.37007874) ||
        display_length(2.0, LengthDisplayUnit::automatic_metric).unit !=
            LengthDisplayUnit::meters ||
        display_length(0.5, LengthDisplayUnit::automatic_metric).unit !=
            LengthDisplayUnit::centimeters ||
        display_length(0.005, LengthDisplayUnit::automatic_metric).unit !=
            LengthDisplayUnit::millimeters) {
        return 1;
    }

    MeasurementFixture fixture = make_fixture();
    DistanceMeasurementController controller;
    const elf3d::scene::VisibilityFilter visible = visibility_filter(fixture.scene);
    const elf3d::DistanceMeasurementSnapshot initial =
        controller.snapshot(fixture.scene, visible, elf3d::ViewportTool::distance_measurement);
    if (initial.state != elf3d::DistanceMeasurementState::awaiting_first_point ||
        initial.overlay_visible || initial.first_point.has_value()) {
        return 2;
    }

    elf3d::DistanceMeasurementSettings invalid_settings;
    invalid_settings.line_thickness_pixels = std::numeric_limits<float>::quiet_NaN();
    if (controller.set_settings(invalid_settings).error().code() !=
        elf3d::ErrorCode::invalid_measurement_settings) {
        return 3;
    }

    const elf3d::PickHit first_hit = make_hit(fixture, {1.0F, 0.0F, 0.0F});
    const elf3d::PickHit second_hit = make_hit(fixture, {0.0F, 1.0F, 0.0F});
    elf3d::PickHit invalid_hit = first_hit;
    invalid_hit.barycentric_coordinates = {0.6F, 0.6F, 0.0F};
    if (controller.place_hit(fixture.scene, invalid_hit).error().code() !=
        elf3d::ErrorCode::invalid_measurement_hit) {
        return 4;
    }
    invalid_hit = first_hit;
    invalid_hit.triangle_index = 7;
    if (controller.place_hit(fixture.scene, invalid_hit).error().code() !=
        elf3d::ErrorCode::invalid_measurement_hit) {
        return 5;
    }

    if (!controller.place_hit(fixture.scene, first_hit)) {
        return 6;
    }
    elf3d::DistanceMeasurementSnapshot awaiting =
        controller.snapshot(fixture.scene, visible, elf3d::ViewportTool::distance_measurement);
    if (awaiting.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        !awaiting.first_point.has_value() || awaiting.second_point.has_value() ||
        !nearly_equal(awaiting.first_point->world_position, {0.0F, 0.0F, 0.0F})) {
        return 7;
    }
    elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> first_overlay =
        controller.overlay(fixture.scene, visible);
    if (!first_overlay || first_overlay.value().line_count != 0 ||
        first_overlay.value().marker_count != 1) {
        return 8;
    }

    if (!controller.update_preview(fixture.scene, second_hit)) {
        return 9;
    }
    elf3d::DistanceMeasurementSnapshot preview =
        controller.snapshot(fixture.scene, visible, elf3d::ViewportTool::distance_measurement);
    if (!preview.preview_point.has_value() || !nearly_equal(preview.preview_distance_meters, 1.0) ||
        !preview.midpoint_world_position.has_value() ||
        !nearly_equal(preview.midpoint_world_position.value(), {0.5F, 0.0F, 0.0F})) {
        return 10;
    }
    elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> preview_overlay =
        controller.overlay(fixture.scene, visible);
    if (!preview_overlay || preview_overlay.value().line_count != 1 ||
        preview_overlay.value().marker_count != 2) {
        return 11;
    }
    controller.clear_preview();
    if (controller.snapshot(fixture.scene, visible, elf3d::ViewportTool::distance_measurement)
            .preview_point.has_value()) {
        return 12;
    }

    if (!controller.place_hit(fixture.scene, second_hit)) {
        return 13;
    }
    elf3d::DistanceMeasurementSnapshot complete =
        controller.snapshot(fixture.scene, visible, elf3d::ViewportTool::selection);
    if (complete.state != elf3d::DistanceMeasurementState::complete ||
        !complete.first_point.has_value() || !complete.second_point.has_value() ||
        !nearly_equal(complete.distance_meters, 1.0) || !complete.overlay_visible) {
        return 14;
    }
    elf3d::SectionPlane anchor_clip_plane;
    anchor_clip_plane.enabled = true;
    anchor_clip_plane.point = {0.5F, 0.0F, 0.0F};
    anchor_clip_plane.normal = {1.0F, 0.0F, 0.0F};
    const elf3d::clipping::ClippingFilter anchor_clip_filter =
        elf3d::clipping::make_filter(anchor_clip_plane, {}, 1).value();
    const elf3d::DistanceMeasurementSnapshot clipped_complete = controller.snapshot(
        fixture.scene, visible, anchor_clip_filter, elf3d::ViewportTool::selection);
    if (clipped_complete.state != elf3d::DistanceMeasurementState::complete ||
        !nearly_equal(clipped_complete.distance_meters, 1.0) || clipped_complete.overlay_visible ||
        clipped_complete.anchors_currently_visible) {
        return 141;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> clipped_overlay =
        controller.overlay(fixture.scene, visible, anchor_clip_filter);
    if (!clipped_overlay || clipped_overlay.value().line_count != 0 ||
        clipped_overlay.value().marker_count != 0) {
        return 142;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> unclipped_overlay =
        controller.overlay(fixture.scene, visible, elf3d::clipping::disabled_filter());
    if (!unclipped_overlay || unclipped_overlay.value().line_count != 1 ||
        unclipped_overlay.value().marker_count != 2) {
        return 143;
    }

    elf3d::Transform scaled;
    scaled.scale = {2.0F, 3.0F, -1.0F};
    if (!fixture.scene.set_local_transform(fixture.model, scaled)) {
        return 15;
    }
    complete = controller.snapshot(fixture.scene, visibility_filter(fixture.scene),
                                   elf3d::ViewportTool::selection);
    if (!nearly_equal(complete.distance_meters, 2.0) ||
        !nearly_equal(complete.first_point->world_normal, {0.0F, 0.0F, -1.0F})) {
        return 16;
    }

    elf3d::Transform rotated = scaled;
    rotated.rotation = axis_angle_z(1.5707963268F);
    if (!fixture.scene.set_local_transform(fixture.model, rotated)) {
        return 17;
    }
    complete = controller.snapshot(fixture.scene, visibility_filter(fixture.scene),
                                   elf3d::ViewportTool::selection);
    if (!nearly_equal(complete.distance_meters, 2.0)) {
        return 18;
    }

    if (!fixture.scene.set_entity_visible(fixture.model, false)) {
        return 19;
    }
    const elf3d::scene::VisibilityFilter hidden = visibility_filter(fixture.scene);
    complete = controller.snapshot(fixture.scene, hidden, elf3d::ViewportTool::selection);
    if (complete.overlay_visible || complete.anchors_currently_visible) {
        return 20;
    }
    elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> hidden_overlay =
        controller.overlay(fixture.scene, hidden);
    if (!hidden_overlay || hidden_overlay.value().line_count != 0 ||
        hidden_overlay.value().marker_count != 0) {
        return 21;
    }
    if (!fixture.scene.show_entity_and_ancestors(fixture.model)) {
        return 22;
    }
    elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> restored_overlay =
        controller.overlay(fixture.scene, visibility_filter(fixture.scene));
    if (!restored_overlay || restored_overlay.value().line_count != 1 ||
        restored_overlay.value().marker_count != 2) {
        return 23;
    }

    const elf3d::EntityId unrelated = fixture.scene.create_entity().value();
    const elf3d::scene::VisibilityFilter isolated = visibility_filter(fixture.scene, unrelated);
    complete = controller.snapshot(fixture.scene, isolated, elf3d::ViewportTool::selection);
    if (complete.overlay_visible || complete.anchors_currently_visible) {
        return 24;
    }

    if (!controller.place_hit(fixture.scene, first_hit)) {
        return 25;
    }
    awaiting = controller.snapshot(fixture.scene, visibility_filter(fixture.scene),
                                   elf3d::ViewportTool::distance_measurement);
    if (awaiting.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        awaiting.second_point.has_value()) {
        return 26;
    }
    controller.cancel_incomplete();
    if (controller
            .snapshot(fixture.scene, visibility_filter(fixture.scene),
                      elf3d::ViewportTool::distance_measurement)
            .state != elf3d::DistanceMeasurementState::awaiting_first_point) {
        return 27;
    }

    if (!controller.place_hit(fixture.scene, first_hit) ||
        !controller.place_hit(fixture.scene, second_hit)) {
        return 28;
    }
    controller.cancel_incomplete();
    if (controller
            .snapshot(fixture.scene, visibility_filter(fixture.scene),
                      elf3d::ViewportTool::selection)
            .state != elf3d::DistanceMeasurementState::complete) {
        return 29;
    }
    controller.clear();
    if (controller
            .snapshot(fixture.scene, visibility_filter(fixture.scene),
                      elf3d::ViewportTool::selection)
            .state != elf3d::DistanceMeasurementState::empty) {
        return 30;
    }

    if (!controller.place_hit(fixture.scene, first_hit) ||
        !controller.place_hit(fixture.scene, second_hit) ||
        !fixture.scene.destroy_entity(fixture.model)) {
        return 31;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> stale_overlay =
        controller.overlay(fixture.scene, visibility_filter(fixture.scene));
    const elf3d::DistanceMeasurementSnapshot stale_snapshot = controller.snapshot(
        fixture.scene, visibility_filter(fixture.scene), elf3d::ViewportTool::selection);
    if (!stale_overlay || stale_overlay.value().line_count != 0 ||
        stale_snapshot.state != elf3d::DistanceMeasurementState::empty ||
        !stale_snapshot.diagnostic.has_value()) {
        return 32;
    }

    return 0;
}
