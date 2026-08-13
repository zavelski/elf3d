#include <elf3d/app/interaction.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace {

[[nodiscard]] int fail(const char* message) noexcept {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

[[nodiscard]] elf3d::InputSnapshot pointer_input(float x, float y) noexcept {
    elf3d::InputSnapshot input;
    input.pointer_position_window = {x, y};
    input.pointer_inside_window = true;
    input.window_focused = true;
    return input;
}

[[nodiscard]] elf3d::InteractionRegionDescription region(elf3d::Float2 position, elf3d::Float2 size,
                                                         elf3d::Extent2D target_extent) noexcept {
    return elf3d::InteractionRegionDescription{position, size, target_extent, true};
}

[[nodiscard]] bool mapping_and_priority_are_deterministic() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto normal = arbiter.create_owner(elf3d::InteractionPriority::normal);
    const auto elevated = arbiter.create_owner(elf3d::InteractionPriority::elevated);
    if (!normal || !elevated) {
        return false;
    }

    elf3d::InputSnapshot input = pointer_input(50.0F, 50.0F);
    input.pointer_delta_window = {0.5F, -0.25F};
    input.wheel_delta = {0.25F, 1.25F};
    input.dpi_scale = 1.5F;
    input.buttons[static_cast<std::size_t>(elf3d::InputButton::left)] = {true, true, false};
    arbiter.begin_frame(input);
    const auto normal_region = arbiter.register_region(
        normal.value(), {}, region({0.0F, 0.0F}, {100.0F, 100.0F}, {200, 200}));
    const auto elevated_region = arbiter.register_region(
        elevated.value(), {}, region({25.0F, 25.0F}, {50.0F, 50.0F}, {100, 100}));
    if (!normal_region || !elevated_region) {
        return false;
    }
    arbiter.finalize_regions();
    const auto elevated_input = arbiter.region_input(elevated.value(), elevated_region.value());
    const auto normal_input = arbiter.region_input(normal.value(), normal_region.value());
    if (!elevated_input || !normal_input) {
        return false;
    }
    const std::array mapped_correctly{
        elevated_input.value().hovered,
        elevated_input.value().focused,
        !normal_input.value().eligible,
        elevated_input.value().pointer_position_pixels == elf3d::Float2{50.0F, 50.0F},
        elevated_input.value().pointer_delta_pixels == elf3d::Float2{1.0F, -0.5F},
        elevated_input.value().wheel_delta == 1.25F};
    if (!std::all_of(mapped_correctly.begin(), mapped_correctly.end(),
                     [](bool value) { return value; })) {
        return false;
    }
    const auto requested = arbiter.request(elevated.value(), elevated_region.value(),
                                           elf3d::InteractionRequest::exclusive_tool);
    if (!requested || !arbiter.pointer_capture_requested()) {
        return false;
    }
    arbiter.end_frame();
    return true;
}

[[nodiscard]] bool temporary_navigation_restores_tool() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto owner = arbiter.create_owner();
    if (!owner) {
        return false;
    }
    elf3d::InputSnapshot input = pointer_input(20.0F, 20.0F);
    arbiter.begin_frame(input);
    const auto view = arbiter.register_region(owner.value(), {},
                                              region({0.0F, 0.0F}, {100.0F, 100.0F}, {100, 100}));
    if (!view) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(owner.value(), view.value(), elf3d::InteractionRequest::exclusive_tool) ||
        !arbiter.request(owner.value(), view.value(),
                         elf3d::InteractionRequest::temporary_navigation)) {
        return false;
    }
    if (arbiter.snapshot(owner.value()).state != elf3d::InteractionState::temporary_navigation) {
        return false;
    }
    arbiter.release(owner.value());
    const elf3d::InteractionSnapshot restored = arbiter.snapshot(owner.value());
    arbiter.end_frame();
    return restored.state == elf3d::InteractionState::exclusive_tool && restored.pointer_captured;
}

struct ConflictFixture final {
    elf3d::InteractionArbiter arbiter;
    elf3d::InteractionOwnerId low_owner;
    elf3d::InteractionOwnerId high_owner;
    elf3d::InteractionRegionId high_region;
    elf3d::InputSnapshot input;
};

