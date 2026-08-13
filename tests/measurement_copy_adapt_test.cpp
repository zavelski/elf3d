#include "copy_adapt/measurement_tool.hpp"

#include <elf3d/app/application.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

namespace {

[[nodiscard]] elf3d::Result<void> failure(const char* message) noexcept {
    return elf3d::Error{elf3d::ErrorCode::invalid_argument, message};
}

[[nodiscard]] bool nearly_equal(double left, double right) noexcept {
    return std::abs(left - right) <= 1.0e-8;
}

class CopyAdaptApplication final : public elf3d::Application {
  public:
    [[nodiscard]] elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override {
        const elf3d::Result<void> initialized = initialize_scene(context);
        if (!initialized) {
            return initialized.error();
        }
        const elf3d::Result<void> verified = verify_measurement();
        if (!verified) {
            return verified.error();
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override {
        ++update_count_;
        if (update_count_ >= 2) {
            context.request_exit();
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> build_ui(elf3d::ApplicationUiContext&) noexcept override {
        elf3d::ViewportRenderOptions options;
        options.overlay_lines = overlay_.line_span();
        options.overlay_markers = overlay_.marker_span();
        render_options_composed_ =
            options.overlay_lines.size() == 1 && options.overlay_markers.size() == 2;
        return {};
    }

    void stop(elf3d::ApplicationContext&) noexcept override {
        viewport_.reset();
        scene_.reset();
    }

    [[nodiscard]] bool passed() const noexcept {
        return verified_ && render_options_composed_ && scene_ == nullptr && viewport_ == nullptr;
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
        const elf3d::Result<elf3d::MeshHandle> mesh = scene_->create_mesh({vertices, indices});
        const elf3d::Result<elf3d::MaterialHandle> material = scene_->create_material({});
        if (!mesh || !material) {
            return failure("Copy/adapt proof could not create measurement assets");
        }
        mesh_ = mesh.value();
        const elf3d::Result<elf3d::EntityId> model =
            scene_->create_model_entity(mesh_, material.value());
        if (!model) {
            return model.error();
        }
        model_ = model.value();
        return {};
    }

    [[nodiscard]] elf3d::PickHit hit(elf3d::Float3 barycentric) const noexcept {
        elf3d::PickHit result;
        result.entity = model_;
        result.mesh = mesh_;
        result.world_normal = {0.0F, 0.0F, 1.0F};
        result.barycentric_coordinates = barycentric;
        result.world_distance = 1.0F;
        return result;
    }

    [[nodiscard]] elf3d::Result<void> verify_measurement() {
        const elf3d::Result<void> first = verify_first_point();
        if (!first) {
            return first.error();
        }
        const elf3d::Result<void> preview = verify_preview();
        if (!preview) {
            return preview.error();
        }
        const elf3d::Result<void> complete = verify_completion();
        if (!complete) {
            return complete.error();
        }
        const elf3d::Result<void> visibility = verify_visibility_and_clear();
        if (!visibility) {
            return visibility.error();
        }
        verified_ = true;
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_first_point() {
        const elf3d::Result<void> first = tool_.place_hit(*scene_, hit({1.0F, 0.0F, 0.0F}));
        if (!first) {
            return first.error();
        }
        const elf3d::Result<elf3d_external::MeasurementSnapshot> awaiting =
            tool_.snapshot(*scene_, *viewport_);
        if (!awaiting ||
            awaiting.value().state != elf3d_external::MeasurementState::awaiting_second_point ||
            !awaiting.value().first_point.has_value()) {
            return failure("Copied Measurement Tool first-point state is incorrect");
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_preview() {
        elf3d::Transform moved;
        moved.translation = {2.0F, 0.0F, 0.0F};
        const elf3d::Result<void> transformed = scene_->set_local_transform(model_, moved);
        if (!transformed) {
            return transformed.error();
        }
        const elf3d::Result<void> preview = tool_.update_preview(*scene_, hit({0.0F, 1.0F, 0.0F}));
        if (!preview) {
            return preview.error();
        }
        const elf3d::Result<elf3d_external::MeasurementSnapshot> preview_snapshot =
            tool_.snapshot(*scene_, *viewport_);
        if (!preview_snapshot || !preview_snapshot.value().first_point.has_value() ||
            preview_snapshot.value().first_point->world_position !=
                elf3d::Float3{2.0F, 0.0F, 0.0F} ||
            !nearly_equal(preview_snapshot.value().preview_distance_meters, 1.0)) {
            return failure("Copied Measurement Tool did not re-resolve its public surface anchors");
        }
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_completion() {
        const elf3d::Result<void> second = tool_.place_hit(*scene_, hit({0.0F, 1.0F, 0.0F}));
        if (!second) {
            return second.error();
        }
        const elf3d::Result<elf3d_external::MeasurementSnapshot> complete =
            tool_.snapshot(*scene_, *viewport_);
        const elf3d::Result<elf3d_external::MeasurementOverlay> overlay =
            tool_.overlay(*scene_, *viewport_);
        if (!complete || complete.value().state != elf3d_external::MeasurementState::complete ||
            !nearly_equal(complete.value().distance_meters, 1.0) || !overlay ||
            overlay.value().line_count != 1 || overlay.value().marker_count != 2) {
            return failure("Copied Measurement Tool completion or generic overlay is incorrect");
        }
        overlay_ = overlay.value();
        return {};
    }

    [[nodiscard]] elf3d::Result<void> verify_visibility_and_clear() {
        const elf3d::Result<void> hidden = scene_->set_entity_local_visibility(model_, false);
        if (!hidden) {
            return hidden.error();
        }
        const elf3d::Result<elf3d_external::MeasurementSnapshot> hidden_snapshot =
            tool_.snapshot(*scene_, *viewport_);
        const elf3d::Result<elf3d_external::MeasurementOverlay> hidden_overlay =
            tool_.overlay(*scene_, *viewport_);
        if (!hidden_snapshot || hidden_snapshot.value().overlay_visible || !hidden_overlay ||
            hidden_overlay.value().line_count != 0 || hidden_overlay.value().marker_count != 0) {
            return failure("Copied Measurement Tool ignored public viewport visibility");
        }
        const elf3d::Result<void> shown = scene_->set_entity_local_visibility(model_, true);
        if (!shown) {
            return shown.error();
        }
        tool_.clear();
        const elf3d::Result<elf3d_external::MeasurementSnapshot> cleared =
            tool_.snapshot(*scene_, *viewport_);
        if (!cleared || cleared.value().state != elf3d_external::MeasurementState::empty) {
            return failure("Copied Measurement Tool clear transition is incorrect");
        }
        return {};
    }

    std::unique_ptr<elf3d::Scene> scene_;
    std::unique_ptr<elf3d::Viewport> viewport_;
    elf3d::EntityId model_;
    elf3d::MeshHandle mesh_;
    elf3d_external::MeasurementTool tool_;
    elf3d_external::MeasurementOverlay overlay_;
    int update_count_ = 0;
    bool verified_ = false;
    bool render_options_composed_ = false;
};

[[nodiscard]] bool environment_cannot_create_context(elf3d::ErrorCode code) noexcept {
    return code == elf3d::ErrorCode::graphics_initialization_failed ||
           code == elf3d::ErrorCode::graphics_context_unavailable ||
           code == elf3d::ErrorCode::unsupported_graphics_version;
}

} // namespace

int main() {
    CopyAdaptApplication application;
    elf3d::ApplicationOptions options;
    options.title = "Elf3D external Measurement Tool copy/adapt proof";
    options.initial_window_extent = {320, 240};
    options.initial_visibility = elf3d::ApplicationWindowVisibility::hidden;
    const elf3d::Result<int> result = elf3d::run_application(options, application);
    if (!result) {
        if (environment_cannot_create_context(result.error().code())) {
            return 77;
        }
        std::fprintf(stderr, "Measurement copy/adapt proof failed: %s\n", result.error().message());
        return 1;
    }
    return application.passed() ? 0 : 1;
}
