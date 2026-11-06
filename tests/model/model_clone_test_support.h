#ifndef ELF3D_MODEL_TESTS_MODEL_CLONE_TEST_SUPPORT_H
#define ELF3D_MODEL_TESTS_MODEL_CLONE_TEST_SUPPORT_H

#include <elf3d/core/assert.h>
#include <elf3d/model.h>

#include <utility>

namespace elf3d::model::tests {

// Synthetic fixture setup and invariant lookups must succeed in every build.
template <typename T> [[nodiscard]] T require_value(Result<T> result)
{
    if (!result) {
        fatal_error(result.error().message());
    }
    return std::move(result).value();
}

inline void require_success(Result<void> result)
{
    if (!result) {
        fatal_error(result.error().message());
    }
}

[[nodiscard]] Document make_clone_fixture();
[[nodiscard]] PrimitiveData clone_triangle_data();

} // namespace elf3d::model::tests

#endif