[[nodiscard]] bool establish_priority_conflict(ConflictFixture& fixture) noexcept {
    const auto low = fixture.arbiter.create_owner(elf3d::InteractionPriority::background);
    const auto high = fixture.arbiter.create_owner(elf3d::InteractionPriority::elevated);
    if (!low || !high) {
        return false;
    }
    fixture.low_owner = low.value();
    fixture.high_owner = high.value();
    fixture.input = pointer_input(10.0F, 10.0F);
    fixture.arbiter.begin_frame(fixture.input);
    const auto low_region =
        fixture.arbiter.register_region(fixture.low_owner, {}, region({0, 0}, {20, 20}, {20, 20}));
    const auto high_region =
        fixture.arbiter.register_region(fixture.high_owner, {}, region({0, 0}, {20, 20}, {20, 20}));
    if (!low_region || !high_region) {
        return false;
    }
    fixture.high_region = high_region.value();
    fixture.arbiter.finalize_regions();
    if (!fixture.arbiter.request(fixture.high_owner, fixture.high_region,
                                 elf3d::InteractionRequest::navigation)) {
        return false;
    }
    const auto conflict = fixture.arbiter.request(fixture.low_owner, low_region.value(),
                                                  elf3d::InteractionRequest::exclusive_tool);
    fixture.arbiter.end_frame();
    return !conflict && conflict.error().code() == elf3d::ErrorCode::interaction_conflict;
}

[[nodiscard]] bool conflicts_and_escape_are_deterministic() noexcept {
    ConflictFixture fixture;
    if (!establish_priority_conflict(fixture)) {
        return false;
    }
    fixture.input.keys[static_cast<std::size_t>(elf3d::InputKey::escape)] = {true, true, false};
    fixture.arbiter.begin_frame(fixture.input);
    const auto current_high = fixture.arbiter.register_region(
        fixture.high_owner, fixture.high_region, region({0, 0}, {20, 20}, {20, 20}));
    if (!current_high) {
        return false;
    }
    fixture.arbiter.finalize_regions();
    const elf3d::InteractionSnapshot snapshot = fixture.arbiter.snapshot(fixture.high_owner);
    fixture.arbiter.end_frame();
    return snapshot.state == elf3d::InteractionState::idle && !snapshot.pointer_captured &&
           snapshot.cancellation_reason == elf3d::InteractionCancellationReason::escape;
}

[[nodiscard]] bool stale_regions_cancel_capture() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto owner = arbiter.create_owner();
    if (!owner) {
        return false;
    }
    const elf3d::InputSnapshot input = pointer_input(5.0F, 5.0F);
    arbiter.begin_frame(input);
    const auto view =
        arbiter.register_region(owner.value(), {}, region({0, 0}, {10, 10}, {10, 10}));
    if (!view) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(owner.value(), view.value(), elf3d::InteractionRequest::navigation)) {
        return false;
    }
    arbiter.end_frame();

    arbiter.begin_frame(input);
    arbiter.end_frame();
    const elf3d::InteractionSnapshot snapshot = arbiter.snapshot(owner.value());
    return snapshot.state == elf3d::InteractionState::idle &&
           snapshot.cancellation_reason == elf3d::InteractionCancellationReason::region_destroyed;
}

[[nodiscard]] bool capture_loss_and_owner_destruction_cancel() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto owner = arbiter.create_owner();
    if (!owner) {
        return false;
    }
    elf3d::InputSnapshot input = pointer_input(5.0F, 5.0F);
    arbiter.begin_frame(input);
    const auto view =
        arbiter.register_region(owner.value(), {}, region({0, 0}, {10, 10}, {10, 10}));
    if (!view) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(owner.value(), view.value(), elf3d::InteractionRequest::exclusive_tool)) {
        return false;
    }
    arbiter.end_frame();

    input.capture_lost = true;
    arbiter.begin_frame(input);
    if (!arbiter.register_region(owner.value(), view.value(), region({0, 0}, {10, 10}, {10, 10}))) {
        return false;
    }
    arbiter.finalize_regions();
    const elf3d::InteractionSnapshot lost = arbiter.snapshot(owner.value());
    arbiter.end_frame();
    if (lost.cancellation_reason != elf3d::InteractionCancellationReason::capture_loss) {
        return false;
    }

    input.capture_lost = false;
    arbiter.begin_frame(input);
    const auto replacement =
        arbiter.register_region(owner.value(), view.value(), region({0, 0}, {10, 10}, {10, 10}));
    if (!replacement) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(owner.value(), replacement.value(),
                         elf3d::InteractionRequest::navigation)) {
        return false;
    }
    arbiter.destroy_owner(owner.value());
    return arbiter.snapshot(owner.value()).cancellation_reason ==
           elf3d::InteractionCancellationReason::owner_destroyed;
}

