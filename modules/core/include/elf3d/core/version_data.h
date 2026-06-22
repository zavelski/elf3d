#ifndef ELF3D_CORE_VERSION_DATA_H
#define ELF3D_CORE_VERSION_DATA_H

#include <cstdint>

namespace elf3d::core {

struct VersionData {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
};

[[nodiscard]] VersionData version_data() noexcept;

[[nodiscard]] const char *version_string() noexcept;

} // namespace elf3d::core

#endif
