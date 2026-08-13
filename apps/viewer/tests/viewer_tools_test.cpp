#include "viewer_tools.hpp"

#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>

namespace {

using elf3d::viewer::DistanceMeasurementSettings;
using elf3d::viewer::DistanceMeasurementSnapshot;
using elf3d::viewer::DistanceMeasurementState;

[[nodiscard]] elf3d::Result<void> failure(const char* message) noexcept {
    return elf3d::Error{elf3d::ErrorCode::invalid_argument, message};
}

[[nodiscard]] bool nearly_equal(double left, double right) noexcept {
    return std::abs(left - right) <= 1.0e-8;
}

class ViewerToolsApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        const elf3d::Result<void> initialized = initialize_scene(context);
        if (!initialized) {
            return initialized.error();
        }
        const elf3d::Result<void> selection = verify_selection_tool();
        if (!selection) {
            return selection.error();
        }
        const elf3d::Result<void> settings = verify_settings();
        if (!settings) {
            return settings.error();
        }
        const elf3d::Result<void> clipping = verify_clipping_tool();
        if (!clipping) {
            return clipping.error();
        }
        const elf3d::Result<void> measurement = verify_measurement_flow();
        if (!measurement) {
            return measurement.error();
        }
        passed_ = true;
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override {
        context.request_exit();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        viewport_.reset();
        scene_.reset();
    }

    [[nodiscard]] bool passed() const noexcept {
        return passed_;
    }

