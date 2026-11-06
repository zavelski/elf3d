#pragma once

#include <elf3d/scene.h>
#include <scene/scene.h>

#include <memory>
#include <utility>

namespace elf3d {
class SceneCpuRetirement::Impl final {
  public:
    explicit Impl(std::unique_ptr<scene::Storage> value) noexcept : storage{std::move(value)}
    {
    }
    std::unique_ptr<scene::Storage> storage;
};
} // namespace elf3d
