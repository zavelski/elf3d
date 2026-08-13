#include "viewer_commands.hpp"

#include <elf3d/core/assert.h>

#include <utility>

namespace elf3d::viewer {
namespace {

[[nodiscard]] bool lifecycle_or_view_command(ViewerCommandKind kind) noexcept {
    return kind == ViewerCommandKind::reload_scene || kind == ViewerCommandKind::close_scene ||
           kind == ViewerCommandKind::fit_view || kind == ViewerCommandKind::reset_view ||
           kind == ViewerCommandKind::toggle_section_plane ||
           kind == ViewerCommandKind::flip_section_plane;
}

[[nodiscard]] bool clipping_or_transient_command(ViewerCommandKind kind) noexcept {
    return kind == ViewerCommandKind::add_clipping_box_from_bounds ||
           kind == ViewerCommandKind::clear_clipping ||
           kind == ViewerCommandKind::fit_clipped_content ||
           kind == ViewerCommandKind::clear_selection ||
           kind == ViewerCommandKind::cancel_measurement ||
           kind == ViewerCommandKind::clear_measurement;
}

[[nodiscard]] bool entity_command(ViewerCommandKind kind) noexcept {
    return kind == ViewerCommandKind::select_entity ||
           kind == ViewerCommandKind::set_entity_visibility ||
           kind == ViewerCommandKind::show_all_entities ||
           kind == ViewerCommandKind::isolate_entity || kind == ViewerCommandKind::exit_isolation;
}

[[nodiscard]] bool command_targets_scene(ViewerCommandKind kind) noexcept {
    return lifecycle_or_view_command(kind) || clipping_or_transient_command(kind) ||
           entity_command(kind);
}

template <typename Command>
[[nodiscard]] bool enabled_for(const Command&, const ViewerCapabilitySnapshot&) noexcept {
    return true;
}

[[nodiscard]] bool enabled_for(const ShowSaveDialogCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.scene_imported;
}

[[nodiscard]] bool enabled_for(const ReloadSceneCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.scene_imported;
}

[[nodiscard]] bool enabled_for(const CloseSceneCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.scene_imported;
}

[[nodiscard]] bool enabled_for(const FitViewCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.view_available && capabilities.visible_content;
}

[[nodiscard]] bool enabled_for(const ResetViewCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.view_available && capabilities.visible_content;
}

[[nodiscard]] bool enabled_for(const FitClippedContentCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.view_available && capabilities.visible_content;
}

[[nodiscard]] bool enabled_for(const ActivateViewerToolCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.view_available;
}

[[nodiscard]] bool enabled_for(const FlipSectionPlaneCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.section_plane_enabled;
}

[[nodiscard]] bool enabled_for(const AddClippingBoxFromBoundsCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.can_add_clipping_box && capabilities.unclipped_visible_content;
}

[[nodiscard]] bool enabled_for(const ClearClippingCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.has_clipping;
}

[[nodiscard]] bool enabled_for(const ClearSelectionCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.selected_entity.has_value();
}

[[nodiscard]] bool enabled_for(const CancelMeasurementCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.measurement_incomplete;
}

[[nodiscard]] bool enabled_for(const ClearMeasurementCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.has_measurement;
}

[[nodiscard]] bool enabled_for(const ExitIsolationCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept {
    return capabilities.isolating;
}

[[nodiscard]] ViewerCommandKind kind_of(const ExitViewerCommand&) noexcept {
    return ViewerCommandKind::exit_viewer;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowOpenDialogCommand&) noexcept {
    return ViewerCommandKind::show_open_dialog;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowSaveDialogCommand&) noexcept {
    return ViewerCommandKind::show_save_dialog;
}
[[nodiscard]] ViewerCommandKind kind_of(const ResetViewerLayoutCommand&) noexcept {
    return ViewerCommandKind::reset_layout;
}
[[nodiscard]] ViewerCommandKind kind_of(const ReloadSceneCommand&) noexcept {
    return ViewerCommandKind::reload_scene;
}
[[nodiscard]] ViewerCommandKind kind_of(const CloseSceneCommand&) noexcept {
    return ViewerCommandKind::close_scene;
}
[[nodiscard]] ViewerCommandKind kind_of(const FitViewCommand&) noexcept {
    return ViewerCommandKind::fit_view;
}
[[nodiscard]] ViewerCommandKind kind_of(const ResetViewCommand&) noexcept {
    return ViewerCommandKind::reset_view;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowViewerPanelCommand&) noexcept {
    return ViewerCommandKind::show_panel;
}
[[nodiscard]] ViewerCommandKind kind_of(const ActivateViewerToolCommand&) noexcept {
    return ViewerCommandKind::activate_tool;
}
[[nodiscard]] ViewerCommandKind kind_of(const ToggleSectionPlaneCommand&) noexcept {
    return ViewerCommandKind::toggle_section_plane;
}
[[nodiscard]] ViewerCommandKind kind_of(const FlipSectionPlaneCommand&) noexcept {
    return ViewerCommandKind::flip_section_plane;
}
[[nodiscard]] ViewerCommandKind kind_of(const AddClippingBoxFromBoundsCommand&) noexcept {
    return ViewerCommandKind::add_clipping_box_from_bounds;
}
[[nodiscard]] ViewerCommandKind kind_of(const ClearClippingCommand&) noexcept {
    return ViewerCommandKind::clear_clipping;
}
[[nodiscard]] ViewerCommandKind kind_of(const ToggleClippingHelpersCommand&) noexcept {
    return ViewerCommandKind::toggle_clipping_helpers;
}
[[nodiscard]] ViewerCommandKind kind_of(const FitClippedContentCommand&) noexcept {
    return ViewerCommandKind::fit_clipped_content;
}
[[nodiscard]] ViewerCommandKind kind_of(const ClearSelectionCommand&) noexcept {
    return ViewerCommandKind::clear_selection;
}
[[nodiscard]] ViewerCommandKind kind_of(const CancelMeasurementCommand&) noexcept {
    return ViewerCommandKind::cancel_measurement;
}
[[nodiscard]] ViewerCommandKind kind_of(const ClearMeasurementCommand&) noexcept {
    return ViewerCommandKind::clear_measurement;
}
[[nodiscard]] ViewerCommandKind kind_of(const SelectEntityCommand&) noexcept {
    return ViewerCommandKind::select_entity;
}
[[nodiscard]] ViewerCommandKind kind_of(const SetEntityVisibilityCommand&) noexcept {
    return ViewerCommandKind::set_entity_visibility;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowAllEntitiesCommand&) noexcept {
    return ViewerCommandKind::show_all_entities;
}
[[nodiscard]] ViewerCommandKind kind_of(const IsolateEntityCommand&) noexcept {
    return ViewerCommandKind::isolate_entity;
}
[[nodiscard]] ViewerCommandKind kind_of(const ExitIsolationCommand&) noexcept {
    return ViewerCommandKind::exit_isolation;
}

} // namespace

ViewerCommandKind viewer_command_kind(const ViewerCommand& command) noexcept {
    return std::visit([](const auto& value) noexcept { return kind_of(value); }, command);
}

bool viewer_command_targets_scene(const ViewerCommand& command) noexcept {
    return command_targets_scene(viewer_command_kind(command));
}

bool viewer_command_replaces_scene(const ViewerCommand& command) noexcept {
    const ViewerCommandKind kind = viewer_command_kind(command);
    return kind == ViewerCommandKind::reload_scene || kind == ViewerCommandKind::close_scene;
}

bool viewer_command_enabled(const ViewerCommand& command,
                            const ViewerCapabilitySnapshot& capabilities) noexcept {
    return std::visit(
        [&capabilities](const auto& value) noexcept { return enabled_for(value, capabilities); },
        command);
}

void ViewerCommandDispatcher::begin_frame(const ViewerCapabilitySnapshot& capabilities) noexcept {
    capabilities_ = capabilities;
    queue_count_ = 0;
    next_index_ = 0;
    outcome_count_ = 0;
    next_sequence_ = 1;
    active_dispatch_.reset();
    enqueue_error_.reset();
    replacement_attempted_ = false;
    scene_replaced_ = false;
}

void ViewerCommandDispatcher::emit(ViewerCommand command) noexcept {
    if (queue_count_ >= queue_.size()) {
        if (!enqueue_error_.has_value()) {
            enqueue_error_ = Error{ErrorCode::resource_limit_exceeded,
                                   "Viewer command queue exceeded its per-frame limit"};
        }
        return;
    }
    queue_[queue_count_] = QueuedCommand{std::move(command), capabilities_.scene, next_sequence_};
    ++queue_count_;
    ++next_sequence_;
}

std::optional<ViewerCommandDispatch>
ViewerCommandDispatcher::take_next(SceneId current_scene) noexcept {
    ELF3D_ASSERT(!active_dispatch_.has_value());
    while (next_index_ < queue_count_) {
        const QueuedCommand& queued = queue_[next_index_++];
        if (!viewer_command_enabled(queued.command, capabilities_)) {
            record_outcome(queued, ViewerCommandOutcomeStatus::disabled);
            continue;
        }
        if (viewer_command_targets_scene(queued.command) &&
            (scene_replaced_ || queued.scene != current_scene)) {
            record_outcome(queued, ViewerCommandOutcomeStatus::rejected_after_scene_replacement);
            continue;
        }
        if (viewer_command_replaces_scene(queued.command) && replacement_attempted_) {
            record_outcome(queued, ViewerCommandOutcomeStatus::rejected_replacement_limit);
            continue;
        }
        active_dispatch_ = ViewerCommandDispatch{queued.command, queued.sequence};
        return active_dispatch_;
    }
    return std::nullopt;
}

void ViewerCommandDispatcher::complete(const ViewerCommandDispatch& dispatch,
                                       const ViewerCommandCompletion& completion) noexcept {
    ELF3D_ASSERT(active_dispatch_.has_value());
    ELF3D_ASSERT(active_dispatch_->sequence == dispatch.sequence);
    const QueuedCommand& queued = queue_[next_index_ - 1U];
    if (viewer_command_replaces_scene(queued.command)) {
        replacement_attempted_ = true;
    }
    if (completion.scene_replaced) {
        ELF3D_ASSERT(viewer_command_replaces_scene(queued.command));
        scene_replaced_ = true;
    }
    record_outcome(queued, completion.status, completion.error);
    active_dispatch_.reset();
}

const ViewerCapabilitySnapshot& ViewerCommandDispatcher::capabilities() const noexcept {
    return capabilities_;
}

std::span<const ViewerCommandOutcome> ViewerCommandDispatcher::outcomes() const noexcept {
    return {outcomes_.data(), outcome_count_};
}

std::optional<Error> ViewerCommandDispatcher::enqueue_error() const noexcept {
    return enqueue_error_;
}

bool ViewerCommandDispatcher::scene_replaced() const noexcept {
    return scene_replaced_;
}

void ViewerCommandDispatcher::record_outcome(const QueuedCommand& queued,
                                             ViewerCommandOutcomeStatus status,
                                             std::optional<Error> error) noexcept {
    ELF3D_ASSERT(outcome_count_ < outcomes_.size());
    outcomes_[outcome_count_] =
        ViewerCommandOutcome{viewer_command_kind(queued.command), status, queued.sequence, error};
    ++outcome_count_;
}

} // namespace elf3d::viewer
