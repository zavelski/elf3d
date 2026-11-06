#pragma once
namespace elf3d::tools {
// CLI: --service <large-mesh|many-meshes|large-image|many-images> <calls> <cycles> <csv>
[[nodiscard]] int run_service_benchmark(int count, char** values);
} // namespace elf3d::tools
