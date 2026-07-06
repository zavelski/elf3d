#ifndef ELF3D_CORE_ASSERT_H
#define ELF3D_CORE_ASSERT_H

#include <elf3d/core/api.h>

#include <cstdint>
#include <string_view>

namespace elf3d {

[[noreturn]] ELF3D_API void fatal_error(std::string_view message) noexcept;

[[noreturn]] ELF3D_API void assertion_failed(const char *expression, const char *file,
                                             std::uint32_t line) noexcept;

} // namespace elf3d

#define ELF3D_ASSERT(expression)                                                                  \
    ((expression) ? static_cast<void>(0)                                                          \
                  : ::elf3d::assertion_failed(#expression, __FILE__,                             \
                                               static_cast<unsigned>(__LINE__)))

#endif
