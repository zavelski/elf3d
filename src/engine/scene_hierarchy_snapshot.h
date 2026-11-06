#pragma once

#include <elf3d/scene.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <scene/scene.h>
#include <string>
#include <vector>

namespace elf3d {
class SceneHierarchySnapshot::Impl final {
  public:
    enum class Phase { roots, walk, complete };
    struct Frame {
        EntityId entity;
        std::uint32_t depth = 0;
        std::size_t child = 0;
        std::size_t name_offset = 0;
        std::optional<std::size_t> row;
    };

    [[nodiscard]] Result<void> step(const scene::Storage& source);
    void scan_root(const scene::Storage& source);
    [[nodiscard]] Result<void> walk_step(const scene::Storage& source);
    [[nodiscard]] Result<void> enter_entity(Frame& frame, const scene::EntityRecord& record);

    SceneId owner;
    std::uint64_t hierarchy_revision = 0;
    std::uint64_t visibility_revision = 0;
    std::vector<SceneHierarchyItem> items;
    std::vector<std::string> names;
    std::vector<EntityId> roots;
    std::vector<std::uint8_t> visited;
    std::vector<Frame> stack;
    std::size_t slot_cursor = 0;
    std::size_t root_cursor = 0;
    std::size_t live_count = 0;
    Phase phase = Phase::roots;
};
} // namespace elf3d
