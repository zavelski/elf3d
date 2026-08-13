#ifndef ELF3D_SELECTION_H
#define ELF3D_SELECTION_H

#include <elf3d/picking.h>

#include <optional>

namespace elf3d {

struct SelectionSnapshot {
    std::optional<EntityId> entity;
    std::optional<PickHit> pick_hit;

    bool operator==(const SelectionSnapshot&) const = default;
};

} // namespace elf3d

#endif
