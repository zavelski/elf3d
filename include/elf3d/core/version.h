#ifndef ELF3D_CORE_VERSION_H
#define ELF3D_CORE_VERSION_H

#include <cstdint>

namespace elf3d {

struct Version {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
    std::uint16_t patch = 0;
};

} // namespace elf3d

#endif