  private:
    [[nodiscard]] elf3d::Result<void> initialize_scene(elf3d::ApplicationContext& context) {
        elf3d::Result<std::unique_ptr<elf3d::Scene>> scene = context.engine().create_scene();
        elf3d::Result<std::unique_ptr<elf3d::Viewport>> viewport =
            context.engine().create_viewport({320, 240});
        if (!scene) {
            return scene.error();
        }
        if (!viewport) {
            return viewport.error();
        }
        scene_ = std::move(scene).value();
        viewport_ = std::move(viewport).value();

        constexpr std::array<elf3d::VertexPositionNormal, 3> vertices{{
            {{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
            {{1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
            {{0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        }};
        constexpr std::array<std::uint32_t, 3> indices{{0, 1, 2}};
        const auto mesh = scene_->create_mesh({vertices, indices});
        const auto material = scene_->create_material({});
        if (!mesh || !material) {
            return failure("Viewer Tool test could not create assets");
        }
        mesh_ = mesh.value();
        const auto model = scene_->create_model_entity(mesh_, material.value());
        if (!model) {
            return model.error();
        }
        model_ = model.value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_settings() {
        DistanceMeasurementSettings invalid;
        invalid.marker_radius_pixels = std::numeric_limits<float>::quiet_NaN();
        const elf3d::Result<void> invalid_result = tools_.measurement().set_settings(invalid);
        if (invalid_result || invalid_result.error().code() != elf3d::ErrorCode::invalid_argument) {
            return failure("Viewer MeasurementTool settings validation is incorrect");
        }
        DistanceMeasurementSettings settings;
        settings.line_thickness_pixels = 3.0F;
        if (!tools_.measurement().set_settings(settings)) {
            return failure("Viewer MeasurementTool rejected valid settings");
        }
        tools_.activate(elf3d::viewer::ViewerTool::distance_measurement);
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_selection_tool() {
        elf3d::viewer::SelectionToolSettings invalid;
        invalid.highlight_strength = std::numeric_limits<float>::quiet_NaN();
        const elf3d::Result<void> rejected = tools_.selection().set_settings(invalid);
        if (rejected || rejected.error().code() != elf3d::ErrorCode::invalid_argument) {
            return failure("Viewer SelectionTool settings validation is incorrect");
        }
        elf3d::viewer::SelectionToolSettings settings;
        settings.highlight_strength = 0.6F;
        if (!tools_.selection().set_settings(settings) ||
            !viewport_->set_selected_entity(*scene_, model_)) {
            return failure("Viewer SelectionTool setup failed");
        }
        const std::optional<elf3d::EntityHighlight> feedback =
            tools_.selection().render_feedback(*viewport_);
        if (!feedback.has_value() || feedback->entity != model_ ||
            feedback->strength != settings.highlight_strength) {
            return failure("Viewer SelectionTool did not produce generic highlight feedback");
        }
        tools_.selection().set_enabled(false);
        if (tools_.selection().render_feedback(*viewport_).has_value()) {
            return failure("Disabled Viewer SelectionTool still produced feedback");
        }
        tools_.selection().set_enabled(true);
        viewport_->clear_selection();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_clipping_tool() {
        const elf3d::Result<void> settings_result = verify_clipping_settings();
        if (!settings_result) {
            return settings_result.error();
        }
        return verify_clipping_overlay();
    }

    [[nodiscard]] elf3d::Result<void> verify_clipping_settings() {
        elf3d::viewer::ClippingToolSettings invalid;
        invalid.line_thickness_pixels = std::numeric_limits<float>::quiet_NaN();
        const elf3d::Result<void> rejected = tools_.clipping().set_settings(invalid);
        if (rejected || rejected.error().code() != elf3d::ErrorCode::invalid_argument) {
            return failure("Viewer ClippingTool settings validation is incorrect");
        }
        elf3d::viewer::ClippingToolSettings settings;
        settings.line_thickness_pixels = 3.0F;
        if (!tools_.clipping().set_settings(settings)) {
            return failure("Viewer ClippingTool rejected valid settings");
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_clipping_overlay() {
        const elf3d::Result<std::uint32_t> added =
            tools_.clipping().add_box_from_visible_bounds(*scene_, *viewport_);
        if (!added || added.value() != 0 || viewport_->clipping_snapshot().box_count != 1) {
            return failure("Viewer ClippingTool did not add a box from visible bounds");
        }
        elf3d::SectionPlane plane;
        plane.enabled = true;
        plane.normal = {1.0F, 0.0F, 0.0F};
        if (!viewport_->set_section_plane(plane)) {
            return failure("Viewer ClippingTool section-plane setup failed");
        }
        const elf3d::Result<elf3d::viewer::ClippingToolOverlay> overlay =
            tools_.clipping().overlay(*scene_, *viewport_);
        if (!overlay || overlay.value().line_count != 16) {
            return failure("Viewer ClippingTool did not compose generic helper lines");
        }
        tools_.clipping().set_helpers_visible(false);
        const auto hidden_overlay = tools_.clipping().overlay(*scene_, *viewport_);
        if (!hidden_overlay || hidden_overlay.value().line_count != 0) {
            return failure("Viewer ClippingTool helper visibility is incorrect");
        }
        tools_.clipping().set_helpers_visible(true);
        viewport_->clear_clipping();
        return {};
    }

    [[nodiscard]] elf3d::PickHit first_hit() const noexcept {
        elf3d::PickHit first;
        first.entity = model_;
        first.mesh = mesh_;
        first.barycentric_coordinates = {1.0F, 0.0F, 0.0F};
        first.world_normal = {0.0F, 0.0F, 1.0F};
        first.world_distance = 1.0F;
        return first;
    }

    [[nodiscard]] elf3d::Result<void> verify_measurement_flow() {
        const elf3d::PickHit first = first_hit();
        elf3d::PickHit second = first;
        second.barycentric_coordinates = {0.0F, 1.0F, 0.0F};
        second.world_position = {1.0F, 0.0F, 0.0F};

        const elf3d::Result<void> first_anchor = verify_first_anchor(first);
        if (!first_anchor) {
            return first_anchor.error();
        }
        const elf3d::Result<void> completed = verify_completion(second);
        if (!completed) {
            return completed.error();
        }
        return verify_visibility_and_clear();
    }

    [[nodiscard]] elf3d::Result<void> verify_first_anchor(const elf3d::PickHit& first) {
        if (!tools_.measurement().place_hit(*scene_, first)) {
            return failure("Viewer MeasurementTool did not accept the first anchor");
        }
        const DistanceMeasurementSnapshot awaiting = tools_.measurement().snapshot(
            *scene_, *viewport_,
            tools_.active_tool() == elf3d::viewer::ViewerTool::distance_measurement);
        if (awaiting.state != DistanceMeasurementState::awaiting_second_point ||
            !awaiting.first_point.has_value()) {
            return failure("Viewer MeasurementTool first-point transition is incorrect");
        }

        elf3d::Transform moved;
        moved.translation = {2.0F, 0.0F, 0.0F};
        if (!scene_->set_local_transform(model_, moved)) {
            return failure("Viewer MeasurementTool transform setup failed");
        }
        const DistanceMeasurementSnapshot moved_snapshot =
            tools_.measurement().snapshot(*scene_, *viewport_, true);
        if (!moved_snapshot.first_point.has_value() ||
            moved_snapshot.first_point->world_position != elf3d::Float3{2.0F, 0.0F, 0.0F}) {
            return failure("Viewer MeasurementTool did not re-resolve a transformed anchor");
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_completion(const elf3d::PickHit& second) {
        if (!tools_.measurement().update_preview(*scene_, second)) {
            return failure("Viewer MeasurementTool did not accept a preview anchor");
        }
        const DistanceMeasurementSnapshot preview =
            tools_.measurement().snapshot(*scene_, *viewport_, true);
        if (!preview.preview_point.has_value() ||
            !nearly_equal(preview.preview_distance_meters, 1.0)) {
            return failure("Viewer MeasurementTool preview is incorrect");
        }
        if (!tools_.measurement().place_hit(*scene_, second)) {
            return failure("Viewer MeasurementTool did not accept the second anchor");
        }
        const DistanceMeasurementSnapshot complete =
            tools_.measurement().snapshot(*scene_, *viewport_, true);
        const auto overlay = tools_.measurement().overlay(*scene_, *viewport_);
        if (complete.state != DistanceMeasurementState::complete ||
            !nearly_equal(complete.distance_meters, 1.0) || !overlay ||
            overlay.value().line_count != 1 || overlay.value().marker_count != 2) {
            return failure("Viewer MeasurementTool completion or overlay is incorrect");
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_visibility_and_clear() {
        if (!scene_->set_entity_local_visibility(model_, false)) {
            return failure("Viewer MeasurementTool visibility setup failed");
        }
        const DistanceMeasurementSnapshot hidden =
            tools_.measurement().snapshot(*scene_, *viewport_, true);
        if (hidden.anchors_currently_visible || hidden.overlay_visible) {
            return failure("Viewer MeasurementTool ignored viewport visibility");
        }
        tools_.clear_scene(scene_->id());
        if (tools_.measurement().snapshot(*scene_, *viewport_, true).state !=
            DistanceMeasurementState::awaiting_first_point) {
            return failure("Viewer MeasurementTool scene clearing is incorrect");
        }
        return {};
    }

    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId model_;
    elf3d::MeshHandle mesh_;
    elf3d::viewer::ToolCoordinator tools_;
    bool passed_ = false;
};

[[nodiscard]] bool environment_cannot_create_context(elf3d::ErrorCode code) noexcept {
    return code == elf3d::ErrorCode::graphics_initialization_failed ||
           code == elf3d::ErrorCode::graphics_context_unavailable ||
           code == elf3d::ErrorCode::unsupported_graphics_version;
}

} // namespace

int main() {
    ViewerToolsApplication application;
    elf3d::ApplicationOptions options;
    options.title = "Elf3D viewer Tools test";
    options.initial_window_extent = {320, 240};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    const elf3d::Result<int> result = elf3d::run_application(options, application);
    if (!result) {
        if (environment_cannot_create_context(result.error().code())) {
            return 77;
        }
        std::fprintf(stderr, "Viewer Tools test failed: %s\n", result.error().message());
        return 1;
    }
    return application.passed() ? 0 : 1;
}
