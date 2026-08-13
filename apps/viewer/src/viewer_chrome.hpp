#pragma once

#include <elf3d/elf3d.h>

#include <imgui.h>

#include "viewer_assets.hpp"
#include "viewer_commands.hpp"
#include "viewer_component_state.hpp"
#include "viewer_tools.hpp"

namespace elf3d::viewer {

struct ToolbarBuildContext {
    ViewerFrameContext& state;
    const ToolbarIcons& icons;
    Viewport& viewport;
    ToolCoordinator& tools;
    const ViewerCapabilitySnapshot& capabilities;
    ViewerCommandDispatcher& commands;
};

void build_main_menu(ViewerFrameContext& state, Viewport& viewport, ToolCoordinator& tools,
                     const ViewerCapabilitySnapshot& capabilities,
                     ViewerCommandDispatcher& commands);
void build_toolbar(const ToolbarBuildContext& context);
[[nodiscard]] ImGuiID build_main_dockspace(ViewerFrameContext& state);

} // namespace elf3d::viewer
