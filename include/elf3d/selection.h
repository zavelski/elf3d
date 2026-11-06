#ifndef ELF3D_SELECTION_H
#define ELF3D_SELECTION_H

#include <elf3d/picking.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace elf3d {

struct SelectionSnapshot {
    std::optional<EntityId> entity;
    std::optional<PickHit> pick_hit;
    // Resource selection is distinct from selecting an entity or a picked hit.
    std::size_t primitive_count = 0;
    std::uint64_t primitive_revision = 0;

    bool operator==(const SelectionSnapshot&) const = default;
};

} // namespace elf3d

#endif
