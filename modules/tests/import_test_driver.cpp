#include <iostream>
#include <string_view>

int elf3d_image_decode_test();
int elf3d_gltf_import_test();
int elf3d_gltf_export_test();
int elf3d_gltf_metadata_round_trip_test();

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
    if (test_case == "image_decode") {
        return elf3d_image_decode_test();
    }
    if (test_case == "model_gltf_import") {
        return elf3d_gltf_import_test();
    }
    if (test_case == "model_gltf_export") {
        return elf3d_gltf_export_test();
    }
    if (test_case == "gltf_metadata") {
        return elf3d_gltf_metadata_round_trip_test();
    }

    std::cerr << "Unknown import test case: " << test_case << '\n';
    return 2;
}
