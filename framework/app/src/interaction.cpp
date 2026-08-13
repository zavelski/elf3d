#include <elf3d/app/interaction.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace elf3d {
namespace {

[[nodiscard]] bool finite_float2(Float2 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool
valid_region_description(const InteractionRegionDescription& description) noexcept {
    return finite_float2(description.minimum_window) && finite_float2(description.size_window) &&
           description.size_window.x > 0.0F && description.size_window.y > 0.0F &&
           description.target_extent.width != 0 && description.target_extent.height != 0;
}

[[nodiscard]] bool any_pointer_pressed(const InputSnapshot& input) noexcept {
    for (const InputTransition& button : input.buttons) {
        if (button.pressed) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool any_pointer_down(const InputSnapshot& input) noexcept {
    for (const InputTransition& button : input.buttons) {
        if (button.down) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] InteractionState state_for_request(InteractionRequest request) noexcept {
    switch (request) {
    case InteractionRequest::pending_click:
        return InteractionState::pending_click;
    case InteractionRequest::exclusive_tool:
        return InteractionState::exclusive_tool;
    case InteractionRequest::navigation:
        return InteractionState::navigation;
    case InteractionRequest::temporary_navigation:
        return InteractionState::temporary_navigation;
    }
    return InteractionState::idle;
}

} // namespace

InteractionArbiter::OwnerRecord*
InteractionArbiter::owner_record(InteractionOwnerId owner) noexcept {
    for (OwnerRecord& record : owners_) {
        if (record.active && record.id == owner) {
            return &record;
        }
    }
    return nullptr;
}

const InteractionArbiter::OwnerRecord*
InteractionArbiter::owner_record(InteractionOwnerId owner) const noexcept {
    for (const OwnerRecord& record : owners_) {
        if (record.active && record.id == owner) {
            return &record;
        }
    }
    return nullptr;
}

InteractionArbiter::RegionRecord*
InteractionArbiter::region_record(InteractionRegionId region) noexcept {
    for (RegionRecord& record : regions_) {
        if (record.active && record.id == region) {
            return &record;
        }
    }
    return nullptr;
}

const InteractionArbiter::RegionRecord*
InteractionArbiter::region_record(InteractionRegionId region) const noexcept {
    for (const RegionRecord& record : regions_) {
        if (record.active && record.id == region) {
            return &record;
        }
    }
    return nullptr;
}

Result<InteractionOwnerId> InteractionArbiter::create_owner(InteractionPriority priority) noexcept {
    if (next_owner_value_ == 0) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The interaction owner identity space is exhausted"};
    }
    for (OwnerRecord& record : owners_) {
        if (!record.active) {
            record = OwnerRecord{InteractionOwnerId{next_owner_value_++}, priority, true};
            return record.id;
        }
    }
    return Error{ErrorCode::resource_limit_exceeded,
                 "The application exceeds the interaction owner limit"};
}

void InteractionArbiter::destroy_owner(InteractionOwnerId owner) noexcept {
    OwnerRecord* record = owner_record(owner);
    if (record == nullptr) {
        return;
    }
    if (active_owner_ == owner) {
        cancel(owner, InteractionCancellationReason::owner_destroyed);
    }
    if (focused_owner_ == owner) {
        focused_owner_ = {};
        focused_region_ = {};
    }
    for (RegionRecord& region : regions_) {
        if (region.active && region.owner == owner) {
            region.active = false;
        }
    }
    record->active = false;
}

Result<InteractionRegionId>
InteractionArbiter::register_region(InteractionOwnerId owner, InteractionRegionId prior,
                                    const InteractionRegionDescription& description) noexcept {
    if (owner_record(owner) == nullptr) {
        return Error{ErrorCode::invalid_interaction_owner,
                     "Interaction region registration requires a live owner"};
    }
    if (!valid_region_description(description)) {
        return Error{ErrorCode::invalid_interaction_region,
                     "Interaction region coordinates and target extent must be finite and nonzero"};
    }
    if (prior.is_valid()) {
        RegionRecord* existing = region_record(prior);
        if (existing == nullptr || existing->owner != owner) {
            return Error{ErrorCode::invalid_interaction_region,
                         "The prior interaction region is stale or belongs to another owner"};
        }
        existing->description = description;
        existing->seen_frame = frame_;
        return existing->id;
    }
    if (next_region_value_ == 0) {
        return Error{ErrorCode::resource_limit_exceeded,
                     "The interaction region identity space is exhausted"};
    }
    for (RegionRecord& record : regions_) {
        if (!record.active) {
            record = RegionRecord{InteractionRegionId{next_region_value_++}, owner, description,
                                  frame_, true};
            return record.id;
        }
    }
    return Error{ErrorCode::resource_limit_exceeded,
                 "The application exceeds the interaction region limit"};
}

void InteractionArbiter::begin_frame(const InputSnapshot& input) noexcept {
    if (frame_ == std::numeric_limits<std::uint64_t>::max()) {
        frame_ = 1;
        for (RegionRecord& region : regions_) {
            region.seen_frame = 0;
        }
    } else {
        ++frame_;
    }
    input_ = input;
    regions_finalized_ = false;
    last_cancelled_owner_ = {};
    last_cancellation_ = InteractionCancellationReason::none;
}

bool InteractionArbiter::pointer_inside(
    const InteractionRegionDescription& description) const noexcept {
    const Float2 maximum{description.minimum_window.x + description.size_window.x,
                         description.minimum_window.y + description.size_window.y};
    return description.enabled && input_.pointer_inside_window &&
           input_.pointer_position_window.x >= description.minimum_window.x &&
           input_.pointer_position_window.y >= description.minimum_window.y &&
           input_.pointer_position_window.x < maximum.x &&
           input_.pointer_position_window.y < maximum.y;
}

InteractionPriority
InteractionArbiter::priority_for_owner(InteractionOwnerId owner) const noexcept {
    const OwnerRecord* record = owner_record(owner);
    return record != nullptr ? record->priority : InteractionPriority::background;
}

InteractionRegionId InteractionArbiter::highest_priority_hovered_region() const noexcept {
    InteractionRegionId selected;
    InteractionPriority selected_priority = InteractionPriority::background;
    bool has_selected = false;
    for (const RegionRecord& region : regions_) {
        if (!region.active || region.seen_frame != frame_ || !pointer_inside(region.description)) {
            continue;
        }
        const InteractionPriority priority = priority_for_owner(region.owner);
        if (!has_selected ||
            static_cast<std::uint8_t>(priority) > static_cast<std::uint8_t>(selected_priority)) {
            selected = region.id;
            selected_priority = priority;
            has_selected = true;
        }
    }
    return selected;
}

InteractionOwnerId InteractionArbiter::owner_for_region(InteractionRegionId region) const noexcept {
    const RegionRecord* record = region_record(region);
    return record != nullptr ? record->owner : InteractionOwnerId{};
}

void InteractionArbiter::cancel_from_frame_input() noexcept {
    if (!active_owner_.is_valid()) {
        return;
    }
    if (input_.capture_lost) {
        cancel(active_owner_, InteractionCancellationReason::capture_loss);
    } else if (!input_.window_focused) {
        cancel(active_owner_, InteractionCancellationReason::focus_loss);
    } else if (input_.key(InputKey::escape).pressed) {
        cancel(active_owner_, InteractionCancellationReason::escape);
    }
}

void InteractionArbiter::focus_hovered_region() noexcept {
    const InteractionRegionId hovered = highest_priority_hovered_region();
    if (!hovered.is_valid() || !any_pointer_pressed(input_)) {
        return;
    }
    focused_region_ = hovered;
    focused_owner_ = owner_for_region(hovered);
    if (!active_owner_.is_valid()) {
        active_owner_ = focused_owner_;
        active_region_ = focused_region_;
        state_ = InteractionState::pending_click;
    }
}

void InteractionArbiter::finalize_regions() noexcept {
    if (regions_finalized_) {
        return;
    }
    regions_finalized_ = true;

    cancel_from_frame_input();
    focus_hovered_region();
    if (state_ == InteractionState::pending_click && !any_pointer_down(input_)) {
        clear_active();
    }
}

bool InteractionArbiter::region_belongs_to(InteractionOwnerId owner,
                                           InteractionRegionId region) const noexcept {
    const RegionRecord* record = region_record(region);
    return record != nullptr && record->owner == owner && record->seen_frame == frame_;
}

InteractionRegionInput
InteractionArbiter::map_region_input(const RegionRecord& record,
                                     InteractionRegionId region) const noexcept {
    const InteractionRegionId hovered_region = highest_priority_hovered_region();
    InteractionRegionInput result;
    result.hovered = hovered_region == region;
    result.focused = focused_region_ == region;
    result.pointer_captured = pointer_captured_ && active_region_ == region;
    result.eligible = result.hovered || result.focused || result.pointer_captured;
    if (!result.eligible) {
        return result;
    }

    const float x_scale = static_cast<float>(record.description.target_extent.width) /
                          record.description.size_window.x;
    const float y_scale = static_cast<float>(record.description.target_extent.height) /
                          record.description.size_window.y;
    result.pointer_position_pixels =
        Float2{(input_.pointer_position_window.x - record.description.minimum_window.x) * x_scale,
               (input_.pointer_position_window.y - record.description.minimum_window.y) * y_scale};
    result.pointer_delta_pixels =
        Float2{input_.pointer_delta_window.x * x_scale, input_.pointer_delta_window.y * y_scale};
    result.wheel_delta = result.hovered ? input_.wheel_delta.y : 0.0F;
    result.buttons = input_.buttons;
    return result;
}

Result<InteractionRegionInput>
InteractionArbiter::region_input(InteractionOwnerId owner,
                                 InteractionRegionId region) const noexcept {
    if (!regions_finalized_) {
        return Error{ErrorCode::invalid_interaction_region,
                     "Interaction regions must be finalized before routed input is queried"};
    }
    if (owner_record(owner) == nullptr) {
        return Error{ErrorCode::invalid_interaction_owner,
                     "Routed input requires a live interaction owner"};
    }
    const RegionRecord* record = region_record(region);
    if (record == nullptr || record->owner != owner || record->seen_frame != frame_) {
        return Error{ErrorCode::invalid_interaction_region,
                     "Routed input requires a current owner-scoped interaction region"};
    }

    return map_region_input(*record, region);
}

bool InteractionArbiter::may_replace_active(InteractionOwnerId owner) const noexcept {
    if (!active_owner_.is_valid() || active_owner_ == owner) {
        return true;
    }
    return static_cast<std::uint8_t>(priority_for_owner(owner)) >
           static_cast<std::uint8_t>(priority_for_owner(active_owner_));
}

Result<void> InteractionArbiter::request(InteractionOwnerId owner, InteractionRegionId region,
                                         InteractionRequest request_value) noexcept {
    if (owner_record(owner) == nullptr) {
        return Error{ErrorCode::invalid_interaction_owner,
                     "Interaction requests require a live owner"};
    }
    if (!region_belongs_to(owner, region)) {
        return Error{ErrorCode::invalid_interaction_region,
                     "Interaction requests require a current owner-scoped region"};
    }
    if (!may_replace_active(owner)) {
        return Error{ErrorCode::interaction_conflict,
                     "Another interaction owner has equal or higher priority"};
    }
    if (active_owner_.is_valid() && active_owner_ != owner) {
        cancel(active_owner_, InteractionCancellationReason::replaced);
    }

    const InteractionState requested_state = state_for_request(request_value);
    if (requested_state == InteractionState::temporary_navigation && active_owner_ == owner &&
        state_ != InteractionState::temporary_navigation) {
        suspended_state_ = state_;
    } else if (requested_state != InteractionState::temporary_navigation) {
        suspended_state_ = InteractionState::idle;
    }
    active_owner_ = owner;
    active_region_ = region;
    state_ = requested_state;
    pointer_captured_ = requested_state != InteractionState::pending_click;
    return {};
}

void InteractionArbiter::clear_active() noexcept {
    active_owner_ = {};
    active_region_ = {};
    state_ = InteractionState::idle;
    suspended_state_ = InteractionState::idle;
    pointer_captured_ = false;
}

void InteractionArbiter::release(InteractionOwnerId owner) noexcept {
    if (active_owner_ != owner) {
        return;
    }
    if (state_ == InteractionState::temporary_navigation &&
        suspended_state_ == InteractionState::exclusive_tool) {
        state_ = suspended_state_;
        suspended_state_ = InteractionState::idle;
        pointer_captured_ = true;
        return;
    }
    clear_active();
}

void InteractionArbiter::remember_cancellation(InteractionOwnerId owner,
                                               InteractionCancellationReason reason) noexcept {
    last_cancelled_owner_ = owner;
    last_cancellation_ = reason;
}

void InteractionArbiter::cancel(InteractionOwnerId owner,
                                InteractionCancellationReason reason) noexcept {
    if (active_owner_ != owner) {
        return;
    }
    remember_cancellation(owner, reason);
    clear_active();
}

void InteractionArbiter::cancel_all(InteractionCancellationReason reason) noexcept {
    if (active_owner_.is_valid()) {
        cancel(active_owner_, reason);
    }
}

void InteractionArbiter::end_frame() noexcept {
    finalize_regions();
    for (RegionRecord& region : regions_) {
        if (!region.active || region.seen_frame == frame_) {
            continue;
        }
        if (active_region_ == region.id) {
            cancel(active_owner_, InteractionCancellationReason::region_destroyed);
        }
        if (focused_region_ == region.id) {
            focused_region_ = {};
            focused_owner_ = {};
        }
        region.active = false;
    }
    regions_finalized_ = false;
}

InteractionSnapshot InteractionArbiter::snapshot(InteractionOwnerId owner) const noexcept {
    InteractionSnapshot result;
    result.active_owner = active_owner_;
    result.active_region = active_region_;
    result.focused_owner = focused_owner_;
    result.focused_region = focused_region_;
    result.state = state_;
    result.pointer_captured = pointer_captured_;
    if (last_cancelled_owner_ == owner) {
        result.cancellation_reason = last_cancellation_;
    }
    return result;
}

bool InteractionArbiter::pointer_capture_requested() const noexcept {
    return pointer_captured_;
}

} // namespace elf3d
