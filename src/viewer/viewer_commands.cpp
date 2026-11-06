#include "viewer_commands.hpp"

#include <elf3d/core/assert.h>

#include <utility>

namespace elf3d::viewer {
namespace {

[[nodiscard]] bool lifecycle_or_view_command(ViewerCommandKind kind) noexcept
{
    return kind == ViewerCommandKind::reload_scene || kind == ViewerCommandKind::close_scene ||
           kind == ViewerCommandKind::fit_view || kind == ViewerCommandKind::reset_view ||
           kind == ViewerCommandKind::frame_selection;
}

[[nodiscard]] bool transient_command(ViewerCommandKind kind) noexcept
{
    return kind == ViewerCommandKind::clear_selection ||
           kind == ViewerCommandKind::cancel_measurement ||
           kind == ViewerCommandKind::clear_measurement;
}

[[nodiscard]] bool entity_command(ViewerCommandKind kind) noexcept
{
    return kind == ViewerCommandKind::select_entity ||
           kind == ViewerCommandKind::select_all_entities ||
           kind == ViewerCommandKind::set_selection_visibility ||
           kind == ViewerCommandKind::isolate_selection ||
           kind == ViewerCommandKind::set_entity_visibility ||
           kind == ViewerCommandKind::show_all_entities ||
           kind == ViewerCommandKind::isolate_entity || kind == ViewerCommandKind::exit_isolation ||
           kind == ViewerCommandKind::resource_action;
}

[[nodiscard]] bool command_targets_scene(ViewerCommandKind kind) noexcept
{
    return kind == ViewerCommandKind::show_save_dialog || kind == ViewerCommandKind::save_model ||
           lifecycle_or_view_command(kind) || transient_command(kind) || entity_command(kind);
}

template <typename Command>
[[nodiscard]] bool enabled_for(const Command&, const ViewerCapabilitySnapshot&) noexcept
{
    return true;
}

[[nodiscard]] bool enabled_for(const ShowOpenDialogCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return !capabilities.model_busy;
}
[[nodiscard]] bool enabled_for(const SaveModelCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported && !capabilities.model_busy;
}

[[nodiscard]] bool enabled_for(const ShowSaveDialogCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported && !capabilities.model_busy;
}

[[nodiscard]] bool enabled_for(const ReloadSceneCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported && !capabilities.model_busy;
}

[[nodiscard]] bool enabled_for(const CloseSceneCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported;
}

[[nodiscard]] bool enabled_for(const FitViewCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available && capabilities.spatial_available &&
           capabilities.visible_content;
}

[[nodiscard]] bool enabled_for(const ResetViewCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available && capabilities.spatial_available &&
           capabilities.visible_content;
}

[[nodiscard]] bool enabled_for(const FrameSelectionCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available && capabilities.spatial_available &&
           capabilities.visible_content && capabilities.has_spatial_selection;
}

[[nodiscard]] bool enabled_for(const ActivateViewerToolCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available;
}

[[nodiscard]] bool enabled_for(const SetShadingModeCommand& command,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available && capabilities.spatial_available &&
           (command.mode == RenderShadingMode::standard ||
            command.mode == RenderShadingMode::unlit);
}

[[nodiscard]] bool enabled_for(const SetBackfaceCullingCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.view_available && capabilities.spatial_available;
}

[[nodiscard]] bool enabled_for(const ClearSelectionCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.has_selection || capabilities.selected_entity.has_value();
}
[[nodiscard]] bool enabled_for(const SelectAllEntitiesCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported;
}
[[nodiscard]] bool enabled_for(const SetSelectionVisibilityCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.selected_entity.has_value() || capabilities.has_spatial_selection;
}
[[nodiscard]] bool enabled_for(const IsolateSelectionCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.selected_entity.has_value() || capabilities.has_spatial_selection;
}
[[nodiscard]] bool enabled_for(const ResourceActionCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.scene_imported;
}

[[nodiscard]] bool enabled_for(const CancelMeasurementCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.measurement_incomplete;
}

[[nodiscard]] bool enabled_for(const ClearMeasurementCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.has_measurement;
}

[[nodiscard]] bool enabled_for(const ExitIsolationCommand&,
                               const ViewerCapabilitySnapshot& capabilities) noexcept
{
    return capabilities.isolating;
}

[[nodiscard]] ViewerCommandKind kind_of(const ExitViewerCommand&) noexcept
{
    return ViewerCommandKind::exit_viewer;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowOpenDialogCommand&) noexcept
{
    return ViewerCommandKind::show_open_dialog;
}
[[nodiscard]] ViewerCommandKind kind_of(const SaveModelCommand&) noexcept
{
    return ViewerCommandKind::save_model;
}

[[nodiscard]] ViewerCommandKind kind_of(const ShowSaveDialogCommand&) noexcept
{
    return ViewerCommandKind::show_save_dialog;
}
[[nodiscard]] ViewerCommandKind kind_of(const ResetViewerLayoutCommand&) noexcept
{
    return ViewerCommandKind::reset_layout;
}
[[nodiscard]] ViewerCommandKind kind_of(const ReloadSceneCommand&) noexcept
{
    return ViewerCommandKind::reload_scene;
}
[[nodiscard]] ViewerCommandKind kind_of(const CloseSceneCommand&) noexcept
{
    return ViewerCommandKind::close_scene;
}
[[nodiscard]] ViewerCommandKind kind_of(const FitViewCommand&) noexcept
{
    return ViewerCommandKind::fit_view;
}
[[nodiscard]] ViewerCommandKind kind_of(const ResetViewCommand&) noexcept
{
    return ViewerCommandKind::reset_view;
}
[[nodiscard]] ViewerCommandKind kind_of(const FrameSelectionCommand&) noexcept
{
    return ViewerCommandKind::frame_selection;
}
[[nodiscard]] ViewerCommandKind kind_of(const SetShadingModeCommand&) noexcept
{
    return ViewerCommandKind::set_shading_mode;
}
[[nodiscard]] ViewerCommandKind kind_of(const SetBackfaceCullingCommand&) noexcept
{
    return ViewerCommandKind::set_backface_culling;
}
[[nodiscard]] ViewerCommandKind kind_of(const ActivateViewerToolCommand&) noexcept
{
    return ViewerCommandKind::activate_tool;
}
[[nodiscard]] ViewerCommandKind kind_of(const ClearSelectionCommand&) noexcept
{
    return ViewerCommandKind::clear_selection;
}
[[nodiscard]] ViewerCommandKind kind_of(const SelectAllEntitiesCommand&) noexcept
{
    return ViewerCommandKind::select_all_entities;
}
[[nodiscard]] ViewerCommandKind kind_of(const SetSelectionVisibilityCommand&) noexcept
{
    return ViewerCommandKind::set_selection_visibility;
}
[[nodiscard]] ViewerCommandKind kind_of(const IsolateSelectionCommand&) noexcept
{
    return ViewerCommandKind::isolate_selection;
}
[[nodiscard]] ViewerCommandKind kind_of(const ResourceActionCommand&) noexcept
{
    return ViewerCommandKind::resource_action;
}
[[nodiscard]] ViewerCommandKind kind_of(const CancelMeasurementCommand&) noexcept
{
    return ViewerCommandKind::cancel_measurement;
}
[[nodiscard]] ViewerCommandKind kind_of(const ClearMeasurementCommand&) noexcept
{
    return ViewerCommandKind::clear_measurement;
}
[[nodiscard]] ViewerCommandKind kind_of(const SelectEntityCommand&) noexcept
{
    return ViewerCommandKind::select_entity;
}
[[nodiscard]] ViewerCommandKind kind_of(const SetEntityVisibilityCommand&) noexcept
{
    return ViewerCommandKind::set_entity_visibility;
}
[[nodiscard]] ViewerCommandKind kind_of(const ShowAllEntitiesCommand&) noexcept
{
    return ViewerCommandKind::show_all_entities;
}
[[nodiscard]] ViewerCommandKind kind_of(const IsolateEntityCommand&) noexcept
{
    return ViewerCommandKind::isolate_entity;
}
[[nodiscard]] ViewerCommandKind kind_of(const ExitIsolationCommand&) noexcept
{
    return ViewerCommandKind::exit_isolation;
}

} // namespace

ViewerCommandKind viewer_command_kind(const ViewerCommand& command) noexcept
{
    return std::visit([](const auto& value) noexcept { return kind_of(value); }, command);
}

bool viewer_command_targets_scene(const ViewerCommand& command) noexcept
{
    return command_targets_scene(viewer_command_kind(command));
}

bool viewer_command_replaces_scene(const ViewerCommand& command) noexcept
{
    const ViewerCommandKind kind = viewer_command_kind(command);
    return kind == ViewerCommandKind::reload_scene || kind == ViewerCommandKind::close_scene;
}

namespace {
[[nodiscard]] const char*
disabled_view_reason(const ViewerCapabilitySnapshot& capabilities) noexcept
{
    if (!capabilities.view_available)
        return "Show a nonempty 3D View panel to use this control.";
    if (!capabilities.spatial_available)
        return "Scene graphics are being prepared or replaced.";
    if (!capabilities.visible_content)
        return "The current view has no visible geometry after visibility and clipping.";
    return "Select an object or resource with an active scene instance.";
}
[[nodiscard]] bool uses_view_reason(ViewerCommandKind kind) noexcept
{
    return kind == ViewerCommandKind::set_backface_culling ||
           kind == ViewerCommandKind::frame_selection ||
           kind == ViewerCommandKind::set_shading_mode || kind == ViewerCommandKind::fit_view ||
           kind == ViewerCommandKind::reset_view || kind == ViewerCommandKind::activate_tool;
}
} // namespace
const char* viewer_command_disabled_reason(const ViewerCommand& command,
                                           const ViewerCapabilitySnapshot& capabilities) noexcept
{
    if (viewer_command_enabled(command, capabilities))
        return "";
    const auto kind = viewer_command_kind(command);
    if (uses_view_reason(kind))
        return disabled_view_reason(capabilities);
    if (capabilities.model_busy)
        return "Another model operation is in progress.";
    if (!capabilities.scene_imported)
        return "Open a model to use this action.";
    return "The current scene or selection does not support this action.";
}

bool viewer_command_enabled(const ViewerCommand& command,
                            const ViewerCapabilitySnapshot& capabilities) noexcept
{
    if (!capabilities.spatial_available && entity_command(viewer_command_kind(command))) {
        return false;
    }
    return std::visit(
        [&capabilities](const auto& value) noexcept { return enabled_for(value, capabilities); },
        command);
}

void ViewerCommandDispatcher::begin_frame(const ViewerCapabilitySnapshot& capabilities) noexcept
{
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

void ViewerCommandDispatcher::emit(ViewerCommand command) noexcept
{
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
ViewerCommandDispatcher::take_next(SceneId current_scene) noexcept
{
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
                                       const ViewerCommandCompletion& completion) noexcept
{
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

const ViewerCapabilitySnapshot& ViewerCommandDispatcher::capabilities() const noexcept
{
    return capabilities_;
}

std::span<const ViewerCommandOutcome> ViewerCommandDispatcher::outcomes() const noexcept
{
    return {outcomes_.data(), outcome_count_};
}

std::optional<Error> ViewerCommandDispatcher::enqueue_error() const noexcept
{
    return enqueue_error_;
}

bool ViewerCommandDispatcher::scene_replaced() const noexcept
{
    return scene_replaced_;
}

void ViewerCommandDispatcher::record_outcome(const QueuedCommand& queued,
                                             ViewerCommandOutcomeStatus status,
                                             std::optional<Error> error) noexcept
{
    ELF3D_ASSERT(outcome_count_ < outcomes_.size());
    outcomes_[outcome_count_] =
        ViewerCommandOutcome{viewer_command_kind(queued.command), status, queued.sequence, error};
    ++outcome_count_;
}

} // namespace elf3d::viewer
