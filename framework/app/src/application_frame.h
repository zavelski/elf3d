#pragma once

#include <elf3d/app/application.h>

#include "input_collector.h"

#include <span>
#include <string_view>

namespace elf3d {
namespace app::detail {

struct ApplicationServices final {
    Engine& engine;
    InteractionArbiter& interaction_arbiter;
    const GraphicsContextSnapshot& graphics_context;
};

struct ApplicationFrame final {
    WindowSnapshot window;
    double elapsed_seconds = 0.0;
    std::optional<ApplicationFrameStatistics> previous_frame_statistics;
    const InputSnapshot* input = nullptr;
    std::span<const std::string_view> dropped_files;
};

} // namespace app::detail

} // namespace elf3d
