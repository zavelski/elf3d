#pragma once
#include <elf3d/elf3d.h>
#include <memory>

namespace elf3d::tools {
enum class ServiceFixture { large_mesh, many_meshes, large_image, many_images };
// Synthetic procedural runtime data; never reads a model corpus or clones a live Scene.
[[nodiscard]] Result<std::unique_ptr<Scene>> make_service_fixture(Engine& engine,
                                                                  ServiceFixture fixture);
} // namespace elf3d::tools
