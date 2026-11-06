#ifndef ELF3D_CMD_ARGUMENTS_H
#define ELF3D_CMD_ARGUMENTS_H

#include <elf3d/core/result.h>
#include <elf3d/math/value_types.h>

#include <span>
#include <string>
#include <string_view>

namespace elf3d::cmd {

struct Arguments {
    bool help = false;
    std::string input;
    std::string output;
    Float3 rotation_degrees;
};

// Tokens exclude the executable name. The result owns its paths.
[[nodiscard]] Result<Arguments> parse_arguments(std::span<const std::string_view> tokens);

} // namespace elf3d::cmd

#endif
