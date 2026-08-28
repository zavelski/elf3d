if(NOT DEFINED ELF3D_EMBED_INPUT OR NOT DEFINED ELF3D_EMBED_OUTPUT)
    message(FATAL_ERROR "ELF3D_EMBED_INPUT and ELF3D_EMBED_OUTPUT are required")
endif()

file(READ "${ELF3D_EMBED_INPUT}" resource_hex HEX)
string(LENGTH "${resource_hex}" resource_hex_length)
math(EXPR resource_remainder "${resource_hex_length} % 16")
if(NOT resource_remainder EQUAL 0)
    message(FATAL_ERROR "The studio environment byte count must be divisible by eight")
endif()
math(EXPR resource_word_count "${resource_hex_length} / 16")

string(
    REGEX REPLACE
    "([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])([0-9a-f][0-9a-f])"
    "    native_word(0x\\8\\7\\6\\5\\4\\3\\2\\1ULL),\n"
    resource_words
    "${resource_hex}"
)

file(WRITE "${ELF3D_EMBED_OUTPUT}" [=[
#include "studio_environment_resource.h"

#include <elf3d/core/result.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

import elf.renderer;

namespace elf3d::detail {
namespace {

[[nodiscard]] constexpr std::uint64_t native_word(std::uint64_t value) noexcept {
    if constexpr (std::endian::native == std::endian::little) {
        return value;
    }
    return ((value & 0x00000000000000FFULL) << 56U) |
           ((value & 0x000000000000FF00ULL) << 40U) |
           ((value & 0x0000000000FF0000ULL) << 24U) |
           ((value & 0x00000000FF000000ULL) << 8U) |
           ((value & 0x000000FF00000000ULL) >> 8U) |
           ((value & 0x0000FF0000000000ULL) >> 24U) |
           ((value & 0x00FF000000000000ULL) >> 40U) |
           ((value & 0xFF00000000000000ULL) >> 56U);
}

constexpr std::array<std::uint64_t, ]=])
file(APPEND "${ELF3D_EMBED_OUTPUT}" "${resource_word_count}> resource_words{\n")
file(APPEND "${ELF3D_EMBED_OUTPUT}" "${resource_words}")

file(APPEND "${ELF3D_EMBED_OUTPUT}" [=[
};

class PortableStudioEnvironmentSource final : public renderer::StudioEnvironmentSource {
  public:
    [[nodiscard]] Result<std::span<const std::byte>> bytes() noexcept override {
        return std::as_bytes(std::span<const std::uint64_t>{resource_words});
    }
};

} // namespace

std::unique_ptr<renderer::StudioEnvironmentSource> create_studio_environment_source() {
    return std::make_unique<PortableStudioEnvironmentSource>();
}

} // namespace elf3d::detail
]=])
