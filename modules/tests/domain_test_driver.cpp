#include <iostream>
#include <string_view>

int elf3d_interaction_test();
int elf3d_assets_test();
int elf3d_clipping_test();
int elf3d_scene_test();
int elf3d_scene_runtime_adapter_depth_test();

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
    if (test_case == "interaction") {
        return elf3d_interaction_test();
    }
    if (test_case == "assets") {
        return elf3d_assets_test();
    }
    if (test_case == "clipping") {
        return elf3d_clipping_test();
    }
    if (test_case == "scene") {
        return elf3d_scene_test();
    }
    if (test_case == "scene_runtime_adapter_depth") {
        return elf3d_scene_runtime_adapter_depth_test();
    }

    std::cerr << "Unknown domain test case: " << test_case << '\n';
    return 2;
}
