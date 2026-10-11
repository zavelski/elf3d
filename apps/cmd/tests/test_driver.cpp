#include "diagnostics.h"
#include "fixture.h"

#include <iostream>
#include <string_view>

int arguments_test();
int rotation_test();

namespace {

int write_fixture(std::string_view path)
{
    auto fixture = make_fixture();
    if (!fixture) {
        return 1;
    }
    const auto saved = elf3d::save_document(path, fixture.value().view());
    if (!saved) {
        std::cerr << saved.error().message() << '\n';
        return 1;
    }
    return 0;
}

} // namespace

int main(int count, char** arguments)
{
    if (count < 2) {
        return 2;
    }
    const std::string_view command{arguments[1]};
    if (command == "arguments") {
        return arguments_test();
    }
    if (command == "rotation") {
        return rotation_test();
    }
    if (command == "validation") {
        const auto report = elf3d::validate_document(elf3d::DocumentView{});
        elf3d::cmd::print_report(report);
        return report.has_errors() ? 1 : 0;
    }
    if (command == "fixture") {
        if (count != 3) {
            return 2;
        }
        return write_fixture(arguments[2]);
    }
    if (command == "verify") {
        if (count != 5) {
            return 2;
        }
        return round_trip_test(arguments[2], arguments[3],
                               std::string_view{arguments[4]} == "zero");
    }
    return 2;
}
