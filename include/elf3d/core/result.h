#ifndef ELF3D_CORE_RESULT_H
#define ELF3D_CORE_RESULT_H

#include <elf3d/core/assert.h>
#include <elf3d/core/error.h>

#include <type_traits>
#include <optional>
#include <utility>
#include <variant>

namespace elf3d {

template <typename T> class [[nodiscard]] Result final {
  public:
    static_assert(!std::is_same_v<T, Error>);
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_move_assignable_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

    Result(T value) noexcept : storage_(std::in_place_type<T>, std::move(value)) {}

    Result(Error error) noexcept : storage_(std::in_place_type<Error>, error) {}

    Result(const Result &) = delete;
    Result &operator=(const Result &) = delete;
    Result(Result &&) noexcept = default;
    Result &operator=(Result &&) noexcept = default;

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] T &value() & noexcept {
        T *stored = std::get_if<T>(&storage_);
        ELF3D_ASSERT(stored != nullptr);
        return *stored;
    }

    [[nodiscard]] const T &value() const & noexcept {
        const T *stored = std::get_if<T>(&storage_);
        ELF3D_ASSERT(stored != nullptr);
        return *stored;
    }

    [[nodiscard]] T &&value() && noexcept {
        T *stored = std::get_if<T>(&storage_);
        ELF3D_ASSERT(stored != nullptr);
        return std::move(*stored);
    }

    [[nodiscard]] Error &error() & noexcept {
        Error *stored = std::get_if<Error>(&storage_);
        ELF3D_ASSERT(stored != nullptr);
        return *stored;
    }

    [[nodiscard]] const Error &error() const & noexcept {
        const Error *stored = std::get_if<Error>(&storage_);
        ELF3D_ASSERT(stored != nullptr);
        return *stored;
    }

  private:
    std::variant<T, Error> storage_;
};

template <> class [[nodiscard]] Result<void> final {
  public:
    Result() noexcept = default;

    Result(Error error) noexcept : error_(error) {}

    Result(const Result &) = delete;
    Result &operator=(const Result &) = delete;
    Result(Result &&) noexcept = default;
    Result &operator=(Result &&) noexcept = default;

    [[nodiscard]] bool has_value() const noexcept {
        return !error_.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] const Error &error() const noexcept {
        ELF3D_ASSERT(error_.has_value());
        return *error_;
    }

  private:
    std::optional<Error> error_;
};

static_assert(std::is_nothrow_copy_constructible_v<Error>);
static_assert(std::is_nothrow_move_constructible_v<Error>);
static_assert(std::is_nothrow_destructible_v<Error>);

} // namespace elf3d

#endif
