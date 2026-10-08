#ifndef ELF3D_CORE_DETAIL_EXCEPTION_BOUNDARY_H
#define ELF3D_CORE_DETAIL_EXCEPTION_BOUNDARY_H

#include <elf3d/core/assert.h>

#include <new>

namespace elf3d::detail {

// Boundary implementations only: invoke immediately, without storing the callable.
// Expected failures remain Result values; escaping exceptions are process-fatal.
template <typename Operation>
auto fatal_exception_boundary(Operation&& operation) noexcept -> decltype(operation())
{
    try {
        return operation();
    } catch (const std::bad_alloc&) {
        fatal_error("Elf3D memory allocation failed");
    } catch (...) {
        fatal_error("Elf3D boundary encountered an unexpected exception");
    }
}

} // namespace elf3d::detail

#endif
