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

using elf3d::tools::measurement::DistanceMeasurementController;

struct MeasurementContext {
    MeasurementFixture fixture;
    DistanceMeasurementController controller;
    elf3d::scene::VisibilityFilter visible;
    elf3d::PickHit first_hit;
    elf3d::PickHit second_hit;
};

[[nodiscard]] MeasurementContext make_context() {
    MeasurementFixture fixture = make_fixture();
    const elf3d::scene::VisibilityFilter visible = visibility_filter(fixture.scene);
    const elf3d::PickHit first_hit = make_hit(fixture, {1.0F, 0.0F, 0.0F});
    const elf3d::PickHit second_hit = make_hit(fixture, {0.0F, 1.0F, 0.0F});
    return MeasurementContext{std::move(fixture), {}, visible, first_hit, second_hit};
}

[[nodiscard]] bool
has_first_overlay(const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay>& overlay) {
    return overlay && overlay.value().line_count == 0 && overlay.value().marker_count == 1;
}

[[nodiscard]] bool
has_full_overlay(const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay>& overlay) {
    return overlay && overlay.value().line_count == 1 && overlay.value().marker_count == 2;
}

[[nodiscard]] bool
has_empty_overlay(const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay>& overlay) {
    return overlay && overlay.value().line_count == 0 && overlay.value().marker_count == 0;
}

[[nodiscard]] int verify_display_units() {
    using elf3d::LengthDisplayUnit;
    using elf3d::tools::measurement::display_length;
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
    return 0;
}

[[nodiscard]] int verify_initial_and_invalid_state(MeasurementContext& context) {
    const elf3d::DistanceMeasurementSnapshot initial = context.controller.snapshot(
        context.fixture.scene, context.visible, elf3d::ViewportTool::distance_measurement);
    if (initial.state != elf3d::DistanceMeasurementState::awaiting_first_point ||
        initial.overlay_visible || initial.first_point.has_value()) {
        return 2;
    }

    elf3d::DistanceMeasurementSettings invalid_settings;
    invalid_settings.line_thickness_pixels = std::numeric_limits<float>::quiet_NaN();
    if (context.controller.set_settings(invalid_settings).error().code() !=
        elf3d::ErrorCode::invalid_measurement_settings) {
        return 3;
    }

    elf3d::PickHit invalid_hit = context.first_hit;
    invalid_hit.barycentric_coordinates = {0.6F, 0.6F, 0.0F};
    if (context.controller.place_hit(context.fixture.scene, invalid_hit).error().code() !=
        elf3d::ErrorCode::invalid_measurement_hit) {
        return 4;
    }
    invalid_hit = context.first_hit;
    invalid_hit.triangle_index = 7;
    if (context.controller.place_hit(context.fixture.scene, invalid_hit).error().code() !=
        elf3d::ErrorCode::invalid_measurement_hit) {
        return 5;
    }
    return 0;
}

