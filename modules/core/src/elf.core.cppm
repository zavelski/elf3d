module;

#include <cstdint>

export module elf.core;

export namespace elf3d::core {

struct VersionData {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
};

[[nodiscard]] VersionData version_data() noexcept;

[[nodiscard]] const char *version_string() noexcept;

} // namespace elf3d::core
