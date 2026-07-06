#include <elf3d/core/assert.h>

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace elf3d {
namespace {

void write_stderr(std::string_view message) noexcept {
    if (message.empty()) {
        return;
    }
    const std::size_t written = std::fwrite(message.data(), 1, message.size(), stderr);
    (void)written;
    const int newline_result = std::fputc('\n', stderr);
    (void)newline_result;
}

} // namespace

void fatal_error(std::string_view message) noexcept {
    write_stderr(message);
    std::abort();
}

void assertion_failed(const char *expression, const char *file, std::uint32_t line) noexcept {
    std::fputs("Elf3D assertion failed: ", stderr);
    std::fputs(expression != nullptr ? expression : "<unknown>", stderr);
    std::fputs(" at ", stderr);
    std::fputs(file != nullptr ? file : "<unknown>", stderr);
    std::fputs(":", stderr);
    std::fprintf(stderr, "%u\n", line);
    std::abort();
}

} // namespace elf3d
