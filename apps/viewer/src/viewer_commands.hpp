#ifndef ELF3D_VIEWER_COMMANDS_HPP
#define ELF3D_VIEWER_COMMANDS_HPP

#include "viewer_tools.hpp"

#include <elf3d/elf3d.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace elf3d::viewer {

enum class ViewerPanel : std::uint8_t {
    clipping,
};

struct ExitViewerCommand {};
struct ShowOpenDialogCommand {};
struct ShowSaveDialogCommand {};
struct ResetViewerLayoutCommand {};
struct ReloadSceneCommand {};
struct CloseSceneCommand {};
struct FitViewCommand {};
struct ResetViewCommand {};
struct ShowViewerPanelCommand {
    ViewerPanel panel = ViewerPanel::clipping;
};
struct ActivateViewerToolCommand {
    ViewerTool tool = ViewerTool::selection;
};
struct ToggleSectionPlaneCommand {};
struct FlipSectionPlaneCommand {};
struct AddClippingBoxFromBoundsCommand {};
struct ClearClippingCommand {};
struct ToggleClippingHelpersCommand {};
struct FitClippedContentCommand {};
struct ClearSelectionCommand {};
struct CancelMeasurementCommand {};
struct ClearMeasurementCommand {};
struct SelectEntityCommand {
    EntityId entity;
};
enum class EntityVisibilityScope : std::uint8_t {
    local,
    entity_and_ancestors,
};
struct SetEntityVisibilityCommand {
    EntityId entity;
    bool visible = true;
    EntityVisibilityScope scope = EntityVisibilityScope::entity_and_ancestors;
};
struct ShowAllEntitiesCommand {};
struct IsolateEntityCommand {
    EntityId entity;
};
struct ExitIsolationCommand {};

using ViewerCommand = std::variant<
    ExitViewerCommand, ShowOpenDialogCommand, ShowSaveDialogCommand, ResetViewerLayoutCommand,
    ReloadSceneCommand, CloseSceneCommand, FitViewCommand, ResetViewCommand, ShowViewerPanelCommand,
    ActivateViewerToolCommand, ToggleSectionPlaneCommand, FlipSectionPlaneCommand,
    AddClippingBoxFromBoundsCommand, ClearClippingCommand, ToggleClippingHelpersCommand,
    FitClippedContentCommand, ClearSelectionCommand, CancelMeasurementCommand,
    ClearMeasurementCommand, SelectEntityCommand, SetEntityVisibilityCommand,
    ShowAllEntitiesCommand, IsolateEntityCommand, ExitIsolationCommand>;

enum class ViewerCommandKind : std::uint8_t {
    exit_viewer,
    show_open_dialog,
    show_save_dialog,
    reset_layout,
    reload_scene,
    close_scene,
    fit_view,
    reset_view,
    show_panel,
    activate_tool,
    toggle_section_plane,
    flip_section_plane,
    add_clipping_box_from_bounds,
    clear_clipping,
    toggle_clipping_helpers,
    fit_clipped_content,
    clear_selection,
    cancel_measurement,
    clear_measurement,
    select_entity,
    set_entity_visibility,
    show_all_entities,
    isolate_entity,
    exit_isolation,
};

struct ViewerCapabilitySnapshot {
    SceneId scene;
    std::optional<EntityId> selected_entity;
    bool scene_imported = false;
    bool view_available = false;
    bool visible_content = false;
    bool unclipped_visible_content = false;
    bool section_plane_enabled = false;
    bool has_clipping = false;
    bool can_add_clipping_box = false;
    bool isolating = false;
    bool measurement_incomplete = false;
    bool has_measurement = false;

    bool operator==(const ViewerCapabilitySnapshot&) const = default;
};

enum class ViewerCommandOutcomeStatus : std::uint8_t {
    executed,
    cancelled,
    disabled,
    rejected_after_scene_replacement,
    rejected_replacement_limit,
    failed,
};

struct ViewerCommandDispatch {
    ViewerCommand command;
    std::uint32_t sequence = 0;
};

struct ViewerCommandCompletion {
    ViewerCommandOutcomeStatus status = ViewerCommandOutcomeStatus::executed;
    std::optional<Error> error;
    bool scene_replaced = false;
};

struct ViewerCommandOutcome {
    ViewerCommandKind kind = ViewerCommandKind::exit_viewer;
    ViewerCommandOutcomeStatus status = ViewerCommandOutcomeStatus::executed;
    std::uint32_t sequence = 0;
    std::optional<Error> error;
};

inline constexpr std::size_t maximum_viewer_commands_per_frame = 64;

class ViewerCommandDispatcher final {
  public:
    void begin_frame(const ViewerCapabilitySnapshot& capabilities) noexcept;
    void emit(ViewerCommand command) noexcept;

    [[nodiscard]] std::optional<ViewerCommandDispatch> take_next(SceneId current_scene) noexcept;
    void complete(const ViewerCommandDispatch& dispatch,
                  const ViewerCommandCompletion& completion) noexcept;

    [[nodiscard]] const ViewerCapabilitySnapshot& capabilities() const noexcept;
    [[nodiscard]] std::span<const ViewerCommandOutcome> outcomes() const noexcept;
    [[nodiscard]] std::optional<Error> enqueue_error() const noexcept;
    [[nodiscard]] bool scene_replaced() const noexcept;

  private:
    struct QueuedCommand {
        ViewerCommand command;
        SceneId scene;
        std::uint32_t sequence = 0;
    };

    void record_outcome(const QueuedCommand& queued, ViewerCommandOutcomeStatus status,
                        std::optional<Error> error = std::nullopt) noexcept;

    ViewerCapabilitySnapshot capabilities_;
    std::array<QueuedCommand, maximum_viewer_commands_per_frame> queue_;
    std::array<ViewerCommandOutcome, maximum_viewer_commands_per_frame> outcomes_;
    std::size_t queue_count_ = 0;
    std::size_t next_index_ = 0;
    std::size_t outcome_count_ = 0;
    std::uint32_t next_sequence_ = 1;
    std::optional<ViewerCommandDispatch> active_dispatch_;
    std::optional<Error> enqueue_error_;
    bool replacement_attempted_ = false;
    bool scene_replaced_ = false;
};

[[nodiscard]] ViewerCommandKind viewer_command_kind(const ViewerCommand& command) noexcept;
[[nodiscard]] bool viewer_command_targets_scene(const ViewerCommand& command) noexcept;
[[nodiscard]] bool viewer_command_replaces_scene(const ViewerCommand& command) noexcept;
[[nodiscard]] bool viewer_command_enabled(const ViewerCommand& command,
                                          const ViewerCapabilitySnapshot& capabilities) noexcept;

} // namespace elf3d::viewer

#endif
