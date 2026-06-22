#ifndef ELF3D_CORE_RESULT_H
#define ELF3D_CORE_RESULT_H

#include <elf3d/core/error.h>

#include <type_traits>
#include <utility>
#include <variant>

namespace elf3d {

template <typename T> class [[nodiscard]] Result final {
  public:
    Result(T value) : storage_(std::in_place_type<T>, std::move(value)) {}

    Result(Error error) noexcept : storage_(std::in_place_type<Error>, error) {}

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] T &value() & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T &value() const & {
        return std::get<T>(storage_);
    }

    [[nodiscard]] T &&value() && {
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] Error &error() & {
        return std::get<Error>(storage_);
    }

    [[nodiscard]] const Error &error() const & {
        return std::get<Error>(storage_);
    }

  private:
    std::variant<T, Error> storage_;
};

template <> class [[nodiscard]] Result<void> final {
  public:
    Result() noexcept = default;

    Result(Error error) noexcept : error_(error) {}

    [[nodiscard]] bool has_value() const noexcept {
        return !error_.has_error();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] const Error &error() const noexcept {
        return error_;
    }

  private:
    Error error_;
};

static_assert(std::is_nothrow_copy_constructible_v<Error>);

} // namespace elf3d

#endif
