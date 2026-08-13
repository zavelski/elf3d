#pragma once

namespace elf3d::viewer {

[[noreturn]] void fatal_viewer_allocation_failure() noexcept;
[[noreturn]] void fatal_unexpected_viewer_exception() noexcept;
int run_viewer_entry(int argument_count, char** arguments);

} // namespace elf3d::viewer
