#pragma once

namespace elf3d::viewer {
struct ViewerAssembly;
struct ViewerRenderingState;
struct SceneSession;
// GUI/context-owned service. It never reads the worker progress publication.
void advance_viewer_graphics(ViewerAssembly& runtime);
[[nodiscard]] bool viewer_scene_graphics_ready(const ViewerRenderingState& rendering,
                                               const SceneSession& scene) noexcept;
} // namespace elf3d::viewer