[[nodiscard]] bool focus_loss_cancels_capture() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto owner = arbiter.create_owner();
    if (!owner) {
        return false;
    }
    elf3d::InputSnapshot input = pointer_input(5.0F, 5.0F);
    arbiter.begin_frame(input);
    const auto view =
        arbiter.register_region(owner.value(), {}, region({0, 0}, {10, 10}, {10, 10}));
    if (!view) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(owner.value(), view.value(), elf3d::InteractionRequest::navigation)) {
        return false;
    }
    arbiter.end_frame();

    input.window_focused = false;
    arbiter.begin_frame(input);
    if (!arbiter.register_region(owner.value(), view.value(), region({0, 0}, {10, 10}, {10, 10}))) {
        return false;
    }
    arbiter.finalize_regions();
    const elf3d::InteractionSnapshot snapshot = arbiter.snapshot(owner.value());
    arbiter.end_frame();
    return snapshot.state == elf3d::InteractionState::idle && !snapshot.pointer_captured &&
           snapshot.cancellation_reason == elf3d::InteractionCancellationReason::focus_loss;
}

[[nodiscard]] bool higher_priority_owner_replaces_capture() noexcept {
    elf3d::InteractionArbiter arbiter;
    const auto background = arbiter.create_owner(elf3d::InteractionPriority::background);
    const auto elevated = arbiter.create_owner(elf3d::InteractionPriority::elevated);
    if (!background || !elevated) {
        return false;
    }
    const elf3d::InputSnapshot input = pointer_input(5.0F, 5.0F);
    arbiter.begin_frame(input);
    const auto background_region =
        arbiter.register_region(background.value(), {}, region({0, 0}, {10, 10}, {10, 10}));
    const auto elevated_region =
        arbiter.register_region(elevated.value(), {}, region({0, 0}, {10, 10}, {10, 10}));
    if (!background_region || !elevated_region) {
        return false;
    }
    arbiter.finalize_regions();
    if (!arbiter.request(background.value(), background_region.value(),
                         elf3d::InteractionRequest::exclusive_tool) ||
        !arbiter.request(elevated.value(), elevated_region.value(),
                         elf3d::InteractionRequest::navigation)) {
        return false;
    }
    const elf3d::InteractionSnapshot replaced = arbiter.snapshot(background.value());
    const elf3d::InteractionSnapshot replacement = arbiter.snapshot(elevated.value());
    arbiter.end_frame();
    return replaced.cancellation_reason == elf3d::InteractionCancellationReason::replaced &&
           replacement.active_owner == elevated.value() &&
           replacement.state == elf3d::InteractionState::navigation && replacement.pointer_captured;
}

} // namespace

int main() {
    if (!mapping_and_priority_are_deterministic()) {
        return fail("Interaction region priority or coordinate mapping failed");
    }
    if (!temporary_navigation_restores_tool()) {
        return fail("Temporary navigation did not restore the suspended Tool state");
    }
    if (!conflicts_and_escape_are_deterministic()) {
        return fail("Interaction priority conflict or Escape cancellation failed");
    }
    if (!stale_regions_cancel_capture()) {
        return fail("A stale interaction region did not cancel capture");
    }
    if (!capture_loss_and_owner_destruction_cancel()) {
        return fail("Capture loss or owner destruction cancellation failed");
    }
    if (!focus_loss_cancels_capture()) {
        return fail("Focus loss did not cancel capture");
    }
    if (!higher_priority_owner_replaces_capture()) {
        return fail("A higher-priority interaction owner did not replace capture");
    }
    return 0;
}
