#include <iostream>
#include <string_view>

int elf3d_model_document_test();
int elf3d_model_processing_test();

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
    if (test_case == "model_document") {
        return elf3d_model_document_test();
    }
    if (test_case == "model_processing") {
        return elf3d_model_processing_test();
    }

    std::cerr << "Unknown model test case: " << test_case << '\n';
    return 2;
}