[[nodiscard]] int verify_first_point(MeasurementContext& context) {
    if (!context.controller.place_hit(context.fixture.scene, context.first_hit)) {
        return 6;
    }
    const elf3d::DistanceMeasurementSnapshot awaiting = context.controller.snapshot(
        context.fixture.scene, context.visible, elf3d::ViewportTool::distance_measurement);
    if (awaiting.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        !awaiting.first_point.has_value() || awaiting.second_point.has_value() ||
        !nearly_equal(awaiting.first_point->world_position, {0.0F, 0.0F, 0.0F})) {
        return 7;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> overlay =
        context.controller.overlay(context.fixture.scene, context.visible);
    if (!has_first_overlay(overlay)) {
        return 8;
    }
    return 0;
}

[[nodiscard]] int verify_preview(MeasurementContext& context) {
    if (!context.controller.update_preview(context.fixture.scene, context.second_hit)) {
        return 9;
    }
    const elf3d::DistanceMeasurementSnapshot preview = context.controller.snapshot(
        context.fixture.scene, context.visible, elf3d::ViewportTool::distance_measurement);
    if (!preview.preview_point.has_value() || !nearly_equal(preview.preview_distance_meters, 1.0) ||
        !preview.midpoint_world_position.has_value() ||
        !nearly_equal(preview.midpoint_world_position.value(), {0.5F, 0.0F, 0.0F})) {
        return 10;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> overlay =
        context.controller.overlay(context.fixture.scene, context.visible);
    if (!has_full_overlay(overlay)) {
        return 11;
    }
    context.controller.clear_preview();
    if (context.controller
            .snapshot(context.fixture.scene, context.visible,
                      elf3d::ViewportTool::distance_measurement)
            .preview_point.has_value()) {
        return 12;
    }
    return 0;
}

[[nodiscard]] int verify_completion(MeasurementContext& context) {
    if (!context.controller.place_hit(context.fixture.scene, context.second_hit)) {
        return 13;
    }
    const elf3d::DistanceMeasurementSnapshot complete = context.controller.snapshot(
        context.fixture.scene, context.visible, elf3d::ViewportTool::selection);
    if (complete.state != elf3d::DistanceMeasurementState::complete ||
        !complete.first_point.has_value() || !complete.second_point.has_value() ||
        !nearly_equal(complete.distance_meters, 1.0) || !complete.overlay_visible) {
        return 14;
    }
    return 0;
}

[[nodiscard]] int verify_clipped_measurement(MeasurementContext& context) {
    elf3d::SectionPlane anchor_clip_plane;
    anchor_clip_plane.enabled = true;
    anchor_clip_plane.point = {0.5F, 0.0F, 0.0F};
    anchor_clip_plane.normal = {1.0F, 0.0F, 0.0F};
    const elf3d::clipping::ClippingFilter anchor_clip_filter =
        elf3d::clipping::make_filter(anchor_clip_plane, {}, 1).value();
    const elf3d::DistanceMeasurementSnapshot clipped_complete = context.controller.snapshot(
        context.fixture.scene, context.visible, anchor_clip_filter, elf3d::ViewportTool::selection);
    if (clipped_complete.state != elf3d::DistanceMeasurementState::complete ||
        !nearly_equal(clipped_complete.distance_meters, 1.0) || clipped_complete.overlay_visible ||
        clipped_complete.anchors_currently_visible) {
        return 141;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> clipped_overlay =
        context.controller.overlay(context.fixture.scene, context.visible, anchor_clip_filter);
    if (!has_empty_overlay(clipped_overlay)) {
        return 142;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> unclipped_overlay =
        context.controller.overlay(context.fixture.scene, context.visible,
                                   elf3d::clipping::disabled_filter());
    if (!has_full_overlay(unclipped_overlay)) {
        return 143;
    }
    return 0;
}

[[nodiscard]] int verify_transforms(MeasurementContext& context) {
    elf3d::Transform scaled;
    scaled.scale = {2.0F, 3.0F, -1.0F};
    if (!context.fixture.scene.set_local_transform(context.fixture.model, scaled)) {
        return 15;
    }
    elf3d::DistanceMeasurementSnapshot complete =
        context.controller.snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                                    elf3d::ViewportTool::selection);
    if (!nearly_equal(complete.distance_meters, 2.0) ||
        !nearly_equal(complete.first_point->world_normal, {0.0F, 0.0F, -1.0F})) {
        return 16;
    }

    elf3d::Transform rotated = scaled;
    rotated.rotation = axis_angle_z(1.5707963268F);
    if (!context.fixture.scene.set_local_transform(context.fixture.model, rotated)) {
        return 17;
    }
    complete =
        context.controller.snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                                    elf3d::ViewportTool::selection);
    if (!nearly_equal(complete.distance_meters, 2.0)) {
        return 18;
    }
    return 0;
}

[[nodiscard]] int verify_hidden_measurement(MeasurementContext& context) {
    if (!context.fixture.scene.set_entity_visible(context.fixture.model, false)) {
        return 19;
    }
    const elf3d::scene::VisibilityFilter hidden = visibility_filter(context.fixture.scene);
    const elf3d::DistanceMeasurementSnapshot complete =
        context.controller.snapshot(context.fixture.scene, hidden, elf3d::ViewportTool::selection);
    if (complete.overlay_visible || complete.anchors_currently_visible) {
        return 20;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> hidden_overlay =
        context.controller.overlay(context.fixture.scene, hidden);
    if (!has_empty_overlay(hidden_overlay)) {
        return 21;
    }
    return 0;
}

[[nodiscard]] int verify_restored_measurement(MeasurementContext& context) {
    if (!context.fixture.scene.show_entity_and_ancestors(context.fixture.model)) {
        return 22;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> restored_overlay =
        context.controller.overlay(context.fixture.scene, visibility_filter(context.fixture.scene));
    if (!has_full_overlay(restored_overlay)) {
        return 23;
    }

    const elf3d::EntityId unrelated = context.fixture.scene.create_entity().value();
    const elf3d::scene::VisibilityFilter isolated =
        visibility_filter(context.fixture.scene, unrelated);
    const elf3d::DistanceMeasurementSnapshot complete = context.controller.snapshot(
        context.fixture.scene, isolated, elf3d::ViewportTool::selection);
    if (complete.overlay_visible || complete.anchors_currently_visible) {
        return 24;
    }
    return 0;
}

[[nodiscard]] int verify_cancellation_and_clear(MeasurementContext& context) {
    if (!context.controller.place_hit(context.fixture.scene, context.first_hit)) {
        return 25;
    }
    const elf3d::DistanceMeasurementSnapshot awaiting =
        context.controller.snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                                    elf3d::ViewportTool::distance_measurement);
    if (awaiting.state != elf3d::DistanceMeasurementState::awaiting_second_point ||
        awaiting.second_point.has_value()) {
        return 26;
    }
    context.controller.cancel_incomplete();
    if (context.controller
            .snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                      elf3d::ViewportTool::distance_measurement)
            .state != elf3d::DistanceMeasurementState::awaiting_first_point) {
        return 27;
    }

    if (!context.controller.place_hit(context.fixture.scene, context.first_hit) ||
        !context.controller.place_hit(context.fixture.scene, context.second_hit)) {
        return 28;
    }
    context.controller.cancel_incomplete();
    if (context.controller
            .snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                      elf3d::ViewportTool::selection)
            .state != elf3d::DistanceMeasurementState::complete) {
        return 29;
    }
    context.controller.clear();
    if (context.controller
            .snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                      elf3d::ViewportTool::selection)
            .state != elf3d::DistanceMeasurementState::empty) {
        return 30;
    }
    return 0;
}

