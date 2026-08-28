#pragma once

#include <elf3d/app/application.h>
#include <elf3d/elf3d.h>

#include <imgui.h>

#include <array>
#include <optional>
#include <string>

#include "viewer_component_state.hpp"
#include "viewer_scene_session.hpp"
#include "viewer_tools.hpp"

namespace elf3d::viewer {

struct ViewPanelContext {
    ImGuiID dockspace_id;
    ViewerFrameContext& state;
    Viewport& viewport;
    SceneSession& scene;
    ToolCoordinator& tools;
    ApplicationUiContext& application;
    InteractionOwnerId interaction_owner;
    InteractionRegionId& interaction_region;
};

[[nodiscard]] bool has_nonzero_extent(Extent2D extent) noexcept;
[[nodiscard]] Float3 bounds_center(const Bounds3& bounds) noexcept;
[[nodiscard]] bool valid_box_for_commit(const ClippingBox& box) noexcept;
[[nodiscard]] bool navigation_blocked_by_modal() noexcept;
void set_viewport_error(ViewerFrameContext state, const Error& error);
[[nodiscard]] RetainedViewportFrameKey viewport_frame_key(const ViewerFrameContext& state,
                                                          const SceneSession& scene,
                                                          const Viewport& viewport,
                                                          const ToolCoordinator& tools);
[[nodiscard]] bool viewport_frame_render_required(const ViewerFrameContext& state,
                                                  const RetainedViewportFrameKey& key,
                                                  const Viewport& viewport) noexcept;
[[nodiscard]] bool
viewport_frame_render_required(const std::optional<RetainedViewportFrameKey>& previous_frame,
                               const RetainedViewportFrameKey& key, bool framebuffer_valid,
                               bool pointer_navigation_captured) noexcept;
bool color_control(const char* label, std::array<float, 4>& rgba);
[[nodiscard]] std::string clipping_status(const ClippingSnapshot& snapshot,
                                          bool has_visible_bounds);
[[nodiscard]] std::string format_distance(double meters, LengthDisplayUnit unit);
void build_3d_view(const ViewPanelContext& context);

} // namespace elf3d::viewer
