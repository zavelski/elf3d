#ifndef ELF3D_APP_INTERACTION_H
#define ELF3D_APP_INTERACTION_H

#include <elf3d/app/input.h>
#include <elf3d/core/result.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace elf3d {

inline constexpr std::size_t maximum_interaction_owners = 64;
inline constexpr std::size_t maximum_interaction_regions = 64;

class InteractionOwnerId final {
  public:
    constexpr InteractionOwnerId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != 0;
    }

    bool operator==(const InteractionOwnerId&) const = default;

  private:
    friend class InteractionArbiter;
    explicit constexpr InteractionOwnerId(std::uint64_t value) noexcept : value_(value) {}

    std::uint64_t value_ = 0;
};

class InteractionRegionId final {
  public:
    constexpr InteractionRegionId() noexcept = default;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != 0;
    }

    bool operator==(const InteractionRegionId&) const = default;

  private:
    friend class InteractionArbiter;
    explicit constexpr InteractionRegionId(std::uint64_t value) noexcept : value_(value) {}

    std::uint64_t value_ = 0;
};

enum class InteractionPriority : std::uint8_t {
    background,
    normal,
    elevated,
};

enum class InteractionState : std::uint8_t {
    idle,
    pending_click,
    exclusive_tool,
    navigation,
    temporary_navigation,
};

enum class InteractionRequest : std::uint8_t {
    pending_click,
    exclusive_tool,
    navigation,
    temporary_navigation,
};

enum class InteractionCancellationReason : std::uint8_t {
    none,
    escape,
    focus_loss,
    capture_loss,
    owner_destroyed,
    region_destroyed,
    replaced,
    shutdown,
};

struct InteractionRegionDescription {
    Float2 minimum_window;
    Float2 size_window;
    Extent2D target_extent;
    bool enabled = true;

    bool operator==(const InteractionRegionDescription&) const = default;
};

struct InteractionRegionInput {
    Float2 pointer_position_pixels;
    Float2 pointer_delta_pixels;
    float wheel_delta = 0.0F;
    std::array<InputTransition, static_cast<std::size_t>(InputButton::count)> buttons;
    bool hovered = false;
    bool focused = false;
    bool eligible = false;
    bool pointer_captured = false;

    bool operator==(const InteractionRegionInput&) const = default;
};

struct InteractionSnapshot {
    InteractionOwnerId active_owner;
    InteractionRegionId active_region;
    InteractionOwnerId focused_owner;
    InteractionRegionId focused_region;
    InteractionState state = InteractionState::idle;
    InteractionCancellationReason cancellation_reason = InteractionCancellationReason::none;
    bool pointer_captured = false;

    bool operator==(const InteractionSnapshot&) const = default;
};

class InteractionArbiter final {
  public:
    InteractionArbiter() noexcept = default;
    ~InteractionArbiter() noexcept = default;

    InteractionArbiter(const InteractionArbiter&) = delete;
    InteractionArbiter& operator=(const InteractionArbiter&) = delete;

    [[nodiscard]] Result<InteractionOwnerId>
    create_owner(InteractionPriority priority = InteractionPriority::normal) noexcept;
    void destroy_owner(InteractionOwnerId owner) noexcept;

    // A valid prior ID updates the same region. An invalid prior ID creates a
    // new owner-scoped region. Regions not registered in a frame are invalidated.
    [[nodiscard]] Result<InteractionRegionId>
    register_region(InteractionOwnerId owner, InteractionRegionId prior,
                    const InteractionRegionDescription& description) noexcept;

    // Standard applications receive framework-owned begin/end calls. These
    // operations are public so deterministic mechanism tests and explicit
    // host integrations can replay normalized input without GLFW or ImGui.
    void begin_frame(const InputSnapshot& input) noexcept;
    void finalize_regions() noexcept;
    void end_frame() noexcept;

    [[nodiscard]] Result<InteractionRegionInput>
    region_input(InteractionOwnerId owner, InteractionRegionId region) const noexcept;

    [[nodiscard]] Result<void> request(InteractionOwnerId owner, InteractionRegionId region,
                                       InteractionRequest request) noexcept;
    void release(InteractionOwnerId owner) noexcept;
    void cancel(InteractionOwnerId owner, InteractionCancellationReason reason) noexcept;
    void cancel_all(InteractionCancellationReason reason) noexcept;

    [[nodiscard]] InteractionSnapshot snapshot(InteractionOwnerId owner) const noexcept;
    [[nodiscard]] bool pointer_capture_requested() const noexcept;

  private:
    struct OwnerRecord final {
        InteractionOwnerId id;
        InteractionPriority priority = InteractionPriority::normal;
        bool active = false;
    };

    struct RegionRecord final {
        InteractionRegionId id;
        InteractionOwnerId owner;
        InteractionRegionDescription description;
        std::uint64_t seen_frame = 0;
        bool active = false;
    };

    [[nodiscard]] OwnerRecord* owner_record(InteractionOwnerId owner) noexcept;
    [[nodiscard]] const OwnerRecord* owner_record(InteractionOwnerId owner) const noexcept;
    [[nodiscard]] RegionRecord* region_record(InteractionRegionId region) noexcept;
    [[nodiscard]] const RegionRecord* region_record(InteractionRegionId region) const noexcept;
    [[nodiscard]] bool region_belongs_to(InteractionOwnerId owner,
                                         InteractionRegionId region) const noexcept;
    [[nodiscard]] InteractionRegionInput
    map_region_input(const RegionRecord& record, InteractionRegionId region) const noexcept;
    [[nodiscard]] InteractionRegionId highest_priority_hovered_region() const noexcept;
    [[nodiscard]] InteractionOwnerId owner_for_region(InteractionRegionId region) const noexcept;
    void cancel_from_frame_input() noexcept;
    void focus_hovered_region() noexcept;
    [[nodiscard]] InteractionPriority priority_for_owner(InteractionOwnerId owner) const noexcept;
    [[nodiscard]] bool
    pointer_inside(const InteractionRegionDescription& description) const noexcept;
    [[nodiscard]] bool may_replace_active(InteractionOwnerId owner) const noexcept;
    void remember_cancellation(InteractionOwnerId owner,
                               InteractionCancellationReason reason) noexcept;
    void clear_active() noexcept;

    std::array<OwnerRecord, maximum_interaction_owners> owners_;
    std::array<RegionRecord, maximum_interaction_regions> regions_;
    InputSnapshot input_;
    InteractionOwnerId active_owner_;
    InteractionRegionId active_region_;
    InteractionOwnerId focused_owner_;
    InteractionRegionId focused_region_;
    InteractionOwnerId last_cancelled_owner_;
    InteractionCancellationReason last_cancellation_ = InteractionCancellationReason::none;
    InteractionState state_ = InteractionState::idle;
    InteractionState suspended_state_ = InteractionState::idle;
    std::uint64_t next_owner_value_ = 1;
    std::uint64_t next_region_value_ = 1;
    std::uint64_t frame_ = 0;
    bool regions_finalized_ = false;
    bool pointer_captured_ = false;
};

} // namespace elf3d

#endif
