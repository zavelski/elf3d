#pragma once

#include <elf3d/elf3d.h>

#include <memory>
#include <utility>

namespace elf3d {
class Viewport::Impl final {
  public:
    Impl(std::unique_ptr<viewport::OffscreenViewport> viewport,
         const std::shared_ptr<renderer::Renderer>& renderer,
         const std::shared_ptr<picking::PickingService>& picking) noexcept
        : viewport(std::move(viewport)), renderer(renderer), picking(picking) {}

    std::unique_ptr<viewport::OffscreenViewport> viewport;
    std::weak_ptr<renderer::Renderer> renderer;
    std::weak_ptr<picking::PickingService> picking;
};

} // namespace elf3d
