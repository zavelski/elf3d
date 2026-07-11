#include <iostream>
#include <string_view>

int elf3d_navigation_test();
int elf3d_picking_test();
int elf3d_selection_test();
int elf3d_tool_visibility_test();
int elf3d_tool_measurement_test();
int elf3d_tool_clipping_test();

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
    if (test_case == "navigation") {
        return elf3d_navigation_test();
    }
    if (test_case == "picking") {
        return elf3d_picking_test();
    }
    if (test_case == "selection") {
        return elf3d_selection_test();
    }
    if (test_case == "visibility") {
        return elf3d_tool_visibility_test();
    }
    if (test_case == "measurement") {
        return elf3d_tool_measurement_test();
    }
    if (test_case == "tool_clipping") {
        return elf3d_tool_clipping_test();
    }

    std::cerr << "Unknown tools test case: " << test_case << '\n';
    return 2;
}
