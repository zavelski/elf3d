#include <iostream>
#include <string_view>

int elf3d_renderer_test();
int elf3d_viewport_lifetime_test();

namespace {

[[nodiscard]] std::string_view selected_case(int argument_count, char** arguments) noexcept {
    if (argument_count == 3 && std::string_view{arguments[1]} == "--case") {
        return arguments[2];
    }
    return {};
}

} // namespace

int main(int argument_count, char** arguments) {
    const std::string_view test_case = selected_case(argument_count, arguments);
    if (test_case == "renderer") {
        return elf3d_renderer_test();
    }
    if (test_case == "viewport_lifetime") {
        return elf3d_viewport_lifetime_test();
    }

    std::cerr << "Unknown view test case: " << test_case << '\n';
    return 2;
}
