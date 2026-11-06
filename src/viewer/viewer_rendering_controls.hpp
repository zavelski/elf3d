#pragma once

#include "viewer_component_state.hpp"
#include "viewer_scene_session.hpp"

namespace elf3d::viewer {

void build_lighting_controls(ViewerFrameContext& state);
void build_camera_evidence(const ViewerFrameContext& state, const SceneSession& scene,
                           const Viewport& viewport);

} // namespace elf3d::viewer