[[nodiscard]] int verify_stale_measurement(MeasurementContext& context) {
    if (!context.controller.place_hit(context.fixture.scene, context.first_hit) ||
        !context.controller.place_hit(context.fixture.scene, context.second_hit) ||
        !context.fixture.scene.destroy_entity(context.fixture.model)) {
        return 31;
    }
    const elf3d::Result<elf3d::tools::measurement::MeasurementOverlay> stale_overlay =
        context.controller.overlay(context.fixture.scene, visibility_filter(context.fixture.scene));
    const elf3d::DistanceMeasurementSnapshot stale_snapshot =
        context.controller.snapshot(context.fixture.scene, visibility_filter(context.fixture.scene),
                                    elf3d::ViewportTool::selection);
    if (!stale_overlay || stale_overlay.value().line_count != 0 ||
        stale_snapshot.state != elf3d::DistanceMeasurementState::empty ||
        !stale_snapshot.diagnostic.has_value()) {
        return 32;
    }
    return 0;
}

[[nodiscard]] int verify_creation_flow(MeasurementContext& context) {
    const int initial = verify_initial_and_invalid_state(context);
    if (initial != 0) {
        return initial;
    }
    const int first = verify_first_point(context);
    if (first != 0) {
        return first;
    }
    const int preview = verify_preview(context);
    if (preview != 0) {
        return preview;
    }
    const int complete = verify_completion(context);
    if (complete != 0) {
        return complete;
    }
    return verify_clipped_measurement(context);
}

[[nodiscard]] int verify_update_flow(MeasurementContext& context) {
    const int transforms = verify_transforms(context);
    if (transforms != 0) {
        return transforms;
    }
    const int hidden = verify_hidden_measurement(context);
    if (hidden != 0) {
        return hidden;
    }
    const int restored = verify_restored_measurement(context);
    if (restored != 0) {
        return restored;
    }
    const int cancellation = verify_cancellation_and_clear(context);
    if (cancellation != 0) {
        return cancellation;
    }
    return verify_stale_measurement(context);
}

} // namespace

int elf3d_tool_measurement_test() {
    const int units = verify_display_units();
    if (units != 0) {
        return units;
    }
    MeasurementContext context = make_context();
    const int created = verify_creation_flow(context);
    if (created != 0) {
        return created;
    }
    return verify_update_flow(context);
}
